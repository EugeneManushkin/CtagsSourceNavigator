#include "callbacks.h"
#include "string.h"

#include <facade/dialog.h>
#include <facade/message.h>
#define NOMINMAX
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
  int const DialogBorder = 3;
  int const DialogVerBorder = 1;
  int const InnerIdent = 1;
  int const FrameThick = 1;

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

  std::string FitToWidth(std::string const& str, int width)
  {
    std::string result;
    for (size_t cur = 0, next = 0; cur < str.size(); cur = next)
    {
      result += result.empty() ? "" : "\n";
      auto bound = std::min(cur + width, str.size());
      next = std::find(str.begin() + cur, str.begin() + bound, '\n') - str.begin();
      result += str.substr(cur, next - cur) + std::string(bound - next, ' ');
      next += next < str.size() && (str[next] == ' ' || str[next] == '\n' || str[next] == '\t') ? 1 : 0;
    }

    return std::move(result);
  }

  struct Rect
  {
    int Left, Top, Width, Height;
  };

  SMALL_RECT ToFarItemRect(Rect const& rect)
  {
    return {static_cast<short>(rect.Left), static_cast<short>(rect.Top), static_cast<short>(rect.Left + rect.Width - 1), static_cast<short>(rect.Top + rect.Height - 1)};
  }

  void SetFarItemRect(void* dialogHandle, intptr_t id, Rect const& rect)
  {
    SMALL_RECT r = ToFarItemRect(rect);
    FarAPI().SendDlgMessage(dialogHandle, DM_SETITEMPOSITION, id, &r);
  }

  void ResizeFarDialog(void* dialogHandle, int width, int height)
  {
    COORD sz = {static_cast<short>(width), static_cast<short>(height)};
    FarAPI().SendDlgMessage(dialogHandle, DM_RESIZEDIALOG, 0, &sz);
    COORD c = {-1, -1};
    FarAPI().SendDlgMessage(dialogHandle, DM_MOVEDIALOG, 1, &c);
  }

  struct DialogItem
  {
    FARDIALOGITEMTYPES FarItemType;
    std::string ID;
    WideString Data;
    std::string Value;
    FARDIALOGITEMFLAGS Flags;
    Callback OnChanged;
    Rect Position;
  };

  FarDialogItem ToFarDialogItem(DialogItem const& item)
  {
    auto farRect = ToFarItemRect(item.Position);
    return {item.FarItemType, farRect.Left, farRect.Top, farRect.Right, farRect.Bottom, 0, nullptr, nullptr, item.Flags, item.Data.c_str()};
  }

  class DialogImpl : public Dialog
  {
  public:
    DialogImpl(int width);
    Dialog& SetTitle(int textID, std::string const& id) override;
    Dialog& SetTitle(std::string const& text, std::string const& id) override;
    Dialog& AddCaption(int textID, std::string const& id, bool enabled) override;
    Dialog& AddCaption(std::string const& text, std::string const& id, bool enabled) override;
    Dialog& AddEditbox(std::string const& value, std::string const& id, bool enabled, Callback cb) override;
    Dialog& AddCheckbox(int textID, bool value, std::string const& id, bool enabled, Callback cb) override;
    Dialog& AddButton(int textID, std::string const& id, bool defaultButton, bool noclose, bool enabled, Callback cb) override;
    Dialog& AddSeparator() override;
    Dialog& SetOnIdle(Callback cb) override;
    std::unordered_map<std::string, std::string> Run() override;
    intptr_t DlgProc(void* hDlg, intptr_t Msg, intptr_t Param1, void* Param2);

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

    void Align();
    Dialog& AddItem(FARDIALOGITEMTYPES type, std::string const& id, bool enabled, std::string const& value = "", FARDIALOGITEMFLAGS flags = 0, Callback cb = Callback(), WideString const& data = L"");
    std::vector<DialogItem>::const_iterator GetItemByID(std::string const& id) const;
    std::string GetValue(void* dialogHandle, std::string const& id) const;
    void SetValue(void* dialogHandle, std::string const& id, std::string const& value);
    void SetEnabled(void* dialogHandle, std::string const& id, bool enabled);
    void SetValue(std::vector<DialogItem>::iterator i, std::string const& value);
    void ResizeItem(std::vector<DialogItem>::iterator i, int height);
    void ApplyValues(void* dialogHandle);
    void ApplyPosition(void* dialogHandle);

    std::vector<DialogItem> Items;
    int Width;
    int Height;
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
    : Items(1, {DI_DOUBLEBOX})
    , Width(std::max(width, MinDialogWidth))
  {
    Align();
  }

  Dialog& DialogImpl::SetTitle(int textID, std::string const& id)
  {
    Items.front().ID = id;
    Items.front().Value = ToStdString(GetMsg(textID));
    return *this;
  }

  Dialog& DialogImpl::SetTitle(std::string const& text, std::string const& id)
  {
    Items.front().ID = id;
    Items.front().Value = text;
    return *this;
  }

  Dialog& DialogImpl::AddCaption(int textID, std::string const& id, bool enabled)
  {
    return AddItem(DI_TEXT, id, enabled, ToStdString(GetMsg(textID)), DIF_WORDWRAP);
  }

  Dialog& DialogImpl::AddCaption(std::string const& text, std::string const& id, bool enabled)
  {
    return AddItem(DI_TEXT, id, enabled, text, DIF_WORDWRAP);
  }

  Dialog& DialogImpl::AddEditbox(std::string const& value, std::string const& id, bool enabled, Callback cb)
  {
    return AddItem(DI_EDIT, id, enabled, value, 0, cb);
  }

  Dialog& DialogImpl::AddCheckbox(int textID, bool value, std::string const& id, bool enabled, Callback cb)
  {
    return AddItem(DI_CHECKBOX, id, enabled, BoolToString(value), 0, cb, GetMsg(textID));
  }

  Dialog& DialogImpl::AddButton(int textID, std::string const& id, bool defaultButton, bool noclose, bool enabled, Callback cb)
  {
    return AddItem(DI_BUTTON, id, enabled, ToStdString(GetMsg(textID)), DIF_CENTERGROUP | (defaultButton ? DIF_DEFAULTBUTTON : 0) | (noclose ? DIF_BTNNOCLOSE : 0), cb);
  }

  Dialog& DialogImpl::AddSeparator()
  {
    return AddItem(DI_TEXT, "", true, "", DIF_SEPARATOR | DIF_BOXCOLOR);
  }

  Dialog& DialogImpl::SetOnIdle(Callback cb)
  {
    OnIdle = cb;
    return *this;
  }

  intptr_t DialogImpl::DlgProc(void* hDlg, intptr_t Msg, intptr_t Param1, void* Param2)
  {
    if (Msg == DN_ENTERIDLE && OnIdle)
    {
      OnIdle(DialogControllerImpl(*this, hDlg));
    }

    if ((Msg == DN_BTNCLICK || Msg == DN_EDITCHANGE) && Items[Param1].OnChanged)
    {
      Items[Param1].OnChanged(DialogControllerImpl(*this, hDlg));
    }

    return FarAPI().DefDlgProc(hDlg, Msg, Param1, Param2);
  }

  intptr_t WINAPI FarDlgProc(void* hDlg, intptr_t Msg, intptr_t Param1, void* Param2)
  {
    return reinterpret_cast<DialogImpl*>(FarAPI().SendDlgMessage(hDlg, DM_GETDLGDATA, 0, nullptr))->DlgProc(hDlg, Msg, Param1, Param2);
  }

  std::unordered_map<std::string, std::string> DialogImpl::Run()
  {
    std::vector<FarDialogItem> farItems(Items.size());
    std::transform(Items.begin(), Items.end(), farItems.begin(), ToFarDialogItem);
    auto handle = FarAPI().DialogInit(GetPluginGuid(), &*InteractiveDialogGuid, -1, -1, Width, Height, L"", &farItems[0], farItems.size(), 0, FDLG_NONE, &FarDlgProc, this);
    std::shared_ptr<void> handleHolder(handle, [](void* h){FarAPI().DialogFree(h);});
    ApplyValues(handle);
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

  void DialogImpl::Align()
  {
    int lastItemBottom = Items.size() > 1 ? Items.back().Position.Top + Items.back().Position.Height : DialogVerBorder + 1;
    Items.front().Position = {DialogBorder, DialogVerBorder, Width - 2 * DialogBorder, lastItemBottom - DialogVerBorder + 1 };
    Height = Items.front().Position.Top + Items.front().Position.Height + DialogVerBorder;
  }

  Dialog& DialogImpl::AddItem(FARDIALOGITEMTYPES type, std::string const& id, bool enabled, std::string const& value, FARDIALOGITEMFLAGS flags, Callback cb, WideString const& data)
  {
    int const ident = DialogBorder + InnerIdent + FrameThick;
    bool onSameLine = type == DI_BUTTON && type == Items.back().FarItemType;
    int lastItemBottom = Items.size() > 1 ? Items.back().Position.Top + Items.back().Position.Height : Items.front().Position.Top + 1;
    Rect position = {ident, onSameLine ? Items.back().Position.Top : lastItemBottom, Width - 2 * ident, 1};
    Items.push_back({type, id, data, "", (enabled ? 0 : DIF_DISABLE) | flags, cb, position});
    SetValue(--Items.end(), value);
    return *this;
  }

  std::vector<DialogItem>::const_iterator DialogImpl::GetItemByID(std::string const& id) const
  {
    auto res = std::find_if(Items.begin(), Items.end(), [&id](DialogItem const& i) {return i.ID == id;});
    if (res == Items.end())
      throw std::logic_error("Item not found: " + id);

    return res;
  }

  std::string DialogImpl::GetValue(void* dialogHandle, std::string const& id) const
  {
    auto item = GetItemByID(id);
    return GetFarItemValue(dialogHandle, std::distance(Items.cbegin(), item), item->FarItemType);
  }

  void DialogImpl::SetValue(void* dialogHandle, std::string const& id, std::string const& value)
  {
    auto item = Items.begin() + std::distance(Items.cbegin(), GetItemByID(id));
    SetValue(item, value);
    ApplyPosition(dialogHandle);
    SetFarItemValue(dialogHandle, std::distance(Items.begin(), item), item->FarItemType, item->Value);
  }

  void DialogImpl::SetEnabled(void* dialogHandle, std::string const& id, bool enabled)
  {
    FarAPI().SendDlgMessage(dialogHandle, DM_ENABLE, std::distance(Items.cbegin(), GetItemByID(id)), reinterpret_cast<void*>(intptr_t(enabled ? 1 : 0)));
  }

  void DialogImpl::ResizeItem(std::vector<DialogItem>::iterator i, int height)
  {
    int const dh = height - i->Position.Height;
    i->Position.Height = height;
    for (++i; i != Items.cend(); ++i)
      i->Position.Top += dh;

    Align();
  }

  void DialogImpl::SetValue(std::vector<DialogItem>::iterator i, std::string const& value)
  {
    i->Value = i->FarItemType == DI_TEXT ? FitToWidth(value, i->Position.Width) : value;
    auto height = i->FarItemType == DI_TEXT ? static_cast<int>(std::count(i->Value.cbegin(), i->Value.cend(), '\n') + 1) : 1;
    ResizeItem(i, height);
  }

  void DialogImpl::ApplyValues(void* dialogHandle)
  {
    for (auto i = Items.begin(); i != Items.end(); ++i)
      SetFarItemValue(dialogHandle, std::distance(Items.begin(), i), i->FarItemType, i->Value);
  }

  void DialogImpl::ApplyPosition(void* dialogHandle)
  {
    for (auto i = Items.cbegin(); i != Items.cend(); ++i)
      SetFarItemRect(dialogHandle, std::distance(Items.cbegin(), i), i->Position);

    ResizeFarDialog(dialogHandle, Width, Height);
  }
}

namespace Facade
{
  std::unique_ptr<Dialog> Dialog::Create(int width)
  {
    return std::unique_ptr<Dialog>(new DialogImpl(width));
  }
}
