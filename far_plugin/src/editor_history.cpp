#include <plugin/editor_history.h>

#include <deque>

namespace
{
  class EditorHistoryImpl : public Plugin::EditorHistory
  {
  public:
    using Index = Plugin::EditorHistory::Index;

    EditorHistoryImpl()
      : Current(Stack.size())
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

  private:
    std::deque<Plugin::EditorPosition> Stack;
    Index Current;
  };
}

namespace Plugin
{
  std::unique_ptr<EditorHistory> EditorHistory::Create()
  {
    return std::unique_ptr<EditorHistory>(new EditorHistoryImpl);
  }
}
