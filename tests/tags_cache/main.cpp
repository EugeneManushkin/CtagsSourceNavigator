#include <gtest/gtest.h>
#include <tags_cache.h>

namespace TagsInternal
{
  namespace Tests
  {
    TagInfo MakeTag(std::string const& name)
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

    auto const FirstTag = MakeTag("Name1");
    auto const SecondTag = MakeTag("Name2");
    auto const ThirdTag = MakeTag("Name3");
    auto const FourthTag = MakeTag("Name4");

    bool Equal(TagInfo const& left, TagInfo const& right)
    {
      return left.name == right.name
          && left.file == right.file
          && left.re == right.re
          && left.lineno == right.lineno
          && left.type == right.type
          && left.info == right.info;
    }

    std::shared_ptr<TagsCache> MakeCache(std::vector<TagInfo>&& tags, size_t capacity = 0)
    {
      auto cache = CreateTagsCache(!!capacity ? capacity : tags.size());
      for (auto && v : tags)
        cache->Insert(std::move(v));

      return cache;
    }

    TEST(TagsCache, InsertsTag)
    {
      auto const tags = MakeCache({FirstTag})->Get();
      ASSERT_EQ(1, tags.size());
      ASSERT_TRUE(Equal(FirstTag, tags.back().first));
      ASSERT_EQ(1, tags.back().second);
    }

    TEST(TagsCache, LastAddedTagsGoFirst)
    {
      size_t const totalTags = 3;
      auto const tags = MakeCache({FirstTag, ThirdTag, SecondTag})->Get();
      ASSERT_EQ(totalTags, tags.size());
      ASSERT_TRUE(Equal(SecondTag, tags[0].first));
      ASSERT_TRUE(Equal(ThirdTag, tags[1].first));
      ASSERT_TRUE(Equal(FirstTag, tags[2].first));
      ASSERT_EQ(1, tags[0].second);
      ASSERT_EQ(1, tags[1].second);
      ASSERT_EQ(1, tags[2].second);
    }

    TEST(TagsCache, IncrementsFrequencyOfSameTags)
    {
      size_t const totalTags = 1;
      size_t const numberOfInsertions = 10;
      auto const tags = MakeCache(std::vector<TagInfo>(numberOfInsertions, FirstTag), totalTags)->Get();
      ASSERT_EQ(totalTags, tags.size());
      ASSERT_EQ(numberOfInsertions, tags.back().second);
    }

    TEST(TagsCache, IncrementsFrequencyOfSameTagsOnSpecifiedFrequency)
    {
      size_t const totalTags = 1;
      size_t const numberOfInsertions = 10;
      size_t const frequency = 5;
      auto sut = MakeCache(std::vector<TagInfo>(numberOfInsertions, FirstTag), 1);
      sut->Insert(FirstTag, frequency);     
      auto const tags = sut->Get();
      ASSERT_EQ(totalTags, tags.size());
      ASSERT_EQ(numberOfInsertions + frequency, tags.back().second);
    }

    TEST(TagsCache, MostFrequentTagsGoFirst)
    {
      size_t const totalTags = 3;
      auto const tags = MakeCache({ThirdTag, SecondTag, ThirdTag, FirstTag, ThirdTag, SecondTag})->Get();
      ASSERT_EQ(totalTags, tags.size());
      ASSERT_TRUE(Equal(ThirdTag, tags[0].first));
      ASSERT_TRUE(Equal(SecondTag, tags[1].first));
      ASSERT_TRUE(Equal(FirstTag, tags[2].first));
      ASSERT_EQ(3, tags[0].second);
      ASSERT_EQ(2, tags[1].second);
      ASSERT_EQ(1, tags[2].second);
    }

    TEST(TagsCache, NewTagRemovesLeastFrequentTag)
    {
      size_t const totalTags = 2;
      auto const tags = MakeCache({FirstTag, ThirdTag, FirstTag, SecondTag}, totalTags)->Get();
      ASSERT_EQ(totalTags, tags.size());
      ASSERT_TRUE(Equal(FirstTag, tags[0].first));
      ASSERT_TRUE(Equal(SecondTag, tags[1].first));
      ASSERT_EQ(2, tags[0].second);
      ASSERT_EQ(1, tags[1].second);
    }

    TEST(TagsCache, ChangingCapacityNotRemovesTags)
    {
      size_t const totalTags = 3;
      size_t const smallerCapacity = 1;
      size_t const largerCapacity = totalTags * 2;
      auto sut = MakeCache({FirstTag, SecondTag, ThirdTag});
      ASSERT_EQ(totalTags, sut->Get().size());
      sut->SetCapacity(smallerCapacity);
      ASSERT_EQ(smallerCapacity, sut->Get().size());
      ASSERT_TRUE(Equal(ThirdTag, sut->Get().back().first));
      sut->SetCapacity(largerCapacity);
      auto const tags = sut->Get();
      ASSERT_EQ(totalTags, tags.size());
      ASSERT_TRUE(Equal(ThirdTag, tags[0].first));
      ASSERT_TRUE(Equal(SecondTag, tags[1].first));
      ASSERT_TRUE(Equal(FirstTag, tags[2].first));
    }

    TEST(TagsCache, ShrinkingToFitAndTagModificationNotRemovesTags)
    {
      size_t const totalTags = 3;
      size_t const largerCapacity = totalTags * 2;
      auto sut = MakeCache({FirstTag, FirstTag, FirstTag, FirstTag, SecondTag, SecondTag, ThirdTag}, largerCapacity);
      sut->SetCapacity(totalTags);
      sut->Insert(SecondTag);
      auto const tags = sut->Get();
      ASSERT_EQ(totalTags, tags.size());
      ASSERT_TRUE(Equal(FirstTag, tags[0].first));
      ASSERT_TRUE(Equal(SecondTag, tags[1].first));
      ASSERT_TRUE(Equal(ThirdTag, tags[2].first));      
    }

    TEST(TagsCache, ModificationTagOutOfCapacityRemovesLastTagInCapacity)
    {
      size_t const totalTags = 3;
      size_t const largerCapacity = totalTags * 2;
      size_t const smallerCapacity = totalTags - 1;
      auto sut = MakeCache({FirstTag, FirstTag, FirstTag, FirstTag, SecondTag, SecondTag, SecondTag, ThirdTag}, largerCapacity);
      ASSERT_EQ(3, sut->Get()[1].second);
      sut->SetCapacity(smallerCapacity);
      sut->Insert(ThirdTag);
      auto const tags = sut->Get();
      ASSERT_EQ(smallerCapacity, tags.size());
      ASSERT_TRUE(Equal(FirstTag, tags[0].first));
      ASSERT_TRUE(Equal(ThirdTag, tags[1].first));
      ASSERT_EQ(2, tags[1].second);
    }

    TEST(TagsCache, InsertingNewTagRemovesAllTagsOutOfCapacity)
    {
      size_t const totalTags = 3;
      size_t const largerCapacity = totalTags * 2;
      size_t const smallerCapacity = 2;
      auto sut = MakeCache({FirstTag, FirstTag, FirstTag, FirstTag, SecondTag, SecondTag, SecondTag, ThirdTag, ThirdTag}, largerCapacity);
      sut->SetCapacity(smallerCapacity);
      sut->Insert(FourthTag);
      ASSERT_EQ(smallerCapacity, sut->Get().size());
      ASSERT_TRUE(Equal(FirstTag, sut->Get()[0].first));
      ASSERT_TRUE(Equal(FourthTag, sut->Get()[1].first));
      sut->Insert(SecondTag);
      ASSERT_EQ(smallerCapacity, sut->Get().size());
      ASSERT_TRUE(Equal(FirstTag, sut->Get()[0].first));
      ASSERT_TRUE(Equal(SecondTag, sut->Get()[1].first));
      ASSERT_EQ(1, sut->Get()[1].second);
    }
  }
}

int main(int argc, char* argv[])
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
