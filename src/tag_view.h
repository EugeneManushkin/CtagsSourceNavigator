#pragma once

#include "tag_info.h"

#include <vector>

namespace TagsInternal
{
  enum class FormatTagFlag
  {
    Default,
    NotDisplayFile,
    DisplayOnlyName,
  };

  class TagView
  {
  public:
    TagView(TagInfo const* tag);
    TagInfo const* GetTag() const;
    size_t ColumnCount(FormatTagFlag flag) const;
    size_t GetColumnLen(size_t index, FormatTagFlag flag) const;
    std::string GetColumn(size_t index, FormatTagFlag flag) const;
    std::string GetColumn(size_t index, FormatTagFlag flag, size_t colLength) const;

  private:
    TagInfo const* Tag;
  };
}
