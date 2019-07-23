#pragma once

#include <string>

namespace FarPlugin
{
  struct Config
  {
    Config();
  
    std::string CtagsPath;
    std::string CtagsCmd;
    std::string TagsMask;
    std::string HistoryFile;
    size_t HistoryLen;
    static const size_t MaxHistoryLen;
    bool CaseseInsensitive;
    size_t MaxResults;
    bool CurFileFirst;
    bool IndexEditedFile;
    bool SortClassMembersByName;
    bool UseBuiltinCtags;
    std::string WordChars;
  };

  Config LoadConfig();
}
