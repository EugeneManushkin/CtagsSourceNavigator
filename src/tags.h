/*
  Copyright (C) 2000 Konstantin Stupnik

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
};

struct Config{
  Config();
  void SetWordchars(std::string const& str);
  std::string GetWordchars() const;
  bool Config::isident(int chr) const;
  String exe;
  String opt;
  String autoload;
  bool casesens;
  bool autoload_changed;

private:
  String wordchars;
  std::bitset<256> wordCharsMap;
};

extern Config config;

struct TagInfo{
  String name;
  String file;
  String re;
  int lineno;
  char type;
  String info;

  TagInfo():lineno(-1){}
  TagInfo(const TagInfo& src)
  {
    file=src.file;
    re=src.re;
    lineno=src.lineno;
    type=src.type;
    info=src.info;
  }
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
PTagArray FindFileSymbols(const char* file);
PTagArray FindClassSymbols(const char* file,const char* classname);
void Autoload(const char* fn);
void GetFiles(StrList& dst);
int SaveChangedFiles(const char* file, const char* outputFilename);
int MergeFiles(const char* target,const char* mfile);
bool IsTagFile(const char* file);
#endif
