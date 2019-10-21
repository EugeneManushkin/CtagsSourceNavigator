#include "tags.h"
#include "tags_repository.h"
#include "tags_selector.h"
#include "tags_selector_impl.h"

#include <functional>
#include <iterator>

namespace
{
  using Tags::Internal::Repository;

  std::vector<TagInfo> ForEachRepository(std::vector<Repository const*> const& repositories, std::function<std::vector<TagInfo>(Tags::Internal::Repository const&)>&& func)
  {
    std::vector<TagInfo> result;
    for (auto const& repos : repositories)
    {
      auto tags = func(*repos);
      std::move(tags.begin(), tags.end(), std::back_inserter(result));
    }

    return std::move(result);
  }

  class SelectorImpl : public Tags::Selector
  {
  public:
    SelectorImpl(std::vector<Repository const*>&& repositories, char const* currentFile, bool caseInsensitive, Tags::SortingOptions sortOptions, size_t limit)
      : Repositories(std::move(repositories))
      , CurrentFile(currentFile)
      , CaseInsensitive(caseInsensitive)
      , SortOptions(sortOptions)
      , Limit(limit)
    {
    }

    std::vector<TagInfo> GetByName(const char* name) const override
    {
      return GetSorted([&name](Repository const& repo){ return repo.FindByName(name); });
    }

    std::vector<TagInfo> GetByNamePart(const char* part) const override
    {
      return GetSorted([this, &part](Repository const& repo){ return repo.FindByName(part, Limit, CaseInsensitive); });
    }

    std::vector<TagInfo> GetFiles(const char* path) const override
    {
      return GetSorted([&path](Repository const& repo){ return repo.FindFiles(path); });
    }

    std::vector<TagInfo> GetFilesByPart(const char* part) const override
    {
      return GetSorted([this, &part](Repository const& repo){ return repo.FindFiles(part, Limit); });
    }

    std::vector<TagInfo> GetClassMembers(const char* classname) const override
    {
      return GetSorted([&classname](Repository const& repo){ return repo.FindClassMembers(classname); });
    }

    std::vector<TagInfo> GetByFile(const char* file) const override
    {
      return GetSorted([&file](Repository const& repo){ return repo.FindByFile(file); } );
    }

    std::vector<TagInfo> GetCachedNames() const override
    {
      //TODO: implement
      return std::vector<TagInfo>();
    }

    std::vector<TagInfo> GetCachedFiles() const override
    {
      //TODO: implement
      return std::vector<TagInfo>();
    }

  protected:
    std::vector<TagInfo> GetSorted(std::function<std::vector<TagInfo>(Tags::Internal::Repository const&)>&& func) const
    {
      return SortTags(ForEachRepository(Repositories, std::move(func)), CurrentFile.c_str(), SortOptions);
    }

    std::vector<Repository const*> Repositories;
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
    std::unique_ptr<Selector> CreateSelector(std::vector<Repository const*>&& repositories, char const* currentFile, bool caseInsensitive, SortingOptions sortOptions, size_t limit)
    {
      return std::unique_ptr<Selector>(new SelectorImpl(std::move(repositories), currentFile, caseInsensitive, sortOptions, limit));
    }
  }
}
