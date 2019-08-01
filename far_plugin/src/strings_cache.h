#pragma once

#include <memory>
#include <string>
#include <vector>

namespace FarPlugin
{
  class StringsCache
  {
  public:
    static std::unique_ptr<StringsCache> Create(size_t capacity);
    virtual ~StringsCache() = default;
    virtual void Insert(std::string const&) = 0;
    virtual std::vector<std::string> Get() const = 0;
  };

  std::unique_ptr<StringsCache> LoadStringsCache(char const* filename, size_t capacity);
  void SaveStringsCache(StringsCache const& cache, char const* filename);
}
