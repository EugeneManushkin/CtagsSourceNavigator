#include "tags.h"
#include "tags_selector.h"
#include "tags_viewer.h"

#include <map>
#include <regex>
#include <set>

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
      return TagsView(!*filter ? Selector->GetCachedTags(GetFiles) : Selector->GetByPart(filter, GetFiles));
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

  auto Less = [](TagInfo const* left, TagInfo const* right)
  {
    return *left < *right;
  };

  class FilterTagsViewer : public TagsViewer
  {
  public:
    FilterTagsViewer(TagsView const& view, bool caseInsensitive, std::vector<TagInfo> const& tagsOnTop)
      : View(view)
      , CaseInsensitive(caseInsensitive)
      , TagsOnTop(Less)
    {
      for (auto const& tag : tagsOnTop)
        TagsOnTop.insert(&tag);
    }
  
    TagsView GetView(char const* filter, FormatTagFlag formatFlag) const override
    {
      TagsView result;
      std::regex regexFilter;
      if (!*filter || !GetRegex(filter, CaseInsensitive, regexFilter))
      {
        for (size_t i = 0; i < View.Size(); ++i) result.PushBack(View[i].GetTag());
        return std::move(result);
      }
  
      std::multimap<size_t, TagInfo const*> idx;
      for (size_t i = 0; i < View.Size(); ++i)
      {
        std::smatch matchResult;
        auto view = View[i];
        auto str = view.GetRaw(" ", formatFlag);
        if (std::regex_search(str, matchResult, regexFilter) && !matchResult.empty())
          idx.insert(std::make_pair(NormalizeWeight(view, formatFlag, matchResult.position()), view.GetTag()));
      }

      for (auto const& v : idx) result.PushBack(v.second);
      return std::move(result);
    }

  private:
    size_t NormalizeWeight(TagView const& view, FormatTagFlag formatFlag, size_t weight) const
    {
      return weight < view.GetColumnLen(0, formatFlag) && TagsOnTop.count(view.GetTag()) > 0 ? 0 : weight;
    }

    TagsView const& View;
    bool CaseInsensitive;
    std::set<TagInfo const*, decltype(Less)> TagsOnTop;
  };
}

namespace Tags
{
  std::unique_ptr<TagsViewer> GetPartiallyMatchedViewer(std::unique_ptr<Selector>&& selector, bool getFiles)
  {
    return std::unique_ptr<TagsViewer>(new PartiallyMatchViewer(std::move(selector), getFiles));
  }

  std::unique_ptr<TagsViewer> GetFilterTagsViewer(TagsView const& view, bool caseInsensitive, std::vector<TagInfo> const& tagsOnTop)
  {
    return std::unique_ptr<TagsViewer>(new FilterTagsViewer(view, caseInsensitive, tagsOnTop));
  }
}
