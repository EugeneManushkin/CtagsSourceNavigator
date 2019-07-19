#include "string.h"

#include <windows.h>

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace FacadeInternal
{
  void StringToGuid(const std::string& str, _GUID& guid)
  {
    std::string lowercased(str);
    std::transform(lowercased.begin(), lowercased.end(), lowercased.begin(), ::tolower);
    auto sannedItems = sscanf(lowercased.c_str(),
      "{%8x-%4hx-%4hx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx}",
      &guid.Data1, &guid.Data2, &guid.Data3,
      &guid.Data4[0], &guid.Data4[1], &guid.Data4[2], &guid.Data4[3],
      &guid.Data4[4], &guid.Data4[5], &guid.Data4[6], &guid.Data4[7]);
  
    if (sannedItems != 11)
      throw std::invalid_argument("Invalid guid string specified");
  }
  
  WideString ToString(std::string const& str)
  {
    UINT const codePage = CP_ACP;
    auto sz = MultiByteToWideChar(codePage, 0, str.c_str(), static_cast<int>(str.length()), nullptr, 0);
    if (!sz)
      return WideString();
  
    std::vector<wchar_t> buffer(sz);
    MultiByteToWideChar(codePage, 0, str.c_str(), static_cast<int>(str.length()), &buffer[0], sz);
    return WideString(buffer.begin(), buffer.end());
  }
  
  std::string ToStdString(WideString const& str)
  {
    UINT const codePage = CP_ACP;
    auto sz = WideCharToMultiByte(codePage, 0, str.c_str(), static_cast<int>(str.length()), nullptr, 0, nullptr, nullptr);
    if (!sz)
      return std::string();
  
    std::vector<char> buffer(sz);
    WideCharToMultiByte(codePage, 0, str.c_str(), static_cast<int>(str.length()), &buffer[0], sz, nullptr, nullptr);
    return std::string(buffer.begin(), buffer.end());
  }
}
