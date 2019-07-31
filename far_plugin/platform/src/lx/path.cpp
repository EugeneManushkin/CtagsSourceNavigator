#include <platform/path.h>

namespace Platform
{
  bool IsAbsolutePath(char const* path)
  {
    return path && path[0] == '/' || path[0] == '~';
  }

  std::string ExpandEnvString(std::string const& str)
  {
    return str;
  }
}
