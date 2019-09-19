#pragma once

#include "task.h"

#include <memory>

namespace FarPlugin
{
  std::unique_ptr<Task> CreateExecuteProcessTask(std::string const& file, std::string const& args, std::string const& workingDirectory);
}
