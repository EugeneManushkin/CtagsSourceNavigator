#pragma once

#include <string>

namespace Platform
{
  using WideString = std::basic_string<wchar_t>;
  WideString ToString(std::string const& str);
  std::string ToStdString(WideString const& str);
}
