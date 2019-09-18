#include <platform_internal.h>
#include <platform/path.h>

namespace Platform
{
  using Internal::DefaultPathSeparator;
  using Internal::PathSeparators;

  char PathSeparator()
  {
    return DefaultPathSeparator;
  }

  bool IsPathSeparator(char c)
  {
    return c && (c == PathSeparators[0] || c == PathSeparators[1]);
  }

  std::string GetDirOfFile(std::string const& fileName)
  {
    auto pos = fileName.find_last_of(PathSeparators);
    return pos == std::string::npos ? std::string() : fileName.substr(0, pos);
  }

  std::string JoinPath(std::string const& dirPath, std::string const& name)
  {
    return dirPath.empty() || IsPathSeparator(dirPath.back()) ? dirPath + name : dirPath + std::string(1, DefaultPathSeparator) + name;
  }
}