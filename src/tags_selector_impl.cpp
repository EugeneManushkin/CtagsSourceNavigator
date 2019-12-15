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
      return ForEach([&name](Repository const& repo){ return repo.FindByName(name); });
    }

    std::vector<TagInfo> GetFiles(const char* path) const override
    {
      return ForEach([&path](Repository const& repo){ return repo.FindFiles(path); });
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
      return getFiles ? ForEach([this, &part, unlimited](Repository const& repo){ return repo.FindFiles(part, unlimited ? 0 : Limit); })
                      : ForEach([this, &part, unlimited](Repository const& repo){ return repo.FindByName(part, unlimited ? 0 : Limit, CaseInsensitive); });
    }

    std::vector<TagInfo> GetCachedTags(bool getFiles) const override
    {
      return ForEach([this, getFiles](Repository const& repo){ return repo.GetCachedTags(getFiles, Limit); }, false, false);
    }

  protected:
    std::vector<TagInfo> ForEach(std::function<std::vector<TagInfo>(Repository const&)>&& func, bool unlimited = true, bool sorted = true) const
    {
      std::vector<TagInfo> result;
      for (auto repos = Repositories.begin(); repos != Repositories.end() && (unlimited || result.size() < Limit); ++repos)
      {
        auto tags = SortTags(func(**repos), CurrentFile.c_str(), sorted ? SortOptions : Tags::SortingOptions::DoNotSort);
        auto tagsEnd = !unlimited && result.size() + tags.size() > Limit ? tags.begin() + (Limit - result.size()) : tags.end();
        std::move(tags.begin(), tagsEnd, std::back_inserter(result));
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
