#ifndef _SUBDEVIL_UTILS_FORMATTERS_H__
#define _SUBDEVIL_UTILS_FORMATTERS_H__

#include <string>
#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////
// Formatters
////////////////////////////////////////////////////////////////////////////////
namespace Subdevil
{
  namespace Utils
  {
    template<typename T>
      std::string hexify(T val)
      {
        std::string str = std::to_string(val);

        char tmp[str.size()];
        snprintf(tmp, sizeof(tmp), "0x%lx", atol(str.c_str()));

        return tmp;
      }
  }
}

#endif // _SUBDEVIL_UTILS_FORMATTERS_H__
