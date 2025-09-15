#include <gtest/gtest.h>

#include <far3/config_stream_reader.h>
#include <far3/error.h>

#include <plugin/config_data_mapper.h>

#include <sstream>

namespace Far3
{
  namespace Tests
  {
    unsigned const UNLIMITED = std::numeric_limits<unsigned>::max();

    TEST(ConfigStreamReader, ReadsConfig)
    {
      std::string const input =
        "\r\n\r\n\r\n"
        "exe=C:/path/to/exe\r\n"
        "usebuiltinctags=false\r\n\r\n\r\n"
        "commandline=-a --command --line"
      ;
      auto const config = ConfigStreamReader::Create()->Read(std::stringstream(input), *Plugin::ConfigDataMapper::Create());
      ASSERT_EQ(config.exe, "C:/path/to/exe");
      ASSERT_EQ(config.use_built_in_ctags, false);
      ASSERT_EQ(config.opt, "-a --command --line");
    }

    TEST(ConfigStreamReader, ReadsEmptyValue)
    {
      std::string const input =
        "\r\n\r\n\r\n"
        "historyfile=\r\n"
        "exe=C:/path/to/exe\r\n"
      ;
      auto const config = ConfigStreamReader::Create()->Read(std::stringstream(input), *Plugin::ConfigDataMapper::Create());
      ASSERT_EQ(config.exe, "C:/path/to/exe");
      ASSERT_EQ(config.history_file, "");
      ASSERT_EQ(config.history_len, 0);
    }

    TEST(ConfigStreamReader, ReadsFromEmptyStream)
    {
      std::string const input = "";
      ASSERT_NO_THROW(ConfigStreamReader::Create()->Read(std::stringstream(input), *Plugin::ConfigDataMapper::Create()));
    }

    TEST(ConfigStreamReader, ReadsFromEmptyLines)
    {
      std::string const input = "\r\n\r\n\r\n\r\n";
      ASSERT_NO_THROW(ConfigStreamReader::Create()->Read(std::stringstream(input), *Plugin::ConfigDataMapper::Create()));
    }

    TEST(ConfigStreamReader, ReusedToReadDifferentConfigs)
    {
      std::string const first =
        "\r\n\r\n\r\n"
        "historyfile=\r\n"
        "exe=C:/path/to/exe\r\n"
      ;
      std::string const second =
        "\r\n\r\n\r\n"
        "historyfile=C:/history/file\r\n"
        "exe=C:/the/exe/path\r\n"
      ;
      auto SUT = ConfigStreamReader::Create();

      auto const firstConfig = SUT->Read(std::stringstream(first), *Plugin::ConfigDataMapper::Create());
      ASSERT_EQ(firstConfig.exe, "C:/path/to/exe");
      ASSERT_EQ(firstConfig.history_file, "");
      ASSERT_EQ(firstConfig.history_len, 0);

      auto const secondConfig = SUT->Read(std::stringstream(second), *Plugin::ConfigDataMapper::Create());
      ASSERT_EQ(secondConfig.exe, "C:/the/exe/path");
      ASSERT_EQ(secondConfig.history_file, "C:/history/file");
      ASSERT_EQ(secondConfig.history_len, Plugin::Config().history_len);
    }

    TEST(ConfigStreamReader, ThrowsOnLongKey)
    {
      size_t const maxKeyLen = 12;
      std::string const input =
        "\r\n"
        "\r\n"
        "historyfile=\r\n"
        "resetcachecounterstimeouthours=1\r\n"
        "threshold=3"
      ;
      try
      {
        ConfigStreamReader::Create(maxKeyLen)->Read(std::stringstream(input), *Plugin::ConfigDataMapper::Create());
        FAIL() << "Expected exception thrown";
      }
      catch(Error const& e)
      {
        ASSERT_EQ(e.Field, decltype(e.Field)("line", "4"));
      }
    }

    TEST(ConfigStreamReader, ThrowsOnLongValue)
    {
      size_t const maxValueLen = 3;
      std::string const input =
        "\n"
        "\n"
        "threshold=3\n"
        "\n"
        "historyfile=c:/too/long/value/\n"
      ;
      try
      {
        ConfigStreamReader::Create(UNLIMITED, maxValueLen)->Read(std::stringstream(input), *Plugin::ConfigDataMapper::Create());
        FAIL() << "Expected exception thrown";
      }
      catch(Error const& e)
      {
        ASSERT_EQ(e.Field, decltype(e.Field)("line", "5"));
      }
    }

    TEST(ConfigStreamReader, ThrowsOnTooManyErrors)
    {
      unsigned const maxErrors = 10;
      std::string const input =
        "exe=\n"
        "\n" // OK
        "unknownkey=\n"
        "usebuiltinctags=27\n"
        "maxresults=true\n" // OK
        "threshold=c:/path\n"
        "thresholdfilterlen=false\n"
        "platformlanguagelookup=notboolean\n"
        "resetcachecounterstimeouthours=12\n" // OK
        "casesensfilt=74\n"
        "sortclassmembersbyname=notboolean\n"
        "curfilefirst=26\n"
        "cachedtagsontop=    \n"
        "indexeditedfile=true\n" // OK
      ;
      try
      {
        ConfigStreamReader::Create(UNLIMITED, UNLIMITED, maxErrors)->Read(std::stringstream(input), *Plugin::ConfigDataMapper::Create());
        FAIL() << "Expected exception thrown";
      }
      catch(Error const& e)
      {
        ASSERT_EQ(e.Field, decltype(e.Field)("line", "13"));
      }
    }

    TEST(ConfigStreamReader, ThrowsOnTooManyLines)
    {
      size_t const maxLines = 9;
      std::string const input =
        "\r\n"
        "\r\n"
        "historyfile=\r\n"
        "\r\n"
        "\r\n"
        "resetcachecounterstimeouthours=1\r\n"
        "threshold=3\r\n"
        "\r\n"
        "\r\n"
        "\r\n"
      ;
      try
      {
        ConfigStreamReader::Create(UNLIMITED, UNLIMITED, UNLIMITED, maxLines)->Read(std::stringstream(input), *Plugin::ConfigDataMapper::Create());
        FAIL() << "Expected exception thrown";
      }
      catch(Error const& e)
      {
        ASSERT_EQ(e.Field, decltype(e.Field)("line", std::to_string(maxLines + 1)));
      }
    }

    TEST(ConfigStreamReader, LoadsMaxLinesWithEmptyAtEnd)
    {
      size_t const maxLines = 10;
      std::string const input =
        "\r\n"
        "\r\n"
        "historyfile=\r\n"
        "\r\n"
        "\r\n"
        "resetcachecounterstimeouthours=1\r\n"
        "threshold=3\r\n"
        "\r\n"
        "\r\n"
        "\r\n"
      ;
      ASSERT_NO_THROW(ConfigStreamReader::Create(UNLIMITED, UNLIMITED, UNLIMITED, maxLines)->Read(std::stringstream(input), *Plugin::ConfigDataMapper::Create()));
    }

    TEST(ConfigStreamReader, LoadsExactlyMaxLines)
    {
      size_t const maxLines = 10;
      std::string const input =
        "\r\n"
        "\r\n"
        "historyfile=\r\n"
        "\r\n"
        "\r\n"
        "\r\n"
        "\r\n"
        "\r\n"
        "resetcachecounterstimeouthours=1\r\n"
        "threshold=3\r\n"
      ;
      ASSERT_NO_THROW(ConfigStreamReader::Create(UNLIMITED, UNLIMITED, UNLIMITED, maxLines)->Read(std::stringstream(input), *Plugin::ConfigDataMapper::Create()));
    }
  }
}

int main(int argc, char* argv[])
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
