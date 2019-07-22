#pragma once

#include <memory>

namespace Facade
{
  class Menu
  {
  public:
    static std::unique_ptr<Menu> Create();
    virtual void Add(char const* text, char label, bool disabled) = 0;
    virtual void Add(int textID, char label, bool disabled) = 0;
    virtual void AddSeparator() = 0;
    virtual int Run(char const* title, int selected) = 0;
    virtual int Run(int titleID, int selected) = 0;
  };
}
