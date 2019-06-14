#include <gtest/gtest.h>
#include <tag_info.h>
#include <tag_view.h>

#include <numeric>

namespace TagsInternal
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

    TagInfo GetTag(std::string const& name = DefaultName, std::string const& filename = DefaultFilename, int line = DefaultLine, std::string const& regex = "", char type = 'd', std::string const& info = "")
    {
      TagInfo result;
      result.name = name;
      result.file = filename;
      result.re = regex;
      result.lineno = line;
      result.type = type;
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
      EXPECT_EQ(expectedLength, ShrinkColumnLengths({LongColumnLength}, expectedLength).back());
    }

    TEST(ShrinkColumnLengths, ShrinksLastTwoColumns)
    {
      size_t const separatorLength = 1;
      size_t const fixedSize = (DefaultColumnCount - 2) * (LongColumnLength + separatorLength);
      size_t const expectedLength = fixedSize + LongColumnLength;
      auto colLengths = std::vector<size_t>(DefaultColumnCount, LongColumnLength);
      auto shrinkedLengths = ShrinkColumnLengths(std::move(colLengths), expectedLength);
      EXPECT_EQ(expectedLength, std::accumulate(shrinkedLengths.begin(), shrinkedLengths.end(), size_t(0)) + (shrinkedLengths.size() - 1) * separatorLength);
    }
  }
}

int main(int argc, char* argv[])
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
