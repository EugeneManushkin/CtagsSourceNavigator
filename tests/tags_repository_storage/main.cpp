#include <gtest/gtest.h>
#include <tags_repository.h>
#include <tags_repository_storage.h>

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
    MockRepository(char const* tagsPath, Tags::RepositoryType)
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

  std::unique_ptr<Tags::Internal::Repository> MockRepositoryFactory(char const* tagsPath, Tags::RepositoryType type)
  {
    return std::unique_ptr<Tags::Internal::Repository>(new MockRepository(tagsPath, type));
  }

  int LoadRepository(std::unique_ptr<Tags::RepositoryStorage> const& storage, Tags::RepositoryInfo const& info)
  {
    size_t symbolsLoaded = 0;
    return storage->Load(info.TagsPath.c_str(), info.Type, symbolsLoaded);
  }
}

namespace Tags
{
  bool operator == (RepositoryInfo const& left, RepositoryInfo const& right)
  {
    return left.TagsPath == right.TagsPath
        && left.Type == right.Type
    ;
  }

  namespace Tests
  {
    RepositoryInfo const RegularRepository = {"regular/repository/tags", "regular/repository", RepositoryType::Regular};

    TEST(RepositoryStorage, LoadsRegularRepository)
    {
      auto sut = RepositoryStorage::Create(&MockRepositoryFactory);
      LoadRepository(sut, RegularRepository);
      ASSERT_EQ(1, sut->GetByType(RepositoryType::Any).size());
      ASSERT_EQ(RegularRepository, sut->GetByType(RepositoryType::Any).back());
    }
  }
}

int main(int argc, char* argv[])
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
