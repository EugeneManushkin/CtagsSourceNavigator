#include "tags_view.h"

#include <algorithm>

namespace Tags
{
  TagsView::TagsView()
  {
  }

  TagsView::TagsView(std::vector<TagInfo>&& tags)
    : Tags(std::move(tags))
  {
  }

  void TagsView::PushBack(TagInfo const* tag)
  {
    Views.push_back(TagView(tag));
  }

  size_t TagsView::Size() const
  {
    return Tags.empty() ? Views.size() : Tags.size();
  }

  TagView TagsView::operator[] (size_t index) const
  {
    return Tags.empty() ? Views[index] : TagView(&Tags[index]);
  }

  std::vector<size_t> TagsView::GetMaxColumnLengths(FormatTagFlag formatFlag) const
  {
    std::vector<size_t> result;
    auto size = Size();
    for (size_t i = 0; i < size; ++i)
    {
      auto view = (*this)[i];
      auto columns = view.ColumnCount(formatFlag);
      result = result.empty() ? std::vector<size_t>(columns) : result;
      for (size_t col = 0; col < columns; ++col)
        result[col] = std::max(result[col], view.GetColumnLen(col, formatFlag));
    }
  
    return std::move(result);
  }
}
