#include <gtest/gtest.h>

#include <plugin/config_data_mapper.h>

namespace Plugin
{
  namespace Tests
  {
    std::string TestValue(ConfigFieldType type)
    {
      return type == ConfigFieldType::Size ? "97"
           : type == ConfigFieldType::Flag ? "true"
           : "not empty string";
    }

    int TestBoolId()
    {
      return static_cast<int>(ConfigFieldId::use_built_in_ctags);
    }

    bool& TestBoolField(Config& config)
    {
      return config.use_built_in_ctags;
    }

    int TestThreeStateId()
    {
      return static_cast<int>(ConfigFieldId::platform_language_lookup);
    }

    ThreeStateFlag TestThreeStateField(Config& config)
    {
      return config.platform_language_lookup;
    }

    int TestNotEmptyStrId()
    {
      return static_cast<int>(ConfigFieldId::exe);
    }

    std::string TestNotEmptyStrField(Config& config)
    {
      return config.exe;
    }

    TEST(ConfigDataMapper, ReadAllFields)
    {
      auto SUT = ConfigDataMapper::Create();
      Config config;
      for (int i = 0; i < static_cast<int>(ConfigFieldId::MaxFieldId); ++i)
        ASSERT_NO_THROW(SUT->Get(i, config)) << "ConfigFieldId = " << i;
    }

    TEST(ConfigDataMapper, GetAndSetFieldsById)
    {
      auto SUT = ConfigDataMapper::Create();
      Config config;
      for (int i = 0; i < static_cast<int>(ConfigFieldId::MaxFieldId); ++i)
      {
        auto field = SUT->Get(i, config);
        ASSERT_TRUE(SUT->Set(i, TestValue(field.type), config)) << "ConfigFieldId = " << i;
      }

      for (int i = 0; i < static_cast<int>(ConfigFieldId::MaxFieldId); ++i)
      {
        auto field = SUT->Get(i, config);
        ASSERT_EQ(field.value, TestValue(field.type)) << "ConfigFieldId = " << i;
      }
    }

    TEST(ConfigDataMapper, GetAndSetFieldsByKey)
    {
      auto SUT = ConfigDataMapper::Create();
      Config config;
      for (int i = 0; i < static_cast<int>(ConfigFieldId::MaxFieldId); ++i)
      {
        auto field = SUT->Get(i, config);
        ASSERT_TRUE(SUT->Set(field.key, TestValue(field.type), config)) << "ConfigFieldId = " << i << ", key = " << field.key;
      }

      for (int i = 0; i < static_cast<int>(ConfigFieldId::MaxFieldId); ++i)
      {
        auto field = SUT->Get(i, config);
        ASSERT_EQ(field.value, TestValue(field.type)) << "ConfigFieldId = " << i;
      }
    }

    TEST(ConfigDataMapper, SetAllNotDefaultValuesByKey)
    {
      Config const defaults;
      std::vector<std::pair<std::string, std::string>> const keyValues = {
        {"pathtoexe", defaults.exe + "?exe"},
        {"usebuiltinctags", !defaults.use_built_in_ctags ? "true" : "false"},
        {"commandline", defaults.opt + "?opt"},
        {"maxresults", std::to_string(defaults.max_results + 1)},
        {"threshold", std::to_string(defaults.threshold + 1)},
        {"thresholdfilterlen", std::to_string(defaults.threshold_filter_len + 1)},
        {"platformlanguagelookup", (defaults.platform_language_lookup != Plugin::ThreeStateFlag::Enabled ? "true" : "false")},
        {"resetcachecounterstimeouthours", std::to_string(defaults.reset_cache_counters_timeout_hours + 1)},
        {"casesensfilt", !defaults.casesens ? "true" : "false"},
        {"sortclassmembersbyname", !defaults.sort_class_members_by_name ? "true" : "false"},
        {"curfilefirst", !defaults.cur_file_first ? "true" : "false"},
        {"cachedtagsontop", !defaults.cached_tags_on_top ? "true" : "false"},
        {"indexeditedfile", !defaults.index_edited_file ? "true" : "false"},
        {"wordchars", defaults.wordchars + "?wordchars"},
        {"tagsmask", defaults.tagsmask + "?tagsmask"},
        {"historyfile", defaults.history_file + "?history_file"},
        {"historylen", std::to_string(defaults.history_len + 1)},
        {"autoload", defaults.permanents + "?permanents"},
        {"restorelastvisitedonload", !defaults.restore_last_visited_on_load ? "true" : "false"},
      };

      auto SUT = ConfigDataMapper::Create();
      Config config;
      for (auto const& kv : keyValues)
      {
        ASSERT_TRUE(SUT->Set(kv.first, kv.second, config)) << "key: " << kv.first << ", value: " << kv.second;
      }

      for (int i = 0; i < static_cast<int>(ConfigFieldId::MaxFieldId); ++i)
      {
        auto field = SUT->Get(i, config);
        auto default = SUT->Get(i, defaults);
        ASSERT_NE(field.value, default.value) << "ConfigFieldId = " << i << ", key = " << field.key;
      }
    }


    TEST(ConfigDataMapper, ThrowIfInvalidId)
    {
      auto SUT = ConfigDataMapper::Create();
      Config config;
      ASSERT_ANY_THROW(SUT->Get(-1, config));
      ASSERT_ANY_THROW(SUT->Get(static_cast<int>(ConfigFieldId::MaxFieldId), config));
      ASSERT_ANY_THROW(SUT->Get(static_cast<int>(ConfigFieldId::MaxFieldId) + 1, config));
    }

    TEST(ConfigDataMapper, FailSetValueForInvalidId)
    {
      auto SUT = ConfigDataMapper::Create();
      Config config;
      ASSERT_FALSE(SUT->Set(-1, "123", config));
      ASSERT_FALSE(SUT->Set(static_cast<int>(ConfigFieldId::MaxFieldId), "123", config));
      ASSERT_FALSE(SUT->Set(static_cast<int>(ConfigFieldId::MaxFieldId) + 1, "123", config));
    }

    TEST(ConfigDataMapper, FailSetValueForInvalidKey)
    {
      auto SUT = ConfigDataMapper::Create();
      Config config;
      ASSERT_FALSE(SUT->Set("#not-existing key&", "123", config));
    }

    TEST(ConfigDataMapper, SetFlagField)
    {
      auto SUT = ConfigDataMapper::Create();
      Config config;
      TestBoolField(config) = true;
      ASSERT_TRUE(SUT->Set(TestBoolId(), "false", config));
      ASSERT_EQ(TestBoolField(config), false);
      ASSERT_TRUE(SUT->Set(TestBoolId(), "true", config));
      ASSERT_EQ(TestBoolField(config), true);

      ASSERT_TRUE(SUT->Set(TestThreeStateId(), "false", config));
      ASSERT_EQ(TestThreeStateField(config), ThreeStateFlag::Disabled);
      ASSERT_TRUE(SUT->Set(TestThreeStateId(), "true", config));
      ASSERT_EQ(TestThreeStateField(config), ThreeStateFlag::Enabled);
    }

    TEST(ConfigDataMapper, FailSetFlagField)
    {
      auto SUT = ConfigDataMapper::Create();
      Config config;
      TestBoolField(config) = true;
      ASSERT_FALSE(SUT->Set(TestBoolId(), "", config));
      ASSERT_EQ(TestBoolField(config), true);
      ASSERT_FALSE(SUT->Set(TestBoolId(), "1234", config));
      ASSERT_EQ(TestBoolField(config), true);

      ASSERT_FALSE(SUT->Set(TestThreeStateId(), "", config));
      ASSERT_EQ(TestThreeStateField(config), ThreeStateFlag::Undefined);
      ASSERT_FALSE(SUT->Set(TestThreeStateId(), "1234", config));
      ASSERT_EQ(TestThreeStateField(config), ThreeStateFlag::Undefined);
    }

    TEST(ConfigDataMapper, FailSetEmptyString)
    {
      auto SUT = ConfigDataMapper::Create();
      Config config;
      auto const expected = TestNotEmptyStrField(config);
      ASSERT_FALSE(expected.empty());
      ASSERT_FALSE(SUT->Set(TestNotEmptyStrId(), "", config));
      ASSERT_EQ(TestNotEmptyStrField(config), expected);
    }

    TEST(ConfigDataMapper, SetHistoryFile)
    {
      auto SUT = ConfigDataMapper::Create();
      Config config;
      config.history_len = 100;
      ASSERT_TRUE(SUT->Set(static_cast<int>(ConfigFieldId::history_file), "expected value", config));
      ASSERT_EQ(config.history_file, "expected value");
      ASSERT_EQ(config.history_len, 100);
    }

    TEST(ConfigDataMapper, SetEmptyHistoryFile)
    {
      auto SUT = ConfigDataMapper::Create();
      Config config;
      config.history_file = "not empty string";
      config.history_len = 100;
      ASSERT_TRUE(SUT->Set(static_cast<int>(ConfigFieldId::history_file), "", config));
      ASSERT_EQ(config.history_file, "");
      ASSERT_EQ(config.history_len, 0);
    }
  }
}

int main(int argc, char* argv[])
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
