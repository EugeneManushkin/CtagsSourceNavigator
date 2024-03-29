#include <gtest/gtest.h>
#include <tag_info.h>
#include <tag_view.h>

#include <numeric>

namespace Tags
{
  namespace Tests
  {
    std::string const DefaultName = "DefaultName";
    std::string const DefaultFilename = "default.cpp";
    std::string const LongFilename = "C:\\long\\file\\name.cpp";
    std::string const Separator = "  ";
    size_t const DefaultLine = 10;
    size_t const MinColLength = 6;
    size_t const DefaultColumnCount = 3;
    size_t const FileColumn = 2;
    size_t const LongColumnLength = 1000;

    TagInfo GetTag(std::string const& name = DefaultName, std::string const& filename = DefaultFilename, int line = DefaultLine, std::string const& regex = "", char kind = 'd', std::string const& info = "")
    {
      TagInfo result;
      result.name = name;
      result.file = filename;
      result.re = regex;
      result.lineno = line;
      result.kind = kind;
      result.info = info;
      return std::move(result);
    }

    TagInfo GetFileTag(std::string const& filename = DefaultFilename, int line = DefaultLine)
    {
      return GetTag("", filename, line);
    }

    std::string GetColumn(TagInfo const& tag, size_t index, size_t size, FormatTagFlag formatFlag = FormatTagFlag::Default)
    {
      return TagView(&tag).GetColumn(index, formatFlag, size);
    }

    size_t GetColumnLen(TagInfo const& tag, size_t index, FormatTagFlag formatFlag)
    {
      return TagView(&tag).GetColumnLen(index, formatFlag);
    }

    TEST(TagView, ReturnsOneColumnForFileTag)
    {
      auto const fileTag = GetFileTag();
      EXPECT_EQ(1, TagView(&fileTag).ColumnCount(FormatTagFlag::Default));
      EXPECT_EQ(1, TagView(&fileTag).ColumnCount(FormatTagFlag::NotDisplayFile));
      EXPECT_EQ(1, TagView(&fileTag).ColumnCount(FormatTagFlag::DisplayOnlyName));
    }

    TEST(TagView, ShrinksColumn)
    {
      auto const size = LongFilename.length() / 2;
      EXPECT_EQ(size, GetColumn(GetFileTag(LongFilename), 0, size).length());
    }

    TEST(TagView, ExpandsColumn)
    {
      auto const size = LongFilename.length() * 2;
      EXPECT_EQ(size, GetColumn(GetFileTag(LongFilename), 0, size).length());
    }

    TEST(TagView, ShrinksFirstColumnInMiddle)
    {
      std::string const expectedColumn = LongFilename.substr(0, 3) + "..." + LongFilename.substr(LongFilename.length() - 8);
      EXPECT_EQ(expectedColumn, GetColumn(GetFileTag(LongFilename), 0, expectedColumn.length()));
    }

    TEST(TagView, ShrinksFileColumnInMiddle)
    {
      std::string const expectedColumn = LongFilename.substr(0, 3) + "..." + LongFilename.substr(LongFilename.length() - 8) + ":" + std::to_string(DefaultLine);
      EXPECT_EQ(expectedColumn, GetColumn(GetTag(DefaultName, LongFilename), FileColumn, expectedColumn.length()));
    }

    TEST(TagView, TrimsLeftFirstColumn)
    {
      std::string const expectedColumn = LongFilename.substr(LongFilename.length() - MinColLength);
      EXPECT_EQ(expectedColumn, GetColumn(GetFileTag(LongFilename), 0, expectedColumn.length()));
    }

    TEST(TagView, TrimsLeftFileColumn)
    {
      std::string expectedColumn = LongFilename + ":" + std::to_string(DefaultLine);
      expectedColumn = expectedColumn.substr(expectedColumn.length() - MinColLength);
      EXPECT_EQ(expectedColumn, GetColumn(GetTag(DefaultName, LongFilename), FileColumn, expectedColumn.length()));
    }

    TEST(TagView, InvalidLinenoNotDisplayedInFileColumn)
    {
      int const invalidLineno = -1;
      auto const invalidLinenoTag = GetTag(DefaultName, DefaultFilename, invalidLineno);
      std::string const expected = DefaultFilename;
      EXPECT_EQ(expected, GetColumn(invalidLinenoTag, FileColumn, expected.length(), FormatTagFlag::Default));
      EXPECT_EQ(expected.length(), GetColumnLen(invalidLinenoTag, FileColumn, FormatTagFlag::Default));
    }

    TEST(TagView, ReturnsEmptyFileColumn)
    {
      int const invalidLine = -1;
      auto const invalidLineTag = GetTag(DefaultName, DefaultFilename, invalidLine);
      std::string const expected = std::string(DefaultFilename.length(), ' ');
      EXPECT_EQ(expected, GetColumn(invalidLineTag, FileColumn, expected.length(), FormatTagFlag::NotDisplayFile));
      EXPECT_EQ(0, GetColumnLen(invalidLineTag, FileColumn, FormatTagFlag::NotDisplayFile));
    }

    TEST(TagView, InvalidKindNotDisplayed)
    {
      char const invalidKind = 0;
      auto const invalidKindTag = GetTag(DefaultName, DefaultFilename, DefaultLine, "", invalidKind);
      std::string const expected = DefaultName;
      EXPECT_EQ(expected, GetColumn(invalidKindTag, 0, expected.length()));
      EXPECT_EQ(expected.length(), GetColumnLen(invalidKindTag, 0, FormatTagFlag::Default));
    }

    TEST(TagView, ReturnsWiderRaw)
    {
      auto const colLengths = std::vector<size_t>(DefaultColumnCount, LongColumnLength);
      auto const expectedRawLength = std::accumulate(colLengths.begin(), colLengths.end(), size_t(0)) + (colLengths.size() - 1) * Separator.length();
      auto const tag = GetTag();
      EXPECT_EQ(expectedRawLength, TagView(&tag).GetRaw(Separator, FormatTagFlag::Default, colLengths).length());
    }

    TEST(TagView, ReturnsShorterRaw)
    {
      auto const colLengths = std::vector<size_t>(DefaultColumnCount, 1);
      auto const expectedRawLength = DefaultColumnCount + (colLengths.size() - 1) * Separator.length();
      auto const tag = GetTag();
      EXPECT_EQ(expectedRawLength, TagView(&tag).GetRaw(Separator, FormatTagFlag::Default, colLengths).length());
    }

    TEST(ShrinkColumnLengths, ShrinksSingleColumn)
    {
      size_t expectedLength = LongColumnLength / 2;
      EXPECT_EQ(expectedLength, ShrinkColumnLengths({LongColumnLength}, Separator.length(), expectedLength).back());
    }

    TEST(ShrinkColumnLengths, ShrinksColumns)
    {
      size_t const separatorLength = Separator.length();
      size_t const expectedLength = (DefaultColumnCount * LongColumnLength / 2) - 1;
      auto shrinkedLengths = ShrinkColumnLengths(std::vector<size_t>(DefaultColumnCount, LongColumnLength), separatorLength, expectedLength);
      EXPECT_EQ(expectedLength, std::accumulate(shrinkedLengths.begin(), shrinkedLengths.end(), size_t(0)) + (shrinkedLengths.size() - 1) * separatorLength);
    }

    TEST(ShrinkColumnLengths, ShrinksLastButOneColumn)
    {
      size_t const separatorLength = Separator.length();
      size_t const expectedLength = (DefaultColumnCount * LongColumnLength / 2) - 1;
      size_t const expectedColLen = LongColumnLength / 4;
      auto colLengths = std::vector<size_t>(DefaultColumnCount, LongColumnLength);
      colLengths.back() = expectedColLen;
      auto shrinkedLengths = ShrinkColumnLengths(std::move(colLengths), separatorLength, expectedLength);
      EXPECT_EQ(expectedLength, std::accumulate(shrinkedLengths.begin(), shrinkedLengths.end(), size_t(0)) + (shrinkedLengths.size() - 1) * separatorLength);
      EXPECT_EQ(expectedColLen, shrinkedLengths.back());
    }

    TEST(ShrinkColumnLengths, ShrinksLastColumn)
    {
      size_t const separatorLength = Separator.length();
      size_t const expectedLength = (DefaultColumnCount * LongColumnLength / 2) - 1;
      size_t const expectedColLen = LongColumnLength / 4;
      auto colLengths = std::vector<size_t>(DefaultColumnCount, LongColumnLength);
      *(colLengths.end() - 2) = expectedColLen;
      auto shrinkedLengths = ShrinkColumnLengths(std::move(colLengths), separatorLength, expectedLength);
      EXPECT_EQ(expectedLength, std::accumulate(shrinkedLengths.begin(), shrinkedLengths.end(), size_t(0)) + (shrinkedLengths.size() - 1) * separatorLength);
      EXPECT_EQ(expectedColLen, *(shrinkedLengths.end() - 2));
    }

    TEST(ShrinkColumnLengths, ShrinksFirstColumn)
    {
      size_t const separatorLength = Separator.length();
      size_t const expectedLength = (DefaultColumnCount * LongColumnLength / 2) - 1;
      size_t const expectedColLen = (expectedLength - (separatorLength * (DefaultColumnCount - 1))) / DefaultColumnCount;
      auto shrinkedLengths = ShrinkColumnLengths(std::vector<size_t>(DefaultColumnCount, LongColumnLength), separatorLength, expectedLength);
      EXPECT_EQ(expectedLength, std::accumulate(shrinkedLengths.begin(), shrinkedLengths.end(), size_t(0)) + (shrinkedLengths.size() - 1) * separatorLength);
      EXPECT_EQ(expectedColLen, shrinkedLengths.front());
    }
  }
}

int main(int argc, char* argv[])
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
