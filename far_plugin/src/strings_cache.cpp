#include "strings_cache.h"

#include <fstream>
#include <iterator>
#include <list>
#include <unordered_map>

namespace
{
  class StringsCacheImpl : public FarPlugin::StringsCache
  {
  public:
    StringsCacheImpl(size_t capacity)
      : Capacity(capacity)
    {
    }

    void Insert(std::string const& str) override
    {
      if (!Capacity)
        return;
  
      auto i = Index.find(str);
      if (i == Index.end())
        Add(str);
      else
        OrderedStrings.splice(OrderedStrings.begin(), OrderedStrings, i->second);
    }

    std::vector<std::string> Get() const override
    {
      std::vector<std::string> result;
      std::copy(OrderedStrings.begin(), OrderedStrings.end(), std::back_inserter(result));
      return std::move(result);
    }

  private:
    void Add(std::string const& str)
    {
      if (OrderedStrings.size() == Capacity)
      {
        Index.erase(OrderedStrings.back());
        OrderedStrings.pop_back();
      }
  
      OrderedStrings.push_front(str);
      Index[str] = OrderedStrings.begin();
    }


    size_t Capacity;
    using StringList = std::list<std::string>;
    StringList OrderedStrings;
    std::unordered_map<std::string, StringList::iterator> Index;
  };
}

namespace FarPlugin
{
  std::unique_ptr<StringsCache> StringsCache::Create(size_t capacity)
  {
    return std::unique_ptr<StringsCache>(new StringsCacheImpl(capacity));
  }

  std::unique_ptr<StringsCache> LoadStringsCache(char const* filename, size_t capacity)
  {
    auto result = StringsCache::Create(capacity);
    if (!capacity)
      return std::move(result);

    std::ifstream file;
    file.exceptions(std::ifstream::goodbit);
    file.open(filename);
    std::shared_ptr<void> fileCloser(0, [&file](void*) { file.close(); });
    std::string buf;
    while (std::getline(file, buf))
    {
      result->Insert(buf);
    }

    return std::move(result);
  }

  void SaveStringsCache(StringsCache const& cache, char const* filename)
  {
    std::ofstream file;
    file.exceptions(std::ifstream::goodbit);
    file.open(filename);
    std::shared_ptr<void> fileCloser(0, [&file](void*) { file.close(); });
    auto strings = cache.Get();
    for (auto i = strings.rbegin(); i != strings.rend(); ++i)
    {
      file << *i << std::endl;
    }
  }
}
