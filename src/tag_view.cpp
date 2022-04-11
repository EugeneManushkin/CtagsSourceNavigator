#include "tag_info.h"
#include "tag_view.h"

#include <array>
#include <bitset>
#include <numeric>

//TODO: remove dependency
std::bitset<256> GetCharsMap(std::string const& str)
{
  std::bitset<256> result;
  result.reset();
  for (auto c : str)
  {
    result.set(static_cast<unsigned char>(c), true);
  }

  return std::move(result);
}

namespace
{
  using Tags::FormatTagFlag;
  std::string const LineNumberCaption = "Line";

  static std::string::iterator ReplaceRegexSpecialChars(std::string::iterator begin, std::string::iterator end)
  {
    static const auto unquotMap = GetCharsMap("s.$^*()|+[]{}?\\/");
    auto cur = begin;
    int lexlen = 0;
    for (; begin != end; ++begin)
    {
      if (lexlen == 1 && unquotMap[static_cast<unsigned char>(*begin)])
        ++lexlen;
      else if (lexlen == 2 && (*begin == '+' || *begin == '*'))
        ++lexlen;
      else
        lexlen = 0;
  
      if (lexlen == 0 && *begin == '\\')
        ++lexlen;
  
      if (lexlen > 1 && *begin != 's')
      {
        cur -= lexlen - 1;
        *cur = lexlen == 3 ? ' ' : *begin;
        cur += lexlen == 3 && *begin == '*' ? 0 : 1;
        lexlen = 0;
      }
      else
      {
        *cur++ = *begin;
      }
    }
  
    return cur;
  }

  std::string RegexToDeclaration(std::string const& regex)
  {
    auto begin = regex.size() > 2 && regex.front() == '^' ? regex.begin() + 1 : regex.begin();
    auto end = regex.size() > 2 && regex.back() == '$' ? regex.end() - 1 : regex.end();
    auto result = std::string(begin, end);
    result.erase(ReplaceRegexSpecialChars(result.begin(), result.end()), result.end());
    return std::move(result);
  }

  std::string Shrink(const std::string& text, size_t maxLength, bool shrinkToLeft)
  {
    if (shrinkToLeft)
      return text.substr(0, maxLength);

    size_t const prefixLen = 3;
    std::string const junction = "...";
    size_t const beginingLen = prefixLen + junction.length();
    return maxLength > beginingLen ? text.substr(0, prefixLen) + junction + text.substr(text.length() - maxLength + beginingLen)
                                   : text.substr(text.length() - maxLength);
  }

  size_t ShrinkEqualParts(std::vector<size_t>::iterator begin, std::vector<size_t>::iterator end, size_t separatorLength, size_t width)
  {
    auto colCount = std::distance(begin, end);
    width = width / colCount;
    width -= width > separatorLength ? separatorLength : width;
    size_t result = 0;
    for (; begin != end; result += *begin + separatorLength, ++begin)
      *begin = std::min(*begin, width);

    return result;
  }

  std::string AdjustToLength(std::string const& text, size_t colLength, bool shrinkToLeft)
  {
    return text.length() > colLength ? Shrink(text, colLength, shrinkToLeft) : text + std::string(colLength - text.length(), ' ');
  }

  std::string GetLeftColumn(TagInfo const& tag, FormatTagFlag flag)
  {
    return tag.name.empty() ? tag.file : flag == FormatTagFlag::DisplayOnlyName || !tag.kind ? tag.name : std::string(1, tag.kind) + ":" + tag.name;
  }

  size_t GetLeftColumnLen(TagInfo const& tag, FormatTagFlag flag)
  {
    return tag.name.empty() ? tag.file.length() : flag == FormatTagFlag::DisplayOnlyName || !tag.kind ? tag.name.length() : tag.name.length() + 2;
  }

  std::string GetDeclarationColumn(TagInfo const& tag, FormatTagFlag flag)
  {
    return RegexToDeclaration(tag.re);
  }

  size_t GetDeclarationColumnLength(TagInfo const& tag, FormatTagFlag flag)
  {
    return GetDeclarationColumn(tag, flag).length();
  }

  std::string GetFileColumn(TagInfo const& tag, FormatTagFlag flag)
  {
    std::string file = flag != FormatTagFlag::NotDisplayFile ? tag.file
                     : tag.lineno >= 0 ? LineNumberCaption : "";
    return file + (tag.lineno >= 0 ? ":" + std::to_string(tag.lineno) : "");
  }

  size_t GetFileColumnLength(TagInfo const& tag, FormatTagFlag flag)
  {
    auto fileLen = flag != FormatTagFlag::NotDisplayFile ? tag.file.length()
                 : tag.lineno >= 0 ? LineNumberCaption.length() : 0;
    return fileLen + (tag.lineno >= 0 ? std::to_string(tag.lineno).length() + 1 : 0);
  }

  std::array<std::string(*)(TagInfo const&, FormatTagFlag), 3> const GetColumnCallbacks = {GetLeftColumn, GetDeclarationColumn, GetFileColumn};
  std::array<size_t(*)(TagInfo const&, FormatTagFlag), 3> const GetColumnLengthCallbacks = {GetLeftColumnLen, GetDeclarationColumnLength, GetFileColumnLength};
}

namespace Tags
{
  TagView::TagView(TagInfo const* tag)
    : Tag(tag)
  {
  }

  TagInfo const* TagView::GetTag() const
  {
    return Tag;
  }

  size_t TagView::ColumnCount(FormatTagFlag flag) const
  {
    return Tag->name.empty() || flag == FormatTagFlag::DisplayOnlyName ? 1 : GetColumnCallbacks.size();
  }

  size_t TagView::GetColumnLen(size_t index, FormatTagFlag flag) const
  {
    return GetColumnLengthCallbacks.at(index)(*Tag, flag);
  }

  std::string TagView::GetColumn(size_t index, FormatTagFlag flag) const
  {
    return GetColumnCallbacks.at(index)(*Tag, flag);
  }

  std::string TagView::GetColumn(size_t index, FormatTagFlag flag, size_t colLength) const
  {
    return AdjustToLength(GetColumn(index, flag), colLength, index != GetColumnCallbacks.size() - 1 && !Tag->name.empty());
  }

  std::string TagView::GetRaw(std::string const& separator, FormatTagFlag formatFlag) const
  {
    return GetRaw(separator, formatFlag, {});
  }

  std::string TagView::GetRaw(std::string const& separator, FormatTagFlag formatFlag, std::vector<size_t> const& colLengths) const
  {
    std::string result;
    auto colCount = ColumnCount(formatFlag);
    for (size_t i = 0; i < colCount; ++i)
      result += (colLengths.empty() ? GetColumn(i, formatFlag) : GetColumn(i, formatFlag, colLengths[i])) + (i != colCount - 1 ? separator : "");
  
    return std::move(result);
  }

  std::vector<size_t> ShrinkColumnLengths(std::vector<size_t>&& colLengths, size_t separatorLength, size_t width)
  {
    if (colLengths.size() <= 1)
      return {width};

    size_t const separators = (colLengths.size() - 1) * separatorLength;
    if (colLengths.size() + separators > width)
      return std::vector<size_t>(colLengths.size(), 1);

    auto maxRawLen = std::accumulate(colLengths.begin(), colLengths.end(), size_t(0)) + separators;
    if (maxRawLen <= width)
      return std::move(colLengths);

    auto fixedColsEnd = colLengths.end() - 2;
    auto larger = fixedColsEnd;
    auto smaller = colLengths.end() - 1;
    if (*smaller > *larger)
      std::swap(smaller, larger);

    size_t const fixedSizeMax = (colLengths.size() - 2) * ((width - separators) / colLengths.size() + separatorLength);
    auto fixedSize = ShrinkEqualParts(colLengths.begin(), fixedColsEnd, separatorLength, fixedSizeMax);
    auto remains = width <= fixedSize ? separatorLength + 2 : width - fixedSize;
    *smaller = std::min(*smaller, remains/2);
    *larger = remains - *smaller - separatorLength;
    return std::move(colLengths);
  }
}
