#pragma once

#include <platform/string.h>

#include <memory>

struct _GUID;

namespace Facade
{
  namespace Internal
  {
    using WideString = Platform::WideString;
  
    WideString ToString(std::string const& str);
    std::string ToStdString(WideString const& str);
    std::unique_ptr<_GUID> StringToGuid(std::string const& str);
  }
}
