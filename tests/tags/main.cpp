#include <gtest/gtest.h>
#include <tags.h>

#include <fstream>
#include <memory>
#include <string>
#include <vector>

Config config;

int isident(int chr)
{
  return config.isident(chr);
}

namespace
{
  using NameList = std::vector<std::string>;
  using TagArrayPtr = std::unique_ptr<TagArray>;

  //TODO: rework: each name must have identifier (regex?)
  NameList LoadNames(std::string const& fileName)
  {
    NameList names;
    std::ifstream file;
    file.exceptions(std::ifstream::badbit);
    file.open(fileName);
    std::shared_ptr<void> fileCloser(0, [&](void*) { file.close(); });
    std::string buf;
    while (std::getline(file, buf))
    {
      if (!buf.empty())
        names.push_back(buf);
    }

    return names;
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

    //TODO: rework
    void LookupName(std::string const& name, std::string const& tags)
    {
      TagArrayPtr tagArray(Find(name.c_str(), tags.c_str()));
      ASSERT_TRUE(!!tagArray) << "Searched name: '" << name << "', tags file: " << tags;
      for (size_t i = 0; i < tagArray->Count(); ++i)
        ASSERT_EQ(name, std::string((*tagArray)[i]->name.Str()));
    }

    void LoadAndLookupNames(std::string const& tagsFile, std::string const& namesFile)
    {
      auto names = LoadNames(namesFile);
      ASSERT_FALSE(names.empty());
      size_t symbolsLoaded = -1;
      ASSERT_EQ(LoadSuccess, Load(tagsFile.c_str(), symbolsLoaded));
      ASSERT_EQ(names.size(), symbolsLoaded);
      for (auto const& name : names)
        LookupName(name, tagsFile);
    }
  };

  TEST_F(Tags, LoadsEmptyRepos)
  {
    char const emptyReposTags[] = "empty_repos\\tags";
    size_t symbolsLoaded = -1;
    ASSERT_EQ(LoadSuccess, Load(emptyReposTags, symbolsLoaded));
    ASSERT_EQ(0, symbolsLoaded);
  }

  TEST_F(Tags, AllNamesFoundInUniversalSimpleFileRepos)
  {
    LoadAndLookupNames("single_file_repos\\tags.universal", "single_file_repos\\tags.universal.names");
  }

  TEST_F(Tags, AllNamesFoundInExuberantWinSimpleFileRepos)
  {
    LoadAndLookupNames("single_file_repos\\tags.exuberant.w", "single_file_repos\\tags.exuberant.w.names");
  }
}

int main(int argc, char* argv[])
{
  testing::InitGoogleTest(&argc, argv);
  RUN_ALL_TESTS();  
}
