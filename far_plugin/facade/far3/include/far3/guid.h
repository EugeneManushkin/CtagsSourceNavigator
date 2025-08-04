#pragma once

#include <guiddef.h>

namespace Far3
{
  class Guid
  {
  public:
    explicit Guid(char const* str);
    ::GUID const* operator&() const;
    operator ::GUID const&() const
    {
      return GuidValue;
    }

  private:
    ::GUID GuidValue;
  };
}
