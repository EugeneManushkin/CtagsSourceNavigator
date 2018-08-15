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
#include <regex>
#include <set>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/utime.h>
#include <string>
#include <string.h>
#include <unordered_map>
#include <vector>
#include <memory>
#define NOMINMAX
#include <windows.h>
#include "tags.h"

static bool FileExists(char const* filename)
{
  return GetFileAttributesA(filename) != INVALID_FILE_ATTRIBUTES;
}

inline bool IsPathSeparator(std::string::value_type c)
{
    return c == '/' || c == '\\';
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
};

struct TagFileInfo{
  TagFileInfo(char const* fname)
    : filename(fname)
    , indexFile(filename + ".idx")
    , modtm(0)
  {
    if (filename.empty() || IsPathSeparator(filename.back()))
      throw std::logic_error("Invalid tags file name");
  }

  OffsetCont offsets;

  char const* GetRelativePath(char const* fileName) const;

  std::string GetFullPath(std::string const& relativePath) const;

  bool HasName(char const* fileName) const;

  bool IsFullPathRepo() const
  {
    return fullpathrepo;
  }

  //TODO: rework: filename must be hidden
  std::string const& GetName() const
  {
    return filename;
  }

  FILE* OpenTags()
  {
    return !Load() ? fopen(filename.c_str(), "rb") : nullptr;
  }

  OffsetCont GetOffsets(IndexType type);

  int Load();

private:
  int CreateIndex(time_t tagsModTime);
  bool LoadIndex(time_t tagsModTime);
  FILE* OpenIndex()
  {
    modtm = !FileExists(indexFile.c_str()) ? 0 : modtm;
    return !Load() ? fopen(indexFile.c_str(), "rb") : nullptr;
  }

  std::string filename;
  std::string indexFile;
  std::string reporoot;
  bool fullpathrepo;
  time_t modtm;
};

using TagFileInfoPtr = std::shared_ptr<TagFileInfo>;
std::vector<TagFileInfoPtr> files;

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

char const IndexFileSignature[] = "tags.idx.v6";

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

static bool ReadRepoRoot(FILE* f, std::string& repoRoot)
{
  uint32_t rootLen = 0;
  if (fread(&rootLen, sizeof(rootLen), 1, f) != 1)
    return false;

  if (rootLen > 0)
  {
    std::vector<char> buf(rootLen);
    if (fread(&buf[0], 1, buf.size(), f) != buf.size())
      return false;  

    repoRoot = std::string(buf.begin(), buf.end());
  }
  else
  {
    repoRoot.clear();
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
    return tag.re.length() > 2 ? std::regex_match(lineText, std::regex(tag.re.substr(1, tag.re.length() - 2))) : false;
  }
  catch(std::exception const&)
  {
  }

  return false;
}

//int Msg(const char*);

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

  size_t j=1;
  if(str.length() >= 2 && str[str.length() - 2]=='$')j=2;
  for(;i<str.length()-j;i++)
  {
    if(map[(unsigned char)str[i]])
    {
      dst+='\\';
    }
    dst+=str[i];
  }
  if(j==2)dst+='$';
  dst+='/';
  str=dst;
}

static void ReplaceSpaces(std::string& str)
{
  std::string dst;
  for(size_t i=0;i<str.length();i++)
  {
    if(str[i]==' ')
    {
      while(i<str.length() && str[i]==' ')i++;
      dst+="\\s+";
    }
    dst+=str[i];
  }
  str=dst;
}

//TODO: consider optimization
static std::string MakeDeclaration(std::string const& str)
{
  std::string declaration = str;
  declaration = declaration.length() > 4 ? declaration.substr(2, declaration.length() - 4) : declaration;
  std::replace(declaration.begin(), declaration.end(), '\t', ' ');
  declaration.resize(std::unique(declaration.begin(), declaration.end(), [](char a, char b) {return a == ' ' && a == b;}) - declaration.begin());
  declaration = !declaration.empty() && *declaration.begin() == ' ' ? declaration.substr(1) : declaration;
  declaration = !declaration.empty() && *declaration.rbegin() == ' ' ? declaration.substr(0, declaration.length() - 1) : declaration;
  return declaration;
}

static std::string MakeFilename(std::string const& str)
{
  std::string result = str;
  result.resize(std::unique(result.begin(), result.end(), [](char a, char b) {return a == '\\' && a == b; }) - result.begin());
  std::replace(result.begin(), result.end(), '/', '\\');
  return result;
}

inline bool IsFieldEnd(char c)
{
  return !c || c == '\r' || c == '\n' || c == '\t';
}

inline bool NextField(char const*& cur, char const*& next)
{
  if (IsFieldEnd(*next) && *next != '\t')
    return false;

  char const* const end = nullptr;
  cur = cur != next ? next + 1 : cur;
  for (next = cur; !IsFieldEnd(*next); ++next);
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
  if (!NextField(buf, next))
    return false;

  std::string const separator = ";\"";
  if (next <= buf + separator.length() || separator.compare(0, separator.length(), next - separator.length(), separator.length()))
    return false;

  std::string excmd(buf, next - separator.length());
  if(excmd.length() > 1 && excmd.front() == '/')
  {
    if (excmd.length() < 2 && excmd.back() != '/')
      return false;

    result.declaration = MakeDeclaration(excmd);
    QuoteMeta(excmd);
    ReplaceSpaces(excmd);
    result.re = std::move(excmd);
  }
  else
  {
    result.lineno=ToInt(excmd);
  }

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
    return right;

  auto begining = left;
  for (; *left && !IsFieldEnd(*right) && *left == *right; ++left, ++right);
  return std::string(begining, left);
}

static void WriteOffsets(FILE* f, std::vector<LineInfo*>::iterator begin, std::vector<LineInfo*>::iterator end)
{
  OffsetType sz = static_cast<OffsetType>(std::distance(begin, end));
  fwrite(&sz, sizeof(sz), 1, f);
  for (; begin != end; ++begin)
  {
    fwrite(&(*begin)->pos, sizeof((*begin)->pos), 1, f);
  }
}

static bool ReadOffsets(FILE* f, OffsetCont& offsets)
{
  OffsetType sz;
  if (fread(&sz, sizeof(sz), 1, f) != 1)
    return false;

  offsets.resize(sz);
  return !sz ? true : fread(&offsets[0], sizeof(offsets[0]), sz, f) == offsets.size();
}

OffsetCont TagFileInfo::GetOffsets(IndexType type)
{
  FILE *f = OpenIndex();
  std::shared_ptr<void> fileCloser(0, [&](void*){ fclose(f); });
  if (!f || !ReadSignature(f))
    throw std::logic_error("Signature must be valid");

  fseek(f, sizeof(time_t), SEEK_CUR);
  std::string repoRoot;
  if (!ReadRepoRoot(f, repoRoot))
    throw std::logic_error("Reporoot must be valid");

  for (int i = 0; i != static_cast<int>(type); ++i)
  {
    OffsetType sz;
    if (fread(&sz, sizeof(sz), 1, f) != 1)
      throw std::runtime_error("Invalid file format");

    fseek(f, sizeof(OffsetType) * sz, SEEK_CUR);
  }

  OffsetCont result;
  if (!ReadOffsets(f, result))
    throw std::runtime_error("Invalid file format");

  return result;
}

int TagFileInfo::CreateIndex(time_t tagsModTime)
{
  TagFileInfo* fi = this;
  int pos=0;
  FILE *f=fopen(fi->filename.c_str(),"rb");
  if(!f)return 0;
  fseek(f,0,SEEK_END);
  int sz=ftell(f);
  fseek(f,0,SEEK_SET);
  fi->offsets.clear();
//  fi->offsets.SetSize(sz/80);
  std::vector<LineInfo*> lines;
  std::vector<LineInfo*> classes;
  lines.reserve(sz/80);

  std::string buffer;
  char const* strbuf;
  if(!GetLine(strbuf, buffer, f) || strncmp(strbuf,"!_TAG_FILE_FORMAT",17))
  {
    fclose(f);
    return 0;
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
  fclose(f);
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
    return 0;
  }
  fwrite(IndexFileSignature, 1, sizeof(IndexFileSignature), g);
  fwrite(&tagsModTime,sizeof(tagsModTime),1,g);
  uint32_t rootLen = static_cast<uint32_t>(fullpathrepo ? reporoot.length() : 0);
  fwrite(&rootLen, 1, sizeof(rootLen), g);
  if (fullpathrepo)
    fwrite(reporoot.c_str(), 1, reporoot.length(), g);

  std::sort(lines.begin(), lines.end(), [](LineInfo* left, LineInfo* right) { return FieldLess(left->line, right->line); });
  WriteOffsets(g, lines.begin(), lines.end());
  fi->offsets.reserve(lines.size());
  std::transform(lines.begin(), lines.end(), std::back_inserter(fi->offsets), [](LineInfo* line){ return line->pos; });
  std::sort(lines.begin(), lines.end(), [](LineInfo* left, LineInfo* right) { return FieldLess(left->line, right->line, CaseInsensitive); });
  WriteOffsets(g, lines.begin(), lines.end());
  std::sort(lines.begin(), lines.end(), [](LineInfo* left, LineInfo* right) { return PathLess(left->fn, right->fn); });
  WriteOffsets(g, lines.begin(), lines.end());
  std::sort(classes.begin(), classes.end(), [](LineInfo* left, LineInfo* right) { return FieldLess(left->cls, right->cls); });
  WriteOffsets(g, classes.begin(), classes.end());
  auto linesEnd = std::unique(lines.begin(), lines.end(), [](LineInfo* left, LineInfo* right) { return PathsEqual(left->fn, right->fn); });
  std::sort(lines.begin(), linesEnd, [](LineInfo* left, LineInfo* right) { return FieldLess(GetFilename(left->fn), GetFilename(right->fn), CaseInsensitive); });
  WriteOffsets(g, lines.begin(), linesEnd);
  delete [] linespool;
  while(poolfirst)
  {
    pool=poolfirst->next;
    delete poolfirst;
    poolfirst=pool;
  }
  fclose(g);

  utimbuf tm;
  tm.actime=fi->modtm;
  tm.modtime=fi->modtm;
  utime(fi->indexFile.c_str(),&tm);
  return 1;
}

bool TagFileInfo::LoadIndex(time_t tagsModTime)
{
  TagFileInfo* fi = this;
  FILE *f=fopen(fi->indexFile.c_str(),"rb");
  if (!f)
    return false;

  std::shared_ptr<void> fileCloser(0, [&](void*){ fclose(f); });
  if(!ReadSignature(f))
    return false;

  time_t storedTagsModTime = 0;
  fread(&storedTagsModTime,sizeof(storedTagsModTime),1,f);
  if (storedTagsModTime != tagsModTime)
    return false;

  if (!ReadRepoRoot(f, reporoot))
    return false;

  fullpathrepo = !reporoot.empty();
  reporoot = reporoot.empty() ? GetDirOfFile(filename) : reporoot;
  bool result = ReadOffsets(f, fi->offsets);
  fclose(f);
  return result;
}

int TagFileInfo::Load()
{
  struct stat st;
  if (stat(filename.c_str(), &st) == -1)
//TODO: return Error(...)
    return ENOENT;

  if (modtm == st.st_mtime)
    return 0;

  if (!LoadIndex(st.st_mtime) && !CreateIndex(st.st_mtime))
  {
    remove(indexFile.c_str());
//TODO: return Error(...)
    return EIO;
  }

  modtm = st.st_mtime;
  return 0;
}

char const* TagFileInfo::GetRelativePath(char const* fileName) const
{
  if (reporoot.empty() || IsPathSeparator(reporoot.back()))
    throw std::logic_error("Invalid reporoot");

  if (!!PathCompare(reporoot.c_str(), fileName, PartialCompare) || (*fileName && !IsPathSeparator(*fileName)))
    return nullptr;

  for (; IsPathSeparator(*fileName); ++fileName);
  return !*fileName ? nullptr : fileName;
}

std::string TagFileInfo::GetFullPath(std::string const& relativePath) const
{
  return IsFullPath(relativePath.c_str()) ? relativePath : JoinPath(reporoot, relativePath);
}

bool TagFileInfo::HasName(char const* fileName) const
{
  // filename.back() is not path separator
  return PathsEqual(filename.c_str(), fileName);
}

//TODO: rework: must return error instead of message id (message id coud be zero)
int Load(const char* filename, size_t& symbolsLoaded)
{
  auto iter = std::find_if(files.begin(), files.end(), [&](TagFileInfoPtr const& file) {return file->HasName(filename);});
  TagFileInfoPtr fi = iter == files.end() ? std::shared_ptr<TagFileInfo>(new TagFileInfo(filename)) : *iter;
  if (auto err = fi->Load())
    return err;

  if (iter == files.end())
    files.push_back(fi);

  symbolsLoaded = fi->offsets.size();
  return 0;
}

class MatchVisitor
{
public:
  MatchVisitor(std::string const& pattern)
    : Pattern(pattern)
  {
  }

  virtual ~MatchVisitor() = default;

  std::string const& GetPattern() const
  {
    return Pattern;
  }

  virtual int Compare(char const*& strbuf) const = 0;

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

class FilenamePatrialMatch : public MatchVisitor
{
public:
  FilenamePatrialMatch(char const* part)
    : MatchVisitor(part)
  {
  }

  int Compare(char const*& strbuf) const override
  {
    for (; !IsFieldEnd(*strbuf); ++strbuf);
    if (!*strbuf)
    //TODO: replace with Error(MNotTagFile)
      throw std::runtime_error("Invalid tags file format");

    strbuf = GetFilename(++strbuf);
    return FieldCompare(GetPattern().c_str(), strbuf, CaseInsensitive, PartialCompare);
  }
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

static std::pair<size_t, size_t> GetMatchedOffsetRange(FILE* f, OffsetCont const& offsets, MatchVisitor const& visitor, size_t maxCount)
{
  if (offsets.empty())
    return std::make_pair(0, 0);

  if (visitor.GetPattern().empty())
    return std::make_pair(0, std::min(static_cast<size_t>(offsets.size()), maxCount));

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
    return std::make_pair(pos, maxCount > 0 ? std::max(exactmatchend, std::min(pos + maxCount, endpos)) : endpos);
  }
  return std::make_pair(0, 0);
}

static std::vector<TagInfo> GetMatchedTags(TagFileInfo* fi, OffsetCont const& offsets, MatchVisitor const& visitor, size_t maxCount)
{
  std::vector<TagInfo> result;
  FILE *f=fi->OpenTags();
  if(!f)return result;
  auto range = GetMatchedOffsetRange(f, offsets, visitor, maxCount);
  for(; range.first < range.second; ++range.first)
  {
    fseek(f, offsets[range.first], SEEK_SET);
    std::string line;
    GetLine(line, f);
    TagInfo tag;
    if (ParseLine(line.c_str(), *fi, tag))
      result.push_back(std::move(tag));
  }
  fclose(f);
  return result;
}

static std::vector<TagInfo> ForEachFileRepository(char const* fileFullPath, IndexType index, MatchVisitor const& visitor, size_t maxCount)
{
  std::vector<TagInfo> result;
  for (auto const& repos : files)
  {
    if (!repos->GetRelativePath(fileFullPath))
      continue;

    auto tags = GetMatchedTags(repos.get(), index == IndexType::Names ? repos->offsets : repos->GetOffsets(index), visitor, maxCount);
    std::move(tags.begin(), tags.end(), std::back_inserter(result));
  }

  return result;
}

class TagsLess
{
public:
  TagsLess(char const* file, int sortOptions)
    : File(file)
    , Options(sortOptions)
  {
  }

  bool operator() (TagInfo const& left, TagInfo const& right) const
  {
    auto leftSamePath = !!(Options & SortOptions::CurFileFirst) && PathsEqual(left.file.c_str(), File);
    auto rightSamePath = !!(Options & SortOptions::CurFileFirst) && PathsEqual(right.file.c_str(), File);
    if (leftSamePath != rightSamePath)
      return leftSamePath && !rightSamePath;
  
    int cmp = 0;
    if (!!(Options & SortOptions::SortByName) && (cmp = left.name.compare(right.name)))
      return cmp < 0;

    if (cmp = left.file.compare(right.file))
      return cmp < 0;

    return left.lineno < right.lineno;
  }

private:
  char const* File;
  int const Options;
};

static std::vector<TagInfo> SortTags(std::vector<TagInfo>&& tags, char const* file, int sortOptions)
{
  if (sortOptions != SortOptions::DoNotSort)
    std::sort(tags.begin(), tags.end(), TagsLess(file, sortOptions));

  return std::move(tags);
}

std::vector<TagInfo> Find(const char* name, const char* file, int sortOptions)
{
  return SortTags(ForEachFileRepository(file, IndexType::Names, NameMatch(name, FullCompare, CaseSensitive), 0), file, sortOptions);
}

std::vector<TagInfo> FindPartiallyMatchedTags(const char* file, const char* part, size_t maxCount, bool caseInsensitive, int sortOptions)
{
  return SortTags(ForEachFileRepository(file, caseInsensitive ? IndexType::NamesCaseInsensitive : IndexType::Names, NameMatch(part, PartialCompare, caseInsensitive), maxCount), file, sortOptions);
}

std::vector<std::string> FindPartiallyMatchedFile(const char* file, const char* part, size_t maxCount)
{
  auto tags = ForEachFileRepository(file, IndexType::Filenames, FilenamePatrialMatch(part), maxCount);
  std::vector<std::string> result;
  std::transform(tags.begin(), tags.end(), std::back_inserter(result), [](TagInfo const& tag) { return std::move(tag.file); });
  std::sort(result.begin(), result.end());
  return result;
}

std::vector<TagInfo> FindClassMembers(const char* file, const char* classname, int sortOptions)
{
  return SortTags(ForEachFileRepository(file, IndexType::Classes, ClassMemberMatch(classname), 0), file, sortOptions);
}

std::vector<TagInfo> FindFileSymbols(const char* file)
{
  std::vector<TagInfo> result;
  for (const auto& repos : files)
  {
    auto relativePath = repos->GetRelativePath(file);
    if (!relativePath || !*relativePath)
      continue;

    auto tags = GetMatchedTags(repos.get(), repos->GetOffsets(IndexType::Paths), PathMatch(repos->IsFullPathRepo() ? file : relativePath), 0);
    std::move(tags.begin(), tags.end(), std::back_inserter(result));
  }

  return SortTags(std::move(result), file, SortOptions::Default);
}

std::vector<std::string> GetFiles()
{
  std::vector<std::string> result;
  std::transform(files.begin(), files.end(), std::back_inserter(result), [](TagFileInfoPtr const& file) {return file->GetName();});
  return result;
}

void UnloadTags(int idx)
{
  if(idx==-1)
  {
    files.clear();
  }else
  {
    files.erase(files.begin() + idx);
  }
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

bool TagsLoadedForFile(const char* file)
{
  return std::find_if(files.begin(), files.end(), [&](TagFileInfoPtr const& repos) {return repos->GetRelativePath(file); }) != files.end();
}

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
