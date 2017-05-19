#include "../subdevil.h"
#include "../usb_common.h"

#include "../utils.h"

#include <windows.h>
#include <windowsx.h>
#include <devguid.h>
#include <initguid.h>
#include <usbiodef.h>
#include <setupapi.h>
#include <winioctl.h>
#include <usbioctl.h>
#include <cfgmgr32.h>
#include <assert.h>

#include <unordered_map>
#include <bitset>

#define FORMAT_FLAGS (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS)

#define _PSTR(str) reinterpret_cast<PSTR>(str)

namespace Subdevil {
  typedef unsigned long ulong;
  typedef unsigned int  uint;

  typedef std::unordered_map<std::string, USBDevicePtr> DeviceMap;

  static DeviceMap gAllDevices;

  /**
   * Create a new windows SP type and automatically set the property cbSize
   * to the sizeof the type, as required by many functions in the windows
   * API.
   */
  template<typename SP_T>
  SP_T _createSPType()
  {
    SP_T sp;
    sp.cbSize = sizeof(sp);

    return sp;
  }

  static bool _deviceProperty(HDEVINFO hDeviceInfo, PSP_DEVINFO_DATA device_info_data,
                              DWORD property, std::string &buf_str)
  {
    DWORD bufLen = MAX_PATH;
    DWORD nSize;

    char *buf = static_cast<char*>(malloc(bufLen));
    assert(buf != NULL);

    bool ok = SetupDiGetDeviceRegistryProperty(hDeviceInfo, device_info_data,
                                               property, NULL, (PBYTE)buf,
                                               bufLen, &nSize);

    if (ok)
      buf_str.assign(buf);
    else
      CORE_ERROR("Failed to get device registry property: " + std::to_string(property));

    free(buf);

    return ok;
  }

  static bool _parseDeviceID(const char *device_path, std::string &vid,
                             std::string &pid, std::string &serial)
  {
    const char *p = device_path;

    if (strncmp(p, "USB\\", 4) != 0) {
      return false;
    }
    p += 4;

    if (strncmp(p, "VID_", 4) != 0) {
      return false;
    }
    p += 4;

    vid.assign("0x");

    while (*p != '&') {
      if (*p == '\0') {
        return false;
      }
      vid.push_back(tolower(*p));
      p++;
    }
    p++;

    if (strncmp(p, "PID_", 4) != 0) {
      return false;
    }
    p += 4;

    // Clear serial, just to be sure we don't have any left over state.
    serial.clear();

    pid.assign("0x");

    while (*p != '\\') {
      if (*p == '\0') {
        return false;
      }
      if (*p == '&') {
        // No serial.
        return true;
      }

      pid.push_back(tolower(*p));
      p++;
    }
    p++;

    while (*p != '\0') {
      if (*p == '&') {
        return true;
      }

      serial.push_back(toupper(*p));
      p++;
    }

    return true;
  }


  static ULONG _deviceNumberFromHandle(HANDLE handle)
  {
    STORAGE_DEVICE_NUMBER sdn;
    sdn.DeviceNumber = -1;

    DWORD bytesReturned = 0; // Ignored

    if (!DeviceIoControl(handle, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, &sdn,
                        sizeof(sdn), &bytesReturned, NULL)) {
      CORE_WARNING("Failed to get device number from handle.");

      return -1;
    }

    return sdn.DeviceNumber;
  }

  static std::string _driveForDeviceNumber(ULONG deviceNumber)
  {
      std::bitset<32> drives(GetLogicalDrives());

      // We start iteration from ANSI C (65)
      for (char c = 'D'; c <= 'Z'; c++) {
        if (!drives[c - 'A']) {
          continue;
        }

        std::string path = std::string("\\\\.\\") + c + ":";

        HANDLE driveHandle = CreateFileA(path.c_str(), GENERIC_READ,
                                         FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                                         FILE_FLAG_NO_BUFFERING | FILE_FLAG_RANDOM_ACCESS, NULL);

        if (driveHandle == INVALID_HANDLE_VALUE) {
          CORE_ERROR("Failed to get file handle to " + path);
          continue;
        }

        ULONG num = _deviceNumberFromHandle(driveHandle);
        if (num == deviceNumber) {
          return std::string(1, c).append(":");
        }

        CloseHandle(driveHandle);
      }

      CORE_ERROR("Failed to get drive for device number: " + std::to_string(deviceNumber));
      return "";
    }

  typedef struct DeviceSPData {
    SP_DEVINFO_DATA info;
    SP_DEVICE_INTERFACE_DATA inter;
    SP_DEVICE_INTERFACE_DETAIL_DATA *interDetails;
  } SPData;

  static USBDevicePtr _findDeviceByLocationID(const DeviceMap &map, int locationID) {
    for(auto it = map.begin(); it != map.end(); ++it) {
      auto device = it->second;

      if(device->locationID == locationID) {
        return device;
      }
    }

    return nullptr;
  }

  std::vector<DeviceSPData> _deviceSPs(HDEVINFO hDeviceInfo, const GUID *guid)
  {
    std::vector<DeviceSPData> sps;

    uint index = 0;

    while(true)
      {
        SP_DEVINFO_DATA spDevInfoData = _createSPType<SP_DEVINFO_DATA>();

        if (!SetupDiEnumDeviceInfo(hDeviceInfo, index, &spDevInfoData))
          {
            if (GetLastError() != ERROR_NO_MORE_ITEMS)
              CORE_ERROR("Failed to retrieve device information.");

            break; // We're out of devices
          }

        SP_DEVICE_INTERFACE_DATA spDeviceInterfaceData = _createSPType<SP_DEVICE_INTERFACE_DATA>();

        if (!SetupDiEnumDeviceInterfaces(hDeviceInfo, 0, guid, index, &spDeviceInterfaceData)) {
          CORE_ERROR("Failed to get device interfaces.");
          continue; // Invalid device, skip it
        }

        DeviceSPData deviceSP;
        deviceSP.info = spDevInfoData;
        deviceSP.inter = spDeviceInterfaceData;

        sps.push_back(deviceSP);

        ++index;
      }

    return sps;
  }

  USBDevicePtr _extractUSBDeviceData(HDEVINFO hDeviceInfo, DeviceSPData &sp)
  {
    std::string deviceName;
    if (!_deviceProperty(hDeviceInfo, &sp.info, SPDRP_FRIENDLYNAME, deviceName)) {
      return nullptr;
    }

    std::string vendor;
    if (!_deviceProperty(hDeviceInfo, &sp.info, SPDRP_MFG, vendor)) {
      return nullptr;
    }

    ULONG interfaceDetailLen = MAX_PATH;
    SP_DEVICE_INTERFACE_DETAIL_DATA *spDeviceInterfaceDetail =
      (SP_DEVICE_INTERFACE_DETAIL_DATA *)malloc(interfaceDetailLen);
    assert(spDeviceInterfaceDetail != NULL);
    spDeviceInterfaceDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

    SP_DEVINFO_DATA spDeviceInfoData = _createSPType<SP_DEVINFO_DATA>();

    if (!SetupDiGetDeviceInterfaceDetail(hDeviceInfo, &sp.inter, spDeviceInterfaceDetail,
                                         interfaceDetailLen, &interfaceDetailLen, &spDeviceInfoData)) {
      CORE_ERROR("Failed to retrieve device interface details.");
      return nullptr;
    }

    HANDLE handle = CreateFile(spDeviceInterfaceDetail->DevicePath,
                               0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING, 0, NULL);

    if (handle == INVALID_HANDLE_VALUE) {
      CORE_ERROR("Failed to create file handle");
      return nullptr;
    }

    free(spDeviceInterfaceDetail);

    std::string mount;
    ULONG deviceNumber = _deviceNumberFromHandle(handle);
    if (deviceNumber != -1) {
      mount = _driveForDeviceNumber(deviceNumber);

      CORE_DEBUG("Found device number: " + std::to_string(deviceNumber));

      if(mount.empty()) {
        CORE_DEBUG("Mount point not found");
      } else {
        CORE_DEBUG("Found mount point: " + mount);
      }
    } else {
      CORE_ERROR("Failed to get device number for " + deviceName);
    }

    CloseHandle(handle);

    DEVINST devInstParent;
    if (CM_Get_Parent(&devInstParent, spDeviceInfoData.DevInst, 0) != CR_SUCCESS) {
      return nullptr;
    }

    char devInstParentID[MAX_DEVICE_ID_LEN];
    if (CM_Get_Device_ID(devInstParent, _PSTR(devInstParentID), MAX_DEVICE_ID_LEN, 0) != CR_SUCCESS) {
      return nullptr;
    }

    std::string vid, pid, serial;
    if (!_parseDeviceID(devInstParentID, vid, pid, serial)) {
      return nullptr;
    }

    int locationID = static_cast<int>(deviceNumber);

    CORE_DEBUG("Found location ID: " + std::to_string(locationID));

    USBDevicePtr pUsbDevice = _findDeviceByLocationID(gAllDevices, locationID);

    if(!pUsbDevice) {
      CORE_DEBUG("USB device with given location ID not found, creating a new one...");
      pUsbDevice = USBDevicePtr(new USBDevice());
    }

    // Emulate location ID using device numbers
    pUsbDevice->locationID = locationID;
    // Convert HEX values to integers
    pUsbDevice->productID = std::stoi(pid, nullptr, 0);
    pUsbDevice->vendorID = std::stoi(vid, nullptr, 0);
    pUsbDevice->product = deviceName;
    pUsbDevice->serialNumber = serial;
    pUsbDevice->vendor = vendor;
    pUsbDevice->mountPoint = mount;
    pUsbDevice->uid = uniqueDeviceID(pUsbDevice);

    // Add to global device map
    gAllDevices[pUsbDevice->uid] = pUsbDevice;

    return pUsbDevice;
  }

  std::vector<USBDevicePtr> getDevices()
  {
    std::vector<USBDevicePtr> ret;

    const GUID *guid = &GUID_DEVINTERFACE_DISK;
    HDEVINFO hDeviceInfo = SetupDiGetClassDevs(guid, NULL, NULL,
                                               (DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));

    if (hDeviceInfo != INVALID_HANDLE_VALUE) {
      std::vector<DeviceSPData> spsData = _deviceSPs(hDeviceInfo, guid);

      for (auto &sp : spsData)
        {
          auto pDevice = _extractUSBDeviceData(hDeviceInfo, sp);

          if (pDevice != nullptr) {
            ret.push_back(pDevice);
          }
        }
    }

    return ret;
  }

  USBDevicePtr getDevice(const std::string &uid)
  {
    return gAllDevices[uid];
  }

  bool unmount(const std::string &uid)
  {
    return false;
	}
}  // namespace usb_driver
