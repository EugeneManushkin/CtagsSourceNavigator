#include "wide_string.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <vector>

namespace Far3
{
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
