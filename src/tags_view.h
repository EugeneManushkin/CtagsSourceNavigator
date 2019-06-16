#pragma once

#include "tag_info.h"
#include "tag_view.h"

#include <vector>

namespace TagsInternal
{
  class TagsView
  {
  public:
    TagsView();
    TagsView(std::vector<TagInfo>&& tags);
    void PushBack(TagInfo const* tag);
    size_t Size() const;
    TagView operator[] (size_t index) const;
    std::vector<size_t> GetMaxColumnLengths(FormatTagFlag formatFlag) const;

  private:
    std::vector<TagInfo> Tags;
    std::vector<TagView> Views;
  };
}
