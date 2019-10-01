#pragma once

#include <memory>

namespace FarPlugin
{
  class Task;

  void RunTask(std::unique_ptr<Task>&& task);
}
