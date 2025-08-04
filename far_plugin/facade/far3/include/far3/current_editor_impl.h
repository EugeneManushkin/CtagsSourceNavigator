#pragma once

#include <plugin/current_editor.h>

#include <memory>

struct PluginStartupInfo;

namespace Far3
{
  class Guid;

  std::unique_ptr<Plugin::CurrentEditor> CreateCurrentEditor(PluginStartupInfo const& i, Guid const& pluginGuid);
}
