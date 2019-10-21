#pragma once

#include "tag_info.h"

#include <vector>

namespace Tags
{
  class Selector
  {
  public:
    virtual ~Selector() = default;
    virtual std::vector<TagInfo> GetByName(const char* name) const = 0;
    virtual std::vector<TagInfo> GetByNamePart(const char* part) const = 0;
    virtual std::vector<TagInfo> GetFiles(const char* path) const = 0;
    virtual std::vector<TagInfo> GetFilesByPart(const char* part) const = 0;
    virtual std::vector<TagInfo> GetClassMembers(const char* classname) const = 0;
    virtual std::vector<TagInfo> GetByFile(const char* file) const = 0;
    virtual std::vector<TagInfo> GetCachedTags(bool getFiles) const = 0;
  };
}
