#pragma once

#include <plugin/current_editor.h>
#include <plugin/editor_history.h>

#include <memory>

namespace Plugin
{
  class Navigator
  {
  public:
    using Index = EditorHistory::Index;

    static std::unique_ptr<Navigator> Create(std::shared_ptr<CurrentEditor> const& currentEditor);
    virtual ~Navigator() = default;
    virtual void GoBack() = 0;
    virtual bool CanGoBack() const = 0;
    virtual void GoForward() = 0;
    virtual bool CanGoForward() const = 0;
    virtual Index HistorySize() const = 0;
    virtual Index CurrentHistoryIndex() const = 0;
    virtual EditorPosition GetHistoryPosition(Index index) const = 0;
    virtual void Open(Plugin::EditorPosition const& newPos) = 0;
    virtual void OnNewEditor() = 0;
    virtual void OnCloseEditor() = 0;
  };
}
