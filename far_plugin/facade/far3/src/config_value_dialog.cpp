#include <far3/config_value_dialog.h>
#include <far3/guid.h>
#include <far3/plugin_sdk/api.h>
#include <far3/text.h>
#include <far3/wide_string.h>

#include <plugin/config_data_mapper.h>

#include <stdexcept>
#include <vector>

namespace
{
  using Far3::Guid;
  using Far3::ToString;
  using Far3::ToStdString;
  using Far3::WideString;
  using Plugin::ConfigFieldType;
  auto const DialogGuid = Guid("{d4045c52-95e5-4cb2-b04a-2c2e21e9315a}");
  intptr_t const Width = 68;
  intptr_t const InnerPadding = 5;
  intptr_t const BoxPadding = 3;
  wchar_t const* const SizeboxMask = L"999999";

  FarDialogItem MakeItem(
      FARDIALOGITEMTYPES type
    , intptr_t left
    , intptr_t top
    , intptr_t right
    , intptr_t bottom
    , WideString::value_type const* text = nullptr
    , FARDIALOGITEMFLAGS flags = 0
  )
  {
    FarDialogItem result{type, left, top, right, bottom};
    result.Data = text;
    result.Flags = flags;
    return result;
  }

  class ConfigValueDialogImpl : public Far3::ConfigValueDialog
  {
  public:
    ConfigValueDialogImpl(PluginStartupInfo const& i, Guid const& pluginGuid);
    std::pair<bool, std::string> Show(std::string const& initValue, ConfigFieldType type, int textId) const override;

  private:
    intptr_t PushCheckbox(std::string const& value, int textId, intptr_t& y, std::vector<FarDialogItem>& items) const;
    intptr_t PushSizebox(WideString::value_type const* value, int textId, intptr_t& y, std::vector<FarDialogItem>& items) const;
    intptr_t PushEditbox(WideString::value_type const* value, int textId, intptr_t& y, std::vector<FarDialogItem>& items) const;
    WideString GetText(intptr_t ctrlId, void* handle) const;
    std::string GetCheckboxValue(intptr_t ctrlId, void* handle) const;
    std::string GetSizeboxValue(intptr_t ctrlId, void* handle) const;
    std::string GetEditboxValue(intptr_t ctrlId, void* handle) const;

  private:
    PluginStartupInfo const I;
    Guid const PluginGuid;
  };

  ConfigValueDialogImpl::ConfigValueDialogImpl(PluginStartupInfo const& i, Guid const& pluginGuid)
    : I(i)
    , PluginGuid(pluginGuid)
  {
  }

  std::pair<bool, std::string> ConfigValueDialogImpl::Show(std::string const& initValue, ConfigFieldType type, int textId) const
  {
    intptr_t y = 0;
    std::vector<FarDialogItem> items;
    items.push_back(MakeItem(DI_DOUBLEBOX, BoxPadding, ++y, Width - 1 - BoxPadding, 0));
    auto init = type != ConfigFieldType::Flag ? ToString(initValue) : WideString();
    intptr_t ctrlId = type == ConfigFieldType::Flag ? PushCheckbox(initValue, textId, y, items)
                    : type == ConfigFieldType::Size ? PushSizebox(init.c_str(), textId, y, items)
                    : PushEditbox(init.c_str(), textId, y, items);
    items.push_back(MakeItem(DI_TEXT, InnerPadding, ++y, Width - 1 - InnerPadding, 0, nullptr, DIF_SEPARATOR|DIF_BOXCOLOR));
    items.push_back(MakeItem(DI_BUTTON, 0, ++y, 0, 0, I.GetMsg(&PluginGuid, MOk), DIF_CENTERGROUP));
    items.push_back(MakeItem(DI_BUTTON, 0, y, 0, 0, I.GetMsg(&PluginGuid, MCancel), DIF_CENTERGROUP));
    intptr_t const cancelIndex = items.size() - 1;
    items.front().Y2 = ++y;

    intptr_t const autocenterX = -1, autocenterY = -1;
    auto handle = I.DialogInit(
      &PluginGuid,
      &DialogGuid,
      autocenterX,
      autocenterY,
      Width,
      y + 2,
      nullptr,
      items.data(),
      items.size(),
      0,
      FDLG_NONE,
      nullptr,
      nullptr
    );

    if (handle == INVALID_HANDLE_VALUE)
      throw std::runtime_error("DialogInit: failed to create config value dialog");

    auto const selected = I.DialogRun(handle);
    if (selected == -1 || selected == cancelIndex)
      return std::make_pair(false, "");

    std::string result = type == ConfigFieldType::Flag ? GetCheckboxValue(ctrlId, handle)
                       : type == ConfigFieldType::Size ? GetSizeboxValue(ctrlId, handle)
                       : GetEditboxValue(ctrlId, handle);
    return std::make_pair(true, std::move(result));
  }

  intptr_t ConfigValueDialogImpl::PushCheckbox(std::string const& value, int textId, intptr_t& y, std::vector<FarDialogItem>& items) const
  {
    items.push_back(MakeItem(DI_CHECKBOX, InnerPadding, ++y, 0, 0, I.GetMsg(&PluginGuid, textId), DIF_FOCUS));
    items.back().Selected = value == "true" ? 1 : 0;
    return items.size() - 1;
  }

  intptr_t ConfigValueDialogImpl::PushSizebox(WideString::value_type const* value, int textId, intptr_t& y, std::vector<FarDialogItem>& items) const
  {
    items.push_back(MakeItem(DI_TEXT, InnerPadding, ++y, 0, 0, I.GetMsg(&PluginGuid, textId)));
    items.push_back(MakeItem(DI_FIXEDIT, InnerPadding, ++y, Width - 1 - InnerPadding, 0, value, DIF_MASKEDIT|DIF_FOCUS));
    items.back().Mask = SizeboxMask;
    return items.size() - 1;
  }

  intptr_t ConfigValueDialogImpl::PushEditbox(WideString::value_type const* value, int textId, intptr_t& y, std::vector<FarDialogItem>& items) const
  {
    items.push_back(MakeItem(DI_TEXT, InnerPadding, ++y, 0, 0, I.GetMsg(&PluginGuid, textId)));
    items.push_back(MakeItem(DI_EDIT, InnerPadding, ++y, Width - 1 - InnerPadding, 0, value, DIF_FOCUS));
    return items.size() - 1;
  }

  WideString ConfigValueDialogImpl::GetText(intptr_t ctrlId, void* handle) const
  {
    FarDialogItemData item = {sizeof(FarDialogItemData)};
    item.PtrLength = I.SendDlgMessage(handle, DM_GETTEXT, ctrlId, 0);
    std::vector<WideString::value_type> buf(item.PtrLength + 1);
    item.PtrData = buf.data();
    I.SendDlgMessage(handle, DM_GETTEXT, ctrlId, &item);
    return WideString(item.PtrData, item.PtrLength);
  }

  std::string ConfigValueDialogImpl::GetCheckboxValue(intptr_t ctrlId, void* handle) const
  {
    return I.SendDlgMessage(handle, DM_GETCHECK, ctrlId, 0) == BSTATE_CHECKED ? "true" : "false";
  }

  std::string ConfigValueDialogImpl::GetSizeboxValue(intptr_t ctrlId, void* handle) const
  {
    auto text = GetText(ctrlId, handle);
    return ToStdString(text.substr(0, text.find_first_not_of(L"0123456789")));
  }

  std::string ConfigValueDialogImpl::GetEditboxValue(intptr_t ctrlId, void* handle) const
  {
    return ToStdString(GetText(ctrlId, handle));
  }
}

namespace Far3
{
  std::unique_ptr<const ConfigValueDialog> ConfigValueDialog::Create(PluginStartupInfo const& i, Guid const& pluginGuid)
  {
    return std::unique_ptr<const ConfigValueDialog>(new ConfigValueDialogImpl(i, pluginGuid));
  }
}
