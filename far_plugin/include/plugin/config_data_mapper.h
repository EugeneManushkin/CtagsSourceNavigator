#pragma once

#include <memory>

namespace Plugin
{
  struct Config;

  enum class ConfigFieldId;

  enum class ConfigFieldType
  {
    Invalid = -1,
    Size,
    Flag,
    String,
  };

  struct ConfigFieldData
  {
    std::string key;
    std::string value;
    ConfigFieldType type;
  };

  class ConfigDataMapper
  {
  public:
    static std::unique_ptr<const ConfigDataMapper> Create();

    virtual ~ConfigDataMapper() = default;
    virtual ConfigFieldData Get(ConfigFieldId fieldId, Config const& config) const = 0;
    virtual bool Set(std::string const& key, std::string const& value, Config& config) const = 0;
    virtual bool Set(ConfigFieldId fieldId, std::string const& value, Config& config) const = 0;
  };
}
