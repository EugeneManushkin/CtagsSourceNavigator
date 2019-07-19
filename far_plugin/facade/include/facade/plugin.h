#pragma once

#include <memory>

namespace Facade
{
  class Plugin
  {
  public:
    virtual void OnPanelMenu() = 0;
    virtual void OnEditorMenu() = 0;
    virtual void OnCmd(char const* cmd) = 0;
    virtual bool CanOpenFile(char const* file) = 0;
    virtual void OpenFile(char const* file) = 0;
    virtual void Cleanup() = 0;
  };

  std::unique_ptr<Plugin> CreatePlugin(char const* pluginFolder);
}
