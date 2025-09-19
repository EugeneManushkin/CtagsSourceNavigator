#include <far3/config_stream_writer.h>

#include <plugin/config.h>
#include <plugin/config_data_mapper.h>

namespace
{
  class ConfigStreamWriterImpl : public Far3::ConfigStreamWriter
  {
  public:
    void Write(std::ostream& stream, Plugin::ConfigDataMapper const& dataMapper, Plugin::Config const& config) const override;
  };

  void ConfigStreamWriterImpl::Write(std::ostream& stream, Plugin::ConfigDataMapper const& dataMapper, Plugin::Config const& config) const
  {
    for (int i = 0; i < static_cast<int>(Plugin::ConfigFieldId::MaxFieldId); ++i)
    {
      auto fieldData = dataMapper.Get(i, config);
      stream << fieldData.key << "=" << fieldData.value << std::endl;
    }
  }
}

namespace Far3
{
  std::unique_ptr<const ConfigStreamWriter> ConfigStreamWriter::Create()
  {
    return std::unique_ptr<const ConfigStreamWriter>(new ConfigStreamWriterImpl());
  }
}
