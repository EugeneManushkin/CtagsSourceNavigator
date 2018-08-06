/*
  Copyright (C) 2000 Konstantin Stupnik
  Copyright (C) 2018 Eugene Manushkin

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef __TAGS_H__
#define __TAGS_H__

#include <string>
#include <bitset>
#include <vector>

enum{
  MPlugin=0,
  MECompileFail,
  MEFailedToOpen,
  MFindSymbol,
  MReloadTags,
  MNotFound,
  MSelectSymbol,
  MUndoNavigation,
  MResetUndo,
  MLoadOk,
  MENotLoaded,
  MLoadTagsFile,
  MUnloadTagsFile,
  MCreateTagsFile,
  MUpdateTagsFile,
  MUnableToUpdate,
  MCompleteSymbol,
  MNothingFound,
  MAll,
  MFailedToWriteIndex,
  MBrowseSymbolsInFile,
  MBrowseClass,
  MItemsCount,
  MBrowseClassTitle,
  MInputClassToBrowse,
  MPathToExe,
  MOk,
  MCancel,
  MCmdLineOptions,
  MAutoloadFile,
  MRegFailed,
  MTagingCurrentDirectory,
  MUpdatingTagsFile,
  MWordChars,
  MCaseSensFilt,
  MNotFoundAsk,
  MAddTagsToAutoload,
  MFailedSaveAutoload,
  MNotTagFile,
  MHistoryFile,
  MHistoryLength,
  MLoadFromHistory,
  MHistoryEmpty,
  MTitleHistory,
  MLookupSymbol,
  MAskSearchTags,
  MTagsMask,
  MSelectTags,
  MReindexRepo,
  MAskReindex,
  MProceed,
  MTagsNotFound,
  MPressEscToCancel,
  MAskCancel,
  MCanceling,
  MCanceled,
  MSearchFile,
  MLoadingTags,
  MMaxResults,
  MSortClassMembersByName,
  MCurFileFirst,
};

struct Config{
  Config();
  void SetWordchars(std::string const& str);
  std::string GetWordchars() const;
  bool isident(int chr) const;
  std::string exe;
  std::string opt;
  std::string autoload;
  std::string tagsmask;
  std::string history_file;
  size_t history_len;
  bool casesens;
  bool autoload_changed;
  size_t max_results;
  bool cur_file_first;
  bool sort_class_members_by_name;
  static const size_t max_history_len;

private:
  std::string wordchars;
  std::bitset<256> wordCharsMap;
};

struct TagInfo{
  std::string name;
  std::string file;
  std::string declaration;
  std::string re;
  int lineno;
  char type;
  std::string info;

  TagInfo():lineno(-1){}
};

class SortOptions
{
public:
  enum
  {
    DoNotSort = -1,
    Default = 0,
    SortByName = 1 << 0,
    CurFileFirst = 1 << 1,
  };
};

int Load(const char* filename, size_t& symbolsLoaded);
void UnloadTags(int idx);
std::vector<TagInfo> Find(const char* name, const char* filename, int sortOptions = SortOptions::Default);
std::vector<TagInfo> FindPartiallyMatchedTags(const char* file, const char* part, size_t maxCount, bool caseInsensitive, int sortOptions = SortOptions::Default);
std::vector<std::string> FindPartiallyMatchedFile(const char* file, const char* part, size_t maxCount); 
std::vector<TagInfo> FindClassMembers(const char* file, const char* classname, int sortOptions = SortOptions::Default);
std::vector<TagInfo> FindFileSymbols(const char* file);
void Autoload(const char* fn);
std::vector<std::string> GetFiles();
bool IsTagFile(const char* file);
bool TagsLoadedForFile(const char* file);
std::vector<TagInfo>::const_iterator FindContextTag(std::vector<TagInfo> const& tags, char const* fileName, int lineNumber, char const* lineText);
std::vector<TagInfo>::const_iterator Reorder(TagInfo const& context, std::vector<TagInfo>& tags);
#endif
