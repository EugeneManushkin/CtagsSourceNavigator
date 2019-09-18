#pragma once

#include <memory>
#include <string>

namespace Platform
{
  class Process
  {
  public:
    static std::unique_ptr<Process> Create(std::string const& file, std::string const& args, std::string const& workingDirectory);
    virtual ~Process() = default;
    virtual void Kill() = 0;
    virtual int Join() = 0;
  };
}
