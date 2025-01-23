#pragma once

#include "tags_sorting_options.h"
#include "tags_view.h"

#include <memory>

namespace Tags
{
  class Selector;

  class TagsViewer
  {
  public:
    virtual ~TagsViewer() = default;
    virtual TagsView GetView(char const* filter, FormatTagFlag formatFlag, size_t threshold, bool& thresholdReached) const = 0;
  };

  std::unique_ptr<TagsViewer> GetPartiallyMatchedViewer(std::unique_ptr<Selector>&& selector, bool getFiles);
  std::unique_ptr<TagsViewer> GetFilterTagsViewer(TagsView const& view, bool caseInsensitive, std::vector<TagInfo> const& tagsOnTop);
}
