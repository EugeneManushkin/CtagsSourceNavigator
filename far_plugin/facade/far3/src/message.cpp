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

  void Message(wchar_t const* text, int numButtons, FARMESSAGEFLAGS flags)
  {
    FarAPI().Message(GetPluginGuid(), &*ErrorMessageGuid, flags | FMSG_ALLINONE, nullptr, reinterpret_cast<const wchar_t *const *>(text), 0, numButtons);
  }

  void ErrorMessageImpl(WideString const& text)
  {
    WideString str = WideString(CTAGS_PRODUCT_NAME) + L"\n" + text + L"\nOk";
    Message(str.c_str(), 1, FMSG_WARNING);
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
}
