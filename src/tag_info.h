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

  bool operator < (TagInfo const& right) const
  {
    TagInfo const& left = *this;
    int cmp = 0;
    return left.lineno != right.lineno ? left.lineno < right.lineno :
           left.kind != right.kind ? left.kind < right.kind :
           !!(cmp = left.name.compare(right.name)) ? cmp < 0 :
           !!(cmp = left.file.compare(right.file)) ? cmp < 0 :
           !!(cmp = left.info.compare(right.info)) ? cmp < 0 :
           !!(cmp = left.re.compare(right.re)) ? cmp < 0 :
           false;
  }
};
