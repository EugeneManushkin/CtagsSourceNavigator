#include <gtest/gtest.h>

#include <execute_process_task.h>
#include <platform/process.h>
#include <text.h>

#include <functional>
#include <future>
#include <stdarg.h>
#include <unordered_map>

namespace
{
  std::chrono::seconds const LargeTimeout(10);
  std::unordered_map<int, std::string> const Texts = {
    {MCanceled, "Operation canceled by user"},
    {MProcessExited, "Process %s terminated with code %d"},
    {MFailedStartProcess, "Failed to start process %s with error:\n\"%s\""},
    {MExecuteProcessStatus, "Running %s\nDirectory:%s\nArguments:%s"},
  };

  std::string ExceptionText(std::function<void(void)> f)
  {
    try
    {
      f();
    }
    catch(std::exception const& e)
    {
      return e.what();
    }

    throw std::logic_error("Excetption not thrown");
  }

  class MockProcess : public Platform::Process
  {
  public:
    MockProcess(std::promise<void>&& starter, std::promise<int>& stopper, int cancelExitCode)
      : Starter(std::move(starter))
      , Stopper(stopper)
      , Result(Stopper.get_future())
      , CancelExitCode(cancelExitCode)
    {
    }

    void Kill() override
    {
      Stopper.set_value(CancelExitCode);
    }

    int Join() override
    {
      Starter.set_value();
      if (Result.wait_for(LargeTimeout) != std::future_status::ready)
        throw std::logic_error("Process hanged");

      return Result.get();
    }

  private:
    std::promise<void> Starter;
    std::promise<int>& Stopper;
    std::future<int> Result;
    int CancelExitCode;
  };

  std::unique_ptr<Platform::Process> MockProcessInstance;
}

namespace Platform
{
  std::unique_ptr<Process> Process::Create(std::string const& file, std::string const& args, std::string const& workingDirectory)
  {
    return std::move(MockProcessInstance);
  }
}

namespace Facade
{
  std::string Format(int formatID, ...)
  {
    auto i = Texts.find(formatID);
    if (i == Texts.end())
      throw std::logic_error("Undefined formatID: " + std::to_string(formatID));

    auto format = i->second;
    va_list args;
    va_start(args, formatID);
    auto required = vsnprintf(nullptr, 0, format.c_str(), args);
    va_end(args);
    std::vector<char> buf(required + 1);
    va_start(args, formatID);
    vsnprintf(&buf[0], buf.size(), format.c_str(), args);
    va_end(args);
    return std::string(buf.begin(), --buf.end());
  }
}

namespace FarPlugin
{
  namespace Tests
  {
    std::string const File = "test_file.exe";
    std::string const Args = "-a --test --argument";
    std::string const WorkingDirectory = "test/working/directory";
    std::string const StatusText = Facade::Format(MExecuteProcessStatus, File.c_str(), WorkingDirectory.c_str(), Args.c_str());
    int const ExitCodeSuccess = 0;
    int const ExitCodeFail = 0xEE;
    int const ExitCodeCanceled = 0xBF;
    std::string const TextCanceledBeforeStart = Facade::Format(MCanceled);
    std::string const TextFailed = Facade::Format(MProcessExited, File.c_str(), ExitCodeFail);
    std::string const TextCanceled = Facade::Format(MProcessExited, File.c_str(), ExitCodeCanceled);

    class ExecuteProcessTask : public ::testing::Test
    {
    protected:
      void SetUp() override
      {
        std::promise<void> starter;
        ProcessStarted = starter.get_future();
        MockProcessInstance.reset(new MockProcess(std::move(starter), ProcessStopper, ExitCodeCanceled));
        Sut = CreateExecuteProcessTask(File, Args, WorkingDirectory);
      }

      void StopProcess(int exitCode)
      {
        ProcessStopper.set_value(exitCode);
      }

      void WaitProcessStarted()
      {
        if (ProcessStarted.wait_for(LargeTimeout) != std::future_status::ready)
          throw std::logic_error("Process not started");

        ProcessStarted.get();
      }

      std::unique_ptr<Task> Sut;
      std::promise<int> ProcessStopper;
      std::future<void> ProcessStarted;
    };

    TEST_F(ExecuteProcessTask, ReturnsStatus)
    {
      ASSERT_EQ(StatusText, Sut->GetStatus().Text);
    }

    TEST_F(ExecuteProcessTask, Executed)
    {
      StopProcess(ExitCodeSuccess);
      ASSERT_NO_THROW(Sut->Execute());
    }

    TEST_F(ExecuteProcessTask, CanceledBeforeExecute)
    {
      Sut->Cancel();
      ASSERT_EQ(TextCanceledBeforeStart, ExceptionText([this]{ Sut->Execute(); }));
    }

    TEST_F(ExecuteProcessTask, ThrowsWhenProcessTerminates)
    {
      StopProcess(ExitCodeFail);
      ASSERT_EQ(TextFailed, ExceptionText([this]{ Sut->Execute(); }));
    }

    TEST_F(ExecuteProcessTask, CanceledAfterProcessStarted)
    {
      auto result = std::async(std::launch::async, [this](){ Sut->Execute(); });
      WaitProcessStarted();
      Sut->Cancel();
      ASSERT_EQ(TextCanceled, ExceptionText([&result]{ result.get(); }));
    }
  }
}

int main(int argc, char* argv[])
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
