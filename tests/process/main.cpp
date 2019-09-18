#include <gtest/gtest.h>
#include <platform/process.h>

#include <string>

std::string MockProcess;

namespace Platform
{
  namespace Tests
  {
    int const ExitCodeSuccess = 0;
    int const ExitCodeFail = 0xEE; // In Linux only 0-255 exit codes are available
    int const LargeTimeout = 10;

    std::unique_ptr<Process> CreateTestProcess(int exitCode, int timeout = 0)
    {
      return Process::Create(MockProcess, std::to_string(exitCode) + " "  + std::to_string(timeout), "");
    }

    std::unique_ptr<Process> CreateCrashedProcess()
    {
      return Process::Create(MockProcess, "", "");
    }

    std::unique_ptr<Process> CreateTestProcess(std::string const& left, std::string const& right, int exitCode)
    {
      return Process::Create(MockProcess, left + " " + right + " " + std::to_string(exitCode), "");
    }

    TEST(Process, RunsNormalProcess)
    {
      auto sut = CreateTestProcess(ExitCodeSuccess);
      ASSERT_EQ(ExitCodeSuccess, sut->Join());
    }

    TEST(Process, RetreivesExitCode)
    {
      auto sut = CreateTestProcess(ExitCodeFail);
      ASSERT_EQ(ExitCodeFail, sut->Join());
    }

    TEST(Process, KillsProcess)
    {
      auto sut = CreateTestProcess(ExitCodeSuccess, LargeTimeout);
      sut->Kill();
      ASSERT_NE(ExitCodeSuccess, sut->Join());
    }

    TEST(Process, JoinsCrashedProcess)
    {
      auto sut = CreateCrashedProcess();
      ASSERT_NE(ExitCodeSuccess, sut->Join());
    }

    TEST(Process, PassArgumentInSingleQuotes)
    {
      auto sut = CreateTestProcess("' Argument With   Spaces'", "_Argument_With___Spaces", ExitCodeFail);
      ASSERT_EQ(ExitCodeFail, sut->Join());
    }

    TEST(Process, PassArgumentInDoubleQuotes)
    {
      auto sut = CreateTestProcess("\" Argument With   Spaces\"", "_Argument_With___Spaces", ExitCodeFail);
      ASSERT_EQ(ExitCodeFail, sut->Join());
    }
  }
}

int main(int argc, char* argv[])
{
  if (argc < 2)
    return 1;

  MockProcess = argv[1];
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
