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

#include "RegExp.hpp"
#include "Array.hpp"
#include "XTools.hpp"
#include <string>
#include <bitset>
#include <vector>

using namespace XClasses;

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
};

struct Config{
  Config();
  void SetWordchars(std::string const& str);
  std::string GetWordchars() const;
  bool isident(int chr) const;
  String exe;
  String opt;
  String autoload;
  String history_file;
  size_t history_len;
  bool casesens;
  bool autoload_changed;
  static const size_t max_history_len;

private:
  String wordchars;
  std::bitset<256> wordCharsMap;
};

extern Config config;

struct TagInfo{
  String name;
  String file;
  String declaration;
  String re;
  int lineno;
  char type;
  String info;

  TagInfo():lineno(-1){}
};

typedef Vector<TagInfo*> TagArray;
typedef TagArray* PTagArray;

int isident(int chr);


int Load(const char* filename,const char* base,bool mainaload=false);
void UnloadTags(int idx);
PTagArray Find(const char* symbol,const char* filename);
int Count();
int Count(char const* fname);
void FindParts(const char* file,const char* part,StrList& dst);
std::vector<TagInfo> FindPartiallyMatchedTags(const char* file, const char* part, size_t maxCount);
PTagArray FindFileSymbols(const char* file);
PTagArray FindClassSymbols(const char* file,const char* classname);
void Autoload(const char* fn);
void GetFiles(StrList& dst);
int SaveChangedFiles(const char* file, const char* outputFilename);
int MergeFiles(const char* target,const char* mfile);
bool IsTagFile(const char* file);
std::string GetTagsFile(std::string const& fileFullPath);
#endif
