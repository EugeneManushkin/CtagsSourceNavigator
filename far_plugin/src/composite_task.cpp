#include "composite_task.h"

#include <deque>
#include <mutex>

namespace
{
  using Task = FarPlugin::Task;

  class CompositeTask : public Task
  {
  public:
    CompositeTask(std::initializer_list<std::shared_ptr<Task>> tasks);
    void Execute() override;
    void Cancel() override;
    Status GetStatus() const override;

  private:
    bool NextTask();
    void ClearTasks();

    std::deque<std::shared_ptr<Task>> Tasks;
    std::shared_ptr<Task> Current;
    std::mutex Guard;
  };

  CompositeTask::CompositeTask(std::initializer_list<std::shared_ptr<Task>> tasks)
    : Tasks(tasks)
    , Current(!Tasks.empty() ? Tasks.front() : std::shared_ptr<Task>())
  {
  }

  void CompositeTask::Execute()
  {
    for (; NextTask(); Current->Execute());
  }

  void CompositeTask::Cancel()
  {
    ClearTasks();
    if (Current)
      Current->Cancel();
  }

  Task::Status CompositeTask::GetStatus() const
  {
    auto current = Current;
    return current ? current->GetStatus() : Task::Status{};
  }

  bool CompositeTask::NextTask()
  {
    std::lock_guard<std::mutex> lock(Guard);
    if (Tasks.empty())
      return false;

    Current = Tasks.front();
    Tasks.pop_front();
    return true;
  }

  void CompositeTask::ClearTasks()
  {
    std::lock_guard<std::mutex> lock(Guard);
    Tasks.clear();
  }
}

namespace FarPlugin
{
  std::unique_ptr<Task> CreateCompositeTask(std::initializer_list<std::shared_ptr<Task>> tasks)
  {
    return std::unique_ptr<CompositeTask>(new CompositeTask(tasks));
  }
}
