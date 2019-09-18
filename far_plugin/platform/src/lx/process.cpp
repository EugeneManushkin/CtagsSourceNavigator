#include <platform/process.h>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wordexp.h>

#include <cstring>

namespace
{
  std::string GetErrorText(int error)
  {
    return std::strerror(error);
  }

  pid_t StartProcess(std::string const& file, std::string const& args, std::string const& workingDirectory)
  {
    auto pid = ::fork();
    if (pid == -1)
    {
      throw std::runtime_error(GetErrorText(errno));
    }

    if (pid == 0)
    {
      if (!::chdir(workingDirectory.c_str()))
        ::exit(errno);

      ::wordexp_t p;
      ::wordexp((file + " " + args).c_str(), &p, 0);
      ::execv(file.c_str(), p.we_wordv);
      ::wordfree(&p);
      ::exit(errno);
    }

    return pid;
  }

  class ProcessImpl : public Platform::Process
  {
  public:
    ProcessImpl(std::string const& file, std::string const& args, std::string const& workingDirectory)
      : Pid(StartProcess(file, args, workingDirectory))
    {
    }

    void Kill() override
    {
      kill(Pid, SIGKILL);
    }

    int Join() override
    {
      int status = 0;
      if (::waitpid(Pid, &status, 0) != Pid)
        throw std::runtime_error(GetErrorText(errno));

      return WIFEXITED(status) ? WEXITSTATUS(status) : ENOTRECOVERABLE;
    }

  private:
    pid_t Pid;
  };
}

namespace Platform
{
  std::unique_ptr<Process> Process::Create(std::string const& file, std::string const& args, std::string const& workingDirectory)
  {
    return std::unique_ptr<Process>(new ProcessImpl(file, args, workingDirectory));
  }
}
