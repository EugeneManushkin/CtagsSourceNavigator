#pragma once

#include "tags_sorting_options.h"

#include <memory>
#include <vector>

namespace Tags
{
  class Selector;

  namespace Internal
  {
    class Repository;

    std::unique_ptr<Selector> CreateSelector(std::vector<std::shared_ptr<Repository> >&& repositories, char const* currentFile, bool caseInsensitive, SortingOptions sortOptions, size_t limit);
  }
}
