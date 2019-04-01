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

    TagInfo TagAt(size_t index, std::shared_ptr<TagsCache> const& cache)
    {
      return cache->Get().at(index);
    }

    size_t FreqAt(size_t index, std::shared_ptr<TagsCache> const& cache)
    {
      return cache->GetStat().at(index).second;
    }

    size_t Size(std::shared_ptr<TagsCache> const& cache)
    {
      return cache->Get().size();
    }

    TEST(TagsCache, InsertsTag)
    {
      auto sut = MakeCache({FirstTag});
      ASSERT_EQ(1, Size(sut));
      ASSERT_TRUE(Equal(FirstTag, TagAt(0, sut)));
      ASSERT_EQ(1, FreqAt(0, sut));
    }

    TEST(TagsCache, LastAddedTagsGoFirst)
    {
      size_t const totalTags = 3;
      auto sut = MakeCache({FirstTag, ThirdTag, SecondTag});
      ASSERT_EQ(totalTags, Size(sut));
      ASSERT_TRUE(Equal(SecondTag, TagAt(0, sut)));
      ASSERT_TRUE(Equal(ThirdTag, TagAt(1, sut)));
      ASSERT_TRUE(Equal(FirstTag, TagAt(2, sut)));
      ASSERT_EQ(1, FreqAt(0, sut));
      ASSERT_EQ(1, FreqAt(1, sut));
      ASSERT_EQ(1, FreqAt(2, sut));
    }

    TEST(TagsCache, IncrementsFrequencyOfSameTags)
    {
      size_t const totalTags = 1;
      size_t const numberOfInsertions = 10;
      auto sut = MakeCache(std::vector<TagInfo>(numberOfInsertions, FirstTag), totalTags);
      ASSERT_EQ(totalTags, Size(sut));
      ASSERT_EQ(numberOfInsertions, FreqAt(0, sut));
    }

    TEST(TagsCache, IncrementsFrequencyOfSameTagsOnSpecifiedFrequency)
    {
      size_t const totalTags = 1;
      size_t const numberOfInsertions = 10;
      size_t const frequency = 5;
      auto sut = MakeCache(std::vector<TagInfo>(numberOfInsertions, FirstTag), 1);
      sut->Insert(FirstTag, frequency);
      ASSERT_EQ(totalTags, Size(sut));
      ASSERT_EQ(numberOfInsertions + frequency, FreqAt(0, sut));
    }

    TEST(TagsCache, MostFrequentTagsGoFirst)
    {
      size_t const totalTags = 3;
      auto sut = MakeCache({ThirdTag, SecondTag, ThirdTag, FirstTag, ThirdTag, SecondTag});
      ASSERT_EQ(totalTags, Size(sut));
      ASSERT_TRUE(Equal(ThirdTag, TagAt(0, sut)));
      ASSERT_TRUE(Equal(SecondTag, TagAt(1, sut)));
      ASSERT_TRUE(Equal(FirstTag, TagAt(2, sut)));
      ASSERT_EQ(3, FreqAt(0, sut));
      ASSERT_EQ(2, FreqAt(1, sut));
      ASSERT_EQ(1, FreqAt(2, sut));
    }

    TEST(TagsCache, NewTagRemovesLeastFrequentTag)
    {
      size_t const totalTags = 2;
      auto sut = MakeCache({FirstTag, ThirdTag, FirstTag, SecondTag}, totalTags);
      ASSERT_EQ(totalTags, Size(sut));
      ASSERT_TRUE(Equal(FirstTag, TagAt(0, sut)));
      ASSERT_TRUE(Equal(SecondTag, TagAt(1, sut)));
      ASSERT_EQ(2, FreqAt(0, sut));
      ASSERT_EQ(1, FreqAt(1, sut));
    }

    TEST(TagsCache, ChangingCapacityNotRemovesTags)
    {
      size_t const totalTags = 3;
      size_t const smallerCapacity = 1;
      size_t const largerCapacity = totalTags * 2;
      auto sut = MakeCache({FirstTag, SecondTag, ThirdTag});
      ASSERT_EQ(totalTags, Size(sut));
      sut->SetCapacity(smallerCapacity);
      ASSERT_EQ(smallerCapacity, Size(sut));
      ASSERT_TRUE(Equal(ThirdTag, TagAt(0, sut)));
      sut->SetCapacity(largerCapacity);
      ASSERT_EQ(totalTags, Size(sut));
      ASSERT_TRUE(Equal(ThirdTag, TagAt(0, sut)));
      ASSERT_TRUE(Equal(SecondTag, TagAt(1, sut)));
      ASSERT_TRUE(Equal(FirstTag, TagAt(2, sut)));
    }

    TEST(TagsCache, ReturnsStatisticsForTagsOutOfCapacity)
    {
      size_t const totalTags = 4;
      size_t const smallerCapacity = 2;
      auto sut = MakeCache({FourthTag, ThirdTag, SecondTag, FirstTag});
      sut->SetCapacity(smallerCapacity);
      ASSERT_EQ(smallerCapacity, Size(sut));
      auto const stat = sut->GetStat();
      ASSERT_EQ(totalTags, stat.size());
      ASSERT_TRUE(Equal(TagAt(0, sut), stat[0].first));
      ASSERT_TRUE(Equal(TagAt(1, sut), stat[1].first));
      ASSERT_TRUE(Equal(ThirdTag, stat[2].first));
      ASSERT_TRUE(Equal(FourthTag, stat[3].first));
    }

    TEST(TagsCache, ReturnsSpecifiedNumberOfTags)
    {
      size_t const totalTags = 4;
      size_t const limit = totalTags / 2;
      auto sut = MakeCache({FirstTag, SecondTag, ThirdTag, FourthTag});
      ASSERT_EQ(limit, sut->Get(limit).size());
    }

    TEST(TagsCache, ReturnsTagsLimitedByCapacity)
    {
      size_t const totalTags = 4;
      size_t const smallerCapacity = totalTags / 2;
      auto sut = MakeCache({FirstTag, SecondTag, ThirdTag, FourthTag});
      sut->SetCapacity(smallerCapacity);
      ASSERT_EQ(smallerCapacity, sut->Get(totalTags).size());
    }

    TEST(TagsCache, ShrinkingToFitAndTagModificationNotRemovesTags)
    {
      size_t const totalTags = 3;
      size_t const largerCapacity = totalTags * 2;
      auto sut = MakeCache({FirstTag, FirstTag, FirstTag, FirstTag, SecondTag, SecondTag, ThirdTag}, largerCapacity);
      sut->SetCapacity(totalTags);
      sut->Insert(SecondTag);
      ASSERT_EQ(totalTags, Size(sut));
      ASSERT_TRUE(Equal(FirstTag, TagAt(0, sut)));
      ASSERT_TRUE(Equal(SecondTag, TagAt(1, sut)));
      ASSERT_TRUE(Equal(ThirdTag, TagAt(2, sut)));
    }

    TEST(TagsCache, ModificationTagOutOfCapacityRemovesLastTagInCapacity)
    {
      size_t const totalTags = 3;
      size_t const largerCapacity = totalTags * 2;
      size_t const smallerCapacity = totalTags - 1;
      auto sut = MakeCache({FirstTag, FirstTag, FirstTag, FirstTag, SecondTag, SecondTag, SecondTag, ThirdTag}, largerCapacity);
      ASSERT_EQ(3, FreqAt(1, sut));
      sut->SetCapacity(smallerCapacity);
      sut->Insert(ThirdTag);
      ASSERT_EQ(smallerCapacity, Size(sut));
      ASSERT_TRUE(Equal(FirstTag, TagAt(0, sut)));
      ASSERT_TRUE(Equal(ThirdTag, TagAt(1, sut)));
      ASSERT_EQ(2, FreqAt(1, sut));
    }

    TEST(TagsCache, InsertingNewTagRemovesAllTagsOutOfCapacity)
    {
      size_t const totalTags = 3;
      size_t const largerCapacity = totalTags * 2;
      size_t const smallerCapacity = 2;
      auto sut = MakeCache({FirstTag, FirstTag, FirstTag, FirstTag, SecondTag, SecondTag, SecondTag, ThirdTag, ThirdTag}, largerCapacity);
      sut->SetCapacity(smallerCapacity);
      sut->Insert(FourthTag);
      ASSERT_EQ(smallerCapacity, Size(sut));
      ASSERT_TRUE(Equal(FirstTag, TagAt(0, sut)));
      ASSERT_TRUE(Equal(FourthTag, TagAt(1, sut)));
      sut->Insert(SecondTag);
      ASSERT_EQ(smallerCapacity, Size(sut));
      ASSERT_TRUE(Equal(FirstTag, TagAt(0, sut)));
      ASSERT_TRUE(Equal(SecondTag, TagAt(1, sut)));
      ASSERT_EQ(1, FreqAt(1, sut));
    }

    TEST(TagsCache, ReturnsTagsInReversedOrderTagsShouldBeInserted)
    {
      auto cache = MakeCache({FirstTag, SecondTag, ThirdTag});
      auto tags = cache->Get();
      auto sut = MakeCache(std::vector<TagInfo>(tags.rbegin(), tags.rend()));
      ASSERT_TRUE(Equal(TagAt(0, cache), TagAt(0, sut)));
      ASSERT_TRUE(Equal(TagAt(1, cache), TagAt(1, sut)));
      ASSERT_TRUE(Equal(TagAt(2, cache), TagAt(2, sut)));
    }

    TEST(TagsCache, ReturnsTagsStatisticsInReversedOrderTagsShouldBeInserted)
    {
      auto cache = MakeCache({FirstTag, FirstTag, SecondTag, SecondTag, SecondTag, ThirdTag, ThirdTag});
      auto stat = cache->GetStat();
      auto sut = CreateTagsCache(stat.size());
      for (auto i = stat.rbegin(); i != stat.rend(); ++i)
        sut->Insert(i->first, i->second);
      
      ASSERT_TRUE(Equal(TagAt(0, cache), TagAt(0, sut)));
      ASSERT_TRUE(Equal(TagAt(1, cache), TagAt(1, sut)));
      ASSERT_TRUE(Equal(TagAt(2, cache), TagAt(2, sut)));
      ASSERT_EQ(FreqAt(0, cache), FreqAt(0, sut));
      ASSERT_EQ(FreqAt(1, cache), FreqAt(1, sut));
      ASSERT_EQ(FreqAt(2, cache), FreqAt(2, sut));
    }

    TEST(TagsCache, ErasesTag)
    {
      auto sut = MakeCache({FirstTag});
      ASSERT_NO_THROW(sut->Erase(FirstTag));
      ASSERT_EQ(0, Size(sut));
      ASSERT_EQ(0, sut->GetStat().size());
    }

    TEST(TagsCache, ErasesMiddleTag)
    {
      auto sut = MakeCache({FirstTag, FirstTag, FirstTag, SecondTag, SecondTag, ThirdTag});
      auto const totalTags = Size(sut);
      ASSERT_NO_THROW(sut->Erase(SecondTag));
      ASSERT_EQ(totalTags - 1, Size(sut));
      ASSERT_TRUE(Equal(FirstTag, TagAt(0, sut)));
      ASSERT_TRUE(Equal(ThirdTag, TagAt(1, sut)));
      ASSERT_EQ(3, FreqAt(0, sut));
      ASSERT_EQ(1, FreqAt(1, sut));
    }

    TEST(TagsCache, NotChangesEmptyCacheWhileErasingTag)
    {
      auto sut = CreateTagsCache(0);
      ASSERT_NO_THROW(sut->Erase(FirstTag));
      ASSERT_EQ(0, Size(sut));
    }

    TEST(TagsCache, NotChangesCacheWhileErasingNotFoundTag)
    {
      auto sut = MakeCache({SecondTag, FirstTag});
      auto const totalTags = Size(sut);
      ASSERT_NO_THROW(sut->Erase(ThirdTag));
      ASSERT_EQ(totalTags, Size(sut));
      ASSERT_TRUE(Equal(FirstTag, TagAt(0, sut)));
      ASSERT_TRUE(Equal(SecondTag, TagAt(1, sut)));
    }
  }
}

int main(int argc, char* argv[])
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
