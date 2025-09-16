#include <gtest/gtest.h>

#include <far3/config_stream_reader.h>
#include <far3/error.h>
#include <far3/text.h>

#include <plugin/config_data_mapper.h>

#include <sstream>

namespace Far3
{
  namespace Tests
  {
    unsigned const UNLIMITED = std::numeric_limits<unsigned>::max();

    std::string ToString(Error const& error)
    {
      return "code: " + std::to_string(error.Code) + ", " + error.Field.first + ":" + error.Field.second;
    }

    TEST(ConfigStreamReader, ReadsConfig)
    {
      Plugin::Config const defaults;
      std::string const input =
        "\r\n\r\n\r\n"
        "pathtoexe=C:\\tools\\yalta\\ctags_wrapper.bat\r\n"
        "usebuiltinctags=false\r\n"
        "commandline=--exclude=*.md --exclude=*.json --exclude=*.css --exclude=*.js --exclude=Makefile --exclude=*.htm? --cmake-types=+fmtvDpr --c++-types=+lpx --c-types=+lpx --fields=+n -R *\r\n"
        "maxresults=7\r\n"
        "threshold=789\r\n"
        "thresholdfilterlen=7\r\n"
        "platformlanguagelookup=true\r\n"
        "resetcachecounterstimeouthours=17\r\n"
        "casesensfilt=true\r\n"
        "sortclassmembersbyname=true\r\n"
        "curfilefirst=false\r\n"
        "cachedtagsontop=false\r\n"
        "indexeditedfile=false\r\n"
        "wordchars=abcd01234\r\n"
        "tagsmask=???,*.???\r\n"
        "historyfile=C:\\.tags-history\r\n"
        "historylen=89\r\n"
        "restorelastvisitedonload=false\r\n"
        "autoload=C:\\.tags-autoload"
      ;
      auto const config = ConfigStreamReader::Create()->Read(std::stringstream(input), *Plugin::ConfigDataMapper::Create());
      ASSERT_EQ(config.exe, "C:\\tools\\yalta\\ctags_wrapper.bat");
      ASSERT_EQ(config.opt, "--exclude=*.md --exclude=*.json --exclude=*.css --exclude=*.js --exclude=Makefile --exclude=*.htm? --cmake-types=+fmtvDpr --c++-types=+lpx --c-types=+lpx --fields=+n -R *");
      ASSERT_EQ(config.wordchars, "abcd01234");
      ASSERT_EQ(config.tagsmask, "???,*.???");
      ASSERT_EQ(config.history_file, "C:\\.tags-history");
      ASSERT_EQ(config.permanents, "C:\\.tags-autoload");
    }

    TEST(ConfigStreamReader, ReadsEmptyValue)
    {
      std::string const input =
        "\r\n\r\n\r\n"
        "historyfile=\r\n"
        "pathtoexe=C:/path/to/exe\r\n"
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

    TEST(ConfigStreamReader, ReadsFromSingleLineStream)
    {
      std::string const input = "resetcachecounterstimeouthours=27";
      auto const config = ConfigStreamReader::Create()->Read(std::stringstream(input), *Plugin::ConfigDataMapper::Create());
      ASSERT_EQ(config.reset_cache_counters_timeout_hours, 27);
    }

    TEST(ConfigStreamReader, ThrowsOnErrorInSingleLineStream)
    {
      std::string const input = "invalid field?syntax";
      try
      {
        ConfigStreamReader::Create()->Read(std::stringstream(input), *Plugin::ConfigDataMapper::Create());
        FAIL() << "Expected exception thrown";
      }
      catch(Error const& e)
      {
        ASSERT_EQ(ToString(e), ToString(Error(MInvalidConfigFileFormat, "line", "1")));
      }
    }

    TEST(ConfigStreamReader, ReusedToReadDifferentConfigs)
    {
      std::string const first =
        "\r\n\r\n\r\n"
        "historyfile=\r\n"
        "pathtoexe=C:/path/to/exe\r\n"
      ;
      std::string const second =
        "\r\n\r\n\r\n"
        "historyfile=C:/history/file\r\n"
        "pathtoexe=C:/the/exe/path\r\n"
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
        ASSERT_EQ(ToString(e), ToString(Error(MInvalidConfigFileFormat, "line", "4")));
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
        ASSERT_EQ(ToString(e), ToString(Error(MInvalidConfigFileFormat, "line", "5")));
      }
    }

    TEST(ConfigStreamReader, ThrowsOnTooManyErrors)
    {
      unsigned const maxErrors = 10;
      std::string const input =
        "pathtoexe=\n"
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
        ASSERT_EQ(ToString(e), ToString(Error(MTooManyErrorsInConfigFile, "line", "13")));
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
        ASSERT_EQ(ToString(e), ToString(Error(MTooManyLinesInConfigFile, "line", std::to_string(maxLines + 1))));
      }
    }

    TEST(ConfigStreamReader, ThrowsOnTooManyEmptyLines)
    {
      size_t const maxLines = 4;
      std::string const input =
        "\r\n"
        "\r\n"
        "\r\n"
        "\r\n"
        "threshold=4"
      ;
      try
      {
        ConfigStreamReader::Create(UNLIMITED, UNLIMITED, UNLIMITED, maxLines)->Read(std::stringstream(input), *Plugin::ConfigDataMapper::Create());
        FAIL() << "Expected exception thrown";
      }
      catch(Error const& e)
      {
        ASSERT_EQ(ToString(e), ToString(Error(MTooManyLinesInConfigFile, "line", std::to_string(maxLines + 1))));
      }
    }

    TEST(ConfigStreamReader, ThrowsOnErrorInLastNotTerminatedLine)
    {
      size_t const maxLines = 5;
      std::string const input =
        "\r\n"
        "\r\n"
        "\r\n"
        "\r\n"
        "invalid field?syntax"
      ;
      try
      {
        ConfigStreamReader::Create(UNLIMITED, UNLIMITED, UNLIMITED, maxLines)->Read(std::stringstream(input), *Plugin::ConfigDataMapper::Create());
        FAIL() << "Expected exception thrown";
      }
      catch(Error const& e)
      {
        ASSERT_EQ(ToString(e), ToString(Error(MInvalidConfigFileFormat, "line", std::to_string(maxLines))));
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

    TEST(ConfigStreamReader, ThrowsOnInvalidSyntaxFollowedByValidField)
    {
      std::string const input =
        "\r\n"
        "historyfile=\r\n"
        "invalid field?syntax\r\n"
        "threshold=3\r\n"
        "\r\n"
      ;
      try
      {
        ConfigStreamReader::Create()->Read(std::stringstream(input), *Plugin::ConfigDataMapper::Create());
        FAIL() << "Expected exception thrown";
      }
      catch(Error const& e)
      {
        ASSERT_EQ(ToString(e), ToString(Error(MInvalidConfigFileFormat, "line", "3")));
      }
    }

    TEST(ConfigStreamReader, ThrowsOnEmptyKey)
    {
      std::string const input =
        "\r\n"
        "historyfile=\r\n"
        "=empty key\r\n"
        "threshold=3\r\n"
      ;
      try
      {
        ConfigStreamReader::Create()->Read(std::stringstream(input), *Plugin::ConfigDataMapper::Create());
        FAIL() << "Expected exception thrown";
      }
      catch(Error const& e)
      {
        ASSERT_EQ(ToString(e), ToString(Error(MInvalidConfigFileFormat, "line", "3")));
      }
    }

    TEST(ConfigStreamReader, ThrowsOnInvalidCharInKey)
    {
      std::string const input =
        "\r\n"
        "historyfile=\r\n"
        "invalid@key=value\r\n"
        "threshold=3\r\n"
      ;
      try
      {
        ConfigStreamReader::Create()->Read(std::stringstream(input), *Plugin::ConfigDataMapper::Create());
        FAIL() << "Expected exception thrown";
      }
      catch(Error const& e)
      {
        ASSERT_EQ(ToString(e), ToString(Error(MInvalidConfigFileFormat, "line", "3")));
      }
    }
  }
}

int main(int argc, char* argv[])
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
