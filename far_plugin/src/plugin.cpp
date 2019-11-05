#include "action_menu.h"
#include "config.h"
#include "tags_history.h"
#include "text.h"

#include <facade/menu.h>
#include <facade/message.h>
#include <facade/plugin.h>
#include <platform/path.h>
#include <tags.h>
#include <tags_repository_storage.h>

#include <stack>
#include <string>
#include <stdexcept>

using Facade::ErrorMessage;
using Facade::InfoMessage;
using Facade::LongOperationMessage;
using FarPlugin::ActionMenu;
using Platform::JoinPath;
using Tags::RepositoryStorage;
using Tags::RepositoryType;

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
      , Storage(RepositoryStorage::Create())
    {
    }

    void OnPanelMenu(char const* currentFile) override
    {
      ActionMenu::Callback dummy = [currentFile]{ ErrorMessage((std::string("OnPanelMenu: ") + currentFile).c_str()); };
      ActionMenu(std::bind(&PluginImpl::LoadConfig, this))
     .Add('1', MLookupSymbol, ActionMenu::Callback(dummy))
     .Add('2', MSearchFile, ActionMenu::Callback(dummy))
     .Add('H', MNavigationHistory, ActionMenu::Callback(dummy), true)
     .Separator()
     .Add('3', MLoadTagsFile, std::bind(&PluginImpl::LoadTags, this, currentFile))
     .Add('4', MLoadFromHistory, std::bind(&PluginImpl::LoadFromHistory, this))
     .Add('5', MUnloadTagsFile, std::bind(&PluginImpl::UnloadTagsFiles, this))
     .Separator()
     .Add('7', MCreateTagsFile, ActionMenu::Callback(dummy))
     .Add('8', MReindexRepo, ActionMenu::Callback(dummy))
     .Separator()
     .Add('C', MPluginConfiguration, std::bind(&FarPlugin::ModifyConfig, ConfigPath))
     .Run(MPlugin, -1);
    }

    void OnEditorMenu(char const* currentFile) override
    {
      ActionMenu::Callback dummy = [currentFile]{ ErrorMessage((std::string("OnEditorMenu: ") + currentFile).c_str()); };
      ActionMenu(std::bind(&PluginImpl::LoadConfig, this))
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
     .Add('C', MPluginConfiguration, std::bind(&FarPlugin::ModifyConfig, ConfigPath))
     .Run(MPlugin, -1);
    }

    void OnCmd(char const* cmd) override
    {
      CleanupActions.push(CleanupAction([](bool success){throw std::runtime_error("Test cleanup action: " + std::to_string(success));}, true));
      LoadConfig();
      LoadTags(cmd);
    }

    bool CanOpenFile(char const* file) override
    {
      return IsTagFile(file);
    }

    void OpenFile(char const* file) override
    {
      LoadConfig();
      LoadTags(file);
    }

    std::function<void(bool)> GetCleanupAction(bool& retriable) override
    {
      auto cleanupAction = !CleanupActions.empty() ? CleanupActions.top() : CleanupAction();
      if (!CleanupActions.empty())
        CleanupActions.pop();

      retriable = cleanupAction.second;
      return cleanupAction.first;
    }

    void LoadFromHistory();

  private:
    void LoadConfig();
    std::string HistoryFileFullPath() const;
    void LoadTags(std::string const& tagsFile);
    void UnloadTagsFiles();

    std::string const PluginFolder;
    std::string const ConfigPath;
    FarPlugin::Config Config;
    std::unique_ptr<RepositoryStorage> Storage;
    using CleanupAction = std::pair<std::function<void(bool)>, bool>;
    std::stack<CleanupAction> CleanupActions;
  };

  void PluginImpl::LoadFromHistory()
  {
    auto tagsFile = FarPlugin::SelectFromTagsHistory(HistoryFileFullPath().c_str(), Config.HistoryLen);
    if (!tagsFile.empty())
      LoadTags(tagsFile);
  }

  void PluginImpl::LoadConfig()
  {
    Config = FarPlugin::LoadConfig(ConfigPath);
  }

  std::string PluginImpl::HistoryFileFullPath() const
  {
    return GetFullPath(Config.HistoryFile, PluginFolder);
  }

  void PluginImpl::LoadTags(std::string const& tagsFile)
  {
    auto message = LongOperationMessage(MLoadingTags, MPlugin);
    size_t symbolsLoaded = 0;
    if (auto err = Storage->Load(tagsFile.c_str(), RepositoryType::Regular, symbolsLoaded))
      throw std::runtime_error(Facade::Format(err == ENOENT ? MEFailedToOpen : MFailedToWriteIndex, tagsFile.c_str()));

    FarPlugin::AddToTagsHistory(tagsFile.c_str(), HistoryFileFullPath().c_str(), Config.HistoryLen);
    InfoMessage(Facade::Format(MLoadOk, symbolsLoaded, tagsFile.c_str()), MPlugin);
  }

  void PluginImpl::UnloadTagsFiles()
  {
  //TODO: rework
  }
}

namespace Facade
{
  std::unique_ptr<Plugin> Plugin::Create(char const* pluginFolder)
  {
    return std::unique_ptr<Plugin>(new PluginImpl(pluginFolder));
  }
}
