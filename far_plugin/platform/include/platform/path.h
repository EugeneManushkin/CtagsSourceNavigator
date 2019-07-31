#pragma once

#include <string>

namespace Platform
{
  char PathSeparator();
  bool IsPathSeparator(char c);
  std::string GetDirOfFile(std::string const& fileName);
  std::string JoinPath(std::string const& dirPath, std::string const& name);
  bool IsAbsolutePath(char const* path);
  std::string ExpandEnvString(std::string const& str);
}
