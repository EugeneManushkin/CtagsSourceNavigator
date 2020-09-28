#pragma once

#include "tag_info.h"

#include <memory>
#include <vector>

namespace Tags
{
namespace Internal
{
  class TagsCache
  {
  public:
    virtual ~TagsCache() = default;
    virtual std::vector<TagInfo> Get(size_t limit = 0) const = 0;
    virtual std::vector<std::pair<TagInfo, size_t>> GetStat() const = 0;
    virtual void Insert(TagInfo const&, size_t frequency = 1) = 0;
    virtual void Erase(TagInfo const&) = 0;
    virtual void SetCapacity(size_t) = 0;
    virtual void ResetCounters() = 0;
  };

  std::shared_ptr<TagsCache> CreateTagsCache(size_t);
}
}
