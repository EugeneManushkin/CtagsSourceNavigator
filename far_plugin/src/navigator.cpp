#include <plugin/navigator.h>

#include <stack>
#include <stdexcept>

namespace
{
  class NavigatorImpl : public Plugin::Navigator
  {
  public:
    using Index = Navigator::Index;

    NavigatorImpl(std::shared_ptr<Plugin::CurrentEditor> const& currentEditor)
      : Editor(currentEditor)
    {
      History.push(Plugin::EditorHistory::Create());
    }

    void GoBack() override
    {
      if (CanGoBack())
        Goto(History.top()->CurrentIndex() - 1);
    }

    bool CanGoBack() const override
    {
      return History.top()->CurrentIndex() > 0;
    }

    void GoForward() override
    {
      if (CanGoForward())
        Goto(History.top()->CurrentIndex() + 1);
    }

    bool CanGoForward() const override
    {
      return History.top()->CurrentIndex() < History.top()->Size() - 1;
    }

    Index HistorySize() const override
    {
      return History.top()->Size();
    }

    Index CurrentHistoryIndex() const override
    {
      return History.top()->CurrentIndex();
    }

    Plugin::EditorPosition GetHistoryPosition(Index index) const override
    {
      return History.top()->GetPosition(index);
    }

    void Open(Plugin::EditorPosition const& newPos) override
    {
      if (!Editor->IsModal() || Editor->IsOpened(newPos.File.c_str()))
      {
        auto curPos = Editor->GetPosition();
        Editor->OpenAsync(newPos);
        History.top()->PushPosition(std::move(curPos));
      }
      else
      {
        Editor->OpenModal(newPos);
      }
    }

    void OnNewEditor() override
    {
      if (Editor->IsModal())
        History.push(Plugin::EditorHistory::Create());
    }

    void OnCloseEditor() override
    {
      if (Editor->IsModal())
      {
        if (History.size() <= 1)
          throw std::logic_error("Internal error: History.size() <= 1");

        History.pop();
      }
    }

  private:
    void Goto(Index index)
    {
      auto curPos = Editor->GetPosition();
      auto newPos = History.top()->GetPosition(index);
      Editor->OpenAsync(newPos);
      History.top()->Goto(index, std::move(curPos));
    }

    std::shared_ptr<Plugin::CurrentEditor> Editor;
    std::stack<std::unique_ptr<Plugin::EditorHistory>> History;
  };

  class CheckedNavigator : public Plugin::Navigator
  {
  public:
    CheckedNavigator(std::unique_ptr<Plugin::Navigator>&& navigator)
      : Navigator(std::move(navigator))
      , Valid(true)
    {
    }

    void GoBack() override
    {
      CheckValid();
      Navigator->GoBack();
    }

    bool CanGoBack() const override
    {
      CheckValid();
      return Navigator->CanGoBack();
    }

    void GoForward() override
    {
      CheckValid();
      Navigator->GoForward();
    }

    bool CanGoForward() const override
    {
      CheckValid();
      return Navigator->CanGoForward();
    }

    Index HistorySize() const override
    {
      CheckValid();
      return Navigator->HistorySize();
    }

    Index CurrentHistoryIndex() const override
    {
      CheckValid();
      return Navigator->CurrentHistoryIndex();
    }

    Plugin::EditorPosition GetHistoryPosition(Index index) const override
    {
      CheckValid();
      return Navigator->GetHistoryPosition(index);
    }

    void Open(Plugin::EditorPosition const& newPos) override
    {
      CheckValid();
      Navigator->Open(newPos);
    }

    void OnNewEditor() override try
    {
      CheckValid();
      Navigator->OnNewEditor();
    }
    catch(...)
    {
      Valid = false;
      throw;
    }

    void OnCloseEditor() override try
    {
      CheckValid();
      Navigator->OnCloseEditor();
    }
    catch(...)
    {
      Valid = false;
      throw;
    }

  private:
    void CheckValid() const
    {
      if (!Valid)
        throw std::runtime_error("Navigator is invalid. Please restart plugin.");
    }

    std::unique_ptr<Plugin::Navigator> Navigator;
    bool Valid;
  };
}

namespace Plugin
{
  std::unique_ptr<Navigator> Navigator::Create(std::shared_ptr<CurrentEditor> const& currentEditor)
  {
    std::unique_ptr<Navigator> navigator(new NavigatorImpl(currentEditor));
    return std::unique_ptr<Navigator>(new CheckedNavigator(std::move(navigator)));
  }
}
