#include "callbacks.h"
#include "resource.h"
#include "string.h"

#include <facade/plugin.h>
#include <plugin_sdk/plugin.hpp>

#include <vector>

using Facade::Internal::StringToGuid;
using Facade::Internal::WideString;

namespace
{
  auto const PluginGuid = StringToGuid("{2e34b611-3df1-463f-8711-74b0f21558a5}");
  auto const MenuGuid = StringToGuid("{a5b1037e-2f54-4609-b6dd-70cd47bd222b}");

  PluginStartupInfo I;
  FarStandardFunctions FSF;
  std::unique_ptr<Facade::Plugin> PluginInstance;
  //TODO: move to platform library
  std::string GetDirOfFile(std::string const& fileName)
  {
    auto pos = fileName.find_last_of("\\/");
    return pos == std::string::npos ? std::string() : fileName.substr(0, pos);
  }
  //TODO: move to platform library
  bool IsPathSeparator(char c)
  {
    return c == '/' || c == '\\';
  }
  //TODO: move to platform library
  std::string JoinPath(std::string const& dirPath, std::string const& name)
  {
    return dirPath.empty() || IsPathSeparator(dirPath.back()) ? dirPath + name : dirPath + std::string("\\") + name;
  }

  WideString GetPanelFile()
  {
    HANDLE const hPanel = PANEL_ACTIVE;
    PanelInfo pi = {sizeof(PanelInfo)};
    I.PanelControl(hPanel, FCTL_GETPANELINFO, 0, &pi);
    size_t sz = I.PanelControl(hPanel, FCTL_GETPANELITEM, pi.CurrentItem, nullptr);
    if (!sz)
      return WideString();
  
    std::vector<wchar_t> buffer(sz);
    FarGetPluginPanelItem* item = reinterpret_cast<FarGetPluginPanelItem*>(&buffer[0]);
    item->StructSize = sizeof(FarGetPluginPanelItem);
    item->Size = sz;
    item->Item = reinterpret_cast<PluginPanelItem*>(item + 1);
    sz = I.PanelControl(hPanel, FCTL_GETPANELITEM, pi.CurrentItem, &buffer[0]);
    return item->Item->FileName;
  }

  WideString GetPanelDir()
  {
    HANDLE const hPanel = PANEL_ACTIVE;
    size_t sz = I.PanelControl(hPanel, FCTL_GETPANELDIRECTORY, 0, nullptr);
    if (!sz)
      return WideString();
  
    std::vector<wchar_t> buffer(sz);
    FarPanelDirectory* dir = reinterpret_cast<FarPanelDirectory*>(&buffer[0]);
    dir->StructSize = sizeof(FarPanelDirectory);
    sz = I.PanelControl(hPanel, FCTL_GETPANELDIRECTORY, sz, &buffer[0]);
    return dir->Name;
  }

  EditorInfo GetCurrentEditorInfo()
  {
    EditorInfo ei = {sizeof(EditorInfo)};
    I.EditorControl(-1, ECTL_GETINFO, 0, &ei);
    return ei;
  }
  
  WideString GetFileNameFromEditor(intptr_t editorID)
  {
    size_t sz = I.EditorControl(editorID, ECTL_GETFILENAME, 0, nullptr);
    if (!sz)
      return WideString();
  
    std::vector<wchar_t> buffer(sz);
    I.EditorControl(editorID, ECTL_GETFILENAME, buffer.size(), &buffer[0]);
    return WideString(buffer.begin(), buffer.end() - 1);
  }
}

namespace Facade
{
  namespace Internal
  {
    _GUID const* GetPluginGuid()
    {
      return PluginGuid.get();
    }
  
    PluginStartupInfo const& FarAPI()
    {
      return I;
    }

    wchar_t const* GetMsg(int textID)
    {
      return FarAPI().GetMsg(GetPluginGuid(), textID);
    }
  
    void GetGlobalInfoW(GlobalInfo *info)
    {
      info->StructSize = sizeof(*info);
      info->MinFarVersion = MAKEFARVERSION(3, 0, 0, 0, VS_RELEASE);
      info->Version = MAKEFARVERSION(CTAGS_VERSION_MAJOR, CTAGS_VERSION_MINOR, 0, CTAGS_BUILD, VS_RELEASE);
      info->Guid = *GetPluginGuid();
      info->Title = CTAGS_PRODUCT_NAME;
      info->Description = CTAGS_FILE_DESCR;
      info->Author = L"Eugene Manushkin";
    }
    
    void GetPluginInfoW(PluginInfo *info)
    {
      static const wchar_t *PluginMenuStrings[1];
      PluginMenuStrings[0] = CTAGS_PRODUCT_NAME;
      info->StructSize = sizeof(*info);
      info->Flags = PF_EDITOR;
      info->PluginMenu.Guids = info->PluginConfig.Guids = &*MenuGuid;
      info->PluginMenu.Strings = info->PluginConfig.Strings = PluginMenuStrings;
      info->PluginMenu.Count = info->PluginConfig.Count = sizeof(PluginMenuStrings) / sizeof(PluginMenuStrings[0]);
      info->CommandPrefix = L"tag";
    }
    
    void SetStartupInfoW(PluginStartupInfo const* info)
    {
      I = *info;
      FSF = *info->FSF;
      I.FSF = &FSF;
      PluginInstance = Plugin::Create(GetDirOfFile(ToStdString(I.ModuleName)).c_str());
    }
    
    void* OpenW(OpenInfo const* info)
    {
      switch (info->OpenFrom)
      {
      case OPEN_EDITOR:
        PluginInstance->OnEditorMenu(ToStdString(GetFileNameFromEditor(GetCurrentEditorInfo().EditorID)).c_str());
        break;
      case OPEN_PLUGINSMENU:
        PluginInstance->OnPanelMenu(JoinPath(ToStdString(GetPanelDir()), ToStdString(GetPanelFile())).c_str());
        break;
      case OPEN_COMMANDLINE:
        PluginInstance->OnCmd(ToStdString(reinterpret_cast<OpenCommandLineInfo const*>(info->Data)->CommandLine).c_str());
        break;
      case OPEN_ANALYSE:
        PluginInstance->OpenFile(ToStdString(reinterpret_cast<const OpenAnalyseInfo*>(info->Data)->Info->FileName).c_str());
        return PANEL_STOP;
      }
  
      return nullptr;
    }
    
    void* AnalyseW(AnalyseInfo const* info)
    {
      return info->FileName && PluginInstance->CanOpenFile(ToStdString(info->FileName).c_str()) ? INVALID_HANDLE_VALUE : nullptr;
    }
    
    intptr_t ConfigureW(ConfigureInfo const*)
    {
      return 0;
    }
    
    intptr_t ProcessEditorEventW(ProcessEditorEventInfo const* info)
    {
      return 0;
    }
    
    void ExitFARW(ExitInfo const* info)
    {
      PluginInstance->Cleanup();
    }
  }
}
