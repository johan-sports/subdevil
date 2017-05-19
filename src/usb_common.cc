#include "usb_common.h"

static const size_t BUF_SIZE = 100;

namespace Subdevil
{
  std::string uniqueDeviceID(const USBDevicePtr device)
  {
    static unsigned long uniqueID = 0;
    std::string uid = device->uid;

    if(uid.empty()) {
      uid.append(std::to_string(device->vendorID));
      uid.append("-");
      uid.append(std::to_string(device->productID));

      if(!device->serialNumber.empty()) {
        uid.append("-");
        uid.append(device->serialNumber);
      }

      // Always generate unique ID data
      char buf[BUF_SIZE];

      snprintf(buf, sizeof(buf), "%lu", uniqueID++);

      uid.append("-");
      uid.append(buf);
    }

    return uid;
  }
}
