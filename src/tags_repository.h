#pragma once

#include "tag_info.h"

#include <functional>
#include <memory>
#include <vector>

namespace Tags
{
  namespace Internal
  {
    class Repository
    {
    public:
      static std::unique_ptr<Repository> Create(const char* filename, bool singleFileRepos);
      virtual ~Repository() = default;
      virtual int Load(size_t& symbolsLoaded) = 0;
      virtual bool Belongs(char const* file) const = 0;
      virtual int CompareTagsPath(const char* tagsPath) const = 0;
      virtual std::string TagsPath() const = 0;
      virtual std::string Root() const = 0;
      virtual std::vector<TagInfo> FindByName(const char* name) const = 0;
      virtual std::vector<TagInfo> FindByName(const char* part, size_t maxCount, bool caseInsensitive) const = 0;
      virtual std::vector<TagInfo> FindFiles(const char* path) const = 0;
      virtual std::vector<TagInfo> FindFiles(const char* part, size_t maxCount) const = 0;
      virtual std::vector<TagInfo> FindClassMembers(const char* classname) const = 0;
      virtual std::vector<TagInfo> FindByFile(const char* file) const = 0;
      virtual void CacheTag(TagInfo const& tag, size_t cacheSize, bool flush) = 0;
      virtual void EraseCachedTag(TagInfo const& tag, bool flush) = 0;
      virtual std::vector<TagInfo> GetCachedTags(bool getFiles, size_t maxCount) const = 0;
      virtual time_t ElapsedSinceCached() const = 0;
      virtual void ResetCacheCounters(bool flush) = 0;
      virtual std::function<void()> UpdateTagsByFile(char const* file, const char* fileTagsPath) const = 0;
    };
  }
}
