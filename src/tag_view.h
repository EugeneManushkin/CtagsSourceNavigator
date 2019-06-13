#pragma once

#include <vector>

struct TagInfo;

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
    std::string GetRaw(std::string separator, FormatTagFlag formatFlag) const;
    std::string GetRaw(std::string separator, FormatTagFlag formatFlag, std::vector<size_t> const& colLengths) const;

  private:
    TagInfo const* Tag;
  };
}
