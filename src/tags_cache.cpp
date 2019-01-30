#include "tags_cache.h"

#include <map>

namespace
{
  struct TagLess
  {
    bool operator()(TagInfo const& left, TagInfo const& right) const
    {
      int cmp = 0;
      return left.lineno != right.lineno ? left.lineno < right.lineno :
             left.type != right.type ? left.type < right.type :
             !!(cmp = left.name.compare(right.name)) ? cmp < 0 :
             !!(cmp = left.file.compare(right.file)) ? cmp < 0 :
             !!(cmp = left.info.compare(right.info)) ? cmp < 0 :
             !!(cmp = left.re.compare(right.re)) ? cmp < 0 :
             false;
    }
  };

  class TagsCacheImpl : public TagsInternal::TagsCache
  {
  public:
    TagsCacheImpl(size_t capacity)
      : Capacity(capacity)
    {
    }

    virtual std::vector<std::pair<TagInfo, size_t>> Get() const override
    {
      std::vector<std::pair<TagInfo, size_t>> result;
      auto limit = Capacity;
      for (auto i = Frequency.begin(); limit > 0 && i != Frequency.end(); ++i, --limit)
        result.push_back(std::make_pair(*i->second, i->first));

      return result;
    }

    virtual void Insert(TagInfo const& tag) override
    {
      auto found = Tags.lower_bound(tag);
      size_t freq = 1;
      if (found != Tags.end() && !TagLess()(tag, found->first))
      {
        freq = found->second->first + 1;
        Frequency.erase(found->second);
      }
      else
      {
        FitToCapacity();
        found = Tags.insert(found, std::make_pair(tag, Frequency.end()));
      }

      found->second = Frequency.insert(std::make_pair(freq, &found->first));
    }

    virtual void SetCapacity(size_t capacity) override
    {
      Capacity = capacity;
    }

  private:
    void FitToCapacity()
    {
      for (auto size = Tags.size(); size > Capacity; --size)
      {
        Tags.erase(*Frequency.rbegin()->second);
        Frequency.erase(--Frequency.end());
      }
    }

    using FrequencyToTagInfo = std::multimap<size_t, TagInfo const*>;
    size_t Capacity;
    FrequencyToTagInfo Frequency;
    std::map<TagInfo, FrequencyToTagInfo::iterator, TagLess> Tags;
  };
}

namespace TagsInternal
{
  std::shared_ptr<TagsCache> CreateTagsCache(size_t capacity)
  {
    return std::shared_ptr<TagsCache>(new TagsCacheImpl(capacity));
  }
}
