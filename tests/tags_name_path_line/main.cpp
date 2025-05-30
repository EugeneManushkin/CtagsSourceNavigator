#include <gtest/gtest.h>
#include <tags.h>

// Visual Studio like paths:
//   C:\path\to\file.cpp(1448,3):
//   C:\path\to\file.cpp(1448):
// GCC like paths:
//   path/to/file.c:409:5:
//   path/to/file.cpp:264:23:
//   path/to/file.lua:3:
//   path/to/file.cmake:15

namespace Tags
{
  namespace Tests
  {
    using ActualExpectedPath = std::pair<std::string, std::string>;
    struct GetNamePathLineP: ::testing::TestWithParam<ActualExpectedPath> {};
    using NamePathLine = std::tuple<std::string, std::string, int>;

    TEST(GetNamePathLine, IgnoresInvalidInput)
    {
      ASSERT_EQ(GetNamePathLine(""), NamePathLine("", "", -1));
      ASSERT_EQ(GetNamePathLine("/////"), NamePathLine("", "", -1));
      ASSERT_EQ(GetNamePathLine("\\\\\\\\"), NamePathLine("", "", -1));
      ASSERT_EQ(GetNamePathLine("\\/\\/\\/\\"), NamePathLine("", "", -1));
    }

    TEST(GetNamePathLine, ScanFileWithoutPathAndLine)
    {
      ASSERT_EQ(GetNamePathLine("file"), NamePathLine("file", "", -1));
      ASSERT_EQ(GetNamePathLine("file.cpp"), NamePathLine("file.cpp", "", -1));
      ASSERT_EQ(GetNamePathLine("file.name.with.multiple.dots"), NamePathLine("file.name.with.multiple.dots", "", -1));
    }

    TEST(GetNamePathLine, StripsExtraSlashes)
    {
      ASSERT_EQ(GetNamePathLine("/////file/////"), NamePathLine("file", "", -1));
      ASSERT_EQ(GetNamePathLine("\\\\\\\\file\\\\\\\\"), NamePathLine("file", "", -1));
      ASSERT_EQ(GetNamePathLine("path/to/file/////"), NamePathLine("file", "path/to", -1));
      ASSERT_EQ(GetNamePathLine("path\\to/file/////"), NamePathLine("file", "path\\to", -1));
      ASSERT_EQ(GetNamePathLine("/path/to/file"), NamePathLine("file", "path/to", -1));
      ASSERT_EQ(GetNamePathLine("\\path/to\\file"), NamePathLine("file", "path/to", -1));
      ASSERT_EQ(GetNamePathLine("////path/to/////file"), NamePathLine("file", "path/to", -1));
      ASSERT_EQ(GetNamePathLine("\\\\\\\\path\\to\\\\\\\\file"), NamePathLine("file", "path\\to", -1));
    }

    TEST_P(GetNamePathLineP, ScanGCCLikePaths)
    {
      ASSERT_EQ(GetNamePathLine((GetParam().first + "file:").c_str()), NamePathLine("file", GetParam().second, -1));

      std::vector<std::string> gccLikeFilenames = {
        "file:12345"
      , "file:12345:"
      , "file:12345:26"
      , "file:12345:26:"
      };

      for (auto const& filename : gccLikeFilenames)
      {
        ASSERT_EQ(GetNamePathLine((GetParam().first + filename).c_str()), NamePathLine("file", GetParam().second, 12345));
      }
    }

    TEST_P(GetNamePathLineP, NotScanLineNumberGCCLikeInvalidPaths)
    {
      std::vector<std::string> gccLikeInvalidFilenames = {
        ":12345:file"
      , ":12345:file:26"
      , "file:12345)"
      , "file:12345,26):"
      , "file::"
      , "file::26"
      , "file:asdf"
      , "file:asdf:"
      , "file:asdf:26"
      };

      for (auto const& filename : gccLikeInvalidFilenames)
      {
        ASSERT_EQ(GetNamePathLine((GetParam().first + filename).c_str()), NamePathLine(filename, GetParam().second, -1));
      }
    }

    TEST_P(GetNamePathLineP, NotScanLargeLineNumberGCCLikePaths)
    {
      std::vector<std::string> gccLikeLargeLineNumberFilenames = {
        "file:123456"
      , "file:123456:"
      , "file:123456:26"
      , "file:123456:26:"
      };

      for (auto const& filename : gccLikeLargeLineNumberFilenames)
      {
        ASSERT_EQ(GetNamePathLine((GetParam().first + filename).c_str()), NamePathLine(filename, GetParam().second, -1));
      }
    }

    TEST_P(GetNamePathLineP, ScanVSLikePaths)
    {
      ASSERT_EQ(GetNamePathLine((GetParam().first + "file(").c_str()), NamePathLine("file", GetParam().second, -1));

      std::vector<std::string> vsLikeFilenames = {
        "file(12345"
      , "file(12345)"
      , "file(12345):"
      , "file(12345,"
      , "file(12345,26"
      , "file(12345,26)"
      , "file(12345,26):"
      };

      for (auto const& filename : vsLikeFilenames)
      {
        ASSERT_EQ(GetNamePathLine((GetParam().first + filename).c_str()), NamePathLine("file", GetParam().second, 12345));
      }
    }

    TEST_P(GetNamePathLineP, NotScanLineNumberVSLikeInvalidPaths)
    {
      std::vector<std::string> vsLikeInvalidFilenames = {
        "(12345)file"
      , "(12345)file:26"
      , "file(12345x"
      , "file(12345x26"
      , "file(12345x26)"
      , "file(12345:"
      , "file()"
      , "file():"
      , "file(,"
      , "file(,)"
      , "file(,):"
      , "file(asdf"
      , "file(asdf)"
      , "file(asdf):"
      , "file(asdf,"
      , "file(asdf,26)"
      , "file(asdf,26):"
      };

      for (auto const& filename : vsLikeInvalidFilenames)
      {
        ASSERT_EQ(GetNamePathLine((GetParam().first + filename).c_str()), NamePathLine(filename, GetParam().second, -1));
      }
    }

    TEST_P(GetNamePathLineP, NotScanLargeLineNumberVSLikePaths)
    {
      std::vector<std::string> vsLikeLargeLineNumberFilenames = {
        "file(123456"
      , "file(123456)"
      , "file(123456):"
      , "file(123456,"
      , "file(123456,26"
      , "file(123456,26)"
      , "file(123456,26):"
      };

      for (auto const& filename : vsLikeLargeLineNumberFilenames)
      {
        ASSERT_EQ(GetNamePathLine((GetParam().first + filename).c_str()), NamePathLine(filename, GetParam().second, -1));
      }
    }

    std::vector<ActualExpectedPath> Paths = {
      {"", ""}
    , {"////", ""}
    , {"\\\\\\\\", ""}
    , {"path/to/", "path/to"}
    , {"path\\to\\", "path\\to"}
    , {"C:\\path\\to\\", "C:\\path\\to"}
    , {"C:/path/to/", "C:/path/to"}
    , {"C:\\path/to\\\\\\\\", "C:\\path/to"}
    , {"////path\\to\\//\\", "path\\to"}
    };

    INSTANTIATE_TEST_CASE_P(, GetNamePathLineP, ::testing::ValuesIn(Paths));
  }
}

int main(int argc, char* argv[])
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
