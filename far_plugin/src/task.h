#pragma once

#include <memory>

namespace FarPlugin
{
  class Task
  {
  public:
    struct Status
    {
      int TextID;
    };

    virtual ~Task() = default;
    virtual void Execute() = 0;
    virtual void Cancel() = 0;
    virtual Status GetStatus() const = 0;
  };

  std::unique_ptr<Task> CreateCompositeTask(std::initializer_list<std::shared_ptr<Task>>);
}
