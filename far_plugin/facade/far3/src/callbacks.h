#pragma once

#include <cstdint>

struct GlobalInfo;
struct PluginInfo;
struct PluginStartupInfo;
struct OpenInfo;
struct AnalyseInfo;
struct ConfigureInfo;
struct ProcessEditorEventInfo;
struct ExitInfo;

namespace FacadeInternal
{
  char const* GetPluginGuid();
  PluginStartupInfo const& FarAPI();
  void GetGlobalInfoW(GlobalInfo *info);
  void GetPluginInfoW(PluginInfo *info);
  void SetStartupInfoW(PluginStartupInfo const* info);
  void* OpenW(OpenInfo const* info);
  void* AnalyseW(AnalyseInfo const* info);
  intptr_t ConfigureW(ConfigureInfo const* info);
  intptr_t ProcessEditorEventW(ProcessEditorEventInfo const* info);
  void ExitFARW(ExitInfo const* info);
}
