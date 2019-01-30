#include <gtest/gtest.h>
#include <tags_cache.h>

namespace TagsInternal
{
  namespace Tests
  {
    TagInfo GetTag(std::string const& name)
    {
      TagInfo tag;
      tag.name = name;
      tag.file = "C:\\Folder\\File.cpp";
      tag.re = "^class TestClass$";
      tag.lineno = 10;
      tag.type = 'c';
      tag.info = "struct:Test::TestClass";
      return tag;
    }

    auto const FirstTag = GetTag("Name1");
    auto const SecondTag = GetTag("Name2");
    auto const ThirdTag = GetTag("Name3");

    bool Equal(TagInfo const& left, TagInfo const& right)
    {
      return left.name == right.name
          && left.file == right.file
          && left.re == right.re
          && left.lineno == right.lineno
          && left.type == right.type
          && left.info == right.info;
    }

    TEST(TagsCache, InsertsTag)
    {
      auto sut = CreateTagsCache(1);
      sut->Insert(FirstTag);
      auto const tags = sut->Get();
      ASSERT_EQ(1, tags.size());
      ASSERT_TRUE(Equal(FirstTag, tags.back().first));
      ASSERT_EQ(1, tags.back().second);
    }

    TEST(TagsCache, InsertsThreeTags)
    {
      size_t const totalTags = 3;
      auto sut = CreateTagsCache(totalTags);
      sut->Insert(FirstTag);
      sut->Insert(ThirdTag);
      sut->Insert(SecondTag);
      auto const tags = sut->Get();
      ASSERT_EQ(totalTags, tags.size());
      ASSERT_TRUE(Equal(FirstTag, tags[0].first));
      ASSERT_TRUE(Equal(ThirdTag, tags[1].first));
      ASSERT_TRUE(Equal(SecondTag, tags[2].first));
      ASSERT_EQ(1, tags[0].second);
      ASSERT_EQ(1, tags[1].second);
      ASSERT_EQ(1, tags[2].second);
    }

    TEST(TagsCache, IncrementsFrequencyOfSameTags)
    {
      auto sut = CreateTagsCache(1);
      size_t const numberOfInsertions = 10;
      for (auto counter = numberOfInsertions; counter; --counter)
        sut->Insert(FirstTag);

      auto const tags = sut->Get();
      ASSERT_EQ(1, tags.size());
      ASSERT_EQ(numberOfInsertions, tags.back().second);
    }

    //TODO: more tests
  }
}

int main(int argc, char* argv[])
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
