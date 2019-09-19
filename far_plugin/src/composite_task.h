#pragma once

#include "task.h"

#include <memory>

namespace FarPlugin
{
  std::unique_ptr<Task> CreateCompositeTask(std::initializer_list<std::shared_ptr<Task>>);
}
