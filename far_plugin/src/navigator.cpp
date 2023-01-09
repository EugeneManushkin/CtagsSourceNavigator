#include <plugin/navigator.h>

#include <stack>
#include <stdexcept>

namespace
{
  bool IsValid(Plugin::EditorPosition const& pos)
  {
    return !pos.File.empty();
  }

  class NavigatorImpl : public Plugin::Navigator
  {
  public:
    using Index = Navigator::Index;
    static const size_t HistoryCapacity = 400;
    static const size_t HistoryCapacityPerEditor = 50;

    NavigatorImpl(std::shared_ptr<Plugin::CurrentEditor> const& currentEditor)
      : Editor(currentEditor)
      , SaveEnabled(true)
    {
      History.push(Plugin::EditorHistory::Create(HistoryCapacity));
    }

    void GoBack() override
    {
      bool cursorChanged = IsCursorChanged(Editor->GetPosition());
      if (CurrentHistoryIndex() > 0 || cursorChanged)
      {
        auto index = CurrentHistoryIndex() - (cursorChanged ? 0 : 1);
        auto curPos = index == HistorySize() - 1 ? Editor->GetPosition() : Plugin::EditorPosition();
        Goto(index);
        Top = IsValid(curPos) ? curPos : Top;
      }
    }

    bool CanGoBack() const override
    {
      return CurrentHistoryIndex() > 0 || IsCursorChanged(Editor->GetPosition());
    }

    void GoForward() override
    {
      if (CurrentHistoryIndex() < HistorySize() - 1)
      {
        Goto(CurrentHistoryIndex() + 1);
      }
      else if (IsValid(Top))
      {
        OpenAsync(Top);
        Top = Plugin::EditorPosition();
      }
    }

    bool CanGoForward() const override
    {
      return CurrentHistoryIndex() < HistorySize() - 1 || IsValid(Top);
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
        OpenAsync(newPos);
        PushPosition(std::move(curPos));
        PushPosition(Editor->GetPosition());
      }
      else
      {
        Editor->OpenModal(newPos);
      }
    }

    void OnNewEditor() override
    {
      if (Editor->IsModal())
        History.push(Plugin::EditorHistory::Create(HistoryCapacityPerEditor));

      SaveCurrentPosition();
    }

    void OnCloseEditor() override
    {
      if (Editor->IsModal())
      {
        if (History.size() <= 1)
          throw std::logic_error("Internal error: History.size() <= 1");

        History.pop();
      }
      else
      {
        SaveCurrentPosition();
      }
    }

  private:
    bool IsCursorChanged(Plugin::EditorPosition const& pos) const
    {
      auto const& hist = *History.top();
      auto const indexPos = hist.CurrentIndex() < hist.Size() ? hist.GetPosition(hist.CurrentIndex()) : Plugin::EditorPosition();
      return IsValid(pos)
          && indexPos.File == pos.File
          && indexPos.Line != pos.Line;
    }

    void PushPosition(Plugin::EditorPosition&& pos)
    {
      History.top()->PushPosition(std::move(pos));
      Top = Plugin::EditorPosition();
    }

    void SaveCurrentPosition()
    {
      if (SaveEnabled)
        PushPosition(Editor->GetPosition());
    }

    void OpenAsync(Plugin::EditorPosition const& pos)
    {
      SaveEnabled = false;
      auto enableSave = [this](void*){ SaveEnabled = true; };
      std::unique_ptr<void, decltype(enableSave)> saveLock(this, enableSave);
      Editor->OpenAsync(pos);
    }

    void Goto(Index index) try
    {
      OpenAsync(History.top()->GetPosition(index));
      History.top()->Goto(index);
    }
    catch(...)
    {
      History.top()->Erase(index);
      throw;
    }

    std::shared_ptr<Plugin::CurrentEditor> Editor;
    std::stack<std::unique_ptr<Plugin::EditorHistory>> History;
    bool SaveEnabled;
    Plugin::EditorPosition Top;
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

    void Open(Plugin::EditorPosition const& newPos) override try
    {
      CheckValid();
      Navigator->Open(newPos);
    }
    catch(std::bad_alloc const&)
    {
      Valid = false;
      throw;
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
