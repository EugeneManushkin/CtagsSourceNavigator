#pragma once

#include <memory>
#include <string>

struct _GUID;

namespace Facade
{
  namespace Internal
  {
    using WideString = std::basic_string<wchar_t>;
  
    WideString ToString(std::string const& str);
    std::string ToStdString(WideString const& str);
    std::unique_ptr<_GUID> StringToGuid(std::string const& str);
  }
}
