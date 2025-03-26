#pragma once

#include <memory>

struct FarKey;

namespace Far3
{
  enum class KeyEvent : int
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

  inline bool operator == (int left, KeyEvent right) { return left == static_cast<int>(right); }

  class BreakKeys
  {
  public:
    static int const InvalidChar = -1;

    static std::unique_ptr<BreakKeys> Create(bool onlyLatin);

    virtual ::FarKey const* GetBreakKeys() const = 0;
    virtual KeyEvent GetEvent(int index) const = 0;
    virtual int GetChar(int index) const = 0;
  };
}
