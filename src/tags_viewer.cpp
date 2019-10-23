#include "tags.h"
#include "tags_selector.h"
#include "tags_viewer.h"

#include <map>
#include <regex>

namespace
{
  using Tags::TagView;
  using Tags::TagsView;
  using Tags::TagsViewer;
  using Tags::FormatTagFlag;

  class PartiallyMatchViewer : public TagsViewer
  {
  public:
    PartiallyMatchViewer(std::unique_ptr<Tags::Selector>&& selector, bool getFiles)
      : Selector(std::move(selector))
      , GetFiles(getFiles)
    {
    }
  
    TagsView GetView(char const* filter, FormatTagFlag) const override
    {
      auto tags = !*filter ? Selector->GetCachedTags(GetFiles) : std::vector<TagInfo>();
      tags = !tags.empty() ? tags : GetFiles ? Selector->GetFilesByPart(filter) :
                                               Selector->GetByNamePart(filter);
      return TagsView(std::move(tags));
    }
  
  private:
    std::unique_ptr<Tags::Selector>&& Selector;
    bool GetFiles;
  };
  
  bool GetRegex(char const* filter, bool caseInsensitive, std::regex& result)
  {
    try
    {
      std::regex_constants::syntax_option_type regexFlags = std::regex_constants::ECMAScript;
      regexFlags = caseInsensitive ? regexFlags | std::regex_constants::icase : regexFlags;
      result = std::regex(filter, regexFlags);
      return true;
    }
    catch(std::exception const&)
    {
    }
  
    return false;
  }

  class FilterTagsViewer : public TagsViewer
  {
  public:
    FilterTagsViewer(std::vector<TagInfo>&& tags, bool caseInsensitive)
      : Tags(std::move(tags))
      , CaseInsensitive(caseInsensitive)
    {
    }
  
    TagsView GetView(char const* filter, FormatTagFlag formatFlag) const override
    {
      TagsView result;
      std::regex regexFilter;
      if (!*filter || !GetRegex(filter, CaseInsensitive, regexFilter))
      {
        for (auto const& tag : Tags) result.PushBack(&tag);
        return std::move(result);
      }
  
      std::multimap<std::string::difference_type, TagInfo const*> idx;
      for (auto const& tag : Tags)
      {
        std::smatch matchResult;
        auto str = TagView(&tag).GetRaw(" ", formatFlag);
        if (std::regex_search(str, matchResult, regexFilter) && !matchResult.empty())
          idx.insert(std::make_pair(matchResult.position(), &tag));
      }

      for (auto const& v : idx) result.PushBack(v.second);
      return std::move(result);
    }

  private:
    std::vector<TagInfo> Tags;
    bool CaseInsensitive;
  };
}

namespace Tags
{
  std::unique_ptr<TagsViewer> GetPartiallyMatchedViewer(std::unique_ptr<Selector>&& selector, bool getFiles)
  {
    return std::unique_ptr<TagsViewer>(new PartiallyMatchViewer(std::move(selector), getFiles));
  }

  std::unique_ptr<TagsViewer> GetFilterTagsViewer(std::vector<TagInfo>&& tags, bool caseInsensitive)
  {
    return std::unique_ptr<TagsViewer>(new FilterTagsViewer(std::move(tags), caseInsensitive));
  }
}
