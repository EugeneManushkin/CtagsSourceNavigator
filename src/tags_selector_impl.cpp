#include "tags.h"
#include "tags_repository.h"
#include "tags_selector.h"
#include "tags_selector_impl.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <stdexcept>

namespace
{
  using Tags::Internal::Repository;
  using RepositoryPtr = std::shared_ptr<Repository>;

  class SelectorImpl : public Tags::Selector
  {
  public:
    SelectorImpl(std::vector<RepositoryPtr>&& repositories, char const* currentFile, bool caseInsensitive, Tags::SortingOptions sortOptions, size_t limit)
      : Repositories(std::move(repositories))
      , CurrentFile(currentFile)
      , CaseInsensitive(caseInsensitive)
      , SortOptions(sortOptions)
      , Limit(limit)
    {
      if (!Limit)
        throw std::invalid_argument("Limit parameter must be greated zero");
    }

    std::vector<TagInfo> GetByName(const char* name) const override
    {
      return ForEach([&name](Repository const& repo){ return repo.FindByName(name); });
    }

    std::vector<TagInfo> GetFiles(const char* path) const override
    {
      return ForEach([&path](Repository const& repo){ return repo.FindFiles(path); }, true, true, true);
    }

    std::vector<TagInfo> GetClassMembers(const char* classname) const override
    {
      return ForEach([&classname](Repository const& repo){ return repo.FindClassMembers(classname); });
    }

    std::vector<TagInfo> GetByFile(const char* file) const override
    {
      return ForEach([&file](Repository const& repo){ return repo.FindByFile(file); } );
    }

    std::vector<TagInfo> GetByPart(const char* part, bool getFiles, bool unlimited) const override
    {
      return ForEach([this, part, getFiles, unlimited](Repository const& repo) { return GetByPart(repo, getFiles, part, unlimited); }, true, true, getFiles);
    }

    std::vector<TagInfo> GetCachedTags(bool getFiles) const override
    {
      return ForEach([this, getFiles](Repository const& repo){ return GetCachedTags(repo, getFiles); }, false, false);
    }

  protected:
    std::vector<TagInfo> ForEach(std::function<std::vector<TagInfo>(Repository const&)>&& func, bool unlimited = true, bool sorted = true, bool getFiles = false) const
    {
      std::vector<TagInfo> result;
      for (auto repos = Repositories.begin(); repos != Repositories.end() && (unlimited || result.size() < Limit); ++repos)
      {
        auto tags = SortTags(func(**repos), CurrentFile.c_str(), sorted ? SortOptions : Tags::SortingOptions::DoNotSort);
        bool cachedOnTop = sorted && !!(SortOptions & Tags::SortingOptions::CachedTagsOnTop);
        auto cached = cachedOnTop && !tags.empty() ? (*repos)->GetCachedTags(getFiles, Limit) : std::vector<TagInfo>();
        tags = !cached.empty() ? Tags::MoveOnTop(std::move(tags), std::move(cached)) : tags;
        auto tagsEnd = !unlimited && result.size() + tags.size() > Limit ? tags.begin() + (Limit - result.size()) : tags.end();
        std::move(tags.begin(), tagsEnd, std::back_inserter(result));
      }

      return std::move(result);
    }

    std::vector<TagInfo> GetByPart(Repository const& repo, bool getFiles, const char* part, bool unlimited) const
    {
      size_t limit = unlimited ? 0 : Limit;
      bool useCached = !!(SortOptions & Tags::SortingOptions::CachedTagsOnTop);
      return getFiles ? repo.FindFiles(part, limit, useCached) : repo.FindByName(part, limit, CaseInsensitive, useCached);
    }

    std::vector<TagInfo> GetCachedTags(Repository const& repo, bool getFiles) const
    {
      auto tags = repo.GetCachedTags(getFiles, Limit);
      return tags.empty() ? GetByPart(repo, getFiles, "", false) : std::move(tags);
    }

    std::vector<RepositoryPtr> Repositories;
    std::string CurrentFile;
    bool CaseInsensitive;
    Tags::SortingOptions SortOptions;
    size_t Limit;
  };
}

namespace Tags
{
  namespace Internal
  {
    std::unique_ptr<Selector> CreateSelector(std::vector<RepositoryPtr>&& repositories, char const* currentFile, bool caseInsensitive, SortingOptions sortOptions, size_t limit)
    {
      return std::unique_ptr<Selector>(new SelectorImpl(std::move(repositories), currentFile, caseInsensitive, sortOptions, limit));
    }
  }
}
