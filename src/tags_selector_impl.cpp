#include "tags.h"
#include "tags_repository.h"
#include "tags_selector.h"
#include "tags_selector_impl.h"

#include <algorithm>
#include <functional>
#include <iterator>

namespace
{
  using Tags::Internal::Repository;

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
      if (!Limit)
        throw std::invalid_argument("Limit parameter must be greated zero");
    }

    std::vector<TagInfo> GetByName(const char* name) const override
    {
      return GetSorted([&name](Repository const& repo){ return repo.FindByName(name); });
    }

    std::vector<TagInfo> GetFiles(const char* path) const override
    {
      return GetSorted([&path](Repository const& repo){ return repo.FindFiles(path); });
    }

    std::vector<TagInfo> GetClassMembers(const char* classname) const override
    {
      return GetSorted([&classname](Repository const& repo){ return repo.FindClassMembers(classname); });
    }

    std::vector<TagInfo> GetByFile(const char* file) const override
    {
      return GetSorted([&file](Repository const& repo){ return repo.FindByFile(file); } );
    }

    std::vector<TagInfo> GetByPart(const char* part, bool getFiles, bool unlimited) const override
    {
      return getFiles ? GetSorted([this, &part, unlimited](Repository const& repo){ return repo.FindFiles(part, unlimited ? 0 : Limit); })
                      : GetSorted([this, &part, unlimited](Repository const& repo){ return repo.FindByName(part, unlimited ? 0 : Limit, CaseInsensitive); });
    }

    std::vector<TagInfo> GetCachedTags(bool getFiles) const override
    {
      std::vector<TagInfo> result;
      for (auto repos = Repositories.begin(); repos != Repositories.end() && result.size() < Limit; ++repos)
      {
        auto stat = (*repos)->GetCachedTags(getFiles, Limit);
        auto statEnd = result.size() + stat.size() > Limit ? stat.begin() + (Limit - result.size()) : stat.end();
        std::move(stat.begin(), statEnd, std::back_inserter(result));
      }

      return std::move(result);
    }

  protected:
    std::vector<TagInfo> GetSorted(std::function<std::vector<TagInfo>(Repository const&)>&& func) const
    {
      std::vector<TagInfo> result;
      for (auto const& repos : Repositories)
      {
        auto tags = SortTags(func(*repos), CurrentFile.c_str(), SortOptions);
        std::move(tags.begin(), tags.end(), std::back_inserter(result));
      }

      return std::move(result);
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
