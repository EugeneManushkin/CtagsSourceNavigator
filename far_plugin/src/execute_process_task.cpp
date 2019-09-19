#include "execute_process_task.h"
#include "text.h"

#include <facade/message.h>
#include <platform/process.h>

#include <mutex>

namespace
{
  class ExecuteProcessTask : public FarPlugin::Task
  {
  public:
    ExecuteProcessTask(std::string const& file, std::string const& args, std::string const& workingDirectory)
      : File(file)
      , Args(args)
      , WorkingDirectory(workingDirectory)
      , Status{Facade::Format(MExecuteProcessStatus, File.c_str(), WorkingDirectory.c_str(), Args.c_str())}
      , Canceled(false)
    {
    }

    void Execute() override
    {
      StartProcess();
      if (!Process)
        throw std::runtime_error(Facade::Format(MCanceled));

      auto exitCode = Process->Join();
      if (!!exitCode)
        throw std::runtime_error(Facade::Format(MProcessExited, File.c_str(), exitCode));
    }

    void Cancel() override
    {
      Guard.lock();
      Canceled = true;
      Guard.unlock();
      if (Process)
        Process->Kill();
    }

    FarPlugin::Task::Status GetStatus() const override
    {
      return Status;
    }

  private:
    std::unique_ptr<Platform::Process> CreateProcessImpl()
    {
      try
      {
        return Platform::Process::Create(File, Args, WorkingDirectory);
      }
      catch(std::exception const& e)
      {
        throw std::runtime_error(Facade::Format(MFailedStartProcess, File.c_str(), e.what()));
      }
    }

    void StartProcess()
    {
      std::lock_guard<std::mutex> lock(Guard);
      if (!Canceled)
        Process = CreateProcessImpl();
    }

    std::string File;
    std::string Args;
    std::string WorkingDirectory;
    FarPlugin::Task::Status Status;
    bool Canceled;
    std::unique_ptr<Platform::Process> Process;
    std::mutex Guard;
  };
}

namespace FarPlugin
{
  std::unique_ptr<Task> CreateExecuteProcessTask(std::string const& file, std::string const& args, std::string const& workingDirectory)
  {
    return std::unique_ptr<ExecuteProcessTask>(new ExecuteProcessTask(file, args, workingDirectory));
  }
}
