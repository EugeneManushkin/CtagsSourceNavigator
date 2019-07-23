#include "action_menu.h"

#include <facade/menu.h>

namespace FarPlugin
{
  ActionMenu::ActionMenu()
    : Menu(Facade::Menu::Create())
  {
  }

  ActionMenu& ActionMenu::Add(char label, int textID, Callback&& cb, bool disabled)
  {
    Menu->Add(textID, label, disabled);
    Callbacks.push_back(std::move(cb));
    return *this;
  }

  ActionMenu& ActionMenu::Separator()
  {
    Menu->AddSeparator();
    return *this;
  }

  void ActionMenu::Run(int titleID, int selected)
  {
    auto res = Menu->Run(titleID, selected);
    if (res >= 0)
      Callbacks.at(res)();
  }
}
