#include "config.h"

#include <algorithm>
#include <fstream>
#include <memory>

namespace FarPlugin
{
  const size_t Config::MaxHistoryLen = 100;

  Config::Config()
    : CtagsPath("ctags")
    , CtagsCmd("--c++-types=+px --c-types=+px --fields=+n -R *")
    , TagsMask("tags")
    , HistoryFile("%USERPROFILE%\\.tags-history")
    , HistoryLen(10)
    , CaseseInsensitive(false)
    , MaxResults(10)
    , CurFileFirst(true)
    , IndexEditedFile(true)
    , SortClassMembersByName(false)
    , UseBuiltinCtags(true)
    , WordChars("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz~$_")
  {
  }

  Config LoadConfig(std::string const& fileName)
  {
    Config result;
    std::ifstream file;
    file.exceptions(std::ifstream::goodbit);
    file.open(fileName);
    std::shared_ptr<void> fileCloser(0, [&](void*) { file.close(); });
    std::string buf;
    while (std::getline(file, buf))
    {
      auto pos = buf.find('=');
      if (pos == std::string::npos)
        continue;
  
      auto key = buf.substr(0, pos);
      auto val = buf.substr(pos + 1);
      if(key == "pathtoexe")
      {
        result.CtagsPath = std::move(val);
      }
      else if(key == "commandline")
      {
        result.CtagsCmd = std::move(val);
      }
      else if(key == "casesensfilt")
      {
        result.CaseseInsensitive = val != "true";
      }
      else if(key == "caseinsensitive")
      {
        result.CaseseInsensitive = val == "true";
      }
      else if(key == "wordchars")
      {
        result.WordChars = std::move(val);
      }
      else if(key == "tagsmask")
      {
        result.TagsMask = std::move(val);
      }
      else if(key == "historyfile")
      {
        result.HistoryFile = std::move(val);
      }
      else if(key == "historylen")
      {
        int len = 0;
        if (sscanf(val.c_str(), "%d", &len) == 1 && len >= 0)
          result.HistoryLen = std::min(static_cast<size_t>(len), Config::MaxHistoryLen);
      }
      else if(key == "maxresults")
      {
        int maxresults = 0;
        if (sscanf(val.c_str(), "%d", &maxresults) == 1 && maxresults > 0)
          result.MaxResults = maxresults;
      }
      else if(key == "curfilefirst")
      {
        result.CurFileFirst = val == "true";
      }
      else if(key == "indexeditedfile")
      {
        result.IndexEditedFile = val == "true";
      }
      else if(key == "sortclassmembersbyname")
      {
        result.SortClassMembersByName = val == "true";
      }
      else if(key == "usebuiltinctags")
      {
        result.UseBuiltinCtags = val == "true";
      }
    }

    result.HistoryLen = result.HistoryFile.empty() ? 0 : result.HistoryLen;
    return std::move(result);
  }
}
