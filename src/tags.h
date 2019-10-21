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

#include "tag_info.h"
#include "tags_sorting_options.h"

#include <string>
#include <vector>

int Load(const char* filename, bool singleFileRepos, size_t& symbolsLoaded);
void UnloadTags(const char* tagsFile);
void UnloadAllTags();
std::vector<TagInfo> Find(const char* name, const char* filename, Tags::SortingOptions sortOptions = Tags::SortingOptions::Default);
std::vector<TagInfo> FindPartiallyMatchedTags(const char* file, const char* part, size_t maxCount, bool caseInsensitive, Tags::SortingOptions sortOptions = Tags::SortingOptions::Default);
std::vector<TagInfo> FindFile(const char* file, const char* path);
std::vector<TagInfo> FindPartiallyMatchedFile(const char* file, const char* part, size_t maxCount); 
std::vector<TagInfo> FindClassMembers(const char* file, const char* classname, Tags::SortingOptions sortOptions = Tags::SortingOptions::Default);
std::vector<TagInfo> FindFileSymbols(const char* file);
std::vector<std::string> GetFiles();
std::vector<std::string> GetLoadedTags(const char* file);
bool IsTagFile(const char* file);
std::vector<TagInfo>::const_iterator FindContextTag(std::vector<TagInfo> const& tags, char const* fileName, int lineNumber, char const* lineText);
std::vector<TagInfo>::const_iterator Reorder(TagInfo const& context, std::vector<TagInfo>& tags);
void CacheTag(TagInfo const& tag, size_t cacheSize, bool flush);
void EraseCachedTag(TagInfo const& tag, bool flush);
std::vector<TagInfo> GetCachedTags(const char* file, size_t limit, bool getFiles);
std::vector<TagInfo> SortTags(std::vector<TagInfo>&& tags, char const* file, Tags::SortingOptions sortOptions);

#endif
