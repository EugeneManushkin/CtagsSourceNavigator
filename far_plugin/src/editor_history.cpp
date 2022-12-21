#include <plugin/editor_history.h>

#include <deque>

namespace
{
  class EditorHistoryImpl : public Plugin::EditorHistory
  {
  public:
    using Index = Plugin::EditorHistory::Index;

    EditorHistoryImpl(size_t capacity)
      : Current(Stack.size())
      , Capacity(capacity)
    {
    }

    Index Size() const override
    {
      return Stack.size();
    }

    Index CurrentIndex() const override
    {
      return Current;
    }

    Plugin::EditorPosition GetPosition(Index index) const override
    {
      return Stack.at(index);
    }

    void PushPosition(Plugin::EditorPosition&& pos) override
    {
      Stack.resize(!Stack.empty() ? Current + 1 : 0);
      if (!pos.File.empty() && pos != (!Stack.empty() ? Stack.back() : Plugin::EditorPosition()))
      {
        while (Capacity > 0 && Stack.size() >= Capacity)
          Stack.pop_front();

        Stack.push_back(std::move(pos));
        Current = Stack.size() - 1;
      }
    }

    Plugin::EditorPosition Goto(Index index) override
    {
      auto result = Stack.at(index);
      Current = index;
      return result;
    }

    void Erase(Index index) override
    {
      Stack.erase(Stack.begin() + index);
      Current -= Current < index || !Current ? 0 : 1;
    }

  private:
    std::deque<Plugin::EditorPosition> Stack;
    Index Current;
    size_t Capacity;
  };
}

namespace Plugin
{
  std::unique_ptr<EditorHistory> EditorHistory::Create(size_t capacity)
  {
    return std::unique_ptr<EditorHistory>(new EditorHistoryImpl(capacity));
  }
}
