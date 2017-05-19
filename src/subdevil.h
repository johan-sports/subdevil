#ifndef _SUBDEVIL_H_
#define _SUBDEVIL_H_

#include <string>
#include <vector>
#include <memory>

namespace Subdevil
{
  typedef struct USBDevice {
    std::string uid;           // Unique ID for each device.
    int locationID;            // USB Location ID data.
    int productID;             // USB product ID data.
    int vendorID;              // USB vendor ID data.
    std::string product;       // The product name.
    std::string serialNumber;  // the full serial number. Can be empty.
    std::string vendor;        // The vendor name.
    std::string mountPoint;    // The disk mount point. Can be empty.
  } USBDevice;

  // Shared resource to the USB device
  // TODO: Make all of this const
  typedef std::shared_ptr<USBDevice> USBDevicePtr;

  /**
   * Get data for all connected devices.
   */
  std::vector<USBDevicePtr> getDevices();
  /**
   * Get a device with the given UID.
   */
  USBDevicePtr getDevice(const std::string &uid);

  /**
   * Unmount the device with the given UID.
   */
  bool unmount(const std::string &uid);

  // TODO: Add a Mount function
}

#endif  // _SUBDEVIL_H_
