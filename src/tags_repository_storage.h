#pragma once

#include "tag_info.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Tags
{
  class Selector;
  enum class SortingOptions;
  namespace Internal
  {
    class Repository;
  }

  enum class RepositoryType : int
  {
    Any = ~0,
    Regular = 1 << 0,
    Temporary = 1 << 1,
    Permanent = 1 << 2,
  };

  inline RepositoryType operator | (RepositoryType left, RepositoryType right) { return static_cast<RepositoryType>(static_cast<int>(left) | static_cast<int>(right)); }
  inline RepositoryType operator & (RepositoryType left, RepositoryType right) { return static_cast<RepositoryType>(static_cast<int>(left) & static_cast<int>(right)); }
  inline RepositoryType operator ~ (RepositoryType type) { return static_cast<RepositoryType>(~static_cast<int>(type)); }
  inline bool operator ! (RepositoryType type) { return !static_cast<int>(type); }

  struct RepositoryInfo
  {
    std::string TagsPath;
    std::string Root;
    RepositoryType Type;
    time_t ElapsedSinceCached;
  };

  class RepositoryStorage
  {
  public:
    static std::unique_ptr<RepositoryStorage> Create();
    static std::unique_ptr<RepositoryStorage> Create(std::function<std::unique_ptr<Internal::Repository>(char const*, RepositoryType)>&& repoFactory);
    virtual ~RepositoryStorage() = default;
    virtual int Load(char const* tagsPath, RepositoryType type, size_t& symbolsLoaded) = 0;
    virtual std::vector<RepositoryInfo> GetOwners(char const* currentFile) const = 0;
    virtual std::vector<RepositoryInfo> GetByType(RepositoryType type) const = 0;
    virtual RepositoryInfo GetInfo(char const* tagsPath) const = 0;
    virtual void Remove(char const* tagsPath) = 0;
    virtual void CacheTag(TagInfo const& tag, size_t cacheSize, bool flush) = 0;
    virtual void EraseCachedTag(TagInfo const& tag, bool flush) = 0;
    virtual void ResetCacheCounters(char const* tagsPath, bool flush) = 0;
    virtual std::unique_ptr<Selector> GetSelector(char const* currentFile, bool caseInsensitive, SortingOptions sortOptions, size_t limit = 0) = 0;
  };
}
