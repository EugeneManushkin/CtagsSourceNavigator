#pragma once

#include <string>

struct TagInfo
{
  std::string name;
  std::string file;
  std::string declaration;
  std::string re;
  int lineno;
  char type;
  std::string info;

  TagInfo()
    : type(0)
    , lineno(-1)
  {
  }
};
