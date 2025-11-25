#include <gtest/gtest.h>

#include <far3/config_stream_writer.h>

#include <plugin/config.h>
#include <plugin/config_data_mapper.h>

#include <sstream>
#include <string>

namespace Far3
{
  namespace Tests
  {
    TEST(ConfigStreamWriter, WritesConfig)
    {
      Plugin::Config const config;
      std::stringstream stream;
      auto const mapper = Plugin::ConfigDataMapper::Create();
      ASSERT_NO_THROW(ConfigStreamWriter::Create()->Write(stream, *mapper, config));

      for (int i = 0; i < static_cast<int>(Plugin::ConfigFieldId::MaxFieldId); ++i)
      {
        std::string line;
        std::getline(stream, line);
        auto field = mapper->Get(static_cast<Plugin::ConfigFieldId>(i), config);
        ASSERT_EQ(line, field.key + "=" + field.value);
      }
    }
  }
}

int main(int argc, char* argv[])
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
