#include "action_menu.h"
#include "config.h"
#include "tags_history.h"
#include "text.h"

#include <facade/menu.h>
#include <facade/message.h>
#include <facade/plugin.h>
#include <platform/path.h>
#include <tags.h>

#include <string>
#include <stdexcept>

using Facade::ErrorMessage;
using Facade::InfoMessage;
using Facade::LongOperationMessage;
using FarPlugin::ActionMenu;
using Platform::JoinPath;

namespace
{
  std::string GetFullPath(std::string const& path, std::string const& pluginFolder)
  {
    auto expandedPath = Platform::ExpandEnvString(path);
    return Platform::IsAbsolutePath(expandedPath.c_str()) ? std::move(expandedPath) : JoinPath(pluginFolder, expandedPath);
  }

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
     .Add('3', MLoadTagsFile, std::bind(&PluginImpl::LoadTags, this, currentFile))
     .Add('4', MLoadFromHistory, std::bind(&PluginImpl::LoadFromHistory, this))
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

    void Cleanup() override
    {
      ErrorMessage("Cleanup");
    }

    void LoadFromHistory();

  private:
    std::string HistoryFileFullPath() const;
    void LoadTags(std::string const& tagsFile);

    std::string const PluginFolder;
    std::string const ConfigPath;
    FarPlugin::Config Config;
  };

  void PluginImpl::LoadFromHistory()
  {
    auto tagsFile = FarPlugin::SelectFromTagsHistory(HistoryFileFullPath().c_str(), Config.HistoryLen);
    if (!tagsFile.empty())
      LoadTags(tagsFile);
  }

  std::string PluginImpl::HistoryFileFullPath() const
  {
    return GetFullPath(Config.HistoryFile, PluginFolder);
  }

  void PluginImpl::LoadTags(std::string const& tagsFile)
  {
    auto message = LongOperationMessage(MLoadingTags, MPlugin);
    size_t symbolsLoaded = 0;
    bool const singleFileRepos = false;
    if (auto err = Load(tagsFile.c_str(), singleFileRepos, symbolsLoaded))
      // TODO: throw Error(err == ENOENT ? MEFailedToOpen : MFailedToWriteIndex, "Tags file", tagsFile);
      throw std::runtime_error(std::string("Failed to load tags file, error: ") + std::to_string(err));

    FarPlugin::AddToTagsHistory(tagsFile.c_str(), HistoryFileFullPath().c_str(), Config.HistoryLen);
    InfoMessage(Facade::Format(MLoadOk, symbolsLoaded, tagsFile.c_str()), MPlugin);
  }
}

namespace Facade
{
  std::unique_ptr<Plugin> Plugin::Create(char const* pluginFolder)
  {
    return std::unique_ptr<Plugin>(new PluginImpl(pluginFolder));
  }
}
