#include "text.h"

#include <facade/menu.h>
#include <facade/message.h>
#include <facade/plugin.h>

#include <functional>
#include <string>
#include <vector>

namespace
{
  class ActionMenu
  {
  public:
    using Callback = std::function<void(void)>;

    ActionMenu();
    ActionMenu& Add(char label, int textID, Callback&& cb, bool disabled = false);
    ActionMenu& Separator();
    void Run(int titleID, int selected);

  private:
    std::unique_ptr<Facade::Menu> Menu;
    std::vector<Callback> Callbacks;
  };

  ActionMenu::ActionMenu()
    : Menu(Facade::Menu::Create())
  {
  }

  ActionMenu& ActionMenu::Add(char label, int textID, Callback&& cb, bool disabled)
  {
    Menu->Add(textID, label, disabled);
    Callbacks.push_back(std::move(cb));
    return *this;
  }

  ActionMenu& ActionMenu::Separator()
  {
    Menu->AddSeparator();
    return *this;
  }

  void ActionMenu::Run(int titleID, int selected)
  {
    auto res = Menu->Run(titleID, selected);
    if (res >= 0)
      Callbacks.at(res)();
  }
}

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
      ActionMenu::Callback dummy = [currentFile]{ ErrorMessage((std::string("OnPanelMenu: ") + currentFile).c_str()); };
      ActionMenu()
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
      ActionMenu()
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
     .Add(' ', MReindexRepo, ActionMenu::Callback(dummy))
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
  };

  std::unique_ptr<Plugin> Plugin::Create(char const* pluginFolder)
  {
    return std::unique_ptr<Plugin>(new PluginImpl);
  }
}
