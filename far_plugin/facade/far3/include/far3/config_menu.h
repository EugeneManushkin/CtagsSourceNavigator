#pragma once

#include <memory>
#include <string>
#include <utility>

struct PluginStartupInfo;

namespace Plugin
{
  struct Config;
  class ConfigDataMapper;
  enum class ConfigFieldId;
}

namespace Far3
{
  class Guid;

  class ConfigMenu
  {
  public:
    static std::unique_ptr<const ConfigMenu> Create(PluginStartupInfo const& i, Guid const& pluginGuid);

    virtual ~ConfigMenu() = default;
    virtual std::pair<Plugin::ConfigFieldId, std::string> Show(Plugin::ConfigDataMapper const& dataMapper, Plugin::Config const& config) const = 0;
  };
}
