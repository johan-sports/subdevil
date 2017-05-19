#include "interop.h"

#include <sys/param.h>

char *cfStringRefToCString(CFStringRef cfString)
{
  if (!cfString) return "";

  static char string[2048];

  string[0] = '\0';
  CFStringGetCString(cfString,
                     string,
                     MAXPATHLEN,
                     kCFStringEncodingASCII);

  return &string[0];
}

char *cfTypeToCString(CFTypeRef cfString)
{
  if(!cfString) {
    return "";
  }

  // Attempt to convert it to a string
  CFStringRef stringRef = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@"), cfString);

  return cfStringRefToCString(stringRef);
}

int cfTypeToInteger(CFTypeRef cfNumber)
{
  if(CFGetTypeID(cfNumber) == CFNumberGetTypeID()) {
    int num = 0;

    CFNumberGetValue((CFNumberRef) cfNumber, kCFNumberIntType, &num);

    return num;
  } else {
    throw "Invalid number passed to cfTypeToInteger";
  }
}
