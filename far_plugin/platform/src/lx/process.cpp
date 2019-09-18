#include <platform/process.h>

namespace
{
//TODO: implement
  class ProcessImpl : public Platform::Process
  {
  public:
    ProcessImpl(std::string const& file, std::string const& args, std::string const& workingDirectory)
    {
    }

    void Kill() override
    {
      throw std::logic_error("Not implemented");
    }

    int Join() override
    {
      throw std::logic_error("Not implemented");
    }

  private:
    void* Handle;
  };
}

namespace Platform
{
  std::unique_ptr<Process> Process::Create(std::string const& file, std::string const& args, std::string const& workingDirectory)
  {
    return std::unique_ptr<Process>(new ProcessImpl(file, args, workingDirectory));
  }
}
