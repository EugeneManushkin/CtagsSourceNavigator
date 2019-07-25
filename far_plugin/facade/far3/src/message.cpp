#include "callbacks.h"
#include "resource.h"
#include "string.h"

#include <facade/message.h>
#include <plugin_sdk/plugin.hpp>

using Facade::Internal::FarAPI;
using Facade::Internal::GetMsg;
using Facade::Internal::GetPluginGuid;
using Facade::Internal::StringToGuid;
using Facade::Internal::ToString;
using Facade::Internal::WideString;

namespace
{
  auto const ErrorMessageGuid = StringToGuid("{03cceb3e-20ba-438a-9972-85a48b0d28e4}");
  auto const InfoMessageGuid = StringToGuid("{58a20c1d-44e2-40ba-9223-5f96d31d8c09}");

  void Message(wchar_t const* text, int numButtons, FARMESSAGEFLAGS flags, GUID* guid)
  {
    FarAPI().Message(GetPluginGuid(), guid, flags | FMSG_ALLINONE, nullptr, reinterpret_cast<const wchar_t *const *>(text), 0, numButtons);
  }

  void ErrorMessageImpl(WideString const& text)
  {
    WideString str = WideString(CTAGS_PRODUCT_NAME) + L"\n" + text;// + L"\nOk";
    Message(str.c_str(), 1, FMSG_MB_OK | FMSG_WARNING, &*ErrorMessageGuid);
  }

  void InfoMessageImpl(WideString const& title, WideString const& text)
  {
    WideString str = title + L"\n" + text;
    Message(str.c_str(), 1, FMSG_MB_OK, &*ErrorMessageGuid);
  }

  std::shared_ptr<void> LongOperationMessageImpl(WideString const& title, WideString const& text)
  {
    auto str = title + L"\n" + text;
    auto hScreen=FarAPI().SaveScreen(0, 0, -1, -1);
    Message(str.c_str(), 0, FMSG_LEFTALIGN, &*InfoMessageGuid);
    return std::shared_ptr<void>(hScreen, [](void* h)
    { 
       FarAPI().RestoreScreen(h); 
    });
  }
}

namespace Facade
{
  void ErrorMessage(char const* text)
  {   
    ErrorMessageImpl(ToString(text));
  }

  void ErrorMessage(int textID)
  {
    ErrorMessageImpl(GetMsg(textID));
  }

  void InfoMessage(int titleID, char const* text)
  {
    return InfoMessageImpl(GetMsg(titleID), ToString(text));
  }

  void InfoMessage(int titleID, int textID)
  {
    return InfoMessageImpl(GetMsg(titleID), GetMsg(textID));
  }

  std::shared_ptr<void> LongOperationMessage(int titleID, int textID)
  {
    return LongOperationMessageImpl(GetMsg(titleID), GetMsg(textID));
  }
}
