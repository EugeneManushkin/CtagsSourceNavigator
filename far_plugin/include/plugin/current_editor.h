#pragma once

#include <string>

namespace Plugin
{
  struct EditorPosition
  {
    enum
    {
      DefaultLine = -1,
      DefaultPos = -1
    };

    EditorPosition(std::string const& file = "", int line = 0, int pos = DefaultPos, int top = 0, int left = DefaultLine)
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

  inline bool operator == (Plugin::EditorPosition const& left, Plugin::EditorPosition const& right)
  {
    return
        left.Line == right.Line
     && left.File == right.File
    ;
  }

  inline bool operator != (Plugin::EditorPosition const& left, Plugin::EditorPosition const& right)
  {
    return !(left == right);
  }

  class CurrentEditor
  {
  public:
    virtual ~CurrentEditor() = default;
    virtual bool IsModal() const = 0;
    virtual bool IsOpened(char const* file) const = 0;
    virtual EditorPosition GetPosition() const = 0;
    virtual void OpenAsync(EditorPosition const& position) = 0;
    virtual void OpenModal(EditorPosition const& position) = 0;
  };
}
