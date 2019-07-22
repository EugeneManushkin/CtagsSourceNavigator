#include "callbacks.h"

#include <plugin_sdk/plugin.hpp>
#include <safe_call.h>

#include <functional>

using Facade::Internal::SafeCall;

void WINAPI GetGlobalInfoW(struct GlobalInfo *info)
{
  SafeCall(std::bind(Facade::Internal::GetGlobalInfoW, info));
}

void WINAPI GetPluginInfoW(struct PluginInfo *info)
{
  SafeCall(std::bind(Facade::Internal::GetPluginInfoW, info));
}

void WINAPI SetStartupInfoW(const struct PluginStartupInfo *info)
{
  SafeCall(std::bind(Facade::Internal::SetStartupInfoW, info));
}

HANDLE WINAPI OpenW(const struct OpenInfo *info)
{
  return SafeCall(std::bind(Facade::Internal::OpenW, info), nullptr);
}

HANDLE WINAPI AnalyseW(const AnalyseInfo* info)
{
  return SafeCall(std::bind(Facade::Internal::AnalyseW, info), nullptr);
}

intptr_t WINAPI ConfigureW(const struct ConfigureInfo *info)
{
  return SafeCall(std::bind(Facade::Internal::ConfigureW, info), FALSE);
}

intptr_t WINAPI ProcessEditorEventW(const struct ProcessEditorEventInfo *info)
{
  return SafeCall(std::bind(Facade::Internal::ProcessEditorEventW, info), 0);
}

void WINAPI ExitFARW(const struct ExitInfo *info)
{
  SafeCall(std::bind(Facade::Internal::ExitFARW, info));
}
