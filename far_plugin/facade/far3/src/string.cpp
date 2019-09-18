#include "string.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace Facade
{
  namespace Internal
  {
    std::unique_ptr<_GUID> StringToGuid(const std::string& str)
    {
      _GUID guid = {};
      std::string lowercased(str);
      std::transform(lowercased.begin(), lowercased.end(), lowercased.begin(), ::tolower);
      auto sannedItems = sscanf(lowercased.c_str(),
        "{%8x-%4hx-%4hx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx}",
        &guid.Data1, &guid.Data2, &guid.Data3,
        &guid.Data4[0], &guid.Data4[1], &guid.Data4[2], &guid.Data4[3],
        &guid.Data4[4], &guid.Data4[5], &guid.Data4[6], &guid.Data4[7]);
    
      if (sannedItems != 11)
        throw std::invalid_argument("Invalid guid string specified");

      return std::unique_ptr<_GUID>(new _GUID(guid));
    }
    
    WideString ToString(std::string const& str)
    {
      return Platform::ToString(str);
    }
    
    std::string ToStdString(WideString const& str)
    {
      return Platform::ToStdString(str);
    }
  }
}
