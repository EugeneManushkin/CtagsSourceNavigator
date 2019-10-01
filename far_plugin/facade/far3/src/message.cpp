#include "callbacks.h"
#include "resource.h"
#include "string.h"

#include <facade/message.h>
#include <plugin_sdk/plugin.hpp>

#include <stdarg.h>
#include <vector>

using Facade::Internal::FarAPI;
using Facade::Internal::GetMsg;
using Facade::Internal::GetPluginGuid;
using Facade::Internal::StringToGuid;
using Facade::Internal::ToStdString;
using Facade::Internal::ToString;
using Facade::Internal::WideString;

namespace
{
  auto const ErrorMessageGuid = StringToGuid("{03cceb3e-20ba-438a-9972-85a48b0d28e4}");
  auto const InfoMessageGuid = StringToGuid("{58a20c1d-44e2-40ba-9223-5f96d31d8c09}");

  intptr_t Message(WideString const& text, WideString const& title, FARMESSAGEFLAGS flags, GUID* guid)
  {
    const int numButtons = 0;
    WideString str = title + L"\n" + text;
    return FarAPI().Message(GetPluginGuid(), guid, flags | FMSG_ALLINONE, nullptr, reinterpret_cast<const wchar_t *const *>(str.c_str()), 0, numButtons);
  }

  std::shared_ptr<void> LongOperationMessageImpl(WideString const& title, WideString const& text)
  {
    auto hScreen=FarAPI().SaveScreen(0, 0, -1, -1);
    Message(title, text, FMSG_LEFTALIGN, &*InfoMessageGuid);
    return std::shared_ptr<void>(hScreen, [](void* h)
    { 
       FarAPI().RestoreScreen(h); 
    });
  }
}

namespace Facade
{
  std::string Format(int formatID, ...)
  {
    auto wFormat = GetMsg(formatID);
    auto format = !wFormat ? std::string() : ToStdString(wFormat);
    va_list args;
    va_start(args, formatID);
    auto required = vsnprintf(nullptr, 0, format.c_str(), args);
    va_end(args);
    std::vector<char> buf(required + 1);
    va_start(args, formatID);
    vsnprintf(&buf[0], buf.size(), format.c_str(), args);
    va_end(args);
    return std::string(buf.begin(), --buf.end());
  }

  void ErrorMessage(std::string const& text, int titleID)
  {
    Message(ToString(text), GetMsg(titleID), FMSG_MB_OK | FMSG_WARNING, &*ErrorMessageGuid);
  }

  void ErrorMessage(int textID, int titleID)
  {
    Message(GetMsg(textID), GetMsg(titleID), FMSG_MB_OK | FMSG_WARNING, &*ErrorMessageGuid);
  }

  void InfoMessage(std::string const& text, int titleID)
  {
    Message(ToString(text), GetMsg(titleID), FMSG_MB_OK, &*InfoMessageGuid);
  }

  void InfoMessage(int textID, int titleID)
  {
    Message(GetMsg(textID), GetMsg(titleID), FMSG_MB_OK, &*InfoMessageGuid);
  }

  std::shared_ptr<void> LongOperationMessage(std::string const& text, int titleID)
  {
    return LongOperationMessageImpl(ToString(text), GetMsg(titleID));
  }

  std::shared_ptr<void> LongOperationMessage(int textID, int titleID)
  {
    return LongOperationMessageImpl(GetMsg(textID), GetMsg(titleID));
  }

  YesNoCancel YesNoCancelMessage(int textID, int titleID)
  {
    return static_cast<YesNoCancel>(Message(GetMsg(textID), GetMsg(titleID), FMSG_MB_YESNOCANCEL | FMSG_WARNING, &*InfoMessageGuid));
  }
}
