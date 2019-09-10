#include "callbacks.h"
#include "string.h"

#include <facade/dialog.h>
#include <facade/message.h>
#include <plugin_sdk/plugin.hpp>

#include <algorithm>

using Facade::Internal::FarAPI;
using Facade::Internal::GetMsg;
using Facade::Internal::GetPluginGuid;
using Facade::Internal::StringToGuid;
using Facade::Internal::ToStdString;
using Facade::Internal::ToString;
using Facade::Internal::WideString;

namespace
{
  using Facade::Dialog;
  using Callback = Dialog::Callback;

  auto const InteractiveDialogGuid = StringToGuid("{fcd1e1b9-4060-4696-9e40-11f055c2909e}");
  int const MinDialogWidth = 20;

  WideString GetFarItemText(void* dialogHandle, intptr_t id)
  {
    FarDialogItemData item = {sizeof(FarDialogItemData)};
    item.PtrLength = FarAPI().SendDlgMessage(dialogHandle, DM_GETTEXT, id, 0);
    std::vector<wchar_t> buf(item.PtrLength + 1);
    item.PtrData = buf.data();
    FarAPI().SendDlgMessage(dialogHandle, DM_GETTEXT, id, &item);
    return WideString(item.PtrData, item.PtrLength);
  }

  bool GetFarItemCheck(void* dialogHandle, intptr_t id)
  {
    return FarAPI().SendDlgMessage(dialogHandle, DM_GETCHECK, id, 0) == BSTATE_CHECKED;
  }

  void SetFarItemText(void* dialogHandle, intptr_t id, WideString const& text)
  {
    FarDialogItemData item = {sizeof(FarDialogItemData), text.length(), const_cast<wchar_t*>(text.c_str())};
    FarAPI().SendDlgMessage(dialogHandle, DM_SETTEXT, id, &item);
  }

  void SetFarItemCheck(void* dialogHandle, intptr_t id, bool checked)
  {
    FarAPI().SendDlgMessage(dialogHandle, DM_SETCHECK, id, reinterpret_cast<void*>(intptr_t(checked ? BSTATE_CHECKED : BSTATE_UNCHECKED)));
  }

  std::string BoolToString(bool v)
  {
    return v ? "true" : "false";
  }

  bool StringToBool(std::string const& v)
  {
    return v == "true";
  }

  std::string GetFarItemValue(void* dialogHandle, intptr_t id, FARDIALOGITEMTYPES type)
  {
    return type == DI_CHECKBOX || type == DI_RADIOBUTTON ? BoolToString(GetFarItemCheck(dialogHandle, id)) : ToStdString(GetFarItemText(dialogHandle, id));
  }

  void SetFarItemValue(void* dialogHandle, intptr_t id, FARDIALOGITEMTYPES type, std::string const& value)
  {
    if (type == DI_CHECKBOX || type == DI_RADIOBUTTON)
      SetFarItemCheck(dialogHandle, id, StringToBool(value));
    else
      SetFarItemText(dialogHandle, id, ToString(value));
  }

  struct DialogItem
  {
    FARDIALOGITEMTYPES FarItemType;
    std::string ID;
    WideString Text;
    WideString Value;
    FARDIALOGITEMFLAGS Flags;
    Callback OnChanged;
  };

  FarDialogItem DialogItemToFarItem(DialogItem const& item)
  {
    intptr_t checked = item.FarItemType == DI_CHECKBOX && StringToBool(ToStdString(item.Value)) ? 1 : 0;
    wchar_t const* str = item.FarItemType == DI_EDIT ? item.Value.c_str() : item.Text.c_str();
    return {item.FarItemType, 0, 0, 0, 0, checked, nullptr, nullptr, item.Flags, str};
  }

  std::vector<FarDialogItem> GetFarItems(std::vector<DialogItem> const& items, intptr_t width)
  {
    std::vector<FarDialogItem> farItems;
    farItems.reserve(items.size());
    intptr_t y = 0;
    for (auto const& item : items)
    {
      farItems.push_back(DialogItemToFarItem(item));
      bool const onSameLine = farItems.back().Type == DI_BUTTON && farItems.size() > 1 && (farItems.rbegin() + 1)->Type == DI_BUTTON;
      farItems.back().X1 = 5;
      farItems.back().Y1 = onSameLine ? y : ++y;
      farItems.back().X2 = width - 4 - 2;
    }

    farItems.front().X1 = 3;
    farItems.front().Y1 = 1;
    farItems.front().X2 = width - 4;
    farItems.front().Y2 = ++y;
    return std::move(farItems);
  }

  class DialogImpl : public Dialog
  {
  public:
    DialogImpl(int width);
    Dialog& SetTitle(int textID, std::string const& id) override;
    Dialog& SetTitle(std::string const& text, std::string const& id) override;
    Dialog& AddCaption(int textID, std::string const& id, bool enabled) override;
    Dialog& AddEditbox(std::string const& value, std::string const& id, bool enabled, Callback cb) override;
    Dialog& AddCheckbox(int textID, std::string const& value, std::string const& id, bool enabled, Callback cb) override;
    Dialog& AddButton(int textID, std::string const& id, bool default, bool enabled, Callback cb) override;
    Dialog& AddSeparator() override;
    Dialog& SetOnIdle(Callback cb) override;
    std::unordered_map<std::string, std::string> Run() override;
    static intptr_t DlgProc(void* hDlg, intptr_t Msg, intptr_t Param1, void* Param2);

  private:
    class DialogControllerImpl : public Facade::DialogController
    {
    public:
      DialogControllerImpl(DialogImpl& dialog, void* dialogHandle);
      std::string GetValue(std::string const& id) const override;
      void SetValue(std::string const& id, std::string const& value) const override;
      void SetEnabled(std::string const& id, bool enabled) const override;

    private:
      DialogImpl& DialogInstance;
      void* DialogHandle;
    };

    std::string GetValue(void* dialogHandle, std::string const& id) const;
    void SetValue(void* dialogHandle, std::string const& id, std::string const& value);
    void SetEnabled(void* dialogHandle, std::string const& id, bool enabled);

    std::vector<DialogItem> Items;
    intptr_t Width;
    Callback OnIdle;
  };

  DialogImpl::DialogControllerImpl::DialogControllerImpl(DialogImpl& dialog, void* dialogHandle)
    : DialogInstance(dialog)
    , DialogHandle(dialogHandle)
  {
  }

  std::string DialogImpl::DialogControllerImpl::GetValue(std::string const& id) const
  {
    return DialogInstance.GetValue(DialogHandle, id);
  }

  void DialogImpl::DialogControllerImpl::SetValue(std::string const& id, std::string const& value) const
  {
    return DialogInstance.SetValue(DialogHandle, id, value);
  }

  void DialogImpl::DialogControllerImpl::SetEnabled(std::string const& id, bool enabled) const
  {
    DialogInstance.SetEnabled(DialogHandle, id, enabled);
  }

  DialogImpl::DialogImpl(int width)
    : Width(width < MinDialogWidth ? MinDialogWidth : width)
    , Items(1, {DI_DOUBLEBOX})
  {
  }

  Dialog& DialogImpl::SetTitle(int textID, std::string const& id)
  {
    Items.front() = {Items.front().FarItemType, id, GetMsg(textID), WideString(), 0, Callback()};
    return *this;
  }

  Dialog& DialogImpl::SetTitle(std::string const& text, std::string const& id)
  {
    Items.front() = {Items.front().FarItemType, id, ToString(text), WideString(), 0, Callback()};
    return *this;
  }

  Dialog& DialogImpl::AddCaption(int textID, std::string const& id, bool enabled)
  {
    Items.push_back({DI_TEXT, id, GetMsg(textID), WideString(), enabled ? 0 : DIF_DISABLE, Callback()});
    return *this;
  }

  Dialog& DialogImpl::AddEditbox(std::string const& value, std::string const& id, bool enabled, Callback cb)
  {
    Items.push_back({DI_EDIT, id, WideString(), ToString(value), enabled ? 0 : DIF_DISABLE, cb});
    return *this;
  }

  Dialog& DialogImpl::AddCheckbox(int textID, std::string const& value, std::string const& id, bool enabled, Callback cb)
  {
    Items.push_back({DI_CHECKBOX, id, GetMsg(textID), ToString(value), enabled ? 0 : DIF_DISABLE, cb});
    return *this;
  }

  Dialog& DialogImpl::AddButton(int textID, std::string const& id, bool default, bool enabled, Callback cb)
  {
    Items.push_back({DI_BUTTON, id, GetMsg(textID), WideString(), DIF_CENTERGROUP | (default ? DIF_DEFAULTBUTTON : 0) | (enabled ? 0 : DIF_DISABLE), cb});
    return *this;
  }

  Dialog& DialogImpl::AddSeparator()
  {
    Items.push_back({DI_TEXT, "", WideString(), WideString(), DIF_SEPARATOR | DIF_BOXCOLOR, Callback()});
    return *this;
  }

  Dialog& DialogImpl::SetOnIdle(Callback cb)
  {
    OnIdle = cb;
    return *this;
  }

  intptr_t DialogImpl::DlgProc(void* hDlg, intptr_t Msg, intptr_t Param1, void* Param2)
  {
    DialogImpl* dialog = reinterpret_cast<DialogImpl*>(FarAPI().SendDlgMessage(hDlg, DM_GETDLGDATA, 0, nullptr));
    if (Msg == DN_ENTERIDLE && dialog->OnIdle)
    {
      dialog->OnIdle(DialogControllerImpl(*dialog, hDlg));
    }

    if ((Msg == DN_BTNCLICK || Msg == DN_EDITCHANGE) && dialog->Items[Param1].OnChanged)
    {
      dialog->Items[Param1].OnChanged(DialogControllerImpl(*dialog, hDlg));
    }

    return FarAPI().DefDlgProc(hDlg, Msg, Param1, Param2);
  }

  std::unordered_map<std::string, std::string> DialogImpl::Run()
  {
    std::vector<FarDialogItem> farItems = GetFarItems(Items, Width);
    auto handle = FarAPI().DialogInit(GetPluginGuid(), &*InteractiveDialogGuid, -1, -1, Width, farItems.front().Y2 + 2, L"", &farItems[0], farItems.size(), 0, FDLG_NONE, &DialogImpl::DlgProc, this);
    std::shared_ptr<void> handleHolder(handle, [](void* h){FarAPI().DialogFree(h);});
    auto exitCode = FarAPI().DialogRun(handle);
    std::unordered_map<std::string, std::string> result;   
    intptr_t id = 0;
    for (auto i = Items.cbegin(); i != Items.cend(); ++i, ++id)
    {
      if (!i->ID.empty())
        result[i->ID] = i->FarItemType == DI_BUTTON ? BoolToString(exitCode == id) : GetFarItemValue(handle, id, i->FarItemType);
    }

    return std::move(result);
  }

  std::string DialogImpl::GetValue(void* dialogHandle, std::string const& id) const
  {
    auto item = std::find_if(Items.cbegin(), Items.cend(), [&id](DialogItem const& i) {return i.ID == id;});
    return item != Items.cend() ? GetFarItemValue(dialogHandle, std::distance(Items.cbegin(), item), item->FarItemType) : "";
  }

  void DialogImpl::SetValue(void* dialogHandle, std::string const& id, std::string const& value)
  {
    auto item = std::find_if(Items.cbegin(), Items.cend(), [&id](DialogItem const& i) {return i.ID == id;});
    if (item != Items.cend())
      SetFarItemValue(dialogHandle, std::distance(Items.cbegin(), item), item->FarItemType, value);
  }

  void DialogImpl::SetEnabled(void* dialogHandle, std::string const& id, bool enabled)
  {
    auto item = std::find_if(Items.cbegin(), Items.cend(), [&id](DialogItem const& i) {return i.ID == id;});
    if (item != Items.cend())
      FarAPI().SendDlgMessage(dialogHandle, DM_ENABLE, std::distance(Items.cbegin(), item), reinterpret_cast<void*>(intptr_t(enabled ? 1 : 0)));
  }
}

namespace Facade
{
  std::unique_ptr<Dialog> Dialog::Create(int width)
  {
    return std::unique_ptr<Dialog>(new DialogImpl(width));
  }
}
