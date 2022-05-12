#pragma once

namespace Tags
{
  enum class SortingOptions : int
  {
    DoNotSort = -1,
    Default = 0,
    SortByName = 1 << 0,
    CurFileFirst = 1 << 1,
    CachedTagsOnTop = 1 << 2,
  };

  inline SortingOptions operator | (SortingOptions left, SortingOptions right) { return static_cast<SortingOptions>(static_cast<int>(left) | static_cast<int>(right)); }
  inline SortingOptions operator & (SortingOptions left, SortingOptions right) { return static_cast<SortingOptions>(static_cast<int>(left) & static_cast<int>(right)); }
  inline bool operator ! (SortingOptions option) { return !static_cast<int>(option); }
}
