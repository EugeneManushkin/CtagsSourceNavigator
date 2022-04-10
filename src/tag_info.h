#pragma once

#include <string>

struct TagInfo
{
  std::string name;
  std::string file;
  std::string re;
  int lineno;
  char kind;
  std::string info;

  struct
  {
    std::string TagsFile;
  } Owner;

  TagInfo()
    : kind(0)
    , lineno(-1)
    , Owner{}
  {
  }
};
