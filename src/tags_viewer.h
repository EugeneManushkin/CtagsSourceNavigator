#pragma once

#include "tags_sorting_options.h"
#include "tags_view.h"

#include <memory>

namespace TagsInternal
{
  class TagsViewer
  {
  public:
    virtual ~TagsViewer() = default;
    virtual TagsView GetView(char const* filter, FormatTagFlag formatFlag) const = 0;
  };

  std::unique_ptr<TagsViewer> GetPartiallyMatchedNamesViewer(std::string const& file, bool caseInsensitive, size_t maxResults, Tags::SortingOptions sortOptions);
  std::unique_ptr<TagsViewer> GetPartiallyMatchedFilesViewer(std::string const& file, size_t maxResults);
  std::unique_ptr<TagsViewer> GetFilterTagsViewer(std::vector<TagInfo>&& tags, bool caseInsensitive);
}
