#pragma once

#include <memory>

namespace Facade
{
  class Plugin
  {
  public:
    virtual void OnPanelMenu(char const* currentFile) = 0;
    virtual void OnEditorMenu(char const* currentFile) = 0;
    virtual void OnCmd(char const* cmd) = 0;
    virtual bool CanOpenFile(char const* file) = 0;
    virtual void OpenFile(char const* file) = 0;
    virtual void Cleanup() = 0;
  };

  std::unique_ptr<Plugin> CreatePlugin(char const* pluginFolder);
}
