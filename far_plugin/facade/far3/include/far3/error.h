#pragma once

#include <string>
#include <utility>

namespace Far3
{
  struct Error
  {
    Error(int code, std::string const& fieldName = "", std::string const& fieldValue = "")
      : Code(code)
      , Field(std::move(fieldName), std::move(fieldValue))
    {
    }

    int Code;
    std::pair<std::string, std::string> Field;
  };
}
