#pragma once

#include <string>
#include <vector>

namespace Plugin
{
  enum class ThreeStateFlag
  {
    Undefined,
    Enabled,
    Disabled,
  };

  struct Config
  {
    std::string exe = "ctags.exe";
    bool use_built_in_ctags = true;
    std::string opt = "--c++-types=+lpx --c-types=+lpx --fields=+n --sort=no -R *";
    size_t max_results = 25;
    size_t threshold = 500;
    size_t threshold_filter_len = 3;
    ThreeStateFlag platform_language_lookup = ThreeStateFlag::Undefined;
    size_t reset_cache_counters_timeout_hours = 12;
    bool casesens = false;
    bool sort_class_members_by_name = false;
    bool cur_file_first = true;
    bool cached_tags_on_top = true;
    bool index_edited_file = true;
    std::string wordchars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz~$_";
    std::string tagsmask = "tags,*.tags";
    std::string history_file = "%USERPROFILE%\\.tags-history";
    size_t history_len = 10;
    static size_t const max_history_len = 100;
    std::string permanents = "%USERPROFILE%\\.tags-autoload";
    bool restore_last_visited_on_load = true;
  };

  enum class ConfigFieldId : int
  {
    exe = 0,
    use_built_in_ctags,
    opt,
    max_results,
    threshold,
    threshold_filter_len,
    platform_language_lookup,
    reset_cache_counters_timeout_hours,
    casesens,
    sort_class_members_by_name,
    cur_file_first,
    cached_tags_on_top,
    index_edited_file,
    wordchars,
    tagsmask,
    history_file,
    history_len,
    permanents,
    restore_last_visited_on_load,
    MaxFieldId // past the last element
  };
}
