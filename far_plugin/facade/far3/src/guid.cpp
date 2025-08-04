#include <far3/guid.h>
#include <far3/wide_string.h>

#include <objbase.h>

#include <stdexcept>

namespace Far3
{
  Guid::Guid(char const* str)
  {
    if (*str != '{' || ::CLSIDFromString(ToString(str).c_str(), &GuidValue) != NOERROR)
      throw std::invalid_argument("Invalid guid string specified");
  }

  ::GUID const* Guid::operator&() const
  {
    return &GuidValue;
  }
}
