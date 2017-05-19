#ifndef _SUBDEVIL_USB_COMMON_H__
#define _SUBDEVIL_USB_COMMON_H__

#include "subdevil.h"
#include <string>

namespace Subdevil
{
  std::string uniqueDeviceID(const USBDevicePtr device);
}

#endif // _SUBDEVIL_USB_COMMON_H__
