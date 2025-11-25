#pragma once

#include <memory>
#include <string>
#include <utility>

struct PluginStartupInfo;

namespace Plugin
{
  enum class ConfigFieldType;
}

namespace Far3
{
  class Guid;

  class ConfigValueDialog
  {
  public:
    static std::unique_ptr<const ConfigValueDialog> Create(PluginStartupInfo const& i, Guid const& pluginGuid);

    virtual ~ConfigValueDialog() = default;
    virtual std::pair<bool, std::string> Show(std::string const& initValue, std::string const& defaultValue, Plugin::ConfigFieldType type, int textId) const = 0;
  };
}
