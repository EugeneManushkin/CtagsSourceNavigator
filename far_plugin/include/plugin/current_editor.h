#pragma once

#include <string>

namespace Plugin
{
  struct EditorState
  {
    EditorState(std::string const& file = "", int line = 0, int pos = -1, int top = 0, int left = -1)
      : File(file)
      , Line(line)
      , Pos(pos)
      , Top(top) 
      , Left(left)
    {
    }

    std::string File;
    int Line;
    int Pos;
    int Top;
    int Left;
  };

  class CurrentEditor
  {
  public:
    virtual ~CurrentEditor() = default;
    virtual bool IsModal() const = 0;
    virtual bool IsOpened(char const* file) const = 0;
    virtual EditorState GetState() const = 0;
    virtual void OpenAsync(EditorState const& state) = 0;
    virtual void OpenModal(EditorState const& state) = 0;
  };
}
