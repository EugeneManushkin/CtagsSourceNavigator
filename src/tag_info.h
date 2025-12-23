#pragma once

#include <memory>
#include <string>

struct TagInfo
{
  std::string name;
  std::string file;
  std::string re;
  int lineno;
  char kind;
  std::string info;

  struct OwnerInfo
  {
    std::string TagsFile;
  };
  std::shared_ptr<OwnerInfo> Owner;

  TagInfo()
    : kind(0)
    , lineno(-1)
  {
  }

  bool operator < (TagInfo const& right) const
  {
    TagInfo const& left = *this;
    int cmp = 0;
    return left.name.empty() && right.name.empty() ? left.file.compare(right.file) < 0 :
           left.lineno != right.lineno ? left.lineno < right.lineno :
           left.kind != right.kind ? left.kind < right.kind :
           !!(cmp = left.name.compare(right.name)) ? cmp < 0 :
           !!(cmp = left.file.compare(right.file)) ? cmp < 0 :
           !!(cmp = left.info.compare(right.info)) ? cmp < 0 :
           !!(cmp = left.re.compare(right.re)) ? cmp < 0 :
           false;
  }
};
