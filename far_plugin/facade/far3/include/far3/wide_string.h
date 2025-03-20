#pragma once

#include <string>

namespace Far3
{
  using WideString = std::basic_string<wchar_t>;

  WideString ToString(std::string const& str);
  std::string ToStdString(WideString const& str);
}
