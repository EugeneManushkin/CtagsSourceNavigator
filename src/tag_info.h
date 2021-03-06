#pragma once

#include <string>

struct TagInfo
{
  std::string name;
  std::string file;
  std::string re;
  int lineno;
  char type;
  std::string info;

  struct
  {
    std::string TagsFile;
  } Owner;

  TagInfo()
    : type(0)
    , lineno(-1)
    , Owner{}
  {
  }
};
