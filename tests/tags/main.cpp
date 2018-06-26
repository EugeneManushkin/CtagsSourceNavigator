#include <gtest/gtest.h>
#include <tags.h>

#define NOMINMAX
#include <windows.h>

#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <regex>
#include <vector>

Config config;

int isident(int chr)
{
  return config.isident(chr);
}

namespace
{
  bool CheckIdxFiles = false;

  std::string GetFilePath(std::string const& file)
  {
    auto pos = file.rfind('\\');
    return pos == std::string::npos ? std::string() : file.substr(0, pos);
  }

  std::string GetFileName(std::string const& path)
  {
      auto pos = path.rfind('\\');
      return pos == std::string::npos ? path : path.substr(pos + 1);
  }

  std::string ToLower(std::string const& str)
  {
    std::vector<char> result(str.begin(), str.end());
    result.push_back(0);
    ::CharLowerA(&result[0]);
    return std::string(&result[0]);
  }

  std::string ToUpper(std::string const& str)
  {
    std::vector<char> result(str.begin(), str.end());
    result.push_back(0);
    ::CharUpperA(&result[0]);
    return std::string(&result[0]);
  }

  std::string AddExtraSlashes(std::string const& str)
  {
    std::string result;
    for (auto c : str)
    {
      result += std::string(c == '\\' ? 2 : 1, c);
    }
    return result;
  }

  bool PathsEqual(std::string const& left, std::string const& right)
  {
    return ToLower(left) == ToLower(right);
  }

  bool HasFilenamePart(std::string const& path, std::string const& part)
  {
    auto fileName = ToLower(GetFileName(path)).substr(0, part.length());
    return fileName == ToLower(part);
  }

  bool IsFullPath(std::string const& path)
  {
    return path.length() > 1 && path[1] == ':';
  }

  time_t GetModificationTime(std::string const& filename)
  {
    struct stat st;
    return stat(filename.c_str(), &st) == -1 ? 0 : std::max(st.st_mtime, st.st_ctime);
  }

  struct MetaTag
  {
    MetaTag(std::string const& line, std::string const& repoRoot)
    {
      std::smatch matchResult;
      if (!std::regex_match(line, matchResult, Regex) || matchResult.size() != 4)
        throw std::runtime_error("Failed to parse meta tag, line: '" + line + "'");

      Name = matchResult[1];
      File = matchResult[2];
      FullPath = IsFullPath(File) ? File : repoRoot + "\\" + File;
      Line = atoi(std::string(matchResult[3]).c_str());
    }

    void Print(std::ostream& stream) const
    {
      stream << "'" << Name << "' " << FullPath << ":" << Line;
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
    std::string FullPath;
    int Line;
  };

  std::regex const MetaTag::Regex("^([a-zA-Z0-9_~]+):(.+):line:([0-9]+)$");

  using MetaTagCont = std::vector<MetaTag>;
  using MetaClassCont = std::vector<std::pair<std::string, MetaTagCont> >;

  bool operator == (TagInfo const& left, MetaTag const& right)
  {
    return right == left;
  }

  std::ostream& operator << (std::ostream& stream, MetaTag const& metaTag)
  {
    metaTag.Print(stream);
    return stream;
  }

  MetaTagCont LoadMetaTags(std::string const& fileName, std::string const& repoRoot)
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
        result.push_back(MetaTag(buf, repoRoot));
    }

    return result;
  }

  MetaClassCont LoadMetaClasses(std::string const& fileName, std::string const& repoRoot)
  {
    std::ifstream file;
    file.exceptions(std::ifstream::badbit);
    file.open(fileName);
    std::shared_ptr<void> fileCloser(0, [&](void*) { file.close(); });
    MetaClassCont result;
    MetaClassCont::value_type current;
    std::string buf;
    while (std::getline(file, buf))
    {
      if (buf.empty())
        continue;

      if (buf[0] == '#')
      {
        if (!current.first.empty())
          result.push_back(std::move(current));

        std::swap(current, MetaClassCont::value_type());
        current.first = buf.substr(1, std::string::npos);
      }
      else
      {
        current.second.push_back(MetaTag(buf, repoRoot));
      }
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

    void LoadTagsFileImpl(std::string const& tagsFile, size_t expectedTagsCount)
    {
      size_t symbolsLoaded = -1;
      ASSERT_EQ(LoadSuccess, Load(tagsFile.c_str(), symbolsLoaded));
      ASSERT_EQ(expectedTagsCount, expectedTagsCount == -1 ? expectedTagsCount : symbolsLoaded);
    }

    void LoadTagsFile(std::string const& tagsFile, size_t expectedTagsCount)
    {
      auto idxFile = tagsFile + ".idx";
      auto modTime = GetModificationTime(idxFile);
      ASSERT_EQ(CheckIdxFiles, !!modTime);
      ASSERT_NO_FATAL_FAILURE(LoadTagsFileImpl(tagsFile, expectedTagsCount));
      ASSERT_TRUE(!CheckIdxFiles || modTime == GetModificationTime(idxFile));
    }

    void TestLoadTags(std::string const& tagsFile, size_t expectedTagsCount)
    {
      ASSERT_NO_FATAL_FAILURE(LoadTagsFileImpl(ToUpper(tagsFile), expectedTagsCount));
      ASSERT_NO_FATAL_FAILURE(LoadTagsFileImpl(ToLower(tagsFile), expectedTagsCount));
      ASSERT_NO_FATAL_FAILURE(LoadTagsFileImpl(AddExtraSlashes(tagsFile), expectedTagsCount));
      size_t symbolsLoaded = -1;
      ASSERT_NE(LoadSuccess, Load((tagsFile + "extra").c_str(), symbolsLoaded));
      ASSERT_THROW(Load((tagsFile + "\\").c_str(), symbolsLoaded), std::logic_error);
      ASSERT_THROW(Load((tagsFile + "/").c_str(), symbolsLoaded), std::logic_error);
    }

    void LookupMetaTag(MetaTag const& metaTag)
    {
      auto tags = Find(metaTag.Name.c_str(), metaTag.FullPath.c_str());
      ASSERT_FALSE(tags.empty());
      ASSERT_FALSE(std::find(tags.begin(), tags.end(), metaTag) == tags.end());
    }

    void LookupMetaTagInFile(MetaTag const& metaTag)
    {
      auto tags = FindFileSymbols(metaTag.FullPath.c_str());
      ASSERT_FALSE(tags.empty());
      ASSERT_FALSE(std::find(tags.begin(), tags.end(), metaTag) == tags.end());
    }

    void LookupClassMembers(std::string const& className, MetaTagCont const& expectedMembers)
    {
      ASSERT_FALSE(expectedMembers.empty());
      auto tags = FindClassMembers(expectedMembers.back().FullPath.c_str(), className.c_str());
      for (auto const& metaTag : expectedMembers)
      {
        EXPECT_FALSE(std::find(tags.begin(), tags.end(), metaTag) == tags.end()) << "Class: " << className << ", member: " << metaTag;
      }
    }

    void LookupAllPartiallyMatchedNames(MetaTag const& metaTag)
    {
      std::string part = metaTag.Name + "extra";
      while (!part.empty())
      {
        auto tags = FindPartiallyMatchedTags(metaTag.FullPath.c_str(), part.c_str(), 0);
        ASSERT_EQ(part.length() > metaTag.Name.length(), tags.empty()) << "Part: " << part;
        ASSERT_EQ(part.length() > metaTag.Name.length(), std::find(tags.begin(), tags.end(), metaTag) == tags.end()) << "Part: " << part;
        part.resize(part.length() - 1);
      }
    }

    void LookupAllPartiallyMatchedFilanames(MetaTag const& metaTag)
    {
      std::string const fileName = GetFileName(metaTag.FullPath);
      std::string part = fileName + "extra";
      while (!part.empty())
      {
        auto paths = FindPartiallyMatchedFile(metaTag.FullPath.c_str(), part.c_str(), 0);
        ASSERT_EQ(part.length() > fileName.length(), paths.empty()) << "Part: " << part;
        ASSERT_EQ(part.length() > fileName.length(), std::find_if(paths.begin(), paths.end(), std::bind(HasFilenamePart, std::placeholders::_1, part)) == paths.end()) << "Part: " << part;
        part.resize(part.length() - 1);
      }
    }

    void LoadAndLookupNames(std::string const& tagsFile, std::string const& metaTagsFile)
    {
      auto const metaTags = LoadMetaTags(metaTagsFile, GetFilePath(tagsFile));
      ASSERT_FALSE(metaTags.empty());
      ASSERT_NO_FATAL_FAILURE(LoadTagsFile(tagsFile, metaTags.size()));
      EXPECT_NO_FATAL_FAILURE(TestLoadTags(tagsFile, metaTags.size()));
      for (auto const& metaTag : metaTags)
      {
        EXPECT_NO_FATAL_FAILURE(LookupMetaTag(metaTag)) << "Tag info: " << metaTag << ", tags file: " << tagsFile;
        EXPECT_NO_FATAL_FAILURE(LookupMetaTagInFile(metaTag)) << "Tag info: " << metaTag << ", tags file: " << tagsFile;
        EXPECT_NO_FATAL_FAILURE(LookupAllPartiallyMatchedNames(metaTag)) << "Tag info: " << metaTag << ", tags file: " << tagsFile;
        EXPECT_NO_FATAL_FAILURE(LookupAllPartiallyMatchedFilanames(metaTag)) << "Tag info: " << metaTag << ", tags file: " << tagsFile;
      }
    }

    void LookupAllClassMembers(std::string const& tagsFile, std::string const& metaClassesFile)
    {
      auto const metaClasses = LoadMetaClasses(metaClassesFile, GetFilePath(metaClassesFile));
      ASSERT_FALSE(metaClasses.empty());
      ASSERT_NO_FATAL_FAILURE(LoadTagsFile(tagsFile, -1));
      for (auto const& metaClass : metaClasses)
      {
        EXPECT_NO_FATAL_FAILURE(LookupClassMembers(metaClass.first, metaClass.second));
      }
    }

    void TestFileBelongsToRepo(char const* file, bool belongs)
    {
      EXPECT_EQ(belongs, TagsLoadedForFile(file)) << "File: " << file;
      EXPECT_EQ(belongs, !Find("main", file).empty()) << "File: " << file;
    }

    void TestTagsLoadedForFile()
    {
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:\\18743\\Dummy Folder\\Repository Root/lowercasefolder/.", true));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:/18743/Dummy Folder/Repository Root/lowercasefolder/.", true));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:/18743/Dummy Folder/Repository Root/lowercasefolder//.", true));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:\\18743\\Dummy Folder\\Repository Root\\lowercasefolder\\.", true));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:\\18743\\Dummy Folder\\Repository Root\\lowercasefolder\\\\.", true));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("c:\\18743\\dummy folder\\repository root\\lowercasefolder\\.", true));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:\\18743\\DUMMY FOLDER\\REPOSITORY ROOT\\LOWERCASEFOLDER\\.", true));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:\\18743\\Dummy Folder\\Repository Root\\lowercasefolder", false));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:\\18743\\Dummy Folder\\Repository Root/lowercasefolder/", false));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:/18743/Dummy Folder/Repository Root/lowercasefolder/", false));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:/18743/Dummy Folder/Repository Root/lowercasefolder//", false));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:\\18743\\Dummy Folder\\Repository Root\\lowercasefolder\\", false));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:\\18743\\Dummy Folder\\Repository Root\\lowercasefolder\\\\", false));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:\\18743\\Dummy Folder\\Repository Root\\lowercasefolde", false));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:\\18743\\Dummy Folder\\Repository Root\\lowercasefolde\\", false));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("D:\\18743\\Dummy Folder\\Repository Root\\lowercasefolder\\file.cpp", false));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:\\18743\\Dummy Folder\\Repository Root\\lowercasefoldex\\file.cpp", false));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:\\18743\\Dummy Folder\\Repository Root\\lowercasefoldername", false));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:\\18743\\Dummy Folder\\Repository Root\\lowercasefoldername\\", false));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:\\18743\\Dummy Folder\\Repository Root\\lowercasefoldername\\file.cpp", false));
    }

    void TestRepeatedFile(std::string const& reposFile, std::string const& filePart, size_t maxCount, size_t expectedCount)
    {
      auto paths = FindPartiallyMatchedFile(reposFile.c_str(), filePart.c_str(), maxCount);
      ASSERT_EQ(expectedCount, paths.size());
      if (filePart.empty())
        return;

      for (auto const& path : paths)
      {
        EXPECT_TRUE(HasFilenamePart(path, filePart)) << "File part not found: " << filePart;
      }
    }

    void TestRepeatedFiles(std::string const& tagsFile)
    {
      size_t const expectedTags = 30;
      ASSERT_NO_FATAL_FAILURE(LoadTagsFile(tagsFile, expectedTags));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "", expectedTags, expectedTags));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "", 10, 10));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "5times.cpp", 0, 5));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "5times", 0, 5));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "10times.cpp", 0, 10));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "10times", 0, 10));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "10times.cpp", 10, 10));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "15times.cpp", 0, 15));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "15times", 10, 10));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "15times.cpp", 10, 15));
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
    TestTagsLoadedForFile();
  }

  TEST_F(Tags, AllNamesFoundInCygwinMixedSlashFullPathRepos)
  {
    LoadAndLookupNames("full_path_repos\\tags.exuberant.mixed.slashes", "full_path_repos\\tags.meta");
    TestTagsLoadedForFile();
  }

  TEST_F(Tags, AllNamesFoundInExuberantFullPathRepos)
  {
    LoadAndLookupNames("full_path_repos\\tags.exuberant.w", "full_path_repos\\tags.meta");
    TestTagsLoadedForFile();
  }

  TEST_F(Tags, AllNamesFoundInUniversalFullPathRepos)
  {
    LoadAndLookupNames("full_path_repos\\tags.universal", "full_path_repos\\tags.meta");
    TestTagsLoadedForFile();
  }

  TEST_F(Tags, FilePartsFoundInUniversalRepeatedFilesRepos)
  {
    TestRepeatedFiles("repeated_files_repos\\tags.universal");
  }

  TEST_F(Tags, FilePartsFoundInCygwinRepeatedFilesRepos)
  {
    TestRepeatedFiles("repeated_files_repos\\tags.exuberant");
  }

  TEST_F(Tags, FilePartsFoundInExuberantRepeatedFilesRepos)
  {
    TestRepeatedFiles("repeated_files_repos\\tags.exuberant.w");
  }

  TEST_F(Tags, AllClassMembersFoundInExuberantRepos)
  {
    LookupAllClassMembers("classes_repos\\tags.exuberant.w", "classes_repos\\tags.exuberant.w.classmeta");
  }

  TEST_F(Tags, AllClassMembersFoundInUniversalRepos)
  {
    LookupAllClassMembers("classes_repos\\tags.universal", "classes_repos\\tags.universal.classmeta");
  }
}

int main(int argc, char* argv[])
{
  CheckIdxFiles = std::find_if(argv, argv + argc, [](char* argument) {return !strcmp(argument, "--CheckIdxFiles");}) != argv + argc;
  testing::InitGoogleTest(&argc, argv);
  RUN_ALL_TESTS();  
}
