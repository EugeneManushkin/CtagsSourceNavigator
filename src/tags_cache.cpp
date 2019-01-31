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
      for (auto i = Frequency.rbegin(); limit > 0 && i != Frequency.rend(); ++i, --limit)
        result.push_back(std::make_pair(*i->second, i->first));

      return result;
    }

    virtual void Insert(TagInfo const& tag, size_t freq) override
    {
      auto desiredTag = Tags.lower_bound(tag);
      bool found = desiredTag != Tags.end() && !TagLess()(tag, desiredTag->first);
      freq = !freq ? 1 : freq;
      freq += found ? desiredTag->second->first : 0;
      if (found && Capacity >= Tags.size())
      {
        Frequency.erase(desiredTag->second);
      }
      else
      {
        desiredTag = Capacity <= Tags.size() ? Tags.end() : desiredTag;
        Resize(Capacity - 1);
        desiredTag = Tags.insert(desiredTag, std::make_pair(tag, Frequency.end()));
      }

      desiredTag->second = Frequency.insert(std::make_pair(freq, &desiredTag->first));
    }

    virtual void SetCapacity(size_t capacity) override
    {
      Capacity = capacity;
    }

  private:
    void Resize(size_t newSize)
    {
      for (auto size = Tags.size(); size > newSize; --size)
      {
        Tags.erase(*Frequency.begin()->second);
        Frequency.erase(Frequency.begin());
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
