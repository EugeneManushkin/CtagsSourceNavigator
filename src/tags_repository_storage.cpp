#include "tags_repository_storage.h"
#include "tags_repository.h"
#include "tags_selector_impl.h"
#include "tags_selector.h"
#include "tags_sorting_options.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <list>

namespace
{
  using Tags::RepositoryInfo;
  using Tags::RepositoryType;
  using RepositoryPtr = std::shared_ptr<Tags::Internal::Repository>;
  using RepositoryFactoryFunction = std::function<std::unique_ptr<Tags::Internal::Repository>(char const*, RepositoryType)>;

  struct RepositoryRuntimeInfo
  {
    RepositoryType Type;
    RepositoryPtr Repository;
  };

  bool Empty(RepositoryRuntimeInfo const& info)
  {
    return !info.Repository.get();
  }

  RepositoryRuntimeInfo CreateRuntimeInfo(char const* tagsPath, RepositoryType type, RepositoryFactoryFunction const& createReposigory)
  {
    return {type, createReposigory(tagsPath, type)};
  }

  RepositoryInfo ToRepositoryInfo(RepositoryRuntimeInfo const& info)
  {
    return {info.Repository->TagsPath(), info.Repository->Root(), info.Type, info.Repository->ElapsedSinceCached()};
  }

  class RepositoryStorageImpl : public Tags::RepositoryStorage
  {
  public:
    RepositoryStorageImpl(RepositoryFactoryFunction&& repoFactory)
      : RepoFactory(std::move(repoFactory))
    {
    }

    int Load(char const* tagsPath, RepositoryType type, size_t& symbolsLoaded) override
    {
      auto info = Release(tagsPath);
      info = Empty(info) ? CreateRuntimeInfo(tagsPath, type, RepoFactory) : std::move(info);
      auto err = info.Repository->Load(symbolsLoaded);
      if (!err)
        Insert(std::move(info));

      return err;
    }

    std::vector<RepositoryInfo> GetOwners(char const* currentFile) const override
    {
      return Filter([&currentFile](RepositoryRuntimeInfo const& info){ return info.Repository->Belongs(currentFile); });
    }

    std::vector<RepositoryInfo> GetByType(RepositoryType type) const override
    {
      return Filter([&type](RepositoryRuntimeInfo const& info){ return !!(info.Type & type); });
    }

    RepositoryInfo GetInfo(char const* tagsPath) const override
    {
      auto runtimeInfo = GetRuntimeInfo(tagsPath);
      return Empty(runtimeInfo) ? RepositoryInfo() : ToRepositoryInfo(runtimeInfo);
    }

    void Remove(char const* tagsPath) override
    {
      Release(tagsPath);
    }

    void CacheTag(TagInfo const& tag, size_t cacheSize, bool flush) override
    {
      auto info = GetRuntimeInfo(tag.Owner.TagsFile.c_str());
      if (!Empty(info))
        info.Repository->CacheTag(tag, cacheSize, flush);
    }

    void EraseCachedTag(TagInfo const& tag, bool flush) override
    {
      auto info = GetRuntimeInfo(tag.Owner.TagsFile.c_str());
      if (!Empty(info))
        info.Repository->EraseCachedTag(tag, flush);
    }

    void ResetCacheCounters(char const* tagsPath, bool flush) override
    {
      auto info = GetRuntimeInfo(tagsPath);
      if (!Empty(info))
        info.Repository->ResetCacheCounters(flush);
    }

    std::unique_ptr<Tags::Selector> GetSelector(char const* currentFile, bool caseInsensitive, Tags::SortingOptions sortOptions, size_t limit) override
    {
      std::vector<RepositoryPtr> repositories;
      std::vector<RepositoryPtr> permanents;
      for (auto const& info : Repositories)
        if (info.Repository->Belongs(currentFile))
          repositories.push_back(info.Repository);
        else if (info.Type == RepositoryType::Permanent)
          permanents.push_back(info.Repository);

      if (!repositories.empty())
        std::copy(permanents.begin(), permanents.end(), std::back_inserter(repositories));

      return Tags::Internal::CreateSelector(std::move(repositories), currentFile, caseInsensitive, sortOptions, limit);
    }

    std::function<void()> UpdateTagsByFile(const char* tagsPath, char const* file, const char* fileTagsPath) const override
    {
      auto info = GetRuntimeInfo(tagsPath);
      return Empty(info) ? std::function<void()>() : info.Repository->UpdateTagsByFile(file, fileTagsPath);
    }

  private:
    using RepositoriesCont = std::list<RepositoryRuntimeInfo>;
    RepositoryRuntimeInfo GetRuntimeInfo(char const* tagsPath) const;
    void Insert(RepositoryRuntimeInfo&& info);
    RepositoryRuntimeInfo Release(char const* tagsPath);
    std::vector<RepositoryInfo> Filter(std::function<bool(RepositoryRuntimeInfo const&)>&& pred) const;

    RepositoryFactoryFunction RepoFactory;
    RepositoriesCont Repositories;
  };

  RepositoryRuntimeInfo RepositoryStorageImpl::GetRuntimeInfo(char const* tagsPath) const
  {
    auto iter = std::find_if(Repositories.begin(), Repositories.end(), [&tagsPath](RepositoryRuntimeInfo const& r){ return !r.Repository->CompareTagsPath(tagsPath); });
    return iter != Repositories.end() ? *iter : RepositoryRuntimeInfo();
  }

  void RepositoryStorageImpl::Insert(RepositoryRuntimeInfo&& info)
  {
    auto iter = std::find_if(Repositories.begin(), Repositories.end(), [&info](RepositoryRuntimeInfo const& r){ return r.Repository->Root() > info.Repository->Root(); });
    Repositories.insert(iter, std::move(info));
  }

  RepositoryRuntimeInfo RepositoryStorageImpl::Release(char const* tagsPath)
  {
    auto iter = std::find_if(Repositories.begin(), Repositories.end(), [&tagsPath](RepositoryRuntimeInfo const& r){ return !r.Repository->CompareTagsPath(tagsPath); });
    auto result = iter != Repositories.end() ? std::move(*iter) : RepositoryRuntimeInfo();
    if (iter != Repositories.end())
      Repositories.erase(iter);

    return std::move(result);
  }

  std::vector<RepositoryInfo> RepositoryStorageImpl::Filter(std::function<bool(RepositoryRuntimeInfo const&)>&& pred) const
  {
    std::vector<RepositoryInfo> result;
    for (auto const& r : Repositories)
      if (pred(r))
        result.push_back(ToRepositoryInfo(r));

    return std::move(result);
  }
}

namespace Tags
{
  std::unique_ptr<RepositoryStorage> RepositoryStorage::Create()
  {
    auto defaultFactory = [](char const* tagsPath, RepositoryType type){ return Tags::Internal::Repository::Create(tagsPath, type == RepositoryType::Temporary); };
    return Create(std::move(defaultFactory));
  }

  std::unique_ptr<RepositoryStorage> RepositoryStorage::Create(RepositoryFactoryFunction&& repoFactory)
  {
    return std::unique_ptr<RepositoryStorage>(new RepositoryStorageImpl(std::move(repoFactory)));
  }
}
