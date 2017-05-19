#include "../subdevil.h"
#include "../usb_common.h"
#include "../utils.h"
#include "interop.h"

#include <sys/param.h>

#include <stdio.h>

#include <mach/mach_error.h>
#include <mach/mach_port.h>

#include <IOKit/IOBSD.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>

#include <DiskArbitration/DiskArbitration.h>

#include <unordered_map>


// The current OSX version
const auto CURRENT_SUPPORTED_VERSION = __MAC_OS_X_VERSION_MAX_ALLOWED;
// El Capitan is 101100 (in AvailabilityInternal.h)
const auto EL_CAPITAN = 101100;
// IOUSBDevice has become IOUSBHostDevice in El Capitan
const char *SERVICE_MATCHER = CURRENT_SUPPORTED_VERSION < EL_CAPITAN ? "IOUSBDevice" : "IOUSBHostDevice";

namespace Subdevil
{
  typedef std::unordered_map<std::string, USBDevicePtr> DeviceMap;

  // TODO: Remove me!
  static DeviceMap gAllDevices;

  bool unmount(const std::string &uid)
  {
    USBDevicePtr usbInfo = getDevice(uid);

    // Only unmount if we're actually mounted
    if (usbInfo != nullptr && !usbInfo->mountPoint.empty()) {
      DASessionRef daSession = DASessionCreate(kCFAllocatorDefault);
      assert(daSession != nullptr);

      // Attempt to actually reference the path
      CFURLRef volumePath = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
                                                                    (const UInt8 *)usbInfo->mountPoint.c_str(),
                                                                    usbInfo->mountPoint.size(),
                                                                    true);
      assert(volumePath != nullptr);

      // Attempt to get a disk reference
      DADiskRef disk = DADiskCreateFromVolumePath(kCFAllocatorDefault,
                                                  daSession,
                                                  volumePath);

      if (disk != nullptr)
        {
          // Attempt to unmount the disk
          // TODO: pass error callback and escalate error to JS.
          DADiskUnmount(disk, kDADiskUnmountOptionDefault, nullptr, NULL);
          CFRelease(disk);

          // Rewrite mount as empty
          usbInfo->mountPoint = "";

          return true;
        }

      CFRelease(volumePath);
      CFRelease(daSession);
    }

    return false;
  }

  USBDevicePtr getDevice(const std::string &uid)
  {
    return gAllDevices[uid];
  }

  static USBDevicePtr _findDeviceByLocationID(const DeviceMap &map, int locationID)
  {
    for(auto it = map.begin(); it != map.end(); ++it) {
      auto device = it->second;

      if(device->locationID == locationID) {
        return device;
      }
    }

    return nullptr;
  }

  // TODO: Make this referentialy transparent, in the way that it
  // TODO: doesn't modify gAllDevices.
  static USBDevicePtr usbServiceObject(io_service_t usbService)
  {
    CFMutableDictionaryRef properties;
    kern_return_t kr = IORegistryEntryCreateCFProperties(usbService,
                                                         &properties,
                                                         kCFAllocatorDefault,
                                                         kNilOptions);

    CORE_DEBUG("Creating kernel interface...");

    if (kr != kIOReturnSuccess) {
      CORE_ERROR("IORegistryEntryCreateCFProperties() failed: " + std::string(mach_error_string(kr)));

      return nullptr;
    }

    int locationID = PROP_VAL_INT(properties, kUSBDevicePropertyLocationID);

    CORE_DEBUG("Received location ID: " + std::to_string(locationID));

    // Attempt to receive the device
    USBDevicePtr usbInfo = _findDeviceByLocationID(gAllDevices, locationID);

    if (usbInfo == nullptr) {
      CORE_DEBUG("USB device not found, creating a new one...");
      // We don't have one, so create a new one
      usbInfo = USBDevicePtr(new USBDevice);
    }

    usbInfo->locationID    = locationID;
    usbInfo->vendorID      = PROP_VAL_INT(properties, kUSBVendorID);
    usbInfo->productID     = PROP_VAL_INT(properties, kUSBProductID);
    usbInfo->serialNumber  = PROP_VAL_STR(properties, kUSBSerialNumberString);
    usbInfo->product       = PROP_VAL_STR(properties, kUSBProductString);
    usbInfo->vendor        = PROP_VAL_STR(properties, kUSBVendorString);
    usbInfo->uid           = uniqueDeviceID(usbInfo);

    CFRelease(properties);

    // Register in storage
    gAllDevices[usbInfo->uid] = usbInfo;

    CORE_DEBUG("Attempting to access BSD name...");

    CFStringRef bsdName = (CFStringRef)IORegistryEntrySearchCFProperty(usbService,
                                                                       kIOServicePlane,
                                                                       CFSTR(kIOBSDNameKey),
                                                                       kCFAllocatorDefault,
                                                                       kIORegistryIterateRecursively);
    if (bsdName != nullptr) {
      char bsdNameBuf[4096];
      sprintf( bsdNameBuf, "/dev/%ss1", cfStringRefToCString(bsdName));
      char* bsdNameC = &bsdNameBuf[0];

      CORE_INFO("Found BSD Name: " + std::string(bsdNameC));

      DASessionRef daSession = DASessionCreate(kCFAllocatorDefault);
      assert(daSession != nullptr);

      CORE_DEBUG("Creating disk interface..");

      DADiskRef disk = DADiskCreateFromBSDName(kCFAllocatorDefault,
                                               daSession, bsdNameC);

      if (disk != nullptr) {
        CORE_DEBUG("Creating description...");

        CFDictionaryRef desc = DADiskCopyDescription(disk);

        if (desc != nullptr) {
          CFURLRef url    = static_cast<CFURLRef>(CFDictionaryGetValue(desc, kDADiskDescriptionVolumePathKey));

          char volumePath[MAXPATHLEN];

          if(!CFURLGetFileSystemRepresentation(url, true, (UInt8*) volumePath, MAXPATHLEN))
          {
            CORE_ERROR("Could not get the file system representation of the volume path.");
          }
          else if(strlen(volumePath))
          {
              usbInfo->mountPoint = volumePath;

              CORE_INFO("Found volume path: " + std::string(volumePath));
          }

          CFRelease(desc);
        }

        CFRelease(disk);
        CFRelease(daSession);
      }
    }

    return usbInfo;
  }

  std::vector<USBDevicePtr> getDevices()
  {
    mach_port_t masterPort;
    kern_return_t kr = IOMasterPort(MACH_PORT_NULL, &masterPort);

    assert(kr == kIOReturnSuccess);

    CFDictionaryRef usbMatching = IOServiceMatching(SERVICE_MATCHER);

    assert(usbMatching != nullptr);

    std::vector<USBDevicePtr> devices;

    io_iterator_t iter = 0;
    kr = IOServiceGetMatchingServices(kIOMasterPortDefault,
                                      usbMatching,
                                      &iter);

    if (kr != kIOReturnSuccess)
      {
        CORE_ERROR("IOServiceGetMatchingServices() failed: %s" + std::string(mach_error_string(kr)));
      }
    else
      {
        io_service_t usbService;

        while ((usbService = IOIteratorNext(iter)) != 0) {
          CORE_DEBUG("IOIteratorNext found USB device");

          USBDevicePtr usbInfo = usbServiceObject(usbService);

          if (usbInfo != nullptr) {
            CORE_DEBUG("Adding USB info to cache");
            devices.push_back(usbInfo);
          }

          CORE_DEBUG("Releasing USB service resources");
          IOObjectRelease(usbService);
        }
      }


    CORE_DEBUG("Deallocating master port");
    mach_port_deallocate(mach_task_self(), masterPort);

    return devices;
  }

}
