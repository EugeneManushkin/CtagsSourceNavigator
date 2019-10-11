#pragma once

#include <memory>
#include <string>

namespace Facade
{
  enum
  {
    DefaultTextID = -1,
  };

  enum class YesNoCancel
  {
    Yes = 0,
    No = 1,
    Cancel = 2
  };

  std::string Format(int formatID, ...);
  bool ErrorMessage(std::string const& text, int titleID = DefaultTextID, bool retry = false);
  bool ErrorMessage(int textID, int titleID = DefaultTextID, bool retry = false);
  void InfoMessage(std::string const& text, int titleID = DefaultTextID);
  void InfoMessage(int textID, int titleID = DefaultTextID);
  std::shared_ptr<void> LongOperationMessage(std::string const& text, int titleID = DefaultTextID);
  std::shared_ptr<void> LongOperationMessage(int textID, int titleID = DefaultTextID);
  YesNoCancel YesNoCancelMessage(int textID, int titleID);
}
