#include "text.h"

#include <facade/message.h>
#include <facade/plugin.h>

#include <string>

namespace Facade
{
  class PluginImpl : public Plugin
  {
  public:
    PluginImpl()
    {
      ErrorMessage(MPlugin);
    }

    void OnPanelMenu(char const* currentFile) override
    {
      ErrorMessage((std::string("OnPanelMenu: ") + currentFile).c_str());
    }

    void OnEditorMenu(char const* currentFile) override
    {
      ErrorMessage((std::string("OnEditorMenu: ") + currentFile).c_str());
    }

    void OnCmd(char const* cmd) override
    {
      ErrorMessage((std::string("OnCmd: ") + cmd).c_str());
    }

    bool CanOpenFile(char const* file) override
    {
      return true;
    }

    void OpenFile(char const* file) override
    {
      ErrorMessage((std::string("OpenFile: ") + file).c_str());
    }

    void Cleanup()
    {
      ErrorMessage("Cleanup");
    }
  };

  std::unique_ptr<Plugin> Plugin::Create(char const* pluginFolder)
  {
    return std::unique_ptr<Plugin>(new PluginImpl);
  }
}
