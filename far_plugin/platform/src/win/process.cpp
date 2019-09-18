#include <platform/process.h>
#include <platform/string.h>

#define NOMINMAX
#include <windows.h>

using Platform::ToString;
using Platform::WideString;

namespace
{
  std::string GetErrorText(DWORD error)
  {
    char* buffer = 0;
    size_t sz = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, 
                               MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<char*>(&buffer), 0, nullptr);
    std::string result(buffer, sz);
    LocalFree(buffer);
    return std::move(result);
  }

  void* StartProcess(WideString const& file, WideString const& args, WideString const& workingDirectory)
  {
    SHELLEXECUTEINFOW ShExecInfo = {};
    ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
    ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
    ShExecInfo.hwnd = nullptr;
    ShExecInfo.lpVerb = nullptr;
    ShExecInfo.lpFile = file.c_str();
    ShExecInfo.lpParameters = args.c_str();
    ShExecInfo.lpDirectory = workingDirectory.c_str();
    ShExecInfo.nShow = 0;
    ShExecInfo.hInstApp = nullptr;
    if (!::ShellExecuteExW(&ShExecInfo))
      throw std::runtime_error(GetErrorText(GetLastError()));

    return ShExecInfo.hProcess;
  }

  class ProcessImpl : public Platform::Process
  {
  public:
    ProcessImpl(std::string const& file, std::string const& args, std::string const& workingDirectory)
      : Handle(StartProcess(ToString(file), ToString(args), ToString(workingDirectory)))
    {
    }

    void Kill() override
    {
      TerminateProcess(Handle, ERROR_CANCELLED);
    }

    int Join() override
    {
      WaitForSingleObject(Handle, INFINITE);
      DWORD exitCode = 0;
      if (!GetExitCodeProcess(Handle, &exitCode))
        throw std::runtime_error(GetErrorText(GetLastError()));

      return static_cast<int>(exitCode);
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
