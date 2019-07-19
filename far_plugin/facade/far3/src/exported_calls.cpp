#include "callbacks.h"

#include <plugin_sdk/plugin.hpp>
#include <safe_call.h>

#include <functional>

using FacadeInternal::SafeCall;

void WINAPI GetGlobalInfoW(struct GlobalInfo *info)
{
  SafeCall(std::bind(FacadeInternal::GetGlobalInfoW, info));
}

void WINAPI GetPluginInfoW(struct PluginInfo *info)
{
  SafeCall(std::bind(FacadeInternal::GetPluginInfoW, info));
}

void WINAPI SetStartupInfoW(const struct PluginStartupInfo *info)
{
  SafeCall(std::bind(FacadeInternal::SetStartupInfoW, info));
}

HANDLE WINAPI OpenW(const struct OpenInfo *info)
{
  return SafeCall(std::bind(FacadeInternal::OpenW, info), nullptr);
}

HANDLE WINAPI AnalyseW(const AnalyseInfo* info)
{
  return SafeCall(std::bind(FacadeInternal::AnalyseW, info), nullptr);
}

intptr_t WINAPI ConfigureW(const struct ConfigureInfo *info)
{
  return SafeCall(std::bind(FacadeInternal::ConfigureW, info), FALSE);
}

intptr_t WINAPI ProcessEditorEventW(const struct ProcessEditorEventInfo *info)
{
  return SafeCall(std::bind(FacadeInternal::ProcessEditorEventW, info), 0);
}

void WINAPI ExitFARW(const struct ExitInfo *info)
{
  SafeCall(std::bind(FacadeInternal::ExitFARW, info));
}
