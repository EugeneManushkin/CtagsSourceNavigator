#pragma once

#include <string>

namespace FarPlugin
{
  class Task
  {
  public:
    struct Status
    {
      std::string Text;
    };

    virtual ~Task() = default;
    virtual void Execute() = 0;
    virtual void Cancel() = 0;
    virtual Status GetStatus() const = 0;
  };
}
