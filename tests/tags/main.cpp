#include <gtest/gtest.h>
#include <tags.h>

#define NOMINMAX
#include <windows.h>

#include <fstream>
#include <memory>
#include <string>
#include <regex>
#include <vector>

Config config;

int isident(int chr)
{
  return config.isident(chr);
}

namespace
{
  using TagArrayPtr = std::unique_ptr<TagArray>;
  using TagsCont = std::vector<TagInfo>;

  std::string GetFilePath(std::string const& file)
  {
    auto pos = file.rfind('\\');
    return pos == std::string::npos ? std::string() : file.substr(0, pos);
  }

  std::vector<char> ToLower(std::string const& str)
  {
    std::vector<char> result(str.begin(), str.end());
    result.push_back(0);
    ::CharLowerA(&result[0]);
    return result;
  }

  bool PathsEqual(std::string const& left, std::string const& right)
  {
    auto a = ToLower(left);
    auto b = ToLower(right);
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
  }

  bool IsFullPath(std::string const& path)
  {
    return path.length() > 1 && path[1] == ':';
  }

  struct MetaTag
  {
    MetaTag(std::string const& line)
    {
      std::smatch matchResult;
      if (!std::regex_match(line, matchResult, Regex) || matchResult.size() != 4)
        throw std::runtime_error("Failed to parse meta tag, line: '" + line + "'");

      Name = matchResult[1];
      File = matchResult[2];
      Line = atoi(std::string(matchResult[3]).c_str());
    }

    void Print(std::ostream& stream) const
    {
      stream << "'" << Name << "' " << File << ":" << Line;
    }

    bool operator == (TagInfo const& tag) const
    {
      return Name == tag.name.Str()
          && File.length() <= tag.file.Length()
          && PathsEqual(File, std::string(tag.file.Str() + tag.file.Length() - File.length()))
          && Line == tag.lineno;
    }

    static std::regex const Regex;
    std::string Name;
    std::string File;
    int Line;
  };

  std::regex const MetaTag::Regex("^([a-zA-Z0-9_~]+):(.+):line:([0-9]+)$");

  using MetaTagCont = std::vector<MetaTag>;

  bool operator == (TagInfo const& left, MetaTag const& right)
  {
    return right == left;
  }

  std::ostream& operator << (std::ostream& stream, MetaTag const& metaTag)
  {
    metaTag.Print(stream);
    return stream;
  }

  TagsCont ToTagsCont(TagArrayPtr&& arr)
  {
    TagsCont result;
    if (!arr)
      return result;

    for (int i = 0; i < arr->Count(); ++i)
    {
      result.push_back(*(*arr)[i]);
      delete (*arr)[i];
    }

    arr.reset();
    return result;
  }

  MetaTagCont LoadMetaTags(std::string const& fileName)
  {
    MetaTagCont result;
    std::ifstream file;
    file.exceptions(std::ifstream::badbit);
    file.open(fileName);
    std::shared_ptr<void> fileCloser(0, [&](void*) { file.close(); });
    std::string buf;
    while (std::getline(file, buf))
    {
      if (!buf.empty())
        result.push_back(MetaTag(buf));
    }

    return result;
  }
}

namespace TESTS
{
  int const LoadSuccess = 0;

  class Tags : public ::testing::Test
  {
  public:
    virtual void SetUp()
    {
    }

    virtual void TearDown() 
    {
      UnloadTags(-1);
    }

    void LoadTagsFile(std::string const& tagsFile, size_t expectedTagsCount)
    {
      size_t symbolsLoaded = -1;
      ASSERT_EQ(LoadSuccess, Load(tagsFile.c_str(), symbolsLoaded));
      ASSERT_EQ(expectedTagsCount, symbolsLoaded);
    }

    void LookupMetaTag(MetaTag const& metaTag, std::string const& tagsFile)
    {
      auto tags = ToTagsCont(TagArrayPtr(Find(metaTag.Name.c_str(), tagsFile.c_str())));
      ASSERT_FALSE(tags.empty());
      ASSERT_FALSE(std::find(tags.begin(), tags.end(), metaTag) == tags.end());
    }

    void LookupMetaTagInFile(MetaTag const& metaTag, std::string const& repoRoot)
    {
      std::string const fileFullPath = IsFullPath(metaTag.File) ? metaTag.File : repoRoot + "\\" + metaTag.File;
      auto tags = ToTagsCont(TagArrayPtr(FindFileSymbols(fileFullPath.c_str())));
      ASSERT_FALSE(tags.empty());
      ASSERT_FALSE(std::find(tags.begin(), tags.end(), metaTag) == tags.end());
    }

    void LoadAndLookupNames(std::string const& tagsFile, std::string const& metaTagsFile)
    {
      auto const repoRoot = GetFilePath(tagsFile);
      auto const metaTags = LoadMetaTags(metaTagsFile);
      ASSERT_FALSE(metaTags.empty());
      ASSERT_NO_FATAL_FAILURE(LoadTagsFile(tagsFile, metaTags.size()));
      for (auto const& metaTag : metaTags)
      {
        EXPECT_NO_FATAL_FAILURE(LookupMetaTag(metaTag, tagsFile)) << "Tag info: " << metaTag << ", tags file: " << tagsFile;
        EXPECT_NO_FATAL_FAILURE(LookupMetaTagInFile(metaTag, repoRoot)) << "Tag info: " << metaTag << ", tags file: " << tagsFile;
      }
    }
  };

  TEST_F(Tags, LoadsEmptyRepos)
  {
    LoadTagsFile("empty_repos\\tags", 0);
  }

  TEST_F(Tags, AllNamesFoundInUniversalSingleFileRepos)
  {
    LoadAndLookupNames("single_file_repos\\tags.universal", "single_file_repos\\tags.universal.meta");
  }

  TEST_F(Tags, AllNamesFoundInExuberantWinSingleFileRepos)
  {
    LoadAndLookupNames("single_file_repos\\tags.exuberant.w", "single_file_repos\\tags.exuberant.w.meta");
  }

  TEST_F(Tags, AllNamesFoundInExuberantPlatformCharPathsRepos)
  {
    LoadAndLookupNames("platform_char_paths_repos\\tags.exuberant.w", "platform_char_paths_repos\\tags.meta");
  }

  TEST_F(Tags, AllNamesFoundInUniversalPlatformCharPathsRepos)
  {
    LoadAndLookupNames("platform_char_paths_repos\\tags.universal", "platform_char_paths_repos\\tags.meta");
  }

  TEST_F(Tags, AllNamesFoundInCygwinMixedCaseRepos)
  {
    LoadAndLookupNames("Mixed Case Repos\\tags.exuberant", "Mixed Case Repos\\tags.meta");
  }

  TEST_F(Tags, AllNamesFoundInExuberantMixedCaseRepos)
  {
    LoadAndLookupNames("Mixed Case Repos\\tags.exuberant.w", "Mixed Case Repos\\tags.meta");
  }

  TEST_F(Tags, AllNamesFoundInUniversalMixedCaseRepos)
  {
    LoadAndLookupNames("Mixed Case Repos\\tags.universal", "Mixed Case Repos\\tags.meta");
  }

  TEST_F(Tags, AllNamesFoundInCygwinFullPathRepos)
  {
    LoadAndLookupNames("full_path_repos\\tags.exuberant", "full_path_repos\\tags.meta");
  }

  TEST_F(Tags, AllNamesFoundInCygwinMixedSlashFullPathRepos)
  {
    LoadAndLookupNames("full_path_repos\\tags.exuberant.mixed.slashes", "full_path_repos\\tags.meta");
  }

  TEST_F(Tags, AllNamesFoundInExuberantFullPathRepos)
  {
    LoadAndLookupNames("full_path_repos\\tags.exuberant.w", "full_path_repos\\tags.meta");
  }

  TEST_F(Tags, AllNamesFoundInUniversalFullPathRepos)
  {
    LoadAndLookupNames("full_path_repos\\tags.universal", "full_path_repos\\tags.meta");
  }
}

int main(int argc, char* argv[])
{
  testing::InitGoogleTest(&argc, argv);
  RUN_ALL_TESTS();  
}
