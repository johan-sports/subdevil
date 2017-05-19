#ifndef _SUBDEVIL_MAC_UTILS_H__
#define _SUBDEVIL_MAC_UTILS_H__

#include <stdio.h>

#include <CoreFoundation/CoreFoundation.h>

////////////////////////////////////////////////////////////////////////////////
// Converters
////////////////////////////////////////////////////////////////////////////////
/**
 * Convert the given CFString to a CString
 */
char *cfStringRefToCString(CFStringRef cfString);
/**
 * Attempt to convert a CFTypeRef to a c string. This
 * could possibly fail and currently causes segfaults.
 */
char *cfTypeToCString(CFTypeRef cfString);

/**
 * Convert a CFType to an integer
 */
int cfTypeToInteger(CFTypeRef cfString);

////////////////////////////////////////////////////////////////////////////////
// Working with data structures
////////////////////////////////////////////////////////////////////////////////
#define PROP_VAL_STR(dict, key)                               \
  ({                                                          \
    CFTypeRef _str = CFDictionaryGetValue(dict, CFSTR(key));  \
    cfTypeToCString(_str);                                    \
  })

#define PROP_VAL_INT(dict, key)                               \
  ({                                                          \
    CFTypeRef _num = CFDictionaryGetValue(dict, CFSTR(key));  \
    cfTypeToInteger(_num);                                    \
  })

#endif // _SUBDEVIL_MAC_UTILS_H__
