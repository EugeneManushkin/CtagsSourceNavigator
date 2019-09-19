#include <gtest/gtest.h>
#include <composite_task.h>

#include <chrono>
#include <future>
#include <thread>

namespace
{
  int const DefaultTimeout = 5;
  FarPlugin::Task::Status const InvalidStatus;
  FarPlugin::Task::Status const FirstTaskStatus{"First task"};
  FarPlugin::Task::Status const SecondTaskStatus{"Second task"};

  class MockTask
  {
  public:
    MockTask(FarPlugin::Task::Status const& status);
    bool WaitStarted(int timeoutSeconds = DefaultTimeout);
    bool WasStarted();
    void Complete();
    void Fail();
    std::shared_ptr<FarPlugin::Task> Get();

  private:
    class TaskImpl : public FarPlugin::Task
    {
    public:
      TaskImpl(std::promise<std::exception_ptr>& stopper, std::promise<void>&& startEvent, FarPlugin::Task::Status const& status)
        : Stopper(stopper)
        , Result(Stopper.get_future())
        , StartEvent(std::move(startEvent))
        , Status(status)
      {
      }

      void Execute() override
      {
        StartEvent.set_value();
        auto err = Result.get();
        if (err)
          std::rethrow_exception(err);
      }

      void Cancel() override
      {
        Stopper.set_value(std::make_exception_ptr(std::runtime_error("canceled")));
      }

      FarPlugin::Task::Status GetStatus() const override
      {
        return Status;
      }

    private:
      std::promise<std::exception_ptr>& Stopper;
      std::future<std::exception_ptr> Result;
      std::promise<void> StartEvent;
      FarPlugin::Task::Status Status;
    };

    std::promise<std::exception_ptr> Stopper;
    std::future<void> Started;
    std::shared_ptr<FarPlugin::Task> Task;
  };

  MockTask::MockTask(FarPlugin::Task::Status const& status)
  {
    std::promise<void> startEvent;
    Started = startEvent.get_future();
    Task.reset(new TaskImpl(Stopper, std::move(startEvent), status));
  }

  bool MockTask::WaitStarted(int timeoutSeconds)
  {
    return Started.wait_for(std::chrono::seconds(timeoutSeconds)) == std::future_status::ready;
  }

  bool MockTask::WasStarted()
  {
    return WaitStarted(0);
  }

  void MockTask::Fail()
  {
    Stopper.set_value(std::make_exception_ptr(std::runtime_error("failed")));
  }

  void MockTask::Complete()
  {
    Stopper.set_value(std::exception_ptr());
  }

  std::shared_ptr<FarPlugin::Task> MockTask::Get()
  {
    return Task;
  }

  std::future<void> Launch(FarPlugin::Task& task)
  {
    return std::async(std::launch::async, [&task](){return task.Execute();});
  }
}

namespace FarPlugin
{
  bool operator == (Task::Status const& left, Task::Status const& right)
  {
    return left.Text == right.Text;
  }

  namespace Tests
  {
    TEST(CompositeTask, RunsNoTasks)
    {
      auto sut = FarPlugin::CreateCompositeTask({});
      ASSERT_NO_THROW(sut->Execute());
      ASSERT_EQ(InvalidStatus, sut->GetStatus());
    }

    TEST(CompositeTask, ReturnsStatusOfFirstTaskBeforeStart)
    {
      MockTask mockTask(FirstTaskStatus);
      auto sut = CreateCompositeTask({mockTask.Get()});
      ASSERT_FALSE(mockTask.WasStarted());
      ASSERT_EQ(FirstTaskStatus, sut->GetStatus());
    }

    TEST(CompositeTask, CanceledBeforeStart)
    {
      MockTask mockTask(FirstTaskStatus);
      auto sut = CreateCompositeTask({mockTask.Get()});
      sut->Cancel();
      ASSERT_NO_THROW(sut->Execute());
      ASSERT_FALSE(mockTask.WasStarted());
    }

    TEST(CompositeTask, RunsSingleTask)
    {
      MockTask mockTask(FirstTaskStatus);
      auto sut = CreateCompositeTask({mockTask.Get()});
      mockTask.Complete();
      ASSERT_NO_THROW(sut->Execute());
      ASSERT_TRUE(mockTask.WasStarted());
      ASSERT_EQ(FirstTaskStatus, sut->GetStatus());
    }

    TEST(CompositeTask, ConsistentlyPerformsTwoTasks)
    {
      MockTask firstTask(FirstTaskStatus);
      MockTask secondTask(SecondTaskStatus);
      auto sut = CreateCompositeTask({firstTask.Get(), secondTask.Get()});
      auto worker = Launch(*sut);
      EXPECT_TRUE(firstTask.WaitStarted());
      EXPECT_EQ(FirstTaskStatus, sut->GetStatus());
      EXPECT_FALSE(secondTask.WasStarted());
      firstTask.Complete();
      EXPECT_TRUE(secondTask.WaitStarted());
      EXPECT_EQ(SecondTaskStatus, sut->GetStatus());
      secondTask.Complete();
      ASSERT_NO_THROW(worker.get());
    }

    TEST(CompositeTask, StopsIfFirstTaskFailed)
    {
      MockTask firstTask(FirstTaskStatus);
      MockTask secondTask(SecondTaskStatus);
      auto sut = CreateCompositeTask({firstTask.Get(), secondTask.Get()});
      auto worker = Launch(*sut);
      firstTask.Fail();
      ASSERT_THROW(worker.get(), std::runtime_error);
      EXPECT_EQ(FirstTaskStatus, sut->GetStatus());
      ASSERT_FALSE(secondTask.WasStarted());
    }
  }
}

int main(int argc, char* argv[])
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
