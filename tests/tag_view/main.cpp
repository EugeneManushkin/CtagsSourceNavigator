#include <gtest/gtest.h>
#include <tag_info.h>
#include <tag_view.h>

namespace TagsInternal
{
  namespace Tests
  {
    std::string const DefaultName = "DefaultName";
    std::string const DefaultFilename = "default.cpp";
    std::string const LongFilename = "C:\\long\\file\\name.cpp";
    size_t const MinColLength = 6;
    size_t const FileColumn = 2;

    TagInfo GetTag(std::string const& name = DefaultName, std::string const& filename = DefaultFilename, int line = 0, std::string const& regex = "", char type = 'd', std::string const& info = "")
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

    TagInfo GetFileTag(std::string const& filename = DefaultFilename, int lineno = 0)
    {
      return GetTag("", filename, lineno);
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
      int const line = 10;
      std::string const expectedColumn = LongFilename.substr(0, 3) + "..." + LongFilename.substr(LongFilename.length() - 8) + ":" + std::to_string(line);
      EXPECT_EQ(expectedColumn, GetColumn(GetTag(DefaultName, LongFilename, line), FileColumn, expectedColumn.length()));
    }

    TEST(TagView, TrimsLeftFirstColumn)
    {
      std::string const expectedColumn = LongFilename.substr(LongFilename.length() - MinColLength);
      EXPECT_EQ(expectedColumn, GetColumn(GetFileTag(LongFilename), 0, expectedColumn.length()));
    }

    TEST(TagView, TrimsLeftFileColumn)
    {
      int const line = 10;
      std::string expectedColumn = LongFilename + ":" + std::to_string(line);
      expectedColumn = expectedColumn.substr(expectedColumn.length() - MinColLength);
      EXPECT_EQ(expectedColumn, GetColumn(GetTag(DefaultName, LongFilename, line), FileColumn, expectedColumn.length()));
    }
  }
}

int main(int argc, char* argv[])
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
