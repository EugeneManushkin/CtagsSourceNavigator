#pragma once

#include <string>

namespace FarPlugin
{
  std::string SelectFromTagsHistory(char const* historyFile, size_t capacity);
  void AddToTagsHistory(char const* tagsFile, char const* historyFile, size_t capacity);
}
