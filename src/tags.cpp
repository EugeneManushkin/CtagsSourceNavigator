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

#include <functional>
#include <set>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/utime.h>
#include <string>
#include <string.h>
#include <vector>
#include <memory>
#define NOMINMAX
#include <windows.h>
#include "Hash.hpp"
#include "String.hpp"
#include "Array.hpp"
#include "tags.h"

const size_t Config::max_history_len = 100;

Config::Config()
  : exe("ctags.exe")
  , opt("--c++-types=+px --c-types=+px --fields=+n -R *")
  , autoload("%USERPROFILE%\\.tags-autoload")
  , tagsmask("tags,*.tags")
  , history_file("%USERPROFILE%\\.tags-history")
  , history_len(10)
  , casesens(true)
  , autoload_changed(true)
{
  SetWordchars("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz~$_");
}

void Config::SetWordchars(std::string const& str)
{
  wordchars = str.c_str();
  wordCharsMap.reset();
  for (auto c : str)
  {
    wordCharsMap.set((unsigned char)c, true);
  }
}

std::string Config::GetWordchars() const
{
  return wordchars.Str();
}

bool Config::isident(int chr) const
{
  return wordCharsMap[(unsigned char)chr];
}

static bool FileExists(char const* filename)
{
  return GetFileAttributesA(filename) != INVALID_FILE_ATTRIBUTES;
}

static bool IsPathSeparator(std::string::value_type c)
{
    return c == '/' || c == '\\';
}

struct TagFileInfo{
  TagFileInfo(char const* fname)
    : filename(fname)
    , indexFile(filename + ".idx")
    , modtm(0)
  {
    if (filename.empty() || IsPathSeparator(filename.back()))
      throw std::logic_error("Invalid tags file name");
  }

  Vector<int> offsets;

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

  FILE* OpenIndex()
  {
    modtm = !FileExists(indexFile.c_str()) ? 0 : modtm;
    return !Load() ? fopen(indexFile.c_str(), "rb") : nullptr;
  }

  int Load();

private:
  int CreateIndex(time_t tagsModTime);
  bool LoadIndex(time_t tagsModTime);

  std::string filename;
  std::string indexFile;
  std::string reporoot;
  bool fullpathrepo;
  time_t modtm;
};

using TagFileInfoPtr = std::shared_ptr<TagFileInfo>;
std::vector<TagFileInfoPtr> files;
using TagArrayPtr = std::unique_ptr<TagArray>;

static std::string JoinPath(std::string const& dirPath, std::string const& name)
{
  return dirPath.empty() || IsPathSeparator(dirPath.back()) ? dirPath + name : dirPath + std::string("\\") + name;
}

static bool IsFullPath(char const* fn, size_t fsz)
{
  return fsz > 1 && fn[1] == ':';
}

static std::string GetDirOfFile(std::string const& filePath)
{
    auto pos = filePath.find_last_of("\\/");
    return !pos || pos == std::string::npos ? std::string() : filePath.substr(0, pos);
}

//TODO: path comparation must be reworked
static int CompareFilenames(char const* left, char const* &right, size_t len)
{
  int cmp = 0;
  while (len > 0)
  {
    if (IsPathSeparator(*left) && IsPathSeparator(*right))
    {
      while (IsPathSeparator(*(right + 1)))
        ++right;
    }
    else if ((cmp = strnicmp(left, right, 1)) != 0)
    {
      return cmp;
    }
    --len;
    ++left;
    ++right;
  }

  return 0;
}

static void ForEachFileRepository(char const* fileFullPath, std::function<void(TagFileInfo*)> func)
{
  std::vector<TagFileInfoPtr> result;
  for (auto const& repos : files)
  {
    if (repos->GetRelativePath(fileFullPath))
      func(repos.get());
  }
}

char const IndexFileSignature[] = "tags.idx.v2";

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
  if (fread(&rootLen, 1, sizeof(rootLen), f) != sizeof(rootLen))
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

static void SetStr(String& s,const char* buf,SMatch& m)
{
  s.Set(buf,m.start,m.end-m.start);
}

//int Msg(const char*);

static void QuoteMeta(String& str)
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
  String dst(str.Length()*2);
  int i=1;
  dst+=str[0];
  if(str[1]=='^')
  {
    i++;
    dst+=str[1];
  }

  int j=1;
  if(str[-2]=='$')j=2;
  for(;i<str.Length()-j;i++)
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

static void ReplaceSpaces(String& str)
{
  String dst;
  int i;
  for(i=0;i<str.Length();i++)
  {
    if(str[i]==' ')
    {
      while(i<str.Length() && str[i]==' ')i++;
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

RegExp reParse("/(.+?)\\t(.*?)\\t(\\d+|\\/.*?\\/(?<=[^\\\\]\\/));\"\\t(\\w)(?:\\tline:(\\d+))?(?:\\t(\\S*))?/");

TagInfo* ParseLine(const char* buf, TagFileInfo const& fi)
{
  String pos;
  String file;
  SMatch m[10];
  int n=10;
  if(reParse.Match(buf,m,n))
  {
    TagInfo *i=new TagInfo;
    SetStr(i->name,buf,m[1]);
    SetStr(file,buf,m[2]);
    i->file = MakeFilename(fi.GetFullPath(file.Str())).c_str();
    SetStr(pos,buf,m[3]);
    if(pos[0]=='/')
    {
      i->declaration = MakeDeclaration(pos.Str()).c_str();
      QuoteMeta(pos);
      ReplaceSpaces(pos);
      i->re=pos;
      if(m[5].start!=-1)
      {
        SetStr(pos,buf,m[5]);
        i->lineno=pos.ToInt();
      }
    }else
    {
      i->lineno=pos.ToInt();
    }
    SetStr(pos,buf,m[4]);
    i->type=pos[0];
    if(m[5].start!=-1)
    {
      SetStr(i->info,buf,m[6]);
    }
    return i;
  }
  return NULL;
}

static char strbuf[16384];

struct LineInfo{
  char *line;
  int pos;
  char *fn;
  char *cls;
};

int LinesCmp(const void* v1,const void* v2)
{
  LineInfo *a=*(LineInfo**)v1;
  LineInfo *b=*(LineInfo**)v2;
  return strcmp(a->line,b->line);
}

int FilesCmp(const void* v1,const void* v2)
{
  LineInfo *a=*(LineInfo**)v1;
  LineInfo *b=*(LineInfo**)v2;
  int cmp=stricmp(a->fn,b->fn);
  if(cmp!=0)return cmp;
  return strcmp(a->line,b->line);
}

int ClsCmp(const void* v1,const void* v2)
{
  LineInfo *a=*(LineInfo**)v1;
  LineInfo *b=*(LineInfo**)v2;
  return strcmp(a->cls,b->cls);
}

int StrCmp(const void* v1,const void* v2)
{
  return strcmp(*(char**)v1,*(char**)v2);
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

void GetLine(std::string& buffer, FILE *f)
{
  char const* tmp = 0;
  GetLine(tmp, buffer, f);
}

static std::string GetIntersection(char const* left, char const* right)
{
  if (!left || !*left)
    return right;

  auto begining = left;
  for (; *left && *right && *left == *right; ++left, ++right);
  return std::string(begining, left);
}

int TagFileInfo::CreateIndex(time_t tagsModTime)
{
  TagFileInfo* fi = this;
  int pos=0;
  String file;
  FILE *f=fopen(fi->filename.c_str(),"rb");
  if(!f)return 0;
  fseek(f,0,SEEK_END);
  int sz=ftell(f);
  fseek(f,0,SEEK_SET);
  fi->offsets.Clean();
//  fi->offsets.SetSize(sz/80);
  Vector<LineInfo*> lines;
  Vector<LineInfo*> classes;
  lines.SetSize(sz/80);

  Hash<int> files;
  std::string buffer;
  char const* strbuf;
  GetLine(strbuf, buffer, f);
  if(strncmp(strbuf,"!_TAG_FILE_FORMAT",17))
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
  int linespoolpos=0;


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
    int len=strlen(strbuf);
    li->line=linespool+linespoolpos;
    if(strbuf[len-1]==0x0d || strbuf[len-1]==0x0a)len--;
    memcpy(li->line,strbuf,len);
    li->line[len]=0;
    if(sorted && lines.Count()>1 && strcmp(li->line,lines[-1]->line)<0)
    {
      sorted=false;
    }
    linespoolpos+=len+1;
    //strdup(strbuf);
    li->pos=pos;
    li->fn=strchr(li->line,'\t')+1;
    file.Set(li->fn,0,strchr(li->fn,'\t')-li->fn);
    pathIntersection = IsFullPath(file.Str(), file.Length()) ? GetIntersection(pathIntersection.c_str(), file.Str()) : pathIntersection;
    files.Insert(file,1);
    li->cls=strchr(li->line,'\t');
    do{
      if(li->cls)li->cls++;
      if(li->cls && (!strncmp(li->cls,"class:",6) || !strncmp(li->cls,"struct:",7)))
      {
        li->cls=strchr(li->cls,':')+1;
        char *tmp=strchr(li->cls,'\t');
        if(tmp)*tmp=0;
        classes.Push(li);
        break;
      }
    }while((li->cls=strchr(li->cls,'\t')));
    lines.Push(li);
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
  if(lines.Count())
    qsort(&lines[0],lines.Count(),sizeof(LineInfo*),LinesCmp);
  //fi->offsets.Clean();
  fi->offsets.Init(lines.Count());
  int i;
  for(i=0;i<lines.Count();i++)
  {
    fi->offsets[i]=lines[i]->pos;
  }
  int cnt=fi->offsets.Count();
  fwrite(IndexFileSignature, 1, sizeof(IndexFileSignature), g);
  fwrite(&tagsModTime,sizeof(tagsModTime),1,g);
  uint32_t rootLen = fullpathrepo ? reporoot.length() : 0;
  fwrite(&rootLen, 1, sizeof(rootLen), g);
  if (fullpathrepo)
    fwrite(reporoot.c_str(), 1, reporoot.length(), g);

  fwrite(&cnt,4,1,g);
  if (fi->offsets.Count())
    fwrite(&fi->offsets[0],4,fi->offsets.Count(),g);

  if(lines.Count())
    qsort(&lines[0],lines.Count(),sizeof(LineInfo*),FilesCmp);
  for(i=0;i<lines.Count();i++)
  {
    fwrite(&lines[i]->pos,4,1,g);
//    free(lines[i].line);
  }
  if(classes.Count())
    qsort(&classes[0],classes.Count(),sizeof(LineInfo*),ClsCmp);
  sz=classes.Count();
  fwrite(&sz,4,1,g);
  for(i=0;i<classes.Count();i++)
  {
    fwrite(&classes[i]->pos,4,1,g);
  }
  delete [] linespool;
  while(poolfirst)
  {
    pool=poolfirst->next;
    delete poolfirst;
    poolfirst=pool;
  }
  int fstart=ftell(g);
  fwrite(&fstart,4,1,g);
  char *key;
  int val;
  files.First();
  struct stat st;
  cnt=0;
  while(files.Next(key,val))
  {
    file=JoinPath(reporoot, key).c_str();
    if(stat(file,&st)==-1)continue;
    unsigned short fsz=strlen(key);
    fwrite(&fsz,sizeof(fsz),1,g);
    fwrite(key,fsz,1,g);
    fwrite(&st.st_mtime,sizeof(st.st_mtime),1,g);
    cnt++;
  }
  fseek(g,fstart,SEEK_SET);
  fwrite(&cnt,sizeof(cnt),1,g);
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
  int sz;
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
  fread(&sz,4,1,f);
  fi->offsets.Clean();
  fi->offsets.Init(sz);
  if (sz > 0)
    fread(&fi->offsets[0],4,sz,f);

  fclose(f);
  return true;
}

int TagFileInfo::Load()
{
  struct stat st;
  if (stat(filename.c_str(), &st) == -1)
    return MEFailedToOpen;

  if (modtm == st.st_mtime)
    return 0;

  if (!LoadIndex(st.st_mtime) && !CreateIndex(st.st_mtime))
  {
    remove(indexFile.c_str());
    return MFailedToWriteIndex;
  }

  modtm = st.st_mtime;
  return 0;
}

char const* TagFileInfo::GetRelativePath(char const* fileName) const
{
  if (reporoot.empty() || IsPathSeparator(reporoot.back()))
    throw std::logic_error("Invalid reporoot");

  if (!!CompareFilenames(reporoot.c_str(), fileName, reporoot.length()) || (*fileName && !IsPathSeparator(*fileName)))
    return nullptr;

  for (; IsPathSeparator(*fileName); ++fileName);
  return fileName;
}

std::string TagFileInfo::GetFullPath(std::string const& relativePath) const
{
  return IsFullPath(relativePath.c_str(), relativePath.size()) ? relativePath : JoinPath(reporoot, relativePath);
}

bool TagFileInfo::HasName(char const* fileName) const
{
  // filename.back() is not path separator
  return !CompareFilenames(filename.c_str(), fileName, filename.length()) && !*fileName;
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

  symbolsLoaded = fi->offsets.Count();
  return 0;
}

int Load(const char* filename)
{
  size_t symbolsLoaded = 0;
  return Load(filename, symbolsLoaded);
}

static void FindInFile(TagFileInfo* fi,const char* str,PTagArray ta)
{
  FILE *f=fi->OpenTags();
  if(!f)return;
  int len=strlen(str);
  int pos;
  int left=0;
  int right=fi->offsets.Count()-1;
  int cmp = 1;
  while(left<=right)
  {
    pos=(right+left)/2;
    fseek(f,fi->offsets[pos],SEEK_SET);
    fgets(strbuf,sizeof(strbuf),f);
    cmp=strncmp(str,strbuf,len);
    if(!cmp && strbuf[len]=='\t')
    {
      break;
    }else if(cmp<=0)
    {
      right=pos-1;
    }else
    {
      left=pos+1;
    }
  }
  if(!cmp && strbuf[len]=='\t')
  {
    int endpos=pos;
    while(pos>0)
    {
      fseek(f,fi->offsets[pos-1],SEEK_SET);
      fgets(strbuf,sizeof(strbuf),f);
      if(!strncmp(str,strbuf,len) && strbuf[len]=='\t')
      {
        pos--;
      }else
      {
        break;
      }
    }
    while(endpos<fi->offsets.Count()-1)
    {
      fseek(f,fi->offsets[endpos+1],SEEK_SET);
      fgets(strbuf,sizeof(strbuf),f);
      if(!strncmp(str,strbuf,len) && strbuf[len]=='\t')
      {
        endpos++;
      }else
      {
        break;
      }
    }
    for(int i=pos;i<=endpos;i++)
    {
      fseek(f,fi->offsets[i],SEEK_SET);
      fgets(strbuf,sizeof(strbuf),f);
      TagInfo* t=ParseLine(strbuf,*fi);
      if(t)ta->Push(t);
    }
  }
  fclose(f);
}

void FindFile(TagFileInfo* fi,const char* filename,PTagArray ta)
{
  FILE *g=fi->OpenIndex();
  if(!g)return;
  char filePathSeparator = '\\';
  if (!ReadSignature(g))
    throw std::logic_error("Signature must be valid");

  fseek(g, sizeof(time_t), SEEK_CUR);
  std::string repoRoot;
  if (!ReadRepoRoot(g, repoRoot))
    throw std::logic_error("Reporoot must be valid");

  int sz;
  fread(&sz,4,1,g);
  fseek(g,4+sz*4+sizeof(IndexFileSignature)+sizeof(uint32_t)+repoRoot.length()+sizeof(time_t),SEEK_SET);
  Vector<int> offsets;
  offsets.Init(sz);
  if(sz)fread(&offsets[0],4,sz,g);
  fclose(g);
  FILE *f=fi->OpenTags();
  if(!f)return;

  int len=strlen(filename);
  int left=0;
  int right=offsets.Count()-1;
  int cmp=1;
  int pos;
  char const*file;
  while(left<=right)
  {
    pos=(right+left)/2;
    fseek(f,offsets[pos],SEEK_SET);
    fgets(strbuf,sizeof(strbuf),f);
    file=strchr(strbuf,'\t')+1;
    cmp=CompareFilenames(filename,file,len);
    if(!cmp && *file=='\t')
    {
      break;
    }else if(cmp<=0)
    {
      right=pos-1;
    }else
    {
      left=pos+1;
    }
  }
  if(!cmp && *file=='\t')
  {
    int endpos=pos;
    while(pos>0)
    {
      fseek(f,offsets[pos-1],SEEK_SET);
      fgets(strbuf,sizeof(strbuf),f);
      file=strchr(strbuf,'\t')+1;
      if(!CompareFilenames(filename,file,len) && *file=='\t')
      {
        pos--;
      }else
      {
        break;
      }
    }
    while(endpos<offsets.Count()-1)
    {
      fseek(f,offsets[endpos+1],SEEK_SET);
      fgets(strbuf,sizeof(strbuf),f);
      file=strchr(strbuf,'\t')+1;
      if(!CompareFilenames(filename,file,len) && *file=='\t')
      {
        endpos++;
      }else
      {
        break;
      }
    }
    std::set<std::string> lines;
    int i;
    for(i=pos;i<=endpos;i++)
    {
      fseek(f,offsets[i],SEEK_SET);
      fgets(strbuf,sizeof(strbuf),f);
      lines.insert(strbuf);
    }
    for(auto const& line : lines)
    {
      TagInfo *ti=ParseLine(line.c_str(),*fi);
      if(ti)ta->Push(ti);
    }
  }
  fclose(f);
}

void FindClass(TagFileInfo* fi,const char* str,PTagArray ta)
{
  FILE *g=fi->OpenIndex();
  if(!g)return;
  int sz;
  if (!ReadSignature(g))
    throw std::logic_error("Signature must be valid");

  fseek(g,sizeof(time_t),SEEK_CUR);
  std::string repoRoot;
  if (!ReadRepoRoot(g, repoRoot))
    throw std::logic_error("Reporoot must be valid");

  fread(&sz,4,1,g);
  fseek(g,4+sz*4*2+sizeof(IndexFileSignature)+sizeof(uint32_t)+repoRoot.length()+sizeof(time_t),SEEK_SET);
  fread(&sz,4,1,g);
  Vector<int> offsets;
  offsets.Init(sz);
  if(sz)fread(&offsets[0],4,sz,g);
  fclose(g);
  FILE *f=fi->OpenTags();
  if(!f)return;
  int len=strlen(str);
  int left=0;
  int right=offsets.Count()-1;
  int cmp=1;
  int pos;
  const char *cls;

  while(left<=right)
  {
    pos=(right+left)/2;
    fseek(f,offsets[pos],SEEK_SET);
    fgets(strbuf,sizeof(strbuf),f);
    cls=strstr(strbuf,"\tclass:");
    if(!cls)
    {
      cls=strstr(strbuf,"\tstruct:");
      if(!cls)
      {
        cls="";
      }else
      {
        cls+=8;
      }
    }else
    {
      cls+=7;
    }
    cmp=strncmp(str,cls,len);
    if(!cmp && !isident(cls[len]))
    {
      break;
    }else if(cmp<=0)
    {
      right=pos-1;
    }else
    {
      left=pos+1;
    }
  }
  if(!cmp && !isident(cls[len]))
  {
    int endpos=pos;
    while(pos>0)
    {
      fseek(f,offsets[pos-1],SEEK_SET);
      fgets(strbuf,sizeof(strbuf),f);
      cls=strstr(strbuf,"\tclass:");
      if(!cls)
      {
        cls=strstr(strbuf,"\tstruct:");
        if(!cls)
        {
          cls="";
        }else
        {
          cls+=8;
        }
      }else
      {
        cls+=7;
      }
      if(!strncmp(str,cls,len) && !isident(cls[len]))
      {
        pos--;
      }else
      {
        break;
      }
    }
    while(endpos<offsets.Count()-1)
    {
      fseek(f,offsets[endpos+1],SEEK_SET);
      fgets(strbuf,sizeof(strbuf),f);
      cls=strstr(strbuf,"\tclass:");
      cls=strstr(strbuf,"\tclass:");
      if(!cls)
      {
        cls=strstr(strbuf,"\tstruct:");
        if(!cls)
        {
          cls="";
        }else
        {
          cls+=8;
        }
      }else
      {
        cls+=7;
      }
      if(!strncmp(str,cls,len) && !isident(cls[len]))
      {
        endpos++;
      }else
      {
        break;
      }
    }
    //TODO: refactor duplicated code
    std::set<std::string> lines;
    for(int i=pos;i<=endpos;i++)
    {
      fseek(f,offsets[i],SEEK_SET);
      fgets(strbuf,sizeof(strbuf),f);
      lines.insert(strbuf);
    }
    for(auto const& line : lines)
    {
      TagInfo *ti=ParseLine(line.c_str(),*fi);
      if(ti)ta->Push(ti);
    }
  }
  fclose(f);
}

static void CheckFiles(std::string const& projectRoot, TagFileInfo* fi,StrList& dst)
{
  FILE *g=fi->OpenIndex();
  if (ReadSignature(g))
    throw std::logic_error("Signature must be valid");

  fseek(g, sizeof(time_t), SEEK_CUR);
  std::string repoRoot;
  if (!ReadRepoRoot(g, repoRoot))
    throw std::logic_error("Reporoot must be valid");

  int sz;
  fread(&sz,4,1,g);
  fseek(g,sz*4*2,SEEK_CUR);
  fread(&sz,4,1,g);
  fseek(g,sz*4,SEEK_CUR);
  fread(&sz,4,1,g);
  char fn[512];
  unsigned short fsz;
  time_t modt;
  struct stat st;
  for(int i=0;i<sz;i++)
  {
    fread(&fsz,sizeof(fsz),1,g);
    fread(fn,fsz,1,g);
    fn[fsz]=0;
    std::string fullPath = IsFullPath(fn, fsz) ? fn : JoinPath(projectRoot, fn);
    fread(&modt,sizeof(modt),1,g);
    if(stat(fullPath.c_str(),&st)==-1)continue;
    if(st.st_mtime!=modt)
    {
      dst<<fn;
    }
  }
  fclose(g);
}

int CheckChangedFiles(const char* filename,StrList& dst)
{
//  String file=filename;
//  file.ToLower();
//  for(int i=0;i<files.size();i++)
//  {
//    if(files[i]->filename==file && files[i]->indexFile.Length())
//    {
//      struct stat stf,sti;
//      if(stat(file,&stf)==-1)return 0;
//      if(stat(files[i]->indexFile,&sti)==-1)return 0;
//      if(stf.st_mtime!=sti.st_mtime)return 0;
//      CheckFiles(GetDirOfFile(filename), files[i].get(),dst);
//      return 1;
//    }
//  }
  return 0;
}

PTagArray Find(const char* symbol,const char* file)
{
  TagArrayPtr ta(new TagArray);
  ForEachFileRepository(file, std::bind(FindInFile, std::placeholders::_1, symbol, ta.get()));
  return !ta->Count() ? nullptr : ta.release();
}

//TODO: support case insensitive search
static std::pair<size_t, size_t> GetMatchedOffsetRange(FILE* f, TagFileInfo* fi,const char* str, size_t maxCount)
{
  if (!fi->offsets.Count())
    return std::make_pair(0, 0);

  if (!str || str[0] == 0)
    return std::make_pair(0, std::min(static_cast<size_t>(fi->offsets.Count()), maxCount));

  size_t len=strlen(str);
  size_t pos;
  size_t left=0;
  size_t right=fi->offsets.Count()-1;
  int cmp;
  std::string strbuf;
  while(left<=right)
  {
    pos=(right+left)/2;
    fseek(f,fi->offsets[pos],SEEK_SET);
    GetLine(strbuf,f);
    cmp=strncmp(str,strbuf.c_str(),len);
    if(!cmp)
    {
      break;
    }else if(cmp<=0)
    {
      right=pos-1;
    }else
    {
      left=pos+1;
    }
  }
  if(!cmp)
  {
    size_t endpos=pos;
    size_t exactmatchend=strbuf.length() == len || strbuf[len] == '\t' ? pos + 1 : pos;
    while(pos>0)
    {
      fseek(f,fi->offsets[pos-1],SEEK_SET);
      GetLine(strbuf,f);
      if(!strncmp(str,strbuf.c_str(),len))
      {
        pos--;
        exactmatchend = strbuf.length() > len && strbuf[len] != '\t' ? pos : exactmatchend;
      }else
      {
        break;
      }
    }
    while(endpos<fi->offsets.Count()-1)
    {
      fseek(f,fi->offsets[endpos+1],SEEK_SET);
      GetLine(strbuf, f);
      if(!strncmp(str,strbuf.c_str(),len))
      {
        endpos++;
        exactmatchend = strbuf.length() == len || strbuf[len] == '\t' ? endpos + 1 : exactmatchend;
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

static void FindPartiallyMatchedTagsImpl(TagFileInfo* fi, const char* part, size_t maxCount, std::vector<TagInfo>& result)
{
  FILE *f=fi->OpenTags();
  if(!f)return;
  auto range = GetMatchedOffsetRange(f, fi, part, maxCount);
  for(; range.first < range.second; ++range.first)
  {
    fseek(f, fi->offsets[range.first], SEEK_SET);
    std::string line;
    GetLine(line, f);
    std::unique_ptr<TagInfo> tag(ParseLine(line.c_str(), *fi));
    if (tag)
      result.push_back(*tag);
  }
  fclose(f);
}

std::vector<TagInfo> FindPartiallyMatchedTags(const char* file, const char* part, size_t maxCount)
{
  std::vector<TagInfo> result;
  ForEachFileRepository(file, std::bind(FindPartiallyMatchedTagsImpl, std::placeholders::_1, part, maxCount, std::ref(result)));
  return result;
}

PTagArray FindFileSymbols(const char* file)
{
  TagArrayPtr ta (new TagArray);
  for (const auto& repos : files)
  {
    auto relativePath = repos->GetRelativePath(file);
    if (!relativePath || !*relativePath)
      continue;

    FindFile(repos.get(), repos->IsFullPathRepo() ? file : relativePath, ta.get());
  }

  return !ta->Count() ? nullptr : ta.release();
}

PTagArray FindClassSymbols(const char* file,const char* classname)
{
  TagArrayPtr ta(new TagArray);
  ForEachFileRepository(file, std::bind(FindClass, std::placeholders::_1, classname, ta.get()));
  return !ta->Count() ? nullptr : ta.release();
}

void Autoload(const char* fn)
{
  String dir = fn;
  //DebugBreak();
  StrList sl;
  if(FileExists(dir))
  {
    sl.LoadFromFile(dir);
    for(int i=0;i<sl.Count();i++)
    {
      String fn=sl[i];
      if(fn.Length()==0 || !IsFullPath(fn, fn.Length()))continue;
      Load(fn);
    }
  }
}

void GetFiles(StrList& dst)
{
  for(int i=0;i<files.size();i++)
  {
    dst<<files[i]->GetName().c_str();
  }
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

int MergeFiles(const char* target,const char* mfile)
{
  FILE *f=fopen(target,"rt");
  if(!f)return 0;
  FILE *g=fopen(mfile,"rt");
  if(!g)
  {
    fclose(f);
    return 0;
  }
  fgets(strbuf,sizeof(strbuf),g);
  if(strncmp(strbuf,"!_TAG_FILE_FORMAT",17))
  {
    fclose(f);
    fclose(g);
    return 0;
  }
  fgets(strbuf,sizeof(strbuf),f);
  if(strncmp(strbuf,"!_TAG_FILE_FORMAT",17))
  {
    fclose(f);
    fclose(g);
    return 0;
  }
  fseek(f,0,SEEK_SET);
  FILE *h=fopen("tags.temp","wt");
  if(!h)
  {
    fclose(f);
    fclose(g);
    return 0;
  }
  char strbuf[1024]={0};
  Vector<char*> a;
  Hash<int> files;
  String file;

  while(fgets(strbuf,sizeof(strbuf),g))
  {
    if(strbuf[0]=='!')continue;
    a.Push(strdup(strbuf));
    char *tab=strchr(strbuf,'\t');
    if(tab)
    {
      tab++;
      char *tab2=strchr(tab,'\t');
      file.Set(tab,0,tab2-tab-1);
      files[file]=1;
    }
  }
  fclose(g);
  int i=0;
  while(fgets(strbuf,sizeof(strbuf),f))
  {
    if(strbuf[0]=='!')
    {
      fprintf(h,"%s",strbuf);
      continue;
    }
    char *tab=strchr(strbuf,'\t');
    if(tab)
    {
      tab++;
      char *tab2=strchr(tab,'\t');
      file.Set(tab,0,tab2-tab-1);
      if(files.Exists(file))continue;
    }
    while(i<a.Count() && strcmp(a[i],strbuf)<0)
    {
      fprintf(h,"%s",a[i]);
      i++;
    }
    fprintf(h,"%s",strbuf);
  }
  while(i<a.Count())
  {
    fprintf(h,"%s",a[i]);
    i++;
  }
  fclose(f);
  fclose(h);
  remove(target);
  rename("tags.temp",target);
  return 1;
}

int SaveChangedFiles(const char* file, const char* outputFilename)
{
  TagFileInfo *fi=NULL;
  int i;
  for(i=0;i<files.size();i++)
  {
    if(files[i]->HasName(file))
    {
      fi=files[i].get();
      break;
    }
  }
  if(!fi && Load(file) > 1)
  {
    return 0;
  }
  StrList sl;
  if(!CheckChangedFiles(file,sl))return 0;
  return sl.SaveToFile(outputFilename);
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
