#pragma once

#include <memory>
#include <vector>

struct FarKey;

namespace Far3
{
  enum class KeyEvent
  {
    NoEvent,
    Tab,
    Backspace,
    F4,
    CtrlC,
    CtrlV,
    CtrlR,
    CtrlZ,
    CtrlDel,
    CtrlEnter,
  };

  enum class UseLayouts
  {
    None,
    Latin,
    All,
  };

  class BreakKeys
  {
  public:
    static int const InvalidChar = -1;

    static std::unique_ptr<BreakKeys> Create(std::vector<KeyEvent> const& events, UseLayouts useLayouts);

    virtual ::FarKey const* GetBreakKeys() const = 0;
    virtual KeyEvent GetEvent(int index) const = 0;
    virtual int GetChar(int index) const = 0;
  };
}
