#include "tags_cache.h"

#include <algorithm>
#include <iterator>
#include <map>

namespace
{
  class TagsCacheImpl : public Tags::Internal::TagsCache
  {
  public:
    TagsCacheImpl(size_t capacity)
      : Capacity(capacity)
    {
    }

    virtual std::vector<TagInfo> Get(size_t limit) const override
    {
      std::vector<TagInfo> result;
      limit = !limit ? Capacity : std::min(Capacity, limit);
      for (auto i = Frequency.rbegin(); limit > 0 && i != Frequency.rend(); ++i, --limit)
        result.push_back(*i->second);

      return result;
    }

    virtual std::vector<std::pair<TagInfo, size_t>> GetStat() const override
    {
      std::vector<std::pair<TagInfo, size_t>> result;
      result.reserve(Frequency.size());
      std::transform(Frequency.rbegin(), Frequency.rend(), std::back_inserter(result), [](FrequencyToTagInfo::value_type const& v) {return std::make_pair(*v.second, v.first);});
      return result;
    }

    virtual void Insert(TagInfo const& tag, size_t freq) override
    {
      auto desiredTag = Tags.lower_bound(tag);
      bool found = desiredTag != Tags.end() && !(tag < desiredTag->first);
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

    virtual void Erase(TagInfo const& tag) override
    {
      auto found = Tags.find(tag);
      if (found == Tags.end())
        return;

      Frequency.erase(found->second);
      Tags.erase(found);
    }

    virtual void SetCapacity(size_t capacity) override
    {
      Capacity = capacity;
    }

    virtual void ResetCounters() override
    {
      auto tags = Get(0);
      Frequency.clear();
      Tags.clear();
      for (auto i = tags.rbegin(); i != tags.rend(); ++i)
        Insert(*i, 1);
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
    std::map<TagInfo, FrequencyToTagInfo::iterator> Tags;
  };
}

namespace Tags
{
namespace Internal
{
  std::shared_ptr<TagsCache> CreateTagsCache(size_t capacity)
  {
    return std::shared_ptr<TagsCache>(new TagsCacheImpl(capacity));
  }
}
}