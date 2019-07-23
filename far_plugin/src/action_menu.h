#include <memory>
#include <functional>
#include <vector>

namespace Facade
{
  class Menu;
}

namespace FarPlugin
{
  class ActionMenu
  {
  public:
    using Callback = std::function<void(void)>;

    ActionMenu(Callback&& cb = Callback());
    ActionMenu& Add(char label, int textID, Callback&& cb, bool disabled = false);
    ActionMenu& Separator();
    void Run(int titleID, int selected);

  private:
    std::unique_ptr<Facade::Menu> Menu;
    Callback DefaultCallback;
    std::vector<Callback> Callbacks;
  };
}
