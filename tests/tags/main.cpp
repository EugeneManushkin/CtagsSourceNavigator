#include <gtest/gtest.h>
#include <tags_repository_storage.h>
#include <tags_selector.h>
#include <tags.h>

#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <regex>
#include <vector>

namespace
{
  bool CheckIdxFiles = false;

  std::string GetFilePath(std::string const& file)
  {
    auto pos = file.find_last_of("\\/");
    return pos == std::string::npos ? std::string() : file.substr(0, pos);
  }

  std::string GetFileName(std::string const& path)
  {
    auto nameEnd = path.rbegin();
    for (; nameEnd != path.rend() && (*nameEnd == '\\' || *nameEnd == '/'); ++nameEnd);
    auto trimedPath = nameEnd == path.rend() ? std::string() : std::string(path.begin(), nameEnd.base());
    auto pos = trimedPath.find_last_of("/\\");
    return pos == std::string::npos ? trimedPath : trimedPath.substr(pos + 1);
  }

  std::string ToLower(std::string const& str)
  {
    auto result = str;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c){return static_cast<char>(tolower(c));});
    return std::move(result);
  }

  std::string ToUpper(std::string const& str)
  {
    auto result = str;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c){return static_cast<char>(toupper(c));});
    return std::move(result);
  }

  std::string MixCase(std::string const& str)
  {
    return ToUpper(str.substr(0, str.length() / 2)) + ToLower(str.substr(str.length() / 2));
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
    auto partName = ToLower(GetFileName(part));
    auto fileName = ToLower(GetFileName(path)).substr(0, partName.length());
    return fileName == partName;
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

  std::vector<std::string> ToStrings(std::vector<TagInfo>&& tags)
  {
    std::vector<std::string> result;
    std::transform(tags.begin(), tags.end(), std::back_inserter(result), [](TagInfo const& tag) { return std::move(tag.file); });
    return result;
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
      return Name == tag.name
          && File.length() <= tag.file.length()
          && PathsEqual(File, tag.file.substr(tag.file.length() - File.length()))
          && Line == tag.lineno;
    }

    static std::regex const Regex;
    std::string Name;
    std::string File;
    std::string FullPath;
    int Line;
  };

  std::regex const MetaTag::Regex("^([a-zA-Z0-9_~]+):(.+):line:([-]?[0-9]+)\\r*$");

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

        current = MetaClassCont::value_type();
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

namespace Tags
{
namespace TESTS
{
  int const LoadSuccess = 0;
  size_t const DefaultMaxCount = 1;
  bool const Unlimited = true;

  class Tags : public ::testing::Test
  {
  protected:
    std::unique_ptr<RepositoryStorage> Storage;

    std::unique_ptr<Selector> GetSelector(char const* file, bool caseInsensitive, SortingOptions sortOptions = SortingOptions::Default, size_t maxCount = DefaultMaxCount)
    {
      return Storage->GetSelector(file, caseInsensitive, sortOptions, maxCount);
    }

    std::vector<TagInfo> Find(const char* name, const char* file, SortingOptions sortOptions = SortingOptions::Default)
    {
      return GetSelector(file, false, sortOptions)->GetByName(name);
    }

    std::vector<TagInfo> FindFile(const char* file, const char* path)
    {
      return GetSelector(file, true)->GetFiles(path);
    }

    std::vector<TagInfo> FindClassMembers(const char* file, const char* classname, SortingOptions sortOptions = SortingOptions::Default)
    {
      return GetSelector(file, false, sortOptions)->GetClassMembers(classname);
    }

    std::vector<TagInfo> FindFileSymbols(const char* file)
    {
      return GetSelector(file, false)->GetByFile(file);
    }

    std::vector<std::string> GetLoadedTags(const char* file)
    {
      std::vector<std::string> result;
      auto infos = Storage->GetOwners(file);
      std::transform(infos.begin(), infos.end(), std::back_inserter(result), [](RepositoryInfo const& info){ return info.TagsPath; });
      return std::move(result);
    }

  public:
    virtual void SetUp()
    {
      Storage = RepositoryStorage::Create();
    }

    void LoadTagsFileImpl(std::string const& tagsFile, RepositoryType type, size_t expectedTagsCount)
    {
      size_t symbolsLoaded = -1;
      ASSERT_EQ(LoadSuccess, Storage->Load(tagsFile.c_str(), type, symbolsLoaded));
      ASSERT_EQ(expectedTagsCount, expectedTagsCount == -1 ? expectedTagsCount : symbolsLoaded);
    }

    void LoadTagsFile(std::string const& tagsFile, RepositoryType type, size_t expectedTagsCount)
    {
      auto idxFile = tagsFile + ".idx";
      auto modTime = GetModificationTime(idxFile);
      ASSERT_EQ(CheckIdxFiles, !!modTime);
      ASSERT_NO_FATAL_FAILURE(LoadTagsFileImpl(tagsFile, type, expectedTagsCount));
      ASSERT_TRUE(!CheckIdxFiles || modTime == GetModificationTime(idxFile));
    }

    void TestLoadTags(std::string const& tagsFile, RepositoryType type, size_t expectedTagsCount)
    {
      ASSERT_NO_FATAL_FAILURE(LoadTagsFileImpl(ToUpper(tagsFile), type, expectedTagsCount));
      ASSERT_NO_FATAL_FAILURE(LoadTagsFileImpl(ToLower(tagsFile), type, expectedTagsCount));
      ASSERT_NO_FATAL_FAILURE(LoadTagsFileImpl(AddExtraSlashes(tagsFile), type, expectedTagsCount));
      size_t symbolsLoaded = -1;
      ASSERT_NE(LoadSuccess, Storage->Load((tagsFile + "extra").c_str(), type, symbolsLoaded));
      ASSERT_THROW(Storage->Load((tagsFile + "\\").c_str(), type, symbolsLoaded), std::logic_error);
      ASSERT_THROW(Storage->Load((tagsFile + "/").c_str(), type, symbolsLoaded), std::logic_error);
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

    void LookupAllPartiallyMatchedNames(MetaTag const& metaTag, std::string const& name, bool caseInsensitive)
    {
      std::string part = name + "zzzz";
      while (!part.empty())
      {
        auto tags = GetSelector(metaTag.FullPath.c_str(), caseInsensitive)->GetByPart(part.c_str(), false, Unlimited);
        ASSERT_EQ(part.length() > metaTag.Name.length(), tags.empty()) << "Part: " << part;
        ASSERT_EQ(part.length() > metaTag.Name.length(), std::find(tags.begin(), tags.end(), metaTag) == tags.end()) << "Part: " << part;
        part.resize(part.length() - 1);
      }
    }

    void LookupAllPartiallyMatchedNamesEachCase(MetaTag const& metaTag)
    {
      LookupAllPartiallyMatchedNames(metaTag, metaTag.Name, false);
      LookupAllPartiallyMatchedNames(metaTag, metaTag.Name, true);
      LookupAllPartiallyMatchedNames(metaTag, ToLower(metaTag.Name), true);
      LookupAllPartiallyMatchedNames(metaTag, ToUpper(metaTag.Name), true);
      LookupAllPartiallyMatchedNames(metaTag, MixCase(metaTag.Name), true);
    }

    void LookupAllPartiallyMatchedFilanames(MetaTag const& metaTag)
    {
      std::string const fileName = GetFileName(metaTag.FullPath);
      std::string part = fileName + "extra";
      while (!part.empty())
      {
        auto paths = ToStrings(GetSelector(metaTag.FullPath.c_str(), true)->GetByPart(part.c_str(), true, Unlimited));
        ASSERT_EQ(part.length() > fileName.length(), paths.empty()) << "Part: " << part;
        ASSERT_EQ(part.length() > fileName.length(), std::find_if(paths.begin(), paths.end(), std::bind(HasFilenamePart, std::placeholders::_1, part)) == paths.end()) << "Part: " << part;
        part.resize(part.length() - 1);
      }
    }

    void LoadAndLookupNames(std::string const& tagsFile, std::string const& metaTagsFile, RepositoryType type = RepositoryType::Regular)
    {
      auto const metaTags = LoadMetaTags(metaTagsFile, GetFilePath(tagsFile));
      ASSERT_FALSE(metaTags.empty());
      ASSERT_NO_FATAL_FAILURE(LoadTagsFile(tagsFile, type, metaTags.size()));
      EXPECT_NO_FATAL_FAILURE(TestLoadTags(tagsFile, type, metaTags.size()));
      for (auto const& metaTag : metaTags)
      {
        EXPECT_NO_FATAL_FAILURE(LookupMetaTag(metaTag)) << "Tag info: " << metaTag << ", tags file: " << tagsFile;
        EXPECT_NO_FATAL_FAILURE(LookupMetaTagInFile(metaTag)) << "Tag info: " << metaTag << ", tags file: " << tagsFile;
        EXPECT_NO_FATAL_FAILURE(LookupAllPartiallyMatchedNamesEachCase(metaTag)) << "Tag info: " << metaTag << ", tags file: " << tagsFile;
        EXPECT_NO_FATAL_FAILURE(LookupAllPartiallyMatchedFilanames(metaTag)) << "Tag info: " << metaTag << ", tags file: " << tagsFile;
      }
    }

    void LookupAllClassMembers(std::string const& tagsFile, std::string const& metaClassesFile)
    {
      auto const metaClasses = LoadMetaClasses(metaClassesFile, GetFilePath(metaClassesFile));
      ASSERT_FALSE(metaClasses.empty());
      ASSERT_NO_FATAL_FAILURE(LoadTagsFile(tagsFile, RepositoryType::Regular, -1));
      for (auto const& metaClass : metaClasses)
      {
        EXPECT_NO_FATAL_FAILURE(LookupClassMembers(metaClass.first, metaClass.second));
      }
    }

    void TestFileBelongsToRepo(char const* file, bool belongs)
    {
      EXPECT_EQ(belongs, !GetLoadedTags(file).empty()) << "File: " << file;
      EXPECT_EQ(belongs, !Find("main", file).empty()) << "File: " << file;
    }

    void TestTagsLoadedForFile(bool singleFileRepo = false)
    {
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:\\18743\\Dummy Folder\\Repository Root/lowercasefolder/tags.cpp", true));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:/18743/Dummy Folder/Repository Root/lowercasefolder/tags.cpp", true));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:/18743/Dummy Folder/Repository Root/lowercasefolder//tags.cpp", true));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:\\18743\\Dummy Folder\\Repository Root\\lowercasefolder\\tags.cpp", true));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:\\18743\\Dummy Folder\\Repository Root\\lowercasefolder\\\\tags.cpp", true));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("c:\\18743\\dummy folder\\repository root\\lowercasefolder\\tags.cpp", true));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:\\18743\\DUMMY FOLDER\\REPOSITORY ROOT\\LOWERCASEFOLDER\\tags.cpp", true));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:\\18743\\Dummy Folder\\Repository Root/lowercasefolder/.", !singleFileRepo));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:/18743/Dummy Folder/Repository Root/lowercasefolder/.", !singleFileRepo));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:/18743/Dummy Folder/Repository Root/lowercasefolder//.", !singleFileRepo));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:\\18743\\Dummy Folder\\Repository Root\\lowercasefolder\\.", !singleFileRepo));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:\\18743\\Dummy Folder\\Repository Root\\lowercasefolder\\\\.", !singleFileRepo));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("c:\\18743\\dummy folder\\repository root\\lowercasefolder\\.", !singleFileRepo));
      EXPECT_NO_FATAL_FAILURE(TestFileBelongsToRepo("C:\\18743\\DUMMY FOLDER\\REPOSITORY ROOT\\LOWERCASEFOLDER\\.", !singleFileRepo));
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
      auto paths = ToStrings(GetSelector(reposFile.c_str(), true, SortingOptions::Default, !maxCount ? DefaultMaxCount : maxCount)->GetByPart(filePart.c_str(), true, !maxCount));
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
      size_t const expectedTags = 32;
      ASSERT_NO_FATAL_FAILURE(LoadTagsFile(tagsFile, RepositoryType::Regular, expectedTags));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "", 0, 0));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "", expectedTags, expectedTags));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "", 10, 10));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "\\", 10, 10));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "/", 10, 10));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "/\\/", 10, 10));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "5times.cpp", 0, 5));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "5times", 0, 5));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "\\5times", 0, 5));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "/5times", 0, 5));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "/\\/5times", 0, 5));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "5times/", 0, 5));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "5times\\", 0, 5));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "5times/\\/", 0, 5));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "\\5times\\", 0, 5));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "/5times/", 0, 5));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "/\\/5times/\\/", 0, 5));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "folder1\\5times", 0, 1));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "folder1/5times", 0, 1));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "folder1/\\/5times", 0, 1));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "\\folder1/5times", 0, 1));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "/folder1/5times", 0, 1));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "/\\/folder1/5times", 0, 1));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "\\folder1/5times/", 0, 1));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "/folder1/5times\\", 0, 1));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "/\\/folder1/5times/\\/", 0, 1));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "folder1\\folder2\\folder3\\5times", 0, 1));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "folder1/folder2/folder3/5times", 0, 1));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "folder1/\\/folder2/\\/folder3/\\/5times", 0, 1));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "10times.cpp", 0, 10));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "10times", 0, 10));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "10times.cpp", 10, 10));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "15times.cpp", 0, 17));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "15times", 10, 10));
      EXPECT_NO_FATAL_FAILURE(TestRepeatedFile(tagsFile, "15times.cpp", 10, 15));
    }

    void TestFindFile(std::string const& tagsFile, std::string const& path, std::string const& expectedSubpath, size_t expectedSize, int expectedLineNum = -1)
    {
      auto tags = FindFile(tagsFile.c_str(), path.c_str());
      ASSERT_EQ(expectedSize, tags.size());
      for (auto const& tag : tags)
      {
        EXPECT_TRUE(!tag.file.compare(tag.file.length() - expectedSubpath.length(), expectedSubpath.length(), expectedSubpath));
        EXPECT_EQ(expectedLineNum, tag.lineno);
      }
    }

    void TestFindFile(std::string const& tagsFile)
    {
      size_t const expectedTags = 4;
      ASSERT_NO_FATAL_FAILURE(LoadTagsFile(tagsFile, RepositoryType::Regular, expectedTags));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "", "", 0));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "/", "", 0));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header", "", 0));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header.h", "some\\path\\header.h", 2));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header.h:", "some\\path\\header.h", 2));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header.h:asdf", "", 0));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header.h(asdf)", "", 0));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header.h:7", "some\\path\\header.h", 2, 7));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header.h:7:", "some\\path\\header.h", 2, 7));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header.h(7)", "some\\path\\header.h", 2, 7));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header.h(7):", "some\\path\\header.h", 2, 7));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header.h:12345", "some\\path\\header.h", 2, 12345));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header.h:12345:", "some\\path\\header.h", 2, 12345));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header.h(12345)", "some\\path\\header.h", 2, 12345));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header.h(12345):", "some\\path\\header.h", 2, 12345));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header.h:123456", "", 0));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header.h.extra.hpp", "some\\path\\header.h.extra.hpp", 2));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header.h.extra.hpp:", "some\\path\\header.h.extra.hpp", 2));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header.h.extra.hpp:asdf", "", 0));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header.h.extra.hpp(asdf)", "", 0));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header.h.extra.hpp:7", "some\\path\\header.h.extra.hpp", 2, 7));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header.h.extra.hpp:7:", "some\\path\\header.h.extra.hpp", 2, 7));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header.h.extra.hpp(7)", "some\\path\\header.h.extra.hpp", 2, 7));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header.h.extra.hpp(7):", "some\\path\\header.h.extra.hpp", 2, 7));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header.h.extra.hpp:12345", "some\\path\\header.h.extra.hpp", 2, 12345));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header.h.extra.hpp:12345:", "some\\path\\header.h.extra.hpp", 2, 12345));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header.h.extra.hpp(12345)", "some\\path\\header.h.extra.hpp", 2, 12345));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header.h.extra.hpp(12345):", "some\\path\\header.h.extra.hpp", 2, 12345));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header.h.extra.hpp:123456", "", 0));
      EXPECT_NO_FATAL_FAILURE(TestFindFile(tagsFile, "some/path/header.h.extra.hpp(123456)", "", 0));
    }
  };

  TEST_F(Tags, LoadsEmptyRepos)
  {
    LoadTagsFile("empty_repos/tags", RepositoryType::Regular, 0);
  }

  TEST_F(Tags, AllNamesFoundInUniversalSingleFileRepos)
  {
    LoadAndLookupNames("single_file_repos/tags.universal", "single_file_repos/tags.universal.meta");
  }

  TEST_F(Tags, AllNamesFoundInExuberantWinSingleFileRepos)
  {
    LoadAndLookupNames("single_file_repos/tags.exuberant.w", "single_file_repos/tags.exuberant.w.meta");
  }

  TEST_F(Tags, AllNamesFoundInExuberantPlatformCharPathsRepos)
  {
    LoadAndLookupNames("platform_char_paths_repos/tags.exuberant.w", "platform_char_paths_repos/tags.meta");
  }

  TEST_F(Tags, AllNamesFoundInUniversalPlatformCharPathsRepos)
  {
    LoadAndLookupNames("platform_char_paths_repos/tags.universal", "platform_char_paths_repos/tags.meta");
  }

  TEST_F(Tags, AllNamesFoundInCygwinMixedCaseRepos)
  {
    LoadAndLookupNames("Mixed Case Repos/tags.exuberant", "Mixed Case Repos/tags.meta");
  }

  TEST_F(Tags, AllNamesFoundInExuberantMixedCaseRepos)
  {
    LoadAndLookupNames("Mixed Case Repos/tags.exuberant.w", "Mixed Case Repos/tags.meta");
  }

  TEST_F(Tags, AllNamesFoundInUniversalMixedCaseRepos)
  {
    LoadAndLookupNames("Mixed Case Repos/tags.universal", "Mixed Case Repos/tags.meta");
  }

  TEST_F(Tags, AllNamesFoundInExuberantSemicolonQuotesRepos)
  {
    LoadAndLookupNames("semicolon_quotes_repos/tags.exuberant.w", "semicolon_quotes_repos/tags.meta");
  }

  TEST_F(Tags, AllNamesFoundInUniversalSemicolonQuotesRepos)
  {
    LoadAndLookupNames("semicolon_quotes_repos/tags.universal", "semicolon_quotes_repos/tags.meta");
  }

  TEST_F(Tags, AllNamesFoundInExuberantNoKindNoLineRepos)
  {
    LoadAndLookupNames("no_kind_no_line_repos/tags.exuberant.w", "no_kind_no_line_repos/tags.meta");
  }

  TEST_F(Tags, AllNamesFoundInUniversalNoKindNoLineRepos)
  {
    LoadAndLookupNames("no_kind_no_line_repos/tags.universal", "no_kind_no_line_repos/tags.meta");
  }

  TEST_F(Tags, AllNamesFoundInCygwinFullPathRepos)
  {
    LoadAndLookupNames("full_path_repos/tags.exuberant", "full_path_repos/tags.meta");
    TestTagsLoadedForFile();
  }

  TEST_F(Tags, AllNamesFoundInCygwinMixedSlashFullPathRepos)
  {
    LoadAndLookupNames("full_path_repos/tags.exuberant.mixed.slashes", "full_path_repos/tags.meta");
    TestTagsLoadedForFile();
  }

  TEST_F(Tags, AllNamesFoundInExuberantFullPathRepos)
  {
    LoadAndLookupNames("full_path_repos/tags.exuberant.w", "full_path_repos/tags.meta");
    TestTagsLoadedForFile();
  }

  TEST_F(Tags, AllNamesFoundInUniversalFullPathRepos)
  {
    LoadAndLookupNames("full_path_repos/tags.universal", "full_path_repos/tags.meta");
    TestTagsLoadedForFile();
  }

  bool const SingleFileRepos = true;

  TEST_F(Tags, AllNamesFoundInCygwinSingleFileFullPathRepos)
  {
    LoadAndLookupNames("full_path_single_file_repo/tags.exuberant", "full_path_single_file_repo/tags.exuberant.meta", RepositoryType::Temporary);
    TestTagsLoadedForFile(SingleFileRepos);
  }

  TEST_F(Tags, AllNamesFoundInExuberantSingleFileFullPathRepos)
  {
    LoadAndLookupNames("full_path_single_file_repo/tags.exuberant.w", "full_path_single_file_repo/tags.exuberant.meta", RepositoryType::Temporary);
    TestTagsLoadedForFile(SingleFileRepos);
  }

  TEST_F(Tags, AllNamesFoundInUniversalSingleFileFullPathRepos)
  {
    LoadAndLookupNames("full_path_single_file_repo/tags.universal", "full_path_single_file_repo/tags.universal.meta", RepositoryType::Temporary);
    TestTagsLoadedForFile(SingleFileRepos);
  }

  TEST_F(Tags, FilePartsFoundInUniversalRepeatedFilesRepos)
  {
    TestRepeatedFiles("repeated_files_repos/tags.universal");
  }

  TEST_F(Tags, FilePartsFoundInCygwinRepeatedFilesRepos)
  {
    TestRepeatedFiles("repeated_files_repos/tags.exuberant");
  }

  TEST_F(Tags, FilePartsFoundInExuberantRepeatedFilesRepos)
  {
    TestRepeatedFiles("repeated_files_repos/tags.exuberant.w");
  }

  TEST_F(Tags, AllClassMembersFoundInExuberantRepos)
  {
    LookupAllClassMembers("classes_repos/tags.exuberant.w", "classes_repos/tags.exuberant.w.classmeta");
  }

  TEST_F(Tags, AllClassMembersFoundInUniversalRepos)
  {
    LookupAllClassMembers("classes_repos/tags.universal", "classes_repos/tags.universal.classmeta");
  }

  TEST_F(Tags, AnyFileBelongsToMinimalSingleFileRepos)
  {
    ASSERT_NO_FATAL_FAILURE(LoadTagsFile("minimal_single_file_repos/tags", RepositoryType::Regular, 1));
    TestTagsLoadedForFile();
  }

  TEST_F(Tags, FindFileInCygwinIncludeFileRepos)
  {
    TestFindFile("include_file_repos/tags.exuberant");
  }

  TEST_F(Tags, FindFileInExuberantIncludeFileRepos)
  {
    TestFindFile("include_file_repos/tags.exuberant.w");
  }

  TEST_F(Tags, FindFileInUniversalIncludeFileRepos)
  {
    TestFindFile("include_file_repos/tags.universal");
  }

  TEST_F(Tags, CacheTags)
  {
    if (CheckIdxFiles) return;
    ASSERT_NO_FATAL_FAILURE(LoadTagsFile("cache_repos/tags", RepositoryType::Regular, 3));
    ASSERT_EQ(0, Storage->GetInfo("cache_repos/tags").ElapsedSinceCached);
    Storage->CacheTag(Find("first", "cache_repos/tags").at(0), 3, true);
  }

  TEST_F(Tags, ElapsedSinceCachedResetWhenTagCached)
  {
    if (!CheckIdxFiles) return;
    ASSERT_NO_FATAL_FAILURE(LoadTagsFile("cache_repos/tags", RepositoryType::Regular, 3));
    auto elapsed = Storage->GetInfo("cache_repos/tags").ElapsedSinceCached;
    ASSERT_GT(elapsed, 0);
    Storage->CacheTag(Find("first", "cache_repos/tags").at(0), 3, true);
    ASSERT_NO_FATAL_FAILURE(LoadTagsFile("cache_repos/tags", RepositoryType::Regular, 3));
    ASSERT_GT(elapsed, Storage->GetInfo("cache_repos/tags").ElapsedSinceCached);
  }
}
}

int main(int argc, char* argv[])
{
  CheckIdxFiles = std::find_if(argv, argv + argc, [](char* argument) {return !strcmp(argument, "--CheckIdxFiles");}) != argv + argc;
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
