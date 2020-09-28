#include <gtest/gtest.h>
#include <tags_cache.h>

bool operator == (TagInfo const& left, TagInfo const& right)
{
  return left.name == right.name
      && left.file == right.file
      && left.re == right.re
      && left.lineno == right.lineno
      && left.type == right.type
      && left.info == right.info;
}

std::ostream& operator<<(std::ostream& os, TagInfo const& tag)
{
  return os << tag.name;
}

namespace Tags
{
namespace Internal
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

    using TAGS=std::vector<TagInfo>;
    using STAT=std::vector<std::pair<TagInfo, size_t>>;

    std::shared_ptr<TagsCache> MakeCache(std::vector<TagInfo>&& tags, size_t capacity = 0)
    {
      auto cache = CreateTagsCache(!!capacity ? capacity : tags.size());
      for (auto && v : tags)
        cache->Insert(std::move(v));

      return cache;
    }

    TEST(TagsCache, InsertsTag)
    {
      auto sut = MakeCache({FirstTag});
      ASSERT_EQ(TAGS({FirstTag}), sut->Get());
      ASSERT_EQ(STAT({{FirstTag, 1}}), sut->GetStat());
    }

    TEST(TagsCache, LastAddedTagsGoFirst)
    {
      auto sut = MakeCache({FirstTag, ThirdTag, SecondTag});
      ASSERT_EQ(TAGS({SecondTag, ThirdTag, FirstTag}), sut->Get());
      ASSERT_EQ(STAT({{SecondTag, 1}, {ThirdTag, 1}, {FirstTag, 1}}), sut->GetStat());
    }

    TEST(TagsCache, IncrementsFrequencyOfSameTags)
    {
      size_t const numberOfInsertions = 10;
      auto sut = MakeCache(std::vector<TagInfo>(numberOfInsertions, FirstTag));
      ASSERT_EQ(TAGS({FirstTag}), sut->Get());
      ASSERT_EQ(STAT({{FirstTag, numberOfInsertions}}), sut->GetStat());
    }

    TEST(TagsCache, IncrementsFrequencyOfSameTagsOnSpecifiedFrequency)
    {
      size_t const numberOfInsertions = 10;
      size_t const frequency = 5;
      auto sut = MakeCache(std::vector<TagInfo>(numberOfInsertions, FirstTag));
      sut->Insert(FirstTag, frequency);
      ASSERT_EQ(TAGS({FirstTag}), sut->Get());
      ASSERT_EQ(STAT({{FirstTag, numberOfInsertions + frequency}}), sut->GetStat());
    }

    TEST(TagsCache, MostFrequentTagsGoFirst)
    {
      auto sut = MakeCache({ThirdTag, SecondTag, ThirdTag, FirstTag, ThirdTag, SecondTag});
      ASSERT_EQ(TAGS({ThirdTag, SecondTag, FirstTag}), sut->Get());
      ASSERT_EQ(STAT({{ThirdTag, 3}, {SecondTag, 2}, {FirstTag, 1}}), sut->GetStat());
    }

    TEST(TagsCache, NewTagRemovesLeastFrequentTag)
    {
      auto sut = MakeCache({FirstTag, ThirdTag, FirstTag, SecondTag}, 2);
      ASSERT_EQ(TAGS({FirstTag, SecondTag}), sut->Get());
      ASSERT_EQ(STAT({{FirstTag, 2}, {SecondTag, 1}}), sut->GetStat());
    }

    TEST(TagsCache, ChangingCapacityNotRemovesTags)
    {
      size_t const totalTags = 3;
      size_t const smallerCapacity = 1;
      size_t const largerCapacity = totalTags * 2;
      auto sut = MakeCache({FirstTag, SecondTag, ThirdTag});
      ASSERT_EQ(TAGS({ThirdTag, SecondTag, FirstTag}), sut->Get());
      sut->SetCapacity(smallerCapacity);
      ASSERT_EQ(TAGS({ThirdTag}), sut->Get());
      sut->SetCapacity(largerCapacity);
      ASSERT_EQ(TAGS({ThirdTag, SecondTag, FirstTag}), sut->Get());
    }

    TEST(TagsCache, ReturnsStatisticsForTagsOutOfCapacity)
    {
      size_t const smallerCapacity = 2;
      auto sut = MakeCache({FourthTag, ThirdTag, SecondTag, FirstTag});
      sut->SetCapacity(smallerCapacity);
      ASSERT_EQ(TAGS({FirstTag, SecondTag}), sut->Get());
      ASSERT_EQ(STAT({{FirstTag, 1}, {SecondTag, 1}, {ThirdTag, 1}, {FourthTag, 1}}), sut->GetStat());
    }

    TEST(TagsCache, ReturnsSpecifiedNumberOfTags)
    {
      auto sut = MakeCache({FirstTag, SecondTag, ThirdTag, FourthTag});
      ASSERT_EQ(TAGS({FourthTag, ThirdTag}), sut->Get(2));
    }

    TEST(TagsCache, ReturnsTagsLimitedByCapacity)
    {
      size_t const smallerCapacity = 2;
      auto sut = MakeCache({FirstTag, SecondTag, ThirdTag, FourthTag});
      sut->SetCapacity(smallerCapacity);
      ASSERT_EQ(TAGS({FourthTag, ThirdTag}), sut->Get(smallerCapacity * 2));
    }

    TEST(TagsCache, ShrinkingToFitAndTagModificationNotRemovesTags)
    {
      size_t const totalTags = 3;
      size_t const largerCapacity = totalTags * 2;
      auto sut = MakeCache({FirstTag, FirstTag, FirstTag, FirstTag, SecondTag, SecondTag, ThirdTag}, largerCapacity);
      sut->SetCapacity(totalTags);
      sut->Insert(SecondTag);
      ASSERT_EQ(TAGS({FirstTag, SecondTag, ThirdTag}), sut->Get());
    }

    TEST(TagsCache, ModificationTagOutOfCapacityRemovesLastTagInCapacity)
    {
      size_t const totalTags = 3;
      size_t const largerCapacity = totalTags * 2;
      size_t const smallerCapacity = totalTags - 1;
      auto sut = MakeCache({FirstTag, FirstTag, FirstTag, FirstTag, SecondTag, SecondTag, SecondTag, ThirdTag}, largerCapacity);
      ASSERT_EQ(STAT({{FirstTag, 4}, {SecondTag, 3}, {ThirdTag, 1}}), sut->GetStat());
      sut->SetCapacity(smallerCapacity);
      sut->Insert(ThirdTag);
      ASSERT_EQ(STAT({{FirstTag, 4}, {ThirdTag, 2}}), sut->GetStat());
    }

    TEST(TagsCache, InsertingNewTagRemovesAllTagsOutOfCapacity)
    {
      size_t const totalTags = 3;
      size_t const largerCapacity = totalTags * 2;
      size_t const smallerCapacity = 2;
      auto sut = MakeCache({FirstTag, FirstTag, FirstTag, FirstTag, SecondTag, SecondTag, SecondTag, ThirdTag, ThirdTag}, largerCapacity);
      sut->SetCapacity(smallerCapacity);
      sut->Insert(FourthTag);
      ASSERT_EQ(TAGS({FirstTag, FourthTag}), sut->Get());
      ASSERT_EQ(STAT({{FirstTag, 4}, {FourthTag, 1}}), sut->GetStat());
      sut->Insert(SecondTag);
      ASSERT_EQ(TAGS({FirstTag, SecondTag}), sut->Get());
      ASSERT_EQ(STAT({{FirstTag, 4}, {SecondTag, 1}}), sut->GetStat());
    }

    TEST(TagsCache, ReturnsTagsInReversedOrderTagsShouldBeInserted)
    {
      auto cache = MakeCache({FirstTag, SecondTag, ThirdTag});
      auto tags = cache->Get();
      auto sut = MakeCache(std::vector<TagInfo>(tags.rbegin(), tags.rend()));
      ASSERT_EQ(cache->Get(), sut->Get());
    }

    TEST(TagsCache, ReturnsTagsStatisticsInReversedOrderTagsShouldBeInserted)
    {
      auto cache = MakeCache({FirstTag, FirstTag, SecondTag, SecondTag, SecondTag, ThirdTag, ThirdTag});
      auto stat = cache->GetStat();
      auto sut = CreateTagsCache(stat.size());
      for (auto i = stat.rbegin(); i != stat.rend(); ++i)
        sut->Insert(i->first, i->second);
      
      ASSERT_EQ(cache->GetStat(), sut->GetStat());
    }

    TEST(TagsCache, ErasesTag)
    {
      auto sut = MakeCache({FirstTag});
      ASSERT_NO_THROW(sut->Erase(FirstTag));
      ASSERT_EQ(TAGS(), sut->Get());
      ASSERT_EQ(STAT(), sut->GetStat());
    }

    TEST(TagsCache, ErasesMiddleTag)
    {
      auto sut = MakeCache({FirstTag, FirstTag, FirstTag, SecondTag, SecondTag, ThirdTag});
      ASSERT_NO_THROW(sut->Erase(SecondTag));
      ASSERT_EQ(STAT({{FirstTag, 3}, {ThirdTag, 1}}), sut->GetStat());
    }

    TEST(TagsCache, NotChangesEmptyCacheWhileErasingTag)
    {
      auto sut = CreateTagsCache(0);
      ASSERT_NO_THROW(sut->Erase(FirstTag));
      ASSERT_EQ(TAGS(), sut->Get());
      ASSERT_EQ(STAT(), sut->GetStat());
    }

    TEST(TagsCache, NotChangesCacheWhileErasingNotFoundTag)
    {
      auto sut = MakeCache({SecondTag, FirstTag});
      ASSERT_NO_THROW(sut->Erase(ThirdTag));
      ASSERT_EQ(TAGS({FirstTag, SecondTag}), sut->Get());
      ASSERT_EQ(STAT({{FirstTag, 1}, {SecondTag, 1}}), sut->GetStat());
    }

    TEST(TagsCache, ResetsCounters)
    {
      auto sut = MakeCache({FirstTag, FirstTag, FirstTag, SecondTag, SecondTag, ThirdTag});
      sut->ResetCounters();
      ASSERT_EQ(STAT({{FirstTag, 1}, {SecondTag, 1}, {ThirdTag, 1}}), sut->GetStat());
    }

    TEST(TagsCache, OrderNotChangedIfCounterReset)
    {
      auto sut = MakeCache({FirstTag, FirstTag, FirstTag, SecondTag, SecondTag, ThirdTag});
      auto expected = sut->Get();
      sut->ResetCounters();
      ASSERT_EQ(expected, sut->Get());
    }
  }
}
}

int main(int argc, char* argv[])
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
