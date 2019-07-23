#include "action_menu.h"
#include "config.h"
#include "text.h"

#include <facade/menu.h>
#include <facade/message.h>
#include <facade/plugin.h>
#include <platform/path.h>

#include <string>

using Facade::ErrorMessage;
using FarPlugin::ActionMenu;
using Platform::JoinPath;

namespace
{
  class PluginImpl : public Facade::Plugin
  {
  public:
    PluginImpl(std::string&& pluginFolder)
      : PluginFolder(std::move(pluginFolder))
      , ConfigPath(JoinPath(PluginFolder, "config"))
    {
    }

    void OnPanelMenu(char const* currentFile) override
    {
      ActionMenu::Callback dummy = [currentFile]{ ErrorMessage((std::string("OnPanelMenu: ") + currentFile).c_str()); };
      ActionMenu([this]{Config = FarPlugin::LoadConfig(ConfigPath);})
     .Add('1', MLookupSymbol, ActionMenu::Callback(dummy))
     .Add('2', MSearchFile, ActionMenu::Callback(dummy))
     .Add('H', MNavigationHistory, ActionMenu::Callback(dummy), true)
     .Separator()
     .Add('3', MLoadTagsFile, ActionMenu::Callback(dummy))
     .Add('4', MLoadFromHistory, ActionMenu::Callback(dummy))
     .Add('5', MUnloadTagsFile, ActionMenu::Callback(dummy))
     .Separator()
     .Add('7', MCreateTagsFile, ActionMenu::Callback(dummy))
     .Add('8', MReindexRepo, ActionMenu::Callback(dummy))
     .Separator()
     .Add('C', MPluginConfiguration, ActionMenu::Callback(dummy))
     .Run(MPlugin, -1);
    }

    void OnEditorMenu(char const* currentFile) override
    {
      ActionMenu::Callback dummy = [currentFile]{ ErrorMessage((std::string("OnEditorMenu: ") + currentFile).c_str()); };
      ActionMenu([this]{Config = FarPlugin::LoadConfig(ConfigPath);})
     .Add('1', MFindSymbol, ActionMenu::Callback(dummy))
     .Add('2', MCompleteSymbol, ActionMenu::Callback(dummy))
     .Add('3', MUndoNavigation, ActionMenu::Callback(dummy), true)
     .Add('F', MRepeatNavigation, ActionMenu::Callback(dummy), true)
     .Add('H', MNavigationHistory, ActionMenu::Callback(dummy), true)
     .Separator()
     .Add('4', MBrowseClass, ActionMenu::Callback(dummy))
     .Add('5', MBrowseSymbolsInFile, ActionMenu::Callback(dummy))
     .Add('6', MLookupSymbol, ActionMenu::Callback(dummy))
     .Add('7', MSearchFile, ActionMenu::Callback(dummy))
     .Separator()
     .Add('8', MReindexRepo, ActionMenu::Callback(dummy))
     .Separator()
     .Add('C', MPluginConfiguration, ActionMenu::Callback(dummy))
     .Run(MPlugin, -1);
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

  private:
    std::string const PluginFolder;
    std::string const ConfigPath;
    FarPlugin::Config Config;
  };
}

namespace Facade
{
  std::unique_ptr<Plugin> Plugin::Create(char const* pluginFolder)
  {
    return std::unique_ptr<Plugin>(new PluginImpl(pluginFolder));
  }
}
