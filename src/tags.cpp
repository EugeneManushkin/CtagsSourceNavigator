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

struct TagFileInfo{
  String filename;
  String indexFile;
  Array<String> loadBases;
  time_t modtm;
  Vector<int> offsets;

  String GetRepoRoot()
  {
    String root=filename;
    int ri=root.RIndex("\\");
    if(ri!=-1)
      root.Delete(ri+1);

    return root;
  }
};

using TagFileInfoPtr = std::shared_ptr<TagFileInfo>;
std::vector<TagFileInfoPtr> files;

static bool IsPathSeparator(std::string::value_type c)
{
  return c == '/' || c == '\\';
}

static std::string JoinPath(std::string const& dirPath, std::string const& name)
{
  return dirPath.empty() || IsPathSeparator(dirPath.back()) ? dirPath + name : dirPath + std::string("\\") + name;
}

static bool IsFullPath(char const* fn, unsigned short fsz)
{
  return fsz > 1 && fn[1] == ':';
}

static char GetPathSeparator(char const* str)
{
  static char const defaultPathSeparator = '\\';
  while (*str && *str != '\t' && !IsPathSeparator(*str))
    ++str;

  return IsPathSeparator(*str) ? *str : defaultPathSeparator;
}

char const IndexFileSignature[] = "tags.idx";

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

RegExp reParse("/(.+?)\\t(.*?)\\t(\\d+|\\/.*?\\/(?<=[^\\\\]\\/));\"\\t(\\w)(?:\\tline:(\\d+))?(?:\\t(\\S*))?/");

TagInfo* ParseLine(const char* buf,const String& base)
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
    if(file[1]!=':' && file[0]!='\\')
    {
      file.Insert(0,base);
    }
    i->file=file;
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

static int CreateIndex(TagFileInfo* fi)
{
  int pos=0;
  String file;
  FILE *f=fopen(fi->filename,"rb");
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
  char pathSeparator = 0;
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
    pathSeparator = !pathSeparator ? GetPathSeparator(li->fn) : pathSeparator;
    file.Set(li->fn,0,strchr(li->fn,'\t')-li->fn);
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
  pathSeparator = !pathSeparator ? '\\' : pathSeparator;
  fclose(f);
  FILE *g=fopen(fi->indexFile,"wb");
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
  fwrite(&pathSeparator, 1, 1, g);
  fwrite(&cnt,4,1,g);
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
  String base=fi->filename;
  base.Delete(base.RIndex("\\")+1);
  while(files.Next(key,val))
  {
    file=base+key;
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
  utime(fi->indexFile,&tm);
  return 1;
}

void LoadIndex(TagFileInfo* fi)
{
  FILE *f=fopen(fi->indexFile,"rb");
  int sz;
  if(ReadSignature(f))
  {
    fseek(f, 1, SEEK_CUR);
  }
  fread(&sz,4,1,f);
  fi->offsets.Clean();
  fi->offsets.Init(sz);
  fread(&fi->offsets[0],4,sz,f);
  fclose(f);
}

bool FindIdx(TagFileInfo* fi)
{
  fi->indexFile=fi->filename+".idx";
  struct stat sti;
  if(stat(fi->indexFile,&sti)!=-1 && sti.st_mtime==fi->modtm)
  {
    return true;
  }
  char idxdir[512];
  const char *dirs[]={"TAGS_INDEX","TEMP","TMP",NULL};
  const char **dir=dirs;
  for(;*dir;dir++)
  {
    fi->indexFile="";
    if(GetEnvironmentVariable(*dir,idxdir,sizeof(idxdir)))
    {
      fi->indexFile=fi->filename+".idx";
      fi->indexFile.Replace('\\','_');
      fi->indexFile.Replace(':','_');
      if(idxdir[strlen(idxdir)-1]!='\\')
      {
        fi->indexFile.Insert(0,"\\");
      }
      fi->indexFile.Insert(0,idxdir);
      if(stat(fi->indexFile,&sti)!=-1 && sti.st_mtime==fi->modtm)
      {
        return true;
      }
    }
  }
  fi->indexFile="";
  return false;
}

//TODO: add check if index file is consistent
static bool FindAndLoadIndex(TagFileInfo *fi, time_t tagsTimeStamp)
{
  struct stat sti;
  if (fi->indexFile.Length()>0)
  {
    if (stat(fi->indexFile, &sti) != -1 && sti.st_mtime == tagsTimeStamp)
    {
      LoadIndex(fi);
      return 1;
    }
  }
  else
  if (FindIdx(fi))
  {
    if (stat(fi->indexFile, &sti) != -1 && sti.st_mtime == tagsTimeStamp)
    {
      LoadIndex(fi);
      return 1;
    }
  }

  return false;
}

int Load(const char* _filename)
{
  String filename=_filename;
  struct stat st;
  filename.ToLower();
  if(stat(filename,&st)==-1)return MEFailedToOpen;
  auto iter = std::find_if(files.begin(), files.end(), [&](TagFileInfoPtr const& file) {return file->filename == filename;});
  TagFileInfoPtr fi = iter == files.end() ? TagFileInfoPtr() : *iter;
  if(!fi)
  {
    fi= std::shared_ptr<TagFileInfo>(new TagFileInfo);
    fi->filename=filename;
    fi->modtm=st.st_mtime;
  }else
  {
    if(fi->modtm==st.st_mtime)
    {
      return 1;
    }
  }
  if (!FindAndLoadIndex(fi.get(), st.st_mtime))
  {
    fi->indexFile = fi->filename;
    fi->indexFile += ".idx";
    fi->modtm = st.st_mtime;
    remove(fi->indexFile);
    if (!CreateIndex(fi.get()))
    {
      remove(fi->indexFile);
      return MFailedToWriteIndex;
    }
  }

  files.push_back(fi);
  return 0;
}

static void FindInFile(TagFileInfo* fi,const char* str,PTagArray ta)
{
  FILE *f=fopen(fi->filename,"rt");
  if(!f)return;
  struct stat st;
  fstat(fileno(f),&st);
  if(fi->modtm!=st.st_mtime)
  {
    Load(fi->filename);
  }
  int len=strlen(str);
  int pos;
  int left=0;
  int right=fi->offsets.Count()-1;
  int cmp;
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
    String base=fi->filename;
    int ri=base.RIndex("\\");
    if(ri!=-1)
    {
      base.Delete(ri+1);
    }
    for(int i=pos;i<=endpos;i++)
    {
      fseek(f,fi->offsets[i],SEEK_SET);
      fgets(strbuf,sizeof(strbuf),f);
      TagInfo* t=ParseLine(strbuf,base);
      if(t)ta->Push(t);
    }
  }
  fclose(f);
}

void FindFile(TagFileInfo* fi,const char* filename,PTagArray ta)
{
  FILE *g=fopen(fi->indexFile,"rb");
  if(!g)return;
  char filePathSeparator = '\\';
  if (ReadSignature(g))
  {
    fread(&filePathSeparator, 1, 1, g);
  }

  int sz;
  fread(&sz,4,1,g);
  fseek(g,4+sz*4+sizeof(IndexFileSignature)+1,SEEK_SET);
  Vector<int> offsets;
  offsets.Init(sz);
  if(sz)fread(&offsets[0],4,sz,g);
  fclose(g);
  FILE *f=fopen(fi->filename,"rb");
  if(!f)return;
  String str(filename);
  char separator = GetPathSeparator(str);
  if (separator != filePathSeparator)
    str.Replace(separator, filePathSeparator);

  int len=strlen(str);
  int left=0;
  int right=offsets.Count()-1;
  int cmp;
  int pos;
  char *file;
  String base=fi->filename;
  int ri=base.RIndex("\\");
  if(ri!=-1)
  {
    base.Delete(ri+1);
  }
  while(left<=right)
  {
    pos=(right+left)/2;
    fseek(f,offsets[pos],SEEK_SET);
    fgets(strbuf,sizeof(strbuf),f);
    file=strchr(strbuf,'\t')+1;
    cmp=strnicmp(str,file,len);
    if(!cmp && file[len]=='\t')
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
  if(!cmp && file[len]=='\t')
  {
    int endpos=pos;
    while(pos>0)
    {
      fseek(f,offsets[pos-1],SEEK_SET);
      fgets(strbuf,sizeof(strbuf),f);
      file=strchr(strbuf,'\t')+1;
      if(!strnicmp(str,file,len) && file[len]=='\t')
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
      if(!strnicmp(str,file,len) && file[len]=='\t')
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
      TagInfo *ti=ParseLine(line.c_str(),base);
      if(ti)ta->Push(ti);
    }
  }
  fclose(f);
}

void FindClass(TagFileInfo* fi,const char* str,PTagArray ta)
{
  FILE *g=fopen(fi->indexFile,"rb");
  if(!g)return;
  int sz;
  if (ReadSignature(g))
  {
    fseek(g, 1, SEEK_CUR);
  }

  fread(&sz,4,1,g);
  fseek(g,4+sz*4*2+sizeof(IndexFileSignature)+1,SEEK_SET);
  fread(&sz,4,1,g);
  Vector<int> offsets;
  offsets.Init(sz);
  if(sz)fread(&offsets[0],4,sz,g);
  fclose(g);
  FILE *f=fopen(fi->filename,"rb");
  if(!f)return;
  int len=strlen(str);
  int left=0;
  int right=offsets.Count()-1;
  int cmp;
  int pos;
  const char *cls;
  String base=fi->filename;
  int ri=base.RIndex("\\");
  if(ri!=-1)
  {
    base.Delete(ri+1);
  }

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
      TagInfo *ti=ParseLine(line.c_str(),base);
      if(ti)ta->Push(ti);
    }
  }
  fclose(f);
}

static void CheckFiles(std::string const& projectRoot, TagFileInfo* fi,StrList& dst)
{
  FILE *g=fopen(fi->indexFile,"rb");
  if (ReadSignature(g))
  {
    fseek(g, 1, SEEK_CUR);
  }

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

static std::string GetDirOfFile(std::string const& filePath)
{
  auto pos = filePath.rfind('\\');
  pos = pos == std::string::npos ? filePath.rfind('/') : pos;
  return !pos || pos == std::string::npos ? "" : filePath.substr(0, pos);
}

int CheckChangedFiles(const char* filename,StrList& dst)
{
  String file=filename;
  file.ToLower();
  for(int i=0;i<files.size();i++)
  {
    if(files[i]->filename==file && files[i]->indexFile.Length())
    {
      struct stat stf,sti;
      if(stat(file,&stf)==-1)return 0;
      if(stat(files[i]->indexFile,&sti)==-1)return 0;
      if(stf.st_mtime!=sti.st_mtime)return 0;
      CheckFiles(GetDirOfFile(filename), files[i].get(),dst);
      return 1;
    }
  }
  return 0;
}

PTagArray Find(const char* symbol,const char* file)
{
  PTagArray ta=new TagArray;
  String filename=file;
  filename.ToLower();
  for(int i=0;i<files.size();i++)
  {
    if (filename.StartWith(files[i]->GetRepoRoot()))
    {
      FindInFile(files[i].get(),symbol,ta);
    }
  }
  if(ta->Count()==0)
  {
    delete ta;
    ta=NULL;
  }
  return ta;
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

static void FindPartsInFile(TagFileInfo* fi,const char* str,StrList& dst)
{
  FILE *f=fopen(fi->filename,"rt");
  if(!f)return;
  struct stat st;
  fstat(fileno(f),&st);
  if(fi->modtm!=st.st_mtime)
  {
    Load(fi->filename);
  }
  auto range = GetMatchedOffsetRange(f, fi, str, 0);
  if (range.second > range.first)
  {
    for(int i=range.first;i<range.second;i++)
    {
      fseek(f,fi->offsets[i],SEEK_SET);
      fgets(strbuf,sizeof(strbuf),f);
      char *tab=strchr(strbuf,'\t');
      if(tab)
      {
        *tab=0;
        dst.Push(strbuf);
      }
    }
  }
  fclose(f);
}

static std::vector<TagInfo> FindPartiallyMatchedTags(TagFileInfo* fi, const char* part, size_t maxCount)
{
  std::vector<TagInfo> result;
  //TODO: refactor duplicated code
  FILE *f=fopen(fi->filename,"rt");
  if(!f)return result;
  struct stat st;
  fstat(fileno(f),&st);
  if(fi->modtm!=st.st_mtime)
  {
    Load(fi->filename);
  }
  auto range = GetMatchedOffsetRange(f, fi, part, maxCount);
  if (range.second > range.first)
  {
    String repoRoot=fi->GetRepoRoot();
    for(int i=range.first;i<range.second;i++)
    {
      fseek(f,fi->offsets[i],SEEK_SET);
      std::string line;
      GetLine(line, f);
      std::unique_ptr<TagInfo> tag(ParseLine(line.c_str(), repoRoot));
      result.push_back(*tag);
    }
  }
  fclose(f);
  return result;
}

std::vector<TagInfo> FindPartiallyMatchedTags(const char* file, const char* part, size_t maxCount)
{
  auto fi = std::find_if(files.begin(), files.end(), [&](TagFileInfoPtr const& tag){ return tag->filename.CmpNoCase(file); });
  return fi != files.end() ? FindPartiallyMatchedTags(fi->get(), part, maxCount) : std::vector<TagInfo>();
}

void FindParts(const char* file, const char* part,StrList& dst)
{
  String filename=file;
  filename.ToLower();
  dst.Clean();
  StrList tmp;
  int i;
  for(i=0;i<files.size();i++)
  {
    if (filename.StartWith(files[i]->GetRepoRoot()))
    {
      FindPartsInFile(files[i].get(),part,tmp);
    }
  }
  if(tmp.Count()==0)return;
  tmp.Sort(dst);
  i=1;
  String *s=&dst[0];
  while(i<dst.Count())
  {
    if(dst[i]==*s)
    {
      dst.Delete();
    }else
    {
      s=&dst[i];
      i++;
      if(i==dst.Count())break;
    }
  }
}

PTagArray FindFileSymbols(const char* file)
{
  String filename=file;
  filename.ToLower();
  PTagArray ta=new TagArray;
  for(int i=0;i<files.size();i++)
  {
    if (filename.StartWith(files[i]->GetRepoRoot()))
    {
      int j=0;
      while(filename[j]==files[i]->filename[j])j++;
      while(filename[j-1]!='\\')j--;
      if(files[i]->filename.Index("\\",j)==-1)
      {
        FindFile(files[i].get(),filename.Substr(j),ta);
        FindFile(files[i].get(),filename,ta);
      }
    }
  }
  if(ta->Count()==0)
  {
    delete ta;
    ta=NULL;
  }
  return ta;
}

PTagArray FindClassSymbols(const char* file,const char* classname)
{
  PTagArray ta=new TagArray;
  String filename=file;
  filename.ToLower();
  for(int i=0;i<files.size();i++)
  {
    if (filename.StartWith(files[i]->GetRepoRoot()))
    {
      FindClass(files[i].get(),classname,ta);
    }
  }
  if(ta->Count()==0)
  {
    delete ta;
    ta=NULL;
  }
  return ta;
}

void Autoload(const char* fn)
{
  String dir = fn;
  //DebugBreak();
  StrList sl;
  if(GetFileAttributes(dir)!=0xFFFFFFFF)
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

int Count()
{
  int cnt=0;
  for(int i=0;i<files.size();i++)
  {
    cnt+=files[i]->offsets.Count();
  }
  return cnt;
}
//TODO: refactor duplicated code
int Count(char const* fname)
{
  String filename = fname;
  filename.ToLower();
  auto iter = std::find_if(files.begin(), files.end(), [&](TagFileInfoPtr const& file) {return file->filename == filename; });
  TagFileInfoPtr fi = iter == files.end() ? TagFileInfoPtr() : *iter;
  return iter == files.end() ? 0 : (*iter)->offsets.Count();
}

void GetFiles(StrList& dst)
{
  for(int i=0;i<files.size();i++)
  {
    dst<<files[i]->filename;
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
  String filename=file;
  filename.ToLower();
  for(i=0;i<files.size();i++)
  {
    if(files[i]->filename==filename)
    {
      fi=files[i].get();
      break;
    }
  }
  if(!fi)
  {
    struct stat st;
    if(stat(filename,&st)==-1)return 0;
    TagFileInfo tfi;
    tfi.filename=filename;
    tfi.modtm=st.st_mtime;
    if(!FindIdx(&tfi))return 0;
    if(Load(filename)!=1)return 0;
  }
  StrList sl;
  if(!CheckChangedFiles(filename,sl))return 0;
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

std::string GetTagsFile(std::string const& fileFullPath)
{
  String path(fileFullPath.c_str());
  path.ToLower();
  TagFileInfoPtr found;
  size_t foundRootLen = 0;
  for (auto const& i : files)
  {
    auto currentRoot = i->GetRepoRoot();
    if (currentRoot.Length() > foundRootLen && path.StartWith(currentRoot))
    {
      found = i;
      foundRootLen = currentRoot.Length();
    }
  }
  return !found ? std::string() : found->filename.Str();
}
