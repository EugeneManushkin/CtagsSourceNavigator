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
    virtual TagsView GetView(char const* filter, FormatTagFlag formatFlag) const = 0;
  };

  std::unique_ptr<TagsViewer> GetPartiallyMatchedViewer(std::unique_ptr<Selector>&& selector, bool getFiles);
  std::unique_ptr<TagsViewer> GetFilterTagsViewer(std::vector<TagInfo>&& tags, bool caseInsensitive);
}
