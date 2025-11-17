#pragma once

#include <ostream>
#include <memory>

namespace Plugin
{
  struct Config;
  class ConfigDataMapper;
}

namespace Far3
{
  class ConfigStreamWriter
  {
  public:
    static std::unique_ptr<const ConfigStreamWriter> Create();

    virtual ~ConfigStreamWriter() = default;
    virtual void Write(std::ostream& stream, Plugin::ConfigDataMapper const& dataMapper, Plugin::Config const& config) const = 0;
  };
}
