#pragma once

#include <plugin/current_editor.h>

#include <guiddef.h>

#include <memory>

struct PluginStartupInfo;

namespace Far3
{
  std::unique_ptr<Plugin::CurrentEditor> CreateCurrentEditor(PluginStartupInfo const& i, GUID const& pluginGuid);
}
