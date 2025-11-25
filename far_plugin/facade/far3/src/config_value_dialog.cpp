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

  enum class DialogResult
  {
    Ok,
    Cancel,
    Default,
  };

  class Dialog
  {
  public:
    Dialog(std::string const& value, ConfigFieldType type, int textId, PluginStartupInfo const& i, Guid const& pluginGuid);
    DialogResult Show();
    std::string GetValue() const;

  private:
    intptr_t Bottom() const;
    void PushCheckbox(std::string const& value, int textId);
    void PushSizebox(std::string const& value, int textId);
    void PushEditbox(std::string const& value, int textId);
    WideString GetText() const;
    std::string GetCheckboxValue() const;
    std::string GetSizeboxValue() const;
    std::string GetEditboxValue() const;

  private:
    ConfigFieldType const Type;
    PluginStartupInfo const& I;
    Guid const& PluginGuid;
    intptr_t CtrlId;
    intptr_t CancelId;
    intptr_t DefaultId;
    WideString Value;
    void* Handle;
    std::vector<FarDialogItem> Items;
  };

  Dialog::Dialog(std::string const& value, ConfigFieldType type, int textId, PluginStartupInfo const& i, Guid const& pluginGuid)
    : Type(type)
    , I(i)
    , PluginGuid(pluginGuid)
  {
    Items.push_back(MakeItem(DI_DOUBLEBOX, BoxPadding, Bottom(), Width - 1 - BoxPadding, 0));
    if (type == ConfigFieldType::Flag) PushCheckbox(value, textId);
    else if (type == ConfigFieldType::Size) PushSizebox(value, textId);
    else PushEditbox(value, textId);
    CtrlId = Items.size() - 1;
    Items.push_back(MakeItem(DI_TEXT, InnerPadding, Bottom(), Width - 1 - InnerPadding, 0, nullptr, DIF_SEPARATOR|DIF_BOXCOLOR));
    Items.push_back(MakeItem(DI_BUTTON, 0, Bottom(), 0, 0, I.GetMsg(&PluginGuid, MOk), DIF_CENTERGROUP));
    Items.push_back(MakeItem(DI_BUTTON, 0, Bottom() - 1, 0, 0, I.GetMsg(&PluginGuid, MDefault), DIF_CENTERGROUP));
    DefaultId = Items.size() - 1;
    Items.push_back(MakeItem(DI_BUTTON, 0, Bottom() - 1, 0, 0, I.GetMsg(&PluginGuid, MCancel), DIF_CENTERGROUP));
    CancelId = Items.size() - 1;
    Items.front().Y2 = Bottom();
    intptr_t const autocenterX = -1, autocenterY = -1;
    Handle = I.DialogInit(
      &PluginGuid,
      &DialogGuid,
      autocenterX,
      autocenterY,
      Width,
      Bottom() + 2,
      nullptr,
      Items.data(),
      Items.size(),
      0,
      FDLG_NONE,
      nullptr,
      nullptr
    );

    if (Handle == INVALID_HANDLE_VALUE)
      throw std::runtime_error("DialogInit: failed to create config value dialog");
  }

  DialogResult Dialog::Show()
  {
    auto const selected = I.DialogRun(Handle);
    return selected == -1 || selected == CancelId ? DialogResult::Cancel
         : selected == DefaultId ? DialogResult::Default
         : DialogResult::Ok;
  }

  std::string Dialog::GetValue() const
  {
    return Type == ConfigFieldType::Flag ? GetCheckboxValue()
         : Type == ConfigFieldType::Size ? GetSizeboxValue()
         : GetEditboxValue();
  }

  intptr_t Dialog::Bottom() const
  {
    return !Items.empty() ? Items.back().Y1 + 1 : 1;
  }

  void Dialog::PushCheckbox(std::string const& value, int textId)
  {
    Items.push_back(MakeItem(DI_CHECKBOX, InnerPadding, Bottom(), 0, 0, I.GetMsg(&PluginGuid, textId), DIF_FOCUS));
    Items.back().Selected = value == "true" ? 1 : 0;
  }

  void Dialog::PushSizebox(std::string const& value, int textId)
  {
    Value = ToString(value);
    Items.push_back(MakeItem(DI_TEXT, InnerPadding, Bottom(), 0, 0, I.GetMsg(&PluginGuid, textId)));
    Items.push_back(MakeItem(DI_FIXEDIT, InnerPadding, Bottom(), Width - 1 - InnerPadding, 0, Value.c_str(), DIF_MASKEDIT|DIF_FOCUS));
    Items.back().Mask = SizeboxMask;
  }

  void Dialog::PushEditbox(std::string const& value, int textId)
  {
    Value = ToString(value);
    Items.push_back(MakeItem(DI_TEXT, InnerPadding, Bottom(), 0, 0, I.GetMsg(&PluginGuid, textId)));
    Items.push_back(MakeItem(DI_EDIT, InnerPadding, Bottom(), Width - 1 - InnerPadding, 0, Value.c_str(), DIF_FOCUS));
  }

  WideString Dialog::GetText() const
  {
    FarDialogItemData item = {sizeof(FarDialogItemData)};
    item.PtrLength = I.SendDlgMessage(Handle, DM_GETTEXT, CtrlId, 0);
    std::vector<WideString::value_type> buf(item.PtrLength + 1);
    item.PtrData = buf.data();
    I.SendDlgMessage(Handle, DM_GETTEXT, CtrlId, &item);
    return WideString(item.PtrData, item.PtrLength);
  }

  std::string Dialog::GetCheckboxValue() const
  {
    return I.SendDlgMessage(Handle, DM_GETCHECK, CtrlId, 0) == BSTATE_CHECKED ? "true" : "false";
  }

  std::string Dialog::GetSizeboxValue() const
  {
    auto const text = GetText();
    return ToStdString(text.substr(0, text.find_first_not_of(L"0123456789")));
  }

  std::string Dialog::GetEditboxValue() const
  {
    return ToStdString(GetText());
  }

  class ConfigValueDialogImpl : public Far3::ConfigValueDialog
  {
  public:
    ConfigValueDialogImpl(PluginStartupInfo const& i, Guid const& pluginGuid);
    std::pair<bool, std::string> Show(std::string const& initValue, std::string const& defaultValue, ConfigFieldType type, int textId) const override;

  private:
    PluginStartupInfo const I;
    Guid const PluginGuid;
  };

  ConfigValueDialogImpl::ConfigValueDialogImpl(PluginStartupInfo const& i, Guid const& pluginGuid)
    : I(i)
    , PluginGuid(pluginGuid)
  {
  }

  std::pair<bool, std::string> ConfigValueDialogImpl::Show(std::string const& initValue, std::string const& defaultValue, ConfigFieldType type, int textId) const
  {
    auto result = DialogResult::Ok;
    do
    {
      Dialog dialog(result == DialogResult::Default ? defaultValue : initValue, type, textId, I, PluginGuid);
      result = dialog.Show();
      if (result == DialogResult::Ok)
        return std::make_pair(true, dialog.GetValue());
    }
    while(result == DialogResult::Default);
    return std::make_pair(false, "");
  }
}

namespace Far3
{
  std::unique_ptr<const ConfigValueDialog> ConfigValueDialog::Create(PluginStartupInfo const& i, Guid const& pluginGuid)
  {
    return std::unique_ptr<const ConfigValueDialog>(new ConfigValueDialogImpl(i, pluginGuid));
  }
}
