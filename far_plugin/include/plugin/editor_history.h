#pragma once

#include <plugin/current_editor.h>

#include <memory>
#include <string>

namespace Plugin
{
  class EditorHistory
  {
  public:
    using Index = size_t;

    static std::unique_ptr<EditorHistory> Create(size_t capacity);
    virtual ~EditorHistory() = default;
    virtual Index Size() const = 0;
    virtual Index CurrentIndex() const = 0;
    virtual EditorPosition GetPosition(Index index) const = 0;
    virtual void PushPosition(EditorPosition&& pos) = 0;
    virtual EditorPosition Goto(Index index) = 0;
    virtual void Erase(Index index) = 0;
  };
}
