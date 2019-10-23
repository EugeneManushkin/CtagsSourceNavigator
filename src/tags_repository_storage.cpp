#include "tags_repository_storage.h"
#include "tags_repository.h"
#include "tags_selector_impl.h"
#include "tags_selector.h"
#include "tags_sorting_options.h"

#include <algorithm>
#include <functional>
#include <list>

namespace
{
  using Tags::RepositoryInfo;
  using Tags::RepositoryType;
  using RepositoryPtr = std::unique_ptr<Tags::Internal::Repository>;

  struct RepositoryRuntimeInfo
  {
    RepositoryType Type;
    RepositoryPtr Repository;
  };

  bool Empty(RepositoryRuntimeInfo const& info)
  {
    return !info.Repository.get();
  }

  RepositoryRuntimeInfo CreateRuntimeInfo(char const* tagsPath, RepositoryType type)
  {
    return {type, Tags::Internal::Repository::Create(tagsPath, type == RepositoryType::Temporary)};
  }

  RepositoryInfo ToRepositoryInfo(RepositoryRuntimeInfo const& info)
  {
    return {info.Repository->TagsPath(), "TODO: get repository root", info.Type};
  }

  bool Involved(RepositoryRuntimeInfo const& info, char const* currentFile)
  {
    return info.Type == RepositoryType::Permanent || info.Repository->Belongs(currentFile);
  }

  class RepositoryStorageImpl : public Tags::RepositoryStorage
  {
  public:
    int Load(char const* tagsPath, RepositoryType type, size_t& symbolsLoaded) override
    {
      auto info = Release(tagsPath);
      info = Empty(info) ? CreateRuntimeInfo(tagsPath, type) : std::move(info);
      auto err = info.Repository->Load(symbolsLoaded);
      if (!err)
        Repositories.push_front(std::move(info));

      return err;
    }

    std::vector<RepositoryInfo> GetInvolved(char const* currentFile) const override
    {
      return Filter([&currentFile](RepositoryRuntimeInfo const& info){ return Involved(info, currentFile); });
    }

    std::vector<RepositoryInfo> GetOwners(char const* currentFile) const override
    {
      return Filter([&currentFile](RepositoryRuntimeInfo const& info){ return info.Repository->Belongs(currentFile); });
    }

    std::vector<RepositoryInfo> GetAll() const override
    {
      return Filter([](RepositoryRuntimeInfo const&){ return true; });
    }

    void Remove(char const* tagsPath) override
    {
      Repositories.remove_if([&tagsPath](RepositoryRuntimeInfo const& info) { return !info.Repository->CompareTagsPath(tagsPath); });
    }

    void CacheTag(TagInfo const& tag, size_t cacheSize, bool flush) override
    {
      auto iter = FindByTagsPath(tag.Owner.TagsFile.c_str());
      if (iter != Repositories.end())
        iter->Repository->CacheTag(tag, cacheSize, flush);
    }

    void EraseCachedTag(TagInfo const& tag, bool flush) override
    {
      auto iter = FindByTagsPath(tag.Owner.TagsFile.c_str());
      if (iter != Repositories.end())
        iter->Repository->EraseCachedTag(tag, flush);
    }

    std::unique_ptr<Tags::Selector> GetSelector(char const* currentFile, bool caseInsensitive, Tags::SortingOptions sortOptions, size_t limit) override
    {
      std::vector<Tags::Internal::Repository const*> found;
      for (auto const& info : Repositories)
        if (Involved(info, currentFile))
          found.push_back(&*info.Repository);

      return Tags::Internal::CreateSelector(std::move(found), currentFile, caseInsensitive, sortOptions, limit);
    }

  private:
    using RepositoriesCont = std::list<RepositoryRuntimeInfo>;
    RepositoriesCont::iterator FindByTagsPath(char const* tagsPath);
    RepositoryRuntimeInfo Release(char const* tagsPath);
    std::vector<RepositoryInfo> Filter(std::function<bool(RepositoryRuntimeInfo const&)>&& pred) const;

    RepositoriesCont Repositories;
  };

  RepositoryStorageImpl::RepositoriesCont::iterator RepositoryStorageImpl::FindByTagsPath(char const* tagsPath)
  {
    return std::find_if(Repositories.begin(), Repositories.end(), [&tagsPath](RepositoryRuntimeInfo const& r){ return !r.Repository->CompareTagsPath(tagsPath); });
  }

  RepositoryRuntimeInfo RepositoryStorageImpl::Release(char const* tagsPath)
  {
    auto iter = FindByTagsPath(tagsPath);
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
    return std::unique_ptr<RepositoryStorage>(new RepositoryStorageImpl);
  }
}
