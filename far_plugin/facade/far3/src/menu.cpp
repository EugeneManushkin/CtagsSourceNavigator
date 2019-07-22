#include "callbacks.h"
#include "string.h"

#include <facade/menu.h>
#include <plugin_sdk/plugin.hpp>

#include <vector>
#include <utility>

using Facade::Internal::WideString;
using Facade::Internal::FarAPI;
using Facade::Internal::ToString;

namespace
{
  // TODO: refactor duplicated code
  GUID StrToGuid(char const* str)
  {
    GUID result;
    Facade::Internal::StringToGuid(str, result);
    return result;
  }

  GUID const PluginGuid = StrToGuid(Facade::Internal::GetPluginGuid());
  GUID const CtagsMenuGuid = StrToGuid("{7f125c0d-5e18-4b7f-a6df-1caae013c48f}");

  // TODO: refactor duplicated code
  wchar_t const* GetMsg(int textID)
  {
    return FarAPI().GetMsg(&PluginGuid, textID);
  }

  WideString MakeLabeledText(WideString const& text, WideString::value_type label)
  {
    return WideString(label == ' ' ? L"" : L"&") + WideString(1, label) + L" " + text;
  }

  class MenuImpl : public Facade::Menu
  {
  public:
    void Add(char const* text, char label, bool disabled) override
    {
      Add(ToString(text), label, disabled);
    }

    void Add(int textID, char label, bool disabled) override
    {
      Add(GetMsg(textID), label, disabled);
    }

    void AddSeparator() override
    {
      Items.push_back(std::make_pair(WideString(), MIF_SEPARATOR));
    }

    int Run(char const* title, int selected) override
    {
      return Run(ToString(title).c_str(), selected);
    }

    int Run(int titleID, int selected) override
    {
      return Run(GetMsg(titleID), selected);
    }

  private:
    int Run(wchar_t const* title, int selected)
    {
      std::vector<FarMenuItem> farItems;
      farItems.reserve(Items.size());
      intptr_t counter = 0;
      for (auto const& i : Items)
      {
        farItems.push_back({i.second | (counter == selected ? MIF_SELECTED : MIF_NONE), i.first.c_str()});
        farItems.back().UserData = !(i.second & MIF_SEPARATOR) ? counter : -1;
        counter += !(i.second & MIF_SEPARATOR) ? 1 : 0;
      }

      auto res = Facade::Internal::FarAPI().Menu(&PluginGuid, &CtagsMenuGuid, -1, -1, 0, FMENU_WRAPMODE, title, L"", L"", nullptr, nullptr, &farItems[0], farItems.size());
      return res == -1 ? -1 : static_cast<int>(farItems.at(res).UserData);
    }

    void Add(WideString&& text, char label, bool disabled)
    {
      Items.push_back(std::make_pair(!label ? std::move(text) : MakeLabeledText(text, label), disabled ? MIF_DISABLE | MIF_GRAYED : 0));
    }

    std::vector<std::pair<WideString, MENUITEMFLAGS> > Items;
  };
}

namespace Facade
{
  std::unique_ptr<Menu> Menu::Create()
  {
    return std::unique_ptr<Menu>(new MenuImpl);
  }
}
