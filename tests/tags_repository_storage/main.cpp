#include <gtest/gtest.h>
#include <tags_repository.h>
#include <tags_repository_storage.h>

#include <ostream>

namespace
{
  std::string GetDirOfFile(std::string const& fileName)
  {
    auto pos = fileName.find_last_of("/\\");
    return pos == std::string::npos ? std::string() : fileName.substr(0, pos);
  }

  class MockRepository : public Tags::Internal::Repository
  {
  public:
    MockRepository(char const* tagsPath)
      : TagsFilePath(tagsPath)
    {
    }

    int Load(size_t& symbolsLoaded) override
    {
      return 0;
    }

    bool Belongs(char const* file) const override
    {
      auto dir = GetDirOfFile(TagsFilePath);
      return !dir.compare(0, dir.length(), file, 0, dir.length());
    }

    int CompareTagsPath(const char* tagsPath) const override
    {
      return TagsFilePath.compare(tagsPath);
    }

    std::string TagsPath() const override
    {
      return TagsFilePath;
    }

    std::string Root() const override
    {
      return GetDirOfFile(TagsFilePath);
    }

    std::vector<TagInfo> FindByName(const char* name) const override
    {
      return std::vector<TagInfo>();
    }

    std::vector<TagInfo> FindByName(const char* part, size_t maxCount, bool caseInsensitive) const override
    {
      return std::vector<TagInfo>();
    }

    std::vector<TagInfo> FindFiles(const char* path) const override
    {
      return std::vector<TagInfo>();
    }

    std::vector<TagInfo> FindFiles(const char* part, size_t maxCount) const override
    {
      return std::vector<TagInfo>();
    }

    std::vector<TagInfo> FindClassMembers(const char* classname) const override
    {
      return std::vector<TagInfo>();
    }

    std::vector<TagInfo> FindByFile(const char* file) const override
    {
      return std::vector<TagInfo>();
    }

    void CacheTag(TagInfo const& tag, size_t cacheSize, bool flush) override
    {
    }

    void EraseCachedTag(TagInfo const& tag, bool flush) override
    {
    }

    std::vector<std::pair<TagInfo, size_t>> GetCachedTags(bool getFiles) const override
    {
      return std::vector<std::pair<TagInfo, size_t>>();
    }

  private:
    std::string TagsFilePath;
  };

  std::unique_ptr<Tags::Internal::Repository> MockRepositoryFactory(char const* tagsPath, Tags::RepositoryType)
  {
    return std::unique_ptr<Tags::Internal::Repository>(new MockRepository(tagsPath));
  }
}

namespace Tags
{
  bool operator == (RepositoryInfo const& left, RepositoryInfo const& right)
  {
    return left.TagsPath == right.TagsPath
        && left.Type == right.Type
        && left.Root == right.Root
    ;
  }

  bool operator == (std::vector<RepositoryInfo> const& left, std::vector<RepositoryInfo> const& right)
  {
    auto minuend = right;
    for (auto const& repo : left)
    {
      auto i = std::find(minuend.begin(), minuend.end(), repo);
      if (i == minuend.end())
        return false;

      minuend.erase(i);
    }

    return minuend.empty();
  }

  std::ostream& operator << (std::ostream& os, RepositoryInfo const& repo)
  {
    return os << "{'" << repo.TagsPath << "', '" << repo.Root << "', '" << static_cast<int>(repo.Type) << "'}";
  }

  namespace Tests
  {
    using R = std::vector<RepositoryInfo>;
    RepositoryInfo const RegularRepository = {"regular/repository/tags", "regular/repository", RepositoryType::Regular};
    RepositoryInfo const RegularSubRepository = {"regular/repository/sub/tags", "regular/repository/sub", RepositoryType::Regular};
    RepositoryInfo const TemporaryRepository = {"temporary/repository/tags", "temporary/repository", RepositoryType::Temporary};
    RepositoryInfo const PermanentRepository = {"permanent/repository/tags", "permanent/repository", RepositoryType::Permanent};
    R const AllRepositories{TemporaryRepository, RegularRepository, PermanentRepository, RegularSubRepository};

    class RepositoryStorage : public ::testing::Test
    {
    protected:
      void SetUp() override
      {
        SUT = Tags::RepositoryStorage::Create(&MockRepositoryFactory);
      }

      bool LoadRepository(RepositoryInfo const& info)
      {
        size_t symbolsLoaded = 0;
        return !SUT->Load(info.TagsPath.c_str(), info.Type, symbolsLoaded);
      }

      void LoadRepositories(std::vector<RepositoryInfo> const& repositories)
      {
        for (auto const& r : repositories)
          ASSERT_TRUE(LoadRepository(r)) << r;

        ASSERT_EQ(repositories, SUT->GetByType(RepositoryType::Any));
      }

      std::unique_ptr<Tags::RepositoryStorage> SUT;
    };

    TEST_F(RepositoryStorage, LoadsRegularRepository)
    {
      ASSERT_NO_FATAL_FAILURE(LoadRepositories({RegularRepository}));
    }

    TEST_F(RepositoryStorage, ReloadsRepository)
    {
      ASSERT_TRUE(LoadRepository(RegularRepository));
      ASSERT_EQ(R{RegularRepository}, SUT->GetByType(RepositoryType::Any));
      ASSERT_TRUE(LoadRepository(RegularRepository));
      ASSERT_EQ(R{RegularRepository}, SUT->GetByType(RepositoryType::Any));
    }

    TEST_F(RepositoryStorage, LoadsSameRepositoryWithDifferentType)
    {
      RepositoryInfo const sameRepository = {RegularRepository.TagsPath, RegularRepository.Root, RepositoryType::Temporary};
      ASSERT_TRUE(LoadRepository(RegularRepository));
      ASSERT_EQ(R{RegularRepository}, SUT->GetByType(RepositoryType::Any));
      ASSERT_TRUE(LoadRepository(sameRepository));
      ASSERT_EQ(R{RegularRepository}, SUT->GetByType(RepositoryType::Any));
    }

    TEST_F(RepositoryStorage, StoresRepositoriesOrdered)
    {
      ASSERT_TRUE(LoadRepository(RegularSubRepository));
      ASSERT_TRUE(LoadRepository(TemporaryRepository));
      ASSERT_TRUE(LoadRepository(RegularRepository));
      R ordered = {RegularSubRepository, TemporaryRepository, RegularRepository};
      std::sort(ordered.begin(), ordered.end(), [](RepositoryInfo const& l, RepositoryInfo const& r){ return l.Root < r.Root; });
      for (size_t i = 0; i < ordered.size(); ++i)
        ASSERT_EQ(ordered.at(i), SUT->GetByType(RepositoryType::Any).at(i));
    }

    TEST_F(RepositoryStorage, ReturnsRegularRepositories)
    {
      R const Regular{RegularRepository, RegularSubRepository};
      ASSERT_NO_FATAL_FAILURE(LoadRepositories(AllRepositories));
      ASSERT_EQ(Regular, SUT->GetByType(RepositoryType::Regular));
    }

    TEST_F(RepositoryStorage, ReturnsNonregularRepositories)
    {
      R const Nonregular{TemporaryRepository, PermanentRepository};
      ASSERT_NO_FATAL_FAILURE(LoadRepositories(AllRepositories));
      ASSERT_EQ(Nonregular, SUT->GetByType(~RepositoryType::Regular));
    }

    TEST_F(RepositoryStorage, ReturnsOwnersOfSubrepositoryFile)
    {
      std::string const SubrepositoryFile = RegularSubRepository.Root + "/file.cpp";
      R const Owners{RegularRepository, RegularSubRepository};
      ASSERT_NO_FATAL_FAILURE(LoadRepositories(AllRepositories));
      ASSERT_EQ(Owners, SUT->GetOwners(SubrepositoryFile.c_str()));
    }

    TEST_F(RepositoryStorage, RemovesRepository)
    {
      auto const toRemove = PermanentRepository;
      auto remained = AllRepositories;
      remained.erase(std::remove(remained.begin(), remained.end(), toRemove), remained.end());
      ASSERT_LT(remained.size(), AllRepositories.size());
      ASSERT_NO_FATAL_FAILURE(LoadRepositories(AllRepositories));
      SUT->Remove(toRemove.TagsPath.c_str());
      ASSERT_EQ(remained, SUT->GetByType(RepositoryType::Any));
    }

    TEST_F(RepositoryStorage, NotRemoveNotExistingRepository)
    {
      ASSERT_NO_FATAL_FAILURE(LoadRepositories(AllRepositories));
      SUT->Remove("Not/Existing/Repository");
      ASSERT_EQ(AllRepositories, SUT->GetByType(RepositoryType::Any));
    }

    TEST_F(RepositoryStorage, ReturnsInfoOfLoadedRepository)
    {
      ASSERT_NO_FATAL_FAILURE(LoadRepositories(AllRepositories));
      for (auto const& repo : AllRepositories)
        ASSERT_EQ(repo, SUT->GetInfo(repo.TagsPath.c_str()));
    }

    TEST_F(RepositoryStorage, ReturnsEmptyInfoOfNotExistingRepository)
    {
      ASSERT_NO_FATAL_FAILURE(LoadRepositories(AllRepositories));
      ASSERT_EQ(RepositoryInfo(), SUT->GetInfo("Not/Existing/Repository"));
    }
  }
}

int main(int argc, char* argv[])
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
