#pragma once

#include "tag_info.h"

#include <memory>
#include <string>
#include <vector>

namespace Tags
{
  class Selector;
  enum class SortingOptions;

  enum class RepositoryType
  {
    Regular,
    Temporary,
    Permanent,
  };

  struct RepositoryInfo
  {
    std::string TagsPath;
    std::string Root;
    RepositoryType Type;
  };

  class RepositoryStorage
  {
  public:
    static std::unique_ptr<RepositoryStorage> Create();
    virtual ~RepositoryStorage() = default;
    virtual int Load(char const* tagsPath, RepositoryType type, size_t& symbolsLoaded) = 0;
    virtual std::vector<RepositoryInfo> GetInvolved(char const* currentFile) const = 0;
    virtual std::vector<RepositoryInfo> GetOwners(char const* currentFile) const = 0;
    virtual std::vector<RepositoryInfo> GetAll() const = 0;
    virtual void Remove(char const* tagsPath) = 0;
    virtual void CacheTag(TagInfo const& tag, size_t cacheSize, bool flush) = 0;
    virtual void EraseCachedTag(TagInfo const& tag, bool flush) = 0;
    virtual std::unique_ptr<Selector> GetSelector(char const* currentFile, bool caseInsensitive, SortingOptions sortOptions, size_t limit = 0) = 0;
  };
}
