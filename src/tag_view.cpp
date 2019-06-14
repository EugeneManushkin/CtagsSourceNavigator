#include "tag_info.h"
#include "tag_view.h"

#include <array>
#include <bitset>

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
  using TagsInternal::FormatTagFlag;
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

  std::string AdjustToLength(std::string const& text, size_t colLength, bool shrinkToLeft)
  {
    return text.length() > colLength ? Shrink(text, colLength, shrinkToLeft) : text + std::string(colLength - text.length(), ' ');
  }

  std::string GetLeftColumn(TagInfo const& tag, FormatTagFlag flag)
  {
    return tag.name.empty() ? tag.file : flag == FormatTagFlag::DisplayOnlyName ? tag.name : std::string(1, tag.type) + ":" + tag.name;
  }

  size_t GetLeftColumnLen(TagInfo const& tag, FormatTagFlag flag)
  {
    return tag.name.empty() ? tag.file.length() : flag == FormatTagFlag::DisplayOnlyName ? tag.name.length() : tag.name.length() + 2;
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
    return (flag == FormatTagFlag::NotDisplayFile ? LineNumberCaption : tag.file) + ":" + std::to_string(tag.lineno);
  }

  size_t GetFileColumnLength(TagInfo const& tag, FormatTagFlag flag)
  {
    return (flag == FormatTagFlag::NotDisplayFile ? LineNumberCaption.length() : tag.file.length()) + 1 + std::to_string(tag.lineno).length();
  }

  std::array<std::string(*)(TagInfo const&, FormatTagFlag), 3> const GetColumnCallbacks = {GetLeftColumn, GetDeclarationColumn, GetFileColumn};
  std::array<size_t(*)(TagInfo const&, FormatTagFlag), 3> const GetColumnLengthCallbacks = {GetLeftColumnLen, GetDeclarationColumnLength, GetFileColumnLength};
}

namespace TagsInternal
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
}
