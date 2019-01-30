#pragma once

#include "tag_info.h"

#include <memory>
#include <vector>

namespace TagsInternal
{
  class TagsCache
  {
  public:
    virtual ~TagsCache() = default;
    virtual std::vector<std::pair<TagInfo, size_t>> Get() const = 0;
    virtual void Insert(TagInfo const&) = 0;
    virtual void SetCapacity(size_t) = 0;
  };

  std::shared_ptr<TagsCache> CreateTagsCache(size_t);
}
