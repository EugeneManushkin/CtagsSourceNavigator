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

#include <algorithm>
#include <functional>
#include <iterator>
#include <list>
#include <regex>
#include <set>
#include <stdio.h>
#include <sys/stat.h>
#include <string>
#include <string.h>
#include <tuple>
#include <unordered_map>
#include <vector>
#include <memory>
#include "tags.h"
#include "tags_cache.h"
#include "tags_repository.h"
#include "tags_repository_storage.h"
#include "tags_selector.h"
#include "tags_selector_impl.h"

#if defined _WIN32
#include <io.h>
static void Truncate(FILE* f, long size)
{
  _chsize(_fileno(f), size);
}
#else
#include <unistd.h>
static void Truncate(FILE* f, long size)
{
  ftruncate(fileno(f), size);
}
#endif

static bool IsEndOfFile(FILE* f)
{
  auto curPos = ftell(&*f);
  fseek(f, 0, SEEK_END);
  auto endPos = ftell(&*f);
  fseek(f, curPos, SEEK_SET);
  return curPos == endPos;
}

inline bool IsPathSeparator(std::string::value_type c)
{
    return c == '/' || c == '\\';
}

static std::shared_ptr<FILE> FOpen(char const* path, char const* mode)
{
  return std::shared_ptr<FILE>(fopen(path, mode), [](FILE* f) { if (f) fclose(f); });
}

using OffsetType = uint32_t;
using OffsetCont = std::vector<OffsetType>;
enum class IndexType
{
  Names = 0,
  NamesCaseInsensitive,
  Paths,
  Classes,
  Filenames,
  EndOfEnum,
};

struct MakeFileTag
{
  MakeFileTag(int lineNum)
    : LineNum(lineNum)
  {
  }

  TagInfo operator()(TagInfo&& tag) const
  {
    TagInfo temp;
    temp.Owner = std::move(tag.Owner);
    temp.file = std::move(tag.file);
    temp.lineno = LineNum;
    return std::move(temp);
  }

  int LineNum;
};

struct TagFileInfo{
  TagFileInfo(char const* fname, bool singleFileRepos)
    : filename(fname)
    , indexFile(filename + ".idx")
    , singlefilerepos(singleFileRepos)
    , IndexModTime(0)
    , NamesCache(Tags::Internal::CreateTagsCache(0))
    , FilesCache(Tags::Internal::CreateTagsCache(0))
  {
    if (filename.empty() || IsPathSeparator(filename.back()))
      throw std::logic_error("Invalid tags file name");
  }

  char const* GetRelativePath(char const* fileName) const;

  std::string GetFullPath(std::string const& relativePath) const;

  bool IsFullPathRepo() const
  {
    return fullpathrepo;
  }

  std::string const& GetName() const
  {
    return filename;
  }

  std::shared_ptr<FILE> OpenTags(OffsetCont& offsets, IndexType index) const;

  int Load(size_t& symbolsLoaded);

  void CacheTag(TagInfo const& tag, size_t cacheSize)
  {
    Tags::Internal::TagsCache& cache = tag.name.empty() ? *FilesCache : *NamesCache;
    cache.SetCapacity(cacheSize);
    cache.Insert(tag.name.empty() ? MakeFileTag(-1)(TagInfo(tag)) : tag);
  }

  void EraseCachedTag(TagInfo const& tag)
  {
    Tags::Internal::TagsCache& cache = tag.name.empty() ? *FilesCache : *NamesCache;
    cache.Erase(tag);
  }

  std::vector<std::pair<TagInfo, size_t>> GetCachedTags(bool getFiles) const
  {
    return getFiles ? FilesCache->GetStat() : NamesCache->GetStat();
  }

  void FlushCachedTags();

private:
  bool CreateIndex(time_t tagsModTime, bool singleFileRepos);
  bool LoadCache();
  std::shared_ptr<FILE> OpenIndex(char const* mode = "rb") const;
  OffsetCont GetOffsets(FILE* f, IndexType type) const;
  bool Synchronized() const
  {
    return !!OpenIndex();
  }

  bool IndexModified() const
  {
    struct stat st;
    return !IndexModTime || stat(indexFile.c_str(), &st) == -1 || IndexModTime != st.st_mtime;
  }

  void CloseIndexFile(std::shared_ptr<FILE>&& f)
  {
    f.reset();
    struct stat st;
    IndexModTime = stat(indexFile.c_str(), &st) != -1 ? st.st_mtime : 0;
  }

  std::string filename;
  std::string indexFile;
  std::string reporoot;
  std::string singlefile;
  bool singlefilerepos;
  bool fullpathrepo;
  time_t IndexModTime;
  std::shared_ptr<Tags::Internal::TagsCache> NamesCache;
  std::shared_ptr<Tags::Internal::TagsCache> FilesCache;
  OffsetCont NamesOffsets;
};

using Tags::SortingOptions;
auto Storage = Tags::RepositoryStorage::Create();

static std::string JoinPath(std::string const& dirPath, std::string const& name)
{
  return dirPath.empty() || IsPathSeparator(dirPath.back()) ? dirPath + name : dirPath + std::string("\\") + name;
}

static bool IsFullPath(char const* fn)
{
  return fn && fn[0] && fn[1] == ':';
}

static std::string GetDirOfFile(std::string const& filePath)
{
    auto pos = filePath.find_last_of("\\/");
    return !pos || pos == std::string::npos ? std::string() : filePath.substr(0, pos);
}

char const IndexFileSignature[] = "tags.idx.v7";

static bool ReadSignature(FILE* f)
{
  char signature[sizeof(IndexFileSignature)];
  if (fread(signature, 1, sizeof(IndexFileSignature), f) != sizeof(IndexFileSignature) || memcmp(signature, IndexFileSignature, sizeof(IndexFileSignature)))
  {
    fseek(f, 0, SEEK_SET);
    return false;
  }

  return true;
}

static uint32_t const stringLengthThreshold = 32 * 1024;

static bool ReadString(FILE* f, std::string& str)
{
  uint32_t len = 0;
  if (fread(&len, sizeof(len), 1, f) != 1 || len > stringLengthThreshold)
    return false;

  if (len > 0)
  {
    std::vector<char> buf(len);
    if (fread(&buf[0], 1, buf.size(), f) != buf.size() || std::find(buf.begin(), buf.end(), 0) != buf.end())
      return false;  

    str = std::string(buf.begin(), buf.end());
  }
  else
  {
    str.clear();
  }

  return true;
}

static bool SkipString(FILE* f)
{
  uint32_t len = 0;
  if (fread(&len, sizeof(len), 1, f) != 1 || len > stringLengthThreshold)
    return false;

  fseek(f, len, SEEK_CUR);
  return true;
}

static void WriteString(FILE* f, std::string const& str)
{
  auto len = static_cast<uint32_t>(str.length());
  fwrite(&len, 1, sizeof(len), f);
  fwrite(str.c_str(), 1, str.length(), f);
}

template<typename StoredType, typename ValueType> bool ReadInt(FILE* f, ValueType& value)
{
  auto val = static_cast<StoredType>(0);
  bool success = fread(&val, sizeof(val), 1, f) == 1;
  value = success ? static_cast<ValueType>(val) : value;
  return success;
}

template<typename StoredType, typename ValueType> void WriteInt(FILE* f, ValueType value)
{
  auto val = static_cast<StoredType>(value);
  fwrite(&val, 1, sizeof(val), f);
}

static bool ReadSignedInt(FILE* f, int& value)
{
  return ReadInt<int32_t>(f, value);
}

static void WriteSignedInt(FILE* f, int value)
{
  WriteInt<int32_t>(f, value);
}

static bool ReadSignedChar(FILE* f, char& value)
{
  return ReadInt<int32_t>(f, value);
}

static void WriteSignedChar(FILE* f, char value)
{
  WriteInt<int32_t>(f, value);
}

static bool ReadUnsignedInt(FILE* f, size_t& value)
{
  return ReadInt<uint32_t>(f, value);
}

static void WriteUnsignedInt(FILE* f, size_t value)
{
  WriteInt<uint32_t>(f, value);
}

static bool ReadRepoRoot(FILE* f, std::string& repoRoot, std::string& singleFile)
{
  return ReadString(f, repoRoot) && ReadString(f, singleFile);
}

static bool SkipRepoRoot(FILE* f)
{
  return SkipString(f) && SkipString(f);
}

static void WriteTagInfo(FILE* f, TagInfo const& tag)
{
  WriteString(f, tag.name);
  WriteString(f, tag.file);
  WriteString(f, tag.re);
  WriteString(f, tag.info);
  WriteSignedInt(f, tag.lineno);
  WriteSignedChar(f, tag.type);
}

static bool ReadTagInfo(FILE* f, TagInfo& tag)
{
  TagInfo val;
  bool success = true;
  success = success && ReadString(f, val.name);
  success = success && ReadString(f, val.file);
  success = success && ReadString(f, val.re);
  success = success && ReadString(f, val.info);
  success = success && ReadSignedInt(f, val.lineno);
  success = success && ReadSignedChar(f, val.type);
  if (success)
    tag = std::move(val);

  return success;
}

static void WriteTagsStat(FILE* f, std::vector<std::pair<TagInfo, size_t>> const& stat)
{
  WriteUnsignedInt(f, stat.size());
  for (auto i = stat.rbegin(); i != stat.rend(); ++i)
  {
    WriteTagInfo(f, i->first);
    WriteUnsignedInt(f, i->second);
  }
}

static bool ReadTagsCache(FILE* f, Tags::Internal::TagsCache& cache, std::string const& tagsFile)
{
  size_t const capacityThreshold = 500;
  size_t sz = 0;
  if (!ReadUnsignedInt(f, sz) || sz > capacityThreshold)
    return false;

  cache.SetCapacity(sz);
  for (; !!sz; --sz)
  {
    TagInfo tag;
    if (!ReadTagInfo(f, tag))
      return false;

    size_t freq = 0;
    if (!ReadUnsignedInt(f, freq))
      return false;

    tag.Owner.TagsFile = tagsFile;
    cache.Insert(tag, freq);
  }

  return true;
}

static int ToInt(std::string const& str)
{
  try
  {
    return std::stoi(str);
  }
  catch(std::exception const&)
  {
  }

  return -1;
}

static bool LineMatches(char const* lineText, TagInfo const& tag)
{
  try
  {
    return !tag.re.empty() ? std::regex_match(lineText, std::regex(tag.re)) : false;
  }
  catch(std::exception const&)
  {
  }

  return false;
}

static void QuoteMeta(std::string& str)
{
  static char map[256];
  const char *toquote=".$^*()|+[]{}?";
  if(!map[(unsigned char)toquote[0]])
  {
    while(*toquote)
    {
      map[(unsigned char)*toquote]=1;
      toquote++;
    }
  }
  std::string dst;
  size_t i=1;
  dst+=str[0];
  if(str[1]=='^')
  {
    i++;
    dst+=str[1];
  }

  size_t j=0;
  if(str.length() >= 2 && str.back()=='$')j=1;
  for(;i<str.length()-j;i++)
  {
    if(map[(unsigned char)str[i]])
    {
      dst+='\\';
    }
    dst+=str[i];
  }
  if(j==1)dst+='$';
  else dst+=".+";
  str=dst;
}

static void ReplaceSpaces(std::string& str)
{
  std::string dst;
  size_t begin = str.length() > 2 && str.front() == '^' ? 1 : 0;
  size_t end = str.length() > 2 && str.back() == '$' ? str.size() - 1: str.size();
  for(size_t i = begin; i < end; ++i)
  {
    if(isspace(str[i]))
    {
      bool optional = i == begin;
      for (; i < end && isspace(str[i]); ++i);
      optional = optional || i == end;
      dst+=optional ? "\\s*" : "\\s+";
    }
    dst+=str[i];
  }
  str=dst;
}

static std::string MakeFilename(std::string const& str)
{
  std::string result = str;
  std::replace(result.begin(), result.end(), '/', '\\');
  result.erase(std::unique(result.begin(), result.end(), [](char a, char b) {return a == '\\' && a == b; }), result.end());
  return std::move(result);
}

inline bool IsLineEnd(char c)
{
  return !c || c == '\r' || c == '\n';
}

inline bool IsFieldEnd(char c)
{
  return IsLineEnd(c) || c == '\t';
}

inline bool NextField(char const*& cur, char const*& next, std::string const& separator = "\t")
{
  if (IsLineEnd(*next))
    return false;

  cur = cur != next ? next + 1 : cur;
  size_t separatorPos = 0;
  for (next = cur; separatorPos != separator.length() && !IsLineEnd(*next); ++next)
  {
    separatorPos = *next == separator[separatorPos] ? separatorPos + 1 :
                   *next == separator.front() ? 1 : 0;
  }

  next = separatorPos == separator.length() ? next - separator.length() : next;
  return next != cur;
}

bool ParseLine(const char* buf, TagFileInfo const& fi, TagInfo& result)
{
  char const* next = buf;
  if (!NextField(buf, next))
    return false; 

  result.name = std::string(buf, next);
  if (!NextField(buf, next))
    return false;

  result.file = MakeFilename(fi.GetFullPath(std::string(buf, next)));
  std::string const separator = ";\"";
  if (!NextField(buf, next, separator))
    return false;

  std::string excmd(buf, next);
  if(excmd.length() > 1 && excmd.front() == '/')
  {
    if (excmd.length() < 2 || excmd.back() != '/')
      return false;

    excmd = excmd.substr(1, excmd.length() - 2);
    QuoteMeta(excmd);
    ReplaceSpaces(excmd);
    result.re = std::move(excmd);
  }
  else
  {
    result.lineno=ToInt(excmd);
  }

  next += IsLineEnd(*next) ? 0 : separator.length();
  if (*next != '\t')
    return IsLineEnd(*next);

//TODO: support --fields=-k
  if (!NextField(buf, next))
    return false;

  result.type = *buf;
  if (!NextField(buf, next))
    return true;

  std::string const line = "line:";
  if (next > buf + line.length() && !line.compare(0, line.length(), buf, line.length()))
  {
    result.lineno=ToInt(std::string(buf + line.length(), next));
    if (!NextField(buf, next))
      return true;
  }

  result.info = std::string(buf, next);
  return true;
}

static char strbuf[16384];

struct LineInfo{
  char *line;
  int pos;
  char const *fn;
  char const *cls;
};

static bool CaseInsensitive = true;
static bool CaseSensitive = !CaseInsensitive;
static bool PartialCompare = true;
static bool FullCompare = !PartialCompare;

std::string ToLower(std::string const& str)
{
  auto result = str;
  std::transform(result.begin(), result.end(), result.begin(), ::tolower);
  return std::move(result);
}

inline int CharCmp(int left, int right, bool caseInsensitive)
{
  return caseInsensitive ? tolower(left) - tolower(right) : left - right;
}

inline int FieldCompare(char const* left, char const*& right, bool caseInsensitive, bool partialCompare)
{
  for (; left && !IsFieldEnd(*left) && right && !IsFieldEnd(*right) && !CharCmp(*left, *right, caseInsensitive); ++left, ++right);
  int leftChar = left && !IsFieldEnd(*left) ? *left : 0;
  int rightChar = right && !IsFieldEnd(*right) ? *right : 0;
  rightChar = partialCompare && !leftChar ? 0 : rightChar;
  return CharCmp(leftChar, rightChar, caseInsensitive);
}

inline bool FieldLess(char const* left, char const* right, bool caseInsensitive = false)
{
  return FieldCompare(left, right, caseInsensitive, FullCompare) < 0;
}

inline int PathCompare(char const* left, char const* &right, bool partialCompare)
{
  int cmp = 0;
  while (left && !IsFieldEnd(*left) && right && !IsFieldEnd(*right) && !cmp)
  {
    if (IsPathSeparator(*left) && IsPathSeparator(*right))
    {
      while (IsPathSeparator(*(++right)));
      while (IsPathSeparator(*(++left)));
    }
    else if (!(cmp = CharCmp(*left, *right, CaseInsensitive)))
    {
      ++left;
      ++right;
    }
  }

  int leftChar = left && !IsFieldEnd(*left) ? *left : 0;
  leftChar = IsPathSeparator(leftChar) ? '\\' : leftChar;
  int rightChar = right && !IsFieldEnd(*right) ? *right : 0;
  rightChar = partialCompare && !leftChar ? 0 : rightChar;
  rightChar = IsPathSeparator(rightChar) ? '\\' : rightChar;
  return CharCmp(leftChar, rightChar, CaseInsensitive);
}

inline bool PathLess(char const* left, char const* right)
{
  return PathCompare(left, right, FullCompare) < 0;
}

inline bool PathsEqual(const char* left, const char* right)
{
  return !PathCompare(left, right, FullCompare);
}

static char const* GetFilename(char const* path)
{
  char const* pos = path;
  for (; !IsFieldEnd(*path); ++path)
    pos = IsPathSeparator(*path) ? path : pos;

  for (; *pos && IsPathSeparator(*pos); ++pos);
  return pos;
}

inline char const* GetFilenameEnd(char const* path)
{
  for (; !IsFieldEnd(*path); ++path);
  return path;
}

static char const* FindClassFullQualification(char const* str)
{
  std::vector<std::string> const fieldNames = {
                                                "\tclass:"
                                               ,"\tstruct:"
                                               ,"\tunion:"
                                              };

  for (const auto& fieldName : fieldNames)
  {    
    if (auto ptr = strstr(str, fieldName.c_str()))
      return ptr + fieldName.length();
  }

  return nullptr;
}

inline bool IsScopeSeparator(char c)
{
  return c == '.'  // .  default
      || c == ':'  // :: c++, itcl, php, tcl, tcloo
      || c == '/'  // /  automake
      || c == '%'  // /% dtd parameter entry
      || c == '@'  // /@ dtd attribute
      || c == '\\' // \  php namespace
      || c == '-'  // -  rpm
  ;
}

static char const* ExtractClassName(char const* fullQualification)
{
  char const* className = fullQualification;
  for(; fullQualification && !IsFieldEnd(*fullQualification); ++fullQualification)
    if (IsScopeSeparator(*fullQualification))
      className = fullQualification + 1;

  return className && !IsFieldEnd(*className) ? className : nullptr;
}

//TODO: rework
char const* GetLine(char const* &str, std::string& buffer, FILE *f)
{
  buffer.clear();
  str = 0;
  char const* ptr = 0;
  do
  {
    ptr = fgets(strbuf, sizeof(strbuf), f);
    buffer += ptr ? ptr : "";
  }
  while(ptr && *buffer.rbegin() != '\n'); 
  str = !ptr && !buffer.length() ? nullptr : buffer.c_str();
  return str;
}

char const* GetLine(std::string& buffer, FILE *f)
{
  char const* tmp = 0;
  return GetLine(tmp, buffer, f);
}

static std::string GetIntersection(char const* left, char const* right)
{
  if (!left || !*left)
    return std::string(right, GetFilename(right));

  auto begining = left;
  for (; *left && !IsFieldEnd(*right) && *left == *right; ++left, ++right);
  return std::string(begining, left);
}

static void WriteOffsets(FILE* f, std::vector<LineInfo*>::iterator begin, std::vector<LineInfo*>::iterator end)
{
  auto sz = static_cast<OffsetType>(std::distance(begin, end));
  fwrite(&sz, sizeof(sz), 1, f);
  for (; begin != end; ++begin)
  {
    fwrite(&(*begin)->pos, sizeof((*begin)->pos), 1, f);
  }
}

static bool ReadOffsets(FILE* f, OffsetCont& offsets)
{
  auto sz = static_cast<OffsetType>(0);
  if (fread(&sz, sizeof(sz), 1, f) != 1)
    return false;

  offsets.resize(sz);
  return !sz ? true : fread(&offsets[0], sizeof(offsets[0]), sz, f) == offsets.size();
}

static bool SkipOffsets(FILE* f)
{
  auto sz = static_cast<OffsetType>(0);
  if (fread(&sz, sizeof(sz), 1, f) != 1)
    return false;

  fseek(f, sizeof(OffsetType) * sz, SEEK_CUR);
  return true;
}

std::shared_ptr<FILE> TagFileInfo::OpenTags(OffsetCont& offsets, IndexType index) const
{
  auto f = OpenIndex();
  if (!f)
    throw std::logic_error("Not synchronized");

  if (index == IndexType::Names)
    offsets = NamesOffsets;
  else
    GetOffsets(&*f, index).swap(offsets);

  return FOpen(filename.c_str(), "rb");
}

OffsetCont TagFileInfo::GetOffsets(FILE* f, IndexType type) const
{
  for (int i = 0; i != static_cast<int>(type); ++i)
  {
    if (!SkipOffsets(f))
      throw std::runtime_error("Invalid file format");
  }

  OffsetCont result;
  if (!ReadOffsets(f, result))
    throw std::runtime_error("Invalid file format");

  return std::move(result);
}

void TagFileInfo::FlushCachedTags()
{
  auto f = OpenIndex("r+b");
  if (!f)
    return;

  for (int i = 0; i != static_cast<int>(IndexType::EndOfEnum); ++i)
  {
    if (!SkipOffsets(&*f))
      return;
  }
  
  WriteTagsStat(&*f, NamesCache->GetStat());
  WriteTagsStat(&*f, FilesCache->GetStat());
  Truncate(&*f, ftell(&*f));
  CloseIndexFile(std::move(f));
}

static std::vector<std::pair<TagInfo, size_t>> RefreshNamesCache(TagFileInfo* fi, FILE* f, OffsetCont const& offsets, std::vector<std::pair<TagInfo, size_t>>&& tagsWithFreq);
static std::vector<std::pair<TagInfo, size_t>> RefreshFilesCache(TagFileInfo* fi, FILE* f, OffsetCont const& offsets, std::vector<std::pair<TagInfo, size_t>>&& tagsWithFreq);

bool TagFileInfo::CreateIndex(time_t tagsModTime, bool singleFileRepos)
{
  TagFileInfo* fi = this;
  int pos=0;
  FILE *f=fopen(fi->filename.c_str(),"rb");
  if(!f)return false;
  fseek(f,0,SEEK_END);
  int sz=ftell(f);
  fseek(f,0,SEEK_SET);
  std::vector<LineInfo*> lines;
  std::vector<LineInfo*> classes;
  lines.reserve(sz/80);

  std::string buffer;
  char const* strbuf;
  if(!GetLine(strbuf, buffer, f) || strncmp(strbuf,"!_TAG_FILE_FORMAT",17))
  {
    fclose(f);
    return false;
  }

  LineInfo *li;
  struct LIPool{
    LineInfo *ptr;
    int used;
    int size;
    LIPool *next;
    LineInfo* getNext(LIPool*& page)
    {
      if(used==size)
      {
        next=new LIPool(size);
        page=page->next;
      }
      return &page->ptr[page->used++];
    }
    LIPool(int count)
    {
      size=count;
      ptr=new LineInfo[size];
      used=0;
      next=0;
    }
    ~LIPool()
    {
      delete [] ptr;
    }
  };
  LIPool *poolfirst,*pool;
  poolfirst=new LIPool(sz/100);
  pool=poolfirst;
  char *linespool=new char[sz+16];
  size_t linespoolpos=0;


  pos=ftell(f);
  bool sorted=true;
  std::string pathIntersection;
  while(GetLine(strbuf, buffer, f))
  {
    if(strbuf[0]=='!')
    {
      pos=ftell(f);
      continue;
    }
    li=pool->getNext(pool);
    auto len=buffer.length();
    li->line=linespool+linespoolpos;
    if(strbuf[len-1]==0x0d || strbuf[len-1]==0x0a)len--;
    memcpy(li->line,strbuf,len);
    li->line[len]=0;
    if(sorted && lines.size()>1 && strcmp(li->line,lines.back()->line)<0)
    {
      sorted=false;
    }
    linespoolpos+=len+1;
    //strdup(strbuf);
    li->pos=pos;
    for(li->fn = li->line; !IsFieldEnd(*li->fn); ++li->fn);
    li->fn += *li->fn ? 1 : 0;
    singleFileRepos = singleFileRepos && (lines.empty() || PathsEqual(lines.back()->fn, li->fn));
    pathIntersection = IsFullPath(li->fn) ? GetIntersection(pathIntersection.c_str(), li->fn) : pathIntersection;
    if (li->cls = ExtractClassName(FindClassFullQualification(li->line)))
      classes.push_back(li);

    lines.push_back(li);
//    fi->offsets.Push(pos);
    pos=ftell(f);
  }
  for (; !pathIntersection.empty() && IsPathSeparator(pathIntersection.back()); pathIntersection.resize(pathIntersection.length() - 1));
  fullpathrepo = !pathIntersection.empty();
  reporoot = pathIntersection.empty() ? GetDirOfFile(filename) : MakeFilename(pathIntersection);
  singlefile = singleFileRepos && !lines.empty() ? std::string(GetFilename(lines.back()->fn), GetFilenameEnd(lines.back()->fn)) : "";
  FILE *g=fopen(fi->indexFile.c_str(),"wb");
  if(!g)
  {
    delete [] linespool;
    while(poolfirst)
    {
      pool=poolfirst->next;
      delete poolfirst;
      poolfirst=pool;
    }
    return false;
  }
  fwrite(IndexFileSignature, 1, sizeof(IndexFileSignature), g);
  fwrite(&tagsModTime,sizeof(tagsModTime),1,g);
  WriteString(g, fullpathrepo ? reporoot : std::string());
  WriteString(g, singlefile);
  std::sort(lines.begin(), lines.end(), [](LineInfo* left, LineInfo* right) { return FieldLess(left->line, right->line); });
  OffsetCont namesOffsets;
  std::transform(lines.begin(), lines.end(), std::back_inserter(namesOffsets), [](LineInfo* line){ return line->pos; });
  WriteOffsets(g, lines.begin(), lines.end());
  std::sort(lines.begin(), lines.end(), [](LineInfo* left, LineInfo* right) { return FieldLess(left->line, right->line, CaseInsensitive); });
  WriteOffsets(g, lines.begin(), lines.end());
  std::sort(lines.begin(), lines.end(), [](LineInfo* left, LineInfo* right) { return PathLess(left->fn, right->fn); });
  WriteOffsets(g, lines.begin(), lines.end());
  std::sort(classes.begin(), classes.end(), [](LineInfo* left, LineInfo* right) { return FieldLess(left->cls, right->cls); });
  WriteOffsets(g, classes.begin(), classes.end());
  auto linesEnd = std::unique(lines.begin(), lines.end(), [](LineInfo* left, LineInfo* right) { return PathsEqual(left->fn, right->fn); });
  std::sort(lines.begin(), linesEnd, [](LineInfo* left, LineInfo* right) { return FieldLess(GetFilename(left->fn), GetFilename(right->fn), CaseInsensitive); });
  OffsetCont filesOffsets;
  std::transform(lines.begin(), linesEnd, std::back_inserter(filesOffsets), [](LineInfo* line){ return line->pos; });
  WriteOffsets(g, lines.begin(), linesEnd);
  WriteTagsStat(g, RefreshNamesCache(fi, f, namesOffsets, NamesCache->GetStat()));
  WriteTagsStat(g, RefreshFilesCache(fi, f, filesOffsets, FilesCache->GetStat()));
  fclose(f);
  delete [] linespool;
  while(poolfirst)
  {
    pool=poolfirst->next;
    delete poolfirst;
    poolfirst=pool;
  }
  fclose(g);
  return LoadCache();
}

bool TagFileInfo::LoadCache()
{
  IndexModTime = 0;
  auto f = FOpen(indexFile.c_str(), "r+b");
  if (!f || !ReadSignature(&*f))
    return false;

  fseek(&*f, sizeof(time_t), SEEK_CUR);
  if (!ReadRepoRoot(&*f, reporoot, singlefile))
    return false;

  fullpathrepo = !reporoot.empty();
  reporoot = reporoot.empty() ? GetDirOfFile(filename) : reporoot;
  if (!ReadOffsets(&*f, NamesOffsets))
    return false;

  for (int i = 1; i != static_cast<int>(IndexType::EndOfEnum); ++i)
  {
    if (!SkipOffsets(&*f))
      return false;
  }

  if (!IsEndOfFile(&*f))
  {
    auto namesCache = Tags::Internal::CreateTagsCache(0);
    auto filesCache = Tags::Internal::CreateTagsCache(0);
    auto tagsCacheBegins = ftell(&*f);
    if (!ReadTagsCache(&*f, *namesCache, filename) || !ReadTagsCache(&*f, *filesCache, filename))
    {
      NamesCache = Tags::Internal::CreateTagsCache(0);
      FilesCache = Tags::Internal::CreateTagsCache(0);
      Truncate(&*f, tagsCacheBegins);
    }
    else
    {
      NamesCache = std::move(namesCache);
      FilesCache = std::move(filesCache);
    }
  }

  CloseIndexFile(std::move(f));
  return !!IndexModTime;
}

std::shared_ptr<FILE> TagFileInfo::OpenIndex(char const* mode) const
{
  if (IndexModified())
    return std::shared_ptr<FILE>();

  struct stat tagsStat;
  if (stat(filename.c_str(), &tagsStat) == -1)
    return std::shared_ptr<FILE>();

  auto f = FOpen(indexFile.c_str(), mode);
  if (!f || !ReadSignature(&*f))
    return std::shared_ptr<FILE>();

  time_t storedTagsModTime = 0;
  if (fread(&storedTagsModTime, sizeof(storedTagsModTime), 1, &*f) != 1 || storedTagsModTime != tagsStat.st_mtime)
    return std::shared_ptr<FILE>();

  return SkipRepoRoot(&*f) ? std::move(f) : std::shared_ptr<FILE>();
}

int TagFileInfo::Load(size_t& symbolsLoaded)
{
  struct stat st;
  if (stat(filename.c_str(), &st) == -1)
//TODO: return Error(...)
    return ENOENT;

  if (IndexModified())
    LoadCache();

  if ((!IndexModTime || !Synchronized()) && !CreateIndex(st.st_mtime, singlefilerepos))
  {
    remove(indexFile.c_str());
//TODO: return Error(...)
    return EIO;
  }

  symbolsLoaded = NamesOffsets.size();
  return 0;
}

char const* TagFileInfo::GetRelativePath(char const* fileName) const
{
  if (reporoot.empty() || IsPathSeparator(reporoot.back()))
    throw std::logic_error("Invalid reporoot");

  if (!!PathCompare(reporoot.c_str(), fileName, PartialCompare) || (*fileName && !IsPathSeparator(*fileName)))
    return nullptr;

  for (; IsPathSeparator(*fileName); ++fileName);
  return !*fileName || (!singlefile.empty() && !PathsEqual(fileName, singlefile.c_str())) ? nullptr : fileName;
}

std::string TagFileInfo::GetFullPath(std::string const& relativePath) const
{
  return IsFullPath(relativePath.c_str()) ? relativePath : JoinPath(reporoot, relativePath);
}

int Load(const char* filename, bool singleFileRepos, size_t& symbolsLoaded)
{
  return Storage->Load(filename, singleFileRepos ? Tags::RepositoryType::Temporary : Tags::RepositoryType::Regular, symbolsLoaded);
}

class MatchVisitor
{
public:
  MatchVisitor(std::string const& pattern)
    : Pattern(pattern)
  {
  }

  MatchVisitor(std::string&& pattern)
    : Pattern(std::move(pattern))
  {
  }

  virtual ~MatchVisitor() = default;

  std::string const& GetPattern() const
  {
    return Pattern;
  }

  virtual int Compare(char const*& strbuf) const = 0;

  virtual bool Filter(TagInfo const& tag) const
  {
    return true;
  }

private:
  std::string Pattern;
};

class NameMatch : public MatchVisitor
{
public:
  NameMatch(char const* name, bool comparationType, bool caseSensitivity)
    : MatchVisitor(name)
    , ComparationType(comparationType)
    , CaseSensitivity(caseSensitivity)
  {
  }

  int Compare(char const*& strbuf) const override
  {
    return FieldCompare(GetPattern().c_str(), strbuf, CaseSensitivity, ComparationType);
  }

private:
  bool ComparationType;
  bool CaseSensitivity;
};

class FilenameMatch : public MatchVisitor
{
public:
  FilenameMatch(std::string&& namePart, std::string&& filter, bool comparationType)
    : MatchVisitor(std::move(namePart))
    , PathFilter(std::move(filter))
    , ComparationType(comparationType)
  {
  }

  int Compare(char const*& strbuf) const override
  {
    for (; !IsFieldEnd(*strbuf); ++strbuf);
    if (!*strbuf)
    //TODO: replace with Error(MNotTagFile)
      throw std::runtime_error("Invalid tags file format");

    strbuf = GetFilename(++strbuf);
    return FieldCompare(GetPattern().c_str(), strbuf, CaseInsensitive, ComparationType);
  }

  bool Filter(TagInfo const& tag) const override
  {
    if (PathFilter.empty())
      return true;

    std::string const& fullPath = tag.file;
    auto pathIter = std::find_if(fullPath.rbegin(), fullPath.rend(), [](char c) { return IsPathSeparator(c); });
    for (; pathIter != fullPath.rend() && IsPathSeparator(*pathIter); ++pathIter);
    auto filterIter = PathFilter.rbegin();
    int cmp = 0;
////TODO: parameterize PathCompare with direction and remove code duplication
    while (pathIter != fullPath.rend() && filterIter != PathFilter.rend() && !cmp)
    {
      if (IsPathSeparator(*pathIter) && IsPathSeparator(*filterIter))
      {
        for (; pathIter != fullPath.rend() && IsPathSeparator(*pathIter); ++pathIter);
        for (; filterIter != PathFilter.rend() && IsPathSeparator(*filterIter); ++filterIter);
      }
      else if (!(cmp = CharCmp(*pathIter, *filterIter, CaseInsensitive)))
      {
        ++pathIter;
        ++filterIter;
      }
    }

    return filterIter == PathFilter.rend() && (pathIter == fullPath.rend() || IsPathSeparator(*pathIter));
  }

private:
  std::string PathFilter;
  bool ComparationType;
};

class ClassMemberMatch : public MatchVisitor
{
public:
  ClassMemberMatch(char const* classname)
    : MatchVisitor(classname)
  {
  }

  int Compare(char const*& strbuf) const override
  {
    strbuf = ExtractClassName(FindClassFullQualification(strbuf));
    if (!strbuf)
      //TODO: throw Error()
      throw std::runtime_error("Tags file modified");

    return FieldCompare(GetPattern().c_str(), strbuf, CaseSensitive, FullCompare);
  }
};

class PathMatch : public MatchVisitor
{
public:
  PathMatch(char const* path)
    : MatchVisitor(path)
  {
  }

  int Compare(char const*& strbuf) const override
  {
    for (; !IsFieldEnd(*strbuf); ++strbuf);
    if (!*strbuf)
    //TODO: replace with Error(MNotTagFile)
      throw std::runtime_error("Invalid tags file format");

    ++strbuf;
    return PathCompare(GetPattern().c_str(), strbuf, FullCompare);
  }
};

static std::tuple<size_t, size_t, size_t> GetMatchedOffsetRange(FILE* f, OffsetCont const& offsets, MatchVisitor const& visitor)
{
  if (offsets.empty() || visitor.GetPattern().empty())
    return std::make_tuple(0, 0, offsets.size());

  size_t pos;
  size_t left=0;
  size_t right=offsets.size();
  int cmp;
  std::string buffer;
  char const* strbuf = nullptr;
  while(left < right)
  {
    pos=(right+left)/2;
    fseek(f,offsets[pos],SEEK_SET);
    strbuf = GetLine(buffer,f);
    cmp=visitor.Compare(strbuf);
    if(!cmp)
    {
      left = right = pos;
    }else if(cmp < 0)
    {
      right=pos;
    }else
    {
      left=pos+1;
    }
  }
  if(!cmp)
  {
    size_t endpos=pos;
    size_t exactmatchend=IsFieldEnd(*strbuf) ? pos + 1 : pos;
    while(pos>0)
    {
      fseek(f,offsets[pos-1],SEEK_SET);
      strbuf = GetLine(buffer,f);
      if(!visitor.Compare(strbuf))
      {
        pos--;
        exactmatchend = !IsFieldEnd(*strbuf) ? pos : exactmatchend;
      }else
      {
        break;
      }
    }
    while(endpos<offsets.size()-1)
    {
      fseek(f,offsets[endpos+1],SEEK_SET);
      strbuf = GetLine(buffer,f);
      if(!visitor.Compare(strbuf))
      {
        endpos++;
        exactmatchend = IsFieldEnd(*strbuf) ? endpos + 1 : exactmatchend;
      }else
      {
        break;
      }
    }
    ++endpos;
    return std::make_tuple(pos, exactmatchend, endpos);
  }
  return std::make_tuple(0, 0, 0);
}

static std::vector<TagInfo> GetMatchedTags(TagFileInfo const* fi, FILE* f, OffsetCont const& offsets, MatchVisitor const& visitor, size_t maxCount)
{
  std::vector<TagInfo> result;
  auto range = GetMatchedOffsetRange(&*f, offsets, visitor);
  maxCount = !maxCount ? std::numeric_limits<size_t>::max() : maxCount;
  for(auto i = std::get<0>(range); i < std::get<1>(range) || (maxCount > 0 && i < std::get<2>(range)); ++i)
  {
    fseek(&*f, offsets[i], SEEK_SET);
    std::string line;
    GetLine(line, &*f);
    TagInfo tag;
    if (ParseLine(line.c_str(), *fi, tag) && visitor.Filter(tag))
    {
      tag.Owner = {fi->GetName()};
      result.push_back(std::move(tag));
      maxCount -= maxCount > 0 ? 1 : 0;
    }
  }
  return std::move(result);
}

static std::vector<TagInfo> GetMatchedTags(TagFileInfo const* fi, IndexType index, MatchVisitor const& visitor, size_t maxCount)
{
  if (visitor.GetPattern().empty() && !maxCount)
    return std::vector<TagInfo>();

  OffsetCont offsets;
  auto f = fi->OpenTags(offsets, index);
  return !f ? std::vector<TagInfo>() : GetMatchedTags(fi, &*f, offsets, visitor, maxCount);
}

class TagsLess
{
public:
  TagsLess(char const* file, SortingOptions sortOptions)
    : File(file)
    , Options(sortOptions)
  {
  }

  bool operator() (TagInfo const& left, TagInfo const& right) const
  {
    auto leftSamePath = !!(Options & SortingOptions::CurFileFirst) && PathsEqual(left.file.c_str(), File);
    auto rightSamePath = !!(Options & SortingOptions::CurFileFirst) && PathsEqual(right.file.c_str(), File);
    if (leftSamePath != rightSamePath)
      return leftSamePath && !rightSamePath;
  
    int cmp = 0;
    if (!!(Options & SortingOptions::SortByName) && (cmp = left.name.compare(right.name)))
      return cmp < 0;

    if (cmp = left.file.compare(right.file))
      return cmp < 0;

    return left.lineno < right.lineno;
  }

private:
  char const* File;
  SortingOptions const Options;
};

std::vector<TagInfo> SortTags(std::vector<TagInfo>&& tags, char const* file, SortingOptions sortOptions)
{
  if (sortOptions != SortingOptions::DoNotSort)
    std::sort(tags.begin(), tags.end(), TagsLess(file, sortOptions));

  return std::move(tags);
}

std::vector<TagInfo> Find(const char* name, const char* file, SortingOptions sortOptions)
{
  return Storage->GetSelector(file, false, sortOptions)->GetByName(name);
}

std::vector<TagInfo> FindPartiallyMatchedTags(const char* file, const char* part, size_t maxCount, bool caseInsensitive, SortingOptions sortOptions)
{
  return Storage->GetSelector(file, caseInsensitive, sortOptions, maxCount)->GetByPart(part, false);
}

static std::tuple<std::string, std::string, int> GetNamePathLine(char const* path)
{
  for (; *path && IsPathSeparator(*path); ++path);
  auto pathEnd = path;
  for (; *pathEnd; ++pathEnd);
  pathEnd -= pathEnd != path && *(pathEnd - 1) == ':' ? 1 : 0;
  char separator = pathEnd != path && *(pathEnd - 1) == ')' ? '(' : ':';
  pathEnd -= pathEnd != path && *(pathEnd - 1) == ')' ? 1 : 0;
  auto linenumBegin = pathEnd;
  auto linenumEnd = pathEnd;
  size_t const linenumLimit = 5;
  for (; linenumEnd - linenumBegin < linenumLimit && linenumBegin != path && linenumBegin - 1 != path && isdigit(*(linenumBegin - 1)); --linenumBegin);
  pathEnd = linenumBegin != path && *(linenumBegin - 1) == separator ? linenumBegin - 1 : pathEnd;
  linenumBegin = pathEnd != linenumBegin - 1 ? linenumEnd : linenumBegin;
  for (; pathEnd != path && pathEnd - 1 != path && IsPathSeparator(*(pathEnd - 1)); --pathEnd);
  auto nameBegin = pathEnd - 1;
  for (; nameBegin != path - 1 && !IsPathSeparator(*nameBegin); --nameBegin);
  auto name = std::string(nameBegin + 1, pathEnd);
  for (; nameBegin != path - 1 && IsPathSeparator(*nameBegin); --nameBegin);
  int lineNum = linenumBegin == linenumEnd ? -1 : std::stoi(std::string(linenumBegin, linenumEnd));
  return std::make_tuple(std::move(name), std::string(path, nameBegin + 1), lineNum);
}

std::vector<TagInfo> FindFile(const char* file, const char* path)
{
  return Storage->GetSelector(file, true, SortingOptions::Default)->GetFiles(path);
}

std::vector<TagInfo> FindPartiallyMatchedFile(const char* file, const char* part, size_t maxCount)
{
  return Storage->GetSelector(file, true, SortingOptions::Default, maxCount)->GetByPart(part, true);
}

std::vector<TagInfo> FindClassMembers(const char* file, const char* classname, SortingOptions sortOptions)
{
  return Storage->GetSelector(file, false, sortOptions)->GetClassMembers(classname);
}

std::vector<TagInfo> FindFileSymbols(const char* file)
{
  return Storage->GetSelector(file, false, SortingOptions::Default)->GetByFile(file);
}

static std::vector<std::string> ToTagsPaths(std::vector<Tags::RepositoryInfo> &&infos)
{
  std::vector<std::string> result;
  std::transform(infos.begin(), infos.end(), std::back_inserter(result), [](Tags::RepositoryInfo const& info){ return info.TagsPath; });
  return std::move(result);
}

std::vector<std::string> GetFiles()
{
  return ToTagsPaths(Storage->GetAll());
}

std::vector<std::string> GetLoadedTags(const char* file)
{
  return ToTagsPaths(Storage->GetInvolved(file));
}

void UnloadTags(const char* tagsFile)
{
  Storage->Remove(tagsFile);
}

void UnloadAllTags()
{
  Storage = Tags::RepositoryStorage::Create();
}

bool IsTagFile(const char* file)
{
  FILE *f = fopen(file, "rt");
  if (!f)
    return false;

  std::string const pattern("!_TAG_FILE_FORMAT");
  char strbuf[256];
  char *result = fgets(strbuf, sizeof(strbuf), f);
  fclose(f);
  return result && !pattern.compare(0, std::string::npos, result, pattern.length());
};

std::vector<TagInfo>::const_iterator FindContextTag(std::vector<TagInfo> const& tags, char const* fileName, int lineNumber, char const* lineText)
{
  auto possibleContext = tags.end();
  bool possibleContextUniq = true;
  for (auto i = tags.begin(); i != tags.end(); ++i)
  {
    if (!PathsEqual(fileName, i->file.c_str()))
      continue;

    bool lineEqual = i->lineno > 0 && i->lineno - 1 == lineNumber;
    bool lineMatches = LineMatches(lineText, *i);
    if (lineEqual && lineMatches)
      return i;

    possibleContextUniq = (lineEqual || lineMatches) && possibleContext != tags.end() ? false : possibleContextUniq;
    possibleContext = lineEqual || lineMatches ? i : possibleContext;
  }

  return possibleContextUniq ? possibleContext : tags.end();
}

inline bool TypesOpposite(char left, char right)
{
  std::unordered_map<char, char> const oppositeTypes = {{'f', 'p'}, {'p', 'f'}, {'m', 'm'}};
  auto t = oppositeTypes.find(left);
  return t != oppositeTypes.end() && t->second == right;
}

inline bool InfoEqual(std::string const& left, std::string const& right)
{
  std::string leftInfo = "\t" + left;
  std::string rightInfo = "\t" + right;
  auto leftClass = FindClassFullQualification(leftInfo.c_str());
  leftClass = leftClass ? leftClass : left.c_str();
  auto rightClass = FindClassFullQualification(rightInfo.c_str());
  rightClass = rightClass ? rightClass : right.c_str();
  return !strcmp(leftClass, rightClass);
}

inline bool TagsOpposite(TagInfo const& left, TagInfo const& right)
{
  bool tagsNotEqual = left.lineno != right.lineno || !PathsEqual(left.file.c_str(), right.file.c_str());
  return tagsNotEqual && TypesOpposite(left.type, right.type) && InfoEqual(left.info, right.info);
}

std::vector<TagInfo>::const_iterator Reorder(TagInfo const& context, std::vector<TagInfo>& tags)
{
  return std::stable_partition(tags.begin(), tags.end(), [&](TagInfo const& tag) { return TagsOpposite(tag, context); });
}

void CacheTag(TagInfo const& tag, size_t cacheSize, bool flush)
{
  Storage->CacheTag(tag, cacheSize, flush);
}

void EraseCachedTag(TagInfo const& tag, bool flush)
{
  Storage->EraseCachedTag(tag, flush);
}

std::vector<TagInfo> GetCachedTags(const char* file, size_t limit, bool getFiles)
{
  return Storage->GetSelector(file, false, SortingOptions::Default, limit)->GetCachedTags(getFiles);
}

class TagMatch : public NameMatch
{
public:
  TagMatch(TagInfo const& tag)
    : NameMatch(tag.name.c_str(), FullCompare, CaseSensitive)
    , Tag(tag)
  {
  }

  virtual bool Filter(TagInfo const& tag) const
  {
    return Tag.type == tag.type &&
           Tag.info == tag.info &&
           Tag.re == tag.re &&
           Tag.file == tag.file &&
           true;
  }

private:
  TagInfo const& Tag;
};

static std::vector<std::pair<TagInfo, size_t>> RefreshNamesCache(TagFileInfo* fi, FILE* f, OffsetCont const& offsets, std::vector<std::pair<TagInfo, size_t>>&& tagsWithFreq)
{
  auto cur = tagsWithFreq.begin();
  for (auto i = tagsWithFreq.begin(); i != tagsWithFreq.end(); ++i)
  {
    auto visitor = TagMatch(i->first);
    auto foundTags = GetMatchedTags(fi, f, offsets, visitor, 0);
    if (!foundTags.empty())
      (cur++)->first = std::move(foundTags.back());
  }

  tagsWithFreq.erase(cur, tagsWithFreq.end());
  return std::move(tagsWithFreq);
}

static std::vector<std::pair<TagInfo, size_t>> RefreshFilesCache(TagFileInfo* fi, FILE* f, OffsetCont const& offsets, std::vector<std::pair<TagInfo, size_t>>&& tagsWithFreq)
{
//TODO: refactor duplicated code
  auto cur = tagsWithFreq.begin();
  for (auto i = tagsWithFreq.begin(); i != tagsWithFreq.end(); ++i)
  {
    auto namePathLine = GetNamePathLine(i->first.file.c_str());
    auto visitor = FilenameMatch(std::move(std::get<0>(namePathLine)), std::move(std::get<1>(namePathLine)), FullCompare);
    auto foundTags = GetMatchedTags(fi, f, offsets, visitor, 0);
    if (!foundTags.empty())
      (cur++)->first = MakeFileTag(-1)(std::move(foundTags.back()));
  }

  tagsWithFreq.erase(cur, tagsWithFreq.end());
  return std::move(tagsWithFreq);
}

namespace
{
  class RepositoryImpl : public Tags::Internal::Repository
  {
  public:
    RepositoryImpl(char const* filename, bool singleFileRepos)
      : Info(filename, singleFileRepos)
    {
    }

    int Load(size_t& symbolsLoaded) override
    {
      return Info.Load(symbolsLoaded);
    }

    bool Belongs(char const* file) const override
    {
      return !!Info.GetRelativePath(file);
    }

    int CompareTagsPath(const char* tagsPath) const override
    {
      return PathCompare(Info.GetName().c_str(), tagsPath, FullCompare);
    }

    std::string TagsPath() const override
    {
      return Info.GetName();
    }

    std::vector<TagInfo> FindByName(const char* name) const override
    {
      return GetMatchedTags(&Info, IndexType::Names, NameMatch(name, FullCompare, CaseSensitive), 0);
    }

    std::vector<TagInfo> FindByName(const char* part, size_t maxCount, bool caseInsensitive) const override
    {
      return GetMatchedTags(&Info, caseInsensitive ? IndexType::NamesCaseInsensitive : IndexType::Names, NameMatch(part, PartialCompare, caseInsensitive), maxCount);
    }

    std::vector<TagInfo> FindFiles(const char* path) const override
    {
      return FindFilesImpl(path, FullCompare, 0);
    }

    std::vector<TagInfo> FindFiles(const char* part, size_t maxCount) const override
    {
      return FindFilesImpl(part, PartialCompare, maxCount);
    }

    std::vector<TagInfo> FindClassMembers(const char* classname) const override
    {
      return GetMatchedTags(&Info, IndexType::Classes, ClassMemberMatch(classname), 0);
    }

    std::vector<TagInfo> FindByFile(const char* file) const override
    {
      auto relativePath = Info.GetRelativePath(file);
      return !relativePath || !*relativePath ? std::vector<TagInfo>() : GetMatchedTags(&Info, IndexType::Paths, PathMatch(Info.IsFullPathRepo() ? file : relativePath), 0);
    }

    void CacheTag(TagInfo const& tag, size_t cacheSize, bool flush) override
    {
      Info.CacheTag(tag, cacheSize);
      if (flush)
        Info.FlushCachedTags();
    }

    void EraseCachedTag(TagInfo const& tag, bool flush) override
    {
      Info.EraseCachedTag(tag);
      if (flush)
        Info.FlushCachedTags();
    }

    std::vector<std::pair<TagInfo, size_t>> GetCachedTags(bool getFiles) const override
    {
      return Info.GetCachedTags(getFiles);
    }

  private:
    std::vector<TagInfo> FindFilesImpl(const char* part, bool comparationType, size_t maxCount) const
    {
      auto namePathLine = GetNamePathLine(part);
      auto tags = GetMatchedTags(&Info, IndexType::Filenames, FilenameMatch(std::move(std::get<0>(namePathLine)), std::move(std::get<1>(namePathLine)), comparationType), maxCount);
      std::transform(std::make_move_iterator(tags.begin()), std::make_move_iterator(tags.end()), tags.begin(), MakeFileTag(std::get<2>(namePathLine)));
      return std::move(tags);
    }

    TagFileInfo Info;
  };
}

namespace Tags
{
  namespace Internal
  {
    std::unique_ptr<Repository> Repository::Create(const char* filename, bool singleFileRepos)
    {
      return std::unique_ptr<Repository>(new RepositoryImpl(filename, singleFileRepos));
    }
  }
}
