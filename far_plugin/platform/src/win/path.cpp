#include <platform/path.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <vector>

namespace Platform
{
  bool IsAbsolutePath(char const* path)
  {
    return path && path[0] && path[1] == ':';
  }

  std::string ExpandEnvString(std::string const& str)
  {
    auto sz = ::ExpandEnvironmentStringsA(str.c_str(), nullptr, 0);
    if (!sz)
      return std::string();

    std::vector<char> buffer(sz);
    ::ExpandEnvironmentStringsA(str.c_str(), &buffer[0], sz);
    return std::string(buffer.begin(), --buffer.end());
  }
}
