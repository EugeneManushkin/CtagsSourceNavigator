#include "run_task.h"
#include "task.h"
#include "text.h"

#include <facade/dialog.h>
#include <facade/message.h>

#include <future>

namespace
{
  void OnCancel(FarPlugin::Task& task, Facade::DialogController const& controller)
  {
    if (Facade::YesNoCancelMessage(MAskCancel, MPlugin) != Facade::YesNoCancel::Yes)
      return;

    task.Cancel();
    controller.SetValue("cancel", Facade::Format(MCanceling));
  }

  void CheckProgress(std::future<void>& worker, FarPlugin::Task& task, Facade::DialogController const& controller)
  {
    controller.SetValue("status", task.GetStatus().Text);
    if (worker.wait_for(std::chrono::milliseconds(0)) != std::future_status::timeout)
      controller.Close();
  }

  void ShowProgressDialog(std::future<void>& worker, FarPlugin::Task& task)
  {
    int const dialogWidth = 68;
    auto dialog = Facade::Dialog::Create(dialogWidth, true);
    dialog->
      SetTitle(MPlugin)
     .AddCaption(task.GetStatus().Text, "status")
     .AddSeparator()
     .AddButton(MCancel, "cancel", true, true, true, [&task](Facade::DialogController const& c){OnCancel(task, c);})
     .SetOnPoll([&worker, &task](Facade::DialogController const& c) {CheckProgress(worker, task, c);})
    ;
    dialog->Run();
  }
}

namespace FarPlugin
{
  void RunTask(std::unique_ptr<Task>&& task)
  {
    std::chrono::milliseconds const timeout(300);
    auto worker = std::async(std::launch::async, [&task](){ task->Execute(); });
    if (worker.wait_for(timeout) == std::future_status::timeout)
      ShowProgressDialog(worker, *task);

    worker.get();
  }
}
