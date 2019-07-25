#pragma once

#include <memory>

namespace Facade
{
  void ErrorMessage(char const* text);
  void ErrorMessage(int textID);
//TODO: allow combination of text and textID
  void InfoMessage(int titleID, char const* text);
  void InfoMessage(int titleID, int textID);
  std::shared_ptr<void> LongOperationMessage(int titleID, int textID);
}
