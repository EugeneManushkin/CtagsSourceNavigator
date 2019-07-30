#include "tags_history.h"
#include "text.h"
#include "strings_cache.h"

#include <facade/menu.h>
#include <facade/message.h>

namespace
{
  int SelectMenu(std::vector<std::string> const& items, int titleID)
  {
    auto menu = Facade::Menu::Create();
    for (auto const& i : items)
      menu->Add(i.c_str(), 0, false);

    return menu->Run(titleID, -1);
  }

  int EmptyMenuMessage(int textID)
  {
    Facade::InfoMessage(textID, MPlugin);
    return -1;
  }
}

namespace FarPlugin
{
  std::string SelectFromTagsHistory(char const* historyFile, size_t capacity)
  {
    auto history = FarPlugin::LoadStringsCache(historyFile, capacity)->Get();
    auto selected = history.empty() ? EmptyMenuMessage(MHistoryEmpty) : SelectMenu(history, MTitleHistory);
    return selected < 0 ? std::string() : std::move(history.at(selected));
  }

  void AddToTagsHistory(char const* tagsFile, char const* historyFile, size_t capacity)
  {
    auto cache = FarPlugin::LoadStringsCache(historyFile, capacity);
    cache->Insert(tagsFile);
    FarPlugin::SaveStringsCache(*cache, historyFile);
  }
}
