#include "callbacks.h"
#include "resource.h"
#include "string.h"

#include <facade/plugin.h>
#include <plugin_sdk/plugin.hpp>

namespace
{
  GUID StrToGuid(char const* str)
  {
    GUID result;
    Facade::Internal::StringToGuid(str, result);
    return result;
  }

  char const* const PluginGuid = "{2e34b611-3df1-463f-8711-74b0f21558a5}";
  GUID const MenuGuid = StrToGuid("{a5b1037e-2f54-4609-b6dd-70cd47bd222b}");

  PluginStartupInfo I;
  FarStandardFunctions FSF;
  std::unique_ptr<Facade::Plugin> PluginInstance;

  std::string GetDirOfFile(std::string const& fileName)
  {
    auto pos = fileName.find_last_of("\\/");
    return pos == std::string::npos ? std::string() : fileName.substr(0, pos);
  }
}

namespace Facade
{
  namespace Internal
  {
    char const* GetPluginGuid()
    {
      return PluginGuid;
    }
  
    PluginStartupInfo const& FarAPI()
    {
      return I;
    }
  
    void GetGlobalInfoW(GlobalInfo *info)
    {
      info->StructSize = sizeof(*info);
      info->MinFarVersion = MAKEFARVERSION(3, 0, 0, 0, VS_RELEASE);
      info->Version = MAKEFARVERSION(CTAGS_VERSION_MAJOR, CTAGS_VERSION_MINOR, 0, CTAGS_BUILD, VS_RELEASE);
      info->Guid = StrToGuid(PluginGuid);
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
      info->PluginMenu.Guids = info->PluginConfig.Guids = &MenuGuid;
      info->PluginMenu.Strings = info->PluginConfig.Strings = PluginMenuStrings;
      info->PluginMenu.Count = info->PluginConfig.Count = sizeof(PluginMenuStrings) / sizeof(PluginMenuStrings[0]);
      info->CommandPrefix = L"tag";
    }
    
    void SetStartupInfoW(PluginStartupInfo const* info)
    {
      I = *info;
      FSF = *info->FSF;
      I.FSF = &FSF;
      PluginInstance = CreatePlugin(GetDirOfFile(ToStdString(I.ModuleName)).c_str());
    }
    
    void* OpenW(OpenInfo const* info)
    {
      switch (info->OpenFrom)
      {
      case OPEN_EDITOR:
        PluginInstance->OnEditorMenu();
        break;
      case OPEN_PLUGINSMENU:
        PluginInstance->OnPanelMenu();
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
