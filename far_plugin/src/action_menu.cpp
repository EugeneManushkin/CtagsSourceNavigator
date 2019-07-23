#include "action_menu.h"

#include <facade/menu.h>

namespace FarPlugin
{
  ActionMenu::ActionMenu(Callback&& cb)
    : Menu(Facade::Menu::Create())
    , DefaultCallback(std::move(cb))
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
    if (res < 0)
      return;

    if (DefaultCallback)
      DefaultCallback();

    if (Callbacks.at(res))
      Callbacks.at(res)();
  }
}
