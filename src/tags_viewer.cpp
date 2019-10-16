#include "tags.h"
#include "tags_viewer.h"

#include <map>
#include <regex>

namespace
{
  using TagsInternal::TagView;
  using TagsInternal::TagsView;
  using TagsInternal::TagsViewer;
  using TagsInternal::FormatTagFlag;

  class PartiallyMatchViewer : public TagsViewer
  {
  public:
    PartiallyMatchViewer(std::string const& file, bool getFiles, bool caseInsensitive, size_t maxResults, Tags::SortOptions sortOptions)
      : File(file)
      , GetFiles(getFiles)
      , CaseInsensitive(caseInsensitive)
      , MaxResults(maxResults)
      , SortOptions(sortOptions)
    {
    }
  
    TagsView GetView(char const* filter, FormatTagFlag) const override
    {
      auto tags = !*filter ? GetCachedTags(File.c_str(), MaxResults, GetFiles) : std::vector<TagInfo>();
      tags = !tags.empty() ? tags : GetFiles ? FindPartiallyMatchedFile(File.c_str(), filter, MaxResults) :
                                               FindPartiallyMatchedTags(File.c_str(), filter, MaxResults, CaseInsensitive, SortOptions);
      return TagsView(std::move(tags));
    }
  
  private:
    std::string const File;
    bool GetFiles;
    bool CaseInsensitive;
    size_t MaxResults;
    Tags::SortOptions SortOptions;
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

namespace TagsInternal
{
  using Tags::SortOptions;

  std::unique_ptr<TagsViewer> GetPartiallyMatchedNamesViewer(std::string const& file, bool caseInsensitive, size_t maxResults, SortOptions sortOptions)
  {
    return std::unique_ptr<TagsViewer>(new PartiallyMatchViewer(file, false, caseInsensitive, maxResults, sortOptions));
  }

  std::unique_ptr<TagsViewer> GetPartiallyMatchedFilesViewer(std::string const& file, size_t maxResults)
  {
    return std::unique_ptr<TagsViewer>(new PartiallyMatchViewer(file, true, true, maxResults, SortOptions::Default));
  }

  std::unique_ptr<TagsViewer> GetFilterTagsViewer(std::vector<TagInfo>&& tags, bool caseInsensitive)
  {
    return std::unique_ptr<TagsViewer>(new FilterTagsViewer(std::move(tags), caseInsensitive));
  }
}
