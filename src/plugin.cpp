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

#define _WINCON_
#include <windows.h>
#undef _WINCON_
#pragma pack(push,4)
#include <wincon.h>
#pragma pack(pop)

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <memory>

#define FARAPI(type) extern "C" type __declspec(dllexport) WINAPI
#pragma comment(lib,"user32.lib")
#define _FAR_NO_NAMELESS_UNIONS
#include <plugin_sdk/plugin.hpp>
#include "String.hpp"
#include "List.hpp"
#include "XTools.hpp"
#include "Registry.hpp"
#include "tags.h"

#include <algorithm>
#include <string>
#include <vector>

using std::auto_ptr;

static struct PluginStartupInfo I;
FarStandardFunctions FSF;

static const char* APPNAME="Source Navigator";

static String tagfile;

static String targetFile;

static String rootKey;

RegExp RegexInstance;

static char wordChars[256]={0,};

Config config;

struct SUndoInfo{
  String file;
  int line;
  int pos;
  int top;
  int left;
};

Array<SUndoInfo> UndoArray;

GUID StringToGuid(const std::string& str)
{
  GUID guid;
  std::string lowercasedStr(str);
  std::transform(str.begin(), str.end(), lowercasedStr.begin(), ::tolower);
  auto sannedItems = sscanf(lowercasedStr.c_str(),
    "{%8x-%4hx-%4hx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx}",
    &guid.Data1, &guid.Data2, &guid.Data3,
    &guid.Data4[0], &guid.Data4[1], &guid.Data4[2], &guid.Data4[3],
    &guid.Data4[4], &guid.Data4[5], &guid.Data4[6], &guid.Data4[7]);

  if (sannedItems != 11)
    throw std::invalid_argument("Invalid guid string specified");

  return guid;
}

::GUID ErrorMessageGuid = StringToGuid("{7b8b2958-e934-49c8-b08c-84d2f365e2b8}");
::GUID InfoMessageGuid = StringToGuid("{7b8b2958-e934-49c8-b08c-84d2f365e2b8}");
::GUID InteractiveDialogGuid = StringToGuid("{c9dabc4b-ec8a-4d58-b434-8965779f2a56}");
::GUID InputBoxGuid = StringToGuid("{8bb007e1-3db4-4966-97ba-ff99aff828de}");
::GUID PluginGuid = StringToGuid("{10c6ca3c-d051-442f-875c-2615007fa87d}");
::GUID CtagsMenuGuid = StringToGuid("{fd88aac1-213a-40b2-9db8-7d1428b0803f}");
::GUID MenuGuid = StringToGuid("{fd4f4e3e-38b7-4528-830c-13ab39bd07c5}");
using WideString = std::basic_string<wchar_t>;

WideString ToString(std::string const& str)
{
  return WideString(str.begin(), str.end());
}

std::string ToStdString(WideString const& str)
{
  return std::string(str.begin(), str.end());
}

bool IsPathSeparator(WideString::value_type c)
{
  return c == '/' || c == '\\';
}

WideString JoinPath(WideString const& dirPath, WideString const& name)
{
  return dirPath.empty() || IsPathSeparator(dirPath.back()) ? dirPath + name : dirPath + WideString(L"\\") + name;
}

enum class YesNoCancel
{
  Yes = 0,
  No = 1,
  Cancel = 2
};

YesNoCancel YesNoCalncelDialog(WideString const& title, WideString const& what)
{
  WideString msg(title + L"\n" + what);
  auto result = I.Message(&PluginGuid, &InfoMessageGuid, FMSG_MB_YESNOCANCEL | FMSG_ALLINONE, nullptr, reinterpret_cast<const wchar_t* const*>(msg.c_str()), 0, 1);
  return static_cast<YesNoCancel>(result);
}

int Msg(wchar_t const* err)
{
  WideString msg = ToString(APPNAME) + L"\n";
  if(!err)
  {
    msg += L"Wrong argument!\nMsg\n";
  }
  else
  {
    msg += err;
  }
  msg += L"\nOk";
  I.Message(&PluginGuid, &ErrorMessageGuid, FMSG_WARNING | FMSG_ALLINONE, nullptr, reinterpret_cast<const wchar_t* const*>(msg.c_str()), 0, 1);
  return 0;
}
int Msg2(char* header,char* err)
{
  WideString msg;
  if(!err || !header)
  {
    msg = L"Wrong argument!\n";
  }
  else
  {
    msg = ToString(header) + L"\n" + ToString(err);
  }
  I.Message(&PluginGuid, &ErrorMessageGuid, FMSG_WARNING | FMSG_ALLINONE, nullptr, reinterpret_cast<const wchar_t* const*>(msg.c_str()), 0, 1);
  return 0;
}

static const wchar_t*
GetMsg(int MsgId)
{
  return I.GetMsg(&PluginGuid, MsgId);
}

int Msg(int msgid)
{
  Msg(GetMsg(msgid));
  return 0;
}

EditorInfo GetCurrentEditorInfo()
{
  EditorInfo ei;
  I.EditorControl(-1, ECTL_GETINFO, 0, &ei);
  return ei;
}

std::string GetFileNameFromEditor(intptr_t editorID)
{
  auto requiredSize = I.EditorControl(editorID, ECTL_GETFILENAME, 0, nullptr);
  std::vector<wchar_t> buffer(requiredSize);
  I.EditorControl(editorID, ECTL_GETFILENAME, buffer.size(), &buffer[0]);
  //TODO: consider returning wide string or fix encoding
  return ToStdString(WideString(buffer.begin(), buffer.end() - 1));
}

struct MI{
  String item;
  int data;
  MI()
  {
    data=-1;
  }
  MI(const char* str,int value):item(str),data(value){}
  MI(int msgid,int value):item(ToStdString(GetMsg(msgid)).c_str()),data(value){}
};

typedef List<MI> MenuList;

#define MF_LABELS 1
#define MF_FILTER 2
#define MF_SHOWCOUNT 4

int Menu(const wchar_t *title,MenuList& lst,int sel,int flags=MF_LABELS,const void* param=NULL)
{
  Vector<FarMenuItem> menu;
  menu.Init(lst.Count());
  static const char labels[]="1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  static const int labelsCount=sizeof(labels)-1;
  std::vector<WideString> menuTexts;

  int i=0;
  int j=0;
  char buf[256];
  ZeroMemory(&menu[0],sizeof(FarMenuItem)*lst.Count());
  if(!(flags&MF_FILTER))
  {
    for(i=0;i<lst.Count();i++)
    {
      if((flags&MF_LABELS))
      {
        if(i<labelsCount)
        {
          sprintf(buf,"&%c ",labels[i]);
        }else
        {
          strcpy(buf, "  ");
        }
        strcat(buf,lst[i].item.Substr(0,120));
      }else
      {
        strcpy(buf,lst[i].item.Substr(0,120));
      }
      menuTexts.push_back(ToString(buf));
      menu[i].Text = menuTexts.back().c_str();
      if(sel==i)menu[i].Flags |= MIF_SELECTED;
    }
    String cnt;
    cnt.Sprintf(" %s%d ",GetMsg(MItemsCount),lst.Count());
    int res=I.Menu(&PluginGuid, &CtagsMenuGuid, -1, -1, 0, FMENU_WRAPMODE, title,flags&MF_SHOWCOUNT?ToString(cnt.Str()).c_str():NULL,
                   L"content",NULL,NULL,&menu[0],lst.Count());
    return res!=-1?lst[res].data:res;
  }else
  {
    String filter=param?(char*)param:"";
    Vector<int> idx;
    Vector<FarKey> fk;
    static const char *filterkeys="1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_$\\\x08-_=|;':\",./<>? []*&^%#@!~";
    int shift;
    for(i=0;filterkeys[i];i++)
    {
      DWORD k=VkKeyScan(filterkeys[i]);
      if(k==0xffff)
      {
        fk.Push(FarKey());
        continue;
      }
      shift=(k&0xff00)>>8;
      if(shift==1)shift=4;
      else if (shift==2)shift=1;
      else if (shift==4)shift=2;
      k=(k&0xff)|(shift<<16);
      FarKey tmp = {k, 0}; // TODO: what is this?
      fk.Push(tmp);
    }
    fk.Push(FarKey());
#ifdef DEBUG
    //DebugBreak();
#endif
    int mode=0;
    for(;;)
    {
      int oldj=j;
      j=0;
      String match="";
      int minit=0;
      int fnd=-1,oldfnd=-1;
      idx.Clean();
      for(i=0;i<lst.Count();i++)
      {
        lst[i].item.SetNoCase(!config.casesens);
        if(filter.Length() && (fnd=lst[i].item.Index(filter))==-1)continue;
        if(!minit && fnd!=-1)
        {
          match=lst[i].item.Substr(fnd);
          minit=1;
        }
        if(oldfnd!=-1 && oldfnd!=fnd)
        {
          oldj=-1;
        }
        if(fnd!=-1 && filter.Length())
        {
          int xfnd=-1;
          while((xfnd=lst[i].item.Index(filter,xfnd+1))!=-1)
          {
            for(int k=0;k<match.Length();k++)
            {
              if(xfnd+k>=lst[i].item.Length() ||
                  (config.casesens && match[k]!=lst[i].item[xfnd+k]) ||
                  (!config.casesens && tolower(match[k])!=tolower(lst[i].item[xfnd+k]))
                )
              {
                match.Delete(k);
                break;
              }
            }
          }
        }
        idx.Push(i);
        menuTexts.push_back(ToString(lst[i].item.Substr(0,120).Str()));
        menu[j].Text = menuTexts.back().c_str();
        if(sel==j)menu[j].Flags |= MIF_SELECTED;
        j++;
        if(fnd!=-1)oldfnd=fnd;
      }
      if((mode==0 && j==0) || (mode==1 && j==oldj))
      {
        if(filter.Length())
        {
          //DebugBreak();
          filter.Delete(-1);
          continue;
        }
      }
      if(sel>j)
      {
        menu[j-1].Flags |= MIF_SELECTED;
      }
      if(match.Length()>filter.Length() && j>1 && mode!=1)
      {
        filter=match;
      }
      String cnt;
      cnt.Sprintf(" %s%d ",GetMsg(MItemsCount),j);
      intptr_t bkey;
      WideString ftitle=WideString(L" [") + title + L"]";
      int res = I.Menu(&PluginGuid, &CtagsMenuGuid,-1,-1,0,FMENU_WRAPMODE|FMENU_SHOWAMPERSAND,ftitle.c_str(),
                     flags&MF_SHOWCOUNT?ToString(cnt.Str()).c_str():NULL,L"content",&fk[0],&bkey,&menu[0],j);
      if(res==-1 && bkey==-1)return -1;
      if(bkey==-1)
      {
        return lst[idx[res]].data;
      }
      int key=filterkeys[bkey];
      if(key==8)
      {
        filter.Delete(-1);
        mode=1;
        continue;
      }
      filter+=(char)key;
      mode=0;
      sel=res;
    }
  }
  return -1;
}

void WINAPI SetStartupInfoW(const struct PluginStartupInfo *Info)
{
  RegexInstance.InitLocale();
  I=*Info;
  //TODO: don't use registry
  rootKey = "SOFTWARE\\FarCtagsPlugin"; //I.RootKey;
  rootKey += "\\ctags";
  FSF = *Info->FSF;
  I.FSF = &FSF;
}

int isident(int chr)
{
  return wordChars[(unsigned char)chr]!=0;
}

static String GetWord(int offset=0)
{
//  DebugBreak();
  EditorInfo ei = GetCurrentEditorInfo();
  EditorGetString egs = {sizeof(EditorGetString)};
  egs.StringNumber=-1;
  I.EditorControl(ei.EditorID, ECTL_GETSTRING, 0, &egs);
  int pos=ei.CurPos-offset;
  if(pos<0)pos=0;
  if(pos>egs.StringLength)return "";
  if(!isident(egs.StringText[pos]))return "";
  int start=pos,end=pos;
  while(start>0 && isident(egs.StringText[start-1]))start--;
  while(end<egs.StringLength-1 && isident(egs.StringText[end+1]))end++;
  if(start==end || (!isident(egs.StringText[start])))return "";
  String rv;
  rv.Set(ToStdString(egs.StringText).c_str(),start,end-start+1);
  return rv;
}

/*static const char* GetType(char type)
{
  switch(type)
  {
    case 'p':return "proto";
    case 'c':return "class";
    case 'd':return "macro";
    case 'e':return "enum";
    case 'f':return "function";
    case 'g':return "enum name";
    case 'm':return "member";
    case 'n':return "namespace";
    case 's':return "structure";
    case 't':return "typedef";
    case 'u':return "union";
    case 'v':return "variable";
    case 'x':return "extern/forward";
  }
  return "unknown";
}
*/
static void chomp(char* str)
{
  int i=strlen(str)-1;
  while(i>=0 && (unsigned char)str[i]<32)
  {
    str[i]=0;
    i--;
  }
}

int SetPos(const char *filename,int line,int col,int top,int left);

static void NotFound(const char* fn,int line)
{
  if(YesNoCalncelDialog(ToString(APPNAME), GetMsg(MNotFoundAsk)) == YesNoCancel::Yes)
  SetPos(fn,line,0,-1,-1);
}

bool GotoOpenedFile(const char* file)
{
  int c = I.AdvControl(&PluginGuid, ACTL_GETWINDOWCOUNT, 0, nullptr);
  for(int i=0;i<c;i++)
  {
    WindowInfo wi = {sizeof(WindowInfo)};
    wi.Pos=i;
    I.AdvControl(&PluginGuid, ACTL_GETWINDOWINFO, 0, (void*)&wi);
    std::vector<wchar_t> name(wi.NameSize);
    wi.Name = &name[0];
    I.AdvControl(&PluginGuid, ACTL_GETWINDOWINFO, 0, (void*)&wi);
    if(wi.Type==WTYPE_EDITOR && !FSF.LStricmp(wi.Name, ToString(file).c_str()))
    {
      I.AdvControl(&PluginGuid, ACTL_SETCURRENTWINDOW, i, nullptr);
      I.AdvControl(&PluginGuid, ACTL_COMMIT, 0, nullptr);
      return true;
    }
  }

  return false;
}

static void NavigateTo(TagInfo* info)
{
  auto ei = GetCurrentEditorInfo();
  std::string fileName = GetFileNameFromEditor(ei.EditorID); // TODO: auto
  {
    SUndoInfo ui;
    ui.file=fileName.c_str();
    ui.line=ei.CurLine;
    ui.pos=ei.CurPos;
    ui.top=ei.TopScreenLine;
    ui.left=ei.LeftPos;
    UndoArray.Push(ui);
  }

  const char* file=info->file.Str();
  int havere=info->re.Length()>0;
  RegExp& re = RegexInstance;
  if(havere)re.Compile(info->re);
  if(!GotoOpenedFile(file))
  {
    FILE *f=fopen(file,"rt");
    if(!f)
    {
      Msg(MEFailedToOpen);
      return;
    }
    int line=info->lineno-1;
    int cnt=0;
    char buf[512];
    while(fgets(buf,sizeof(buf),f) && cnt<line)cnt++;
    chomp(buf);
    SMatch m[10];
    int n=10;
    if(line!=-1)
    {
      if(havere && !re.Match(buf,m,n))
      {
        line=-1;
        Msg(L"not found in place, searching");
      }
    }
    if(line==-1)
    {
      if(!havere)
      {
        NotFound(file,info->lineno);
        fclose(f);
        return;
      }
      line=0;
      fseek(f,0,SEEK_SET);
      while(fgets(buf,sizeof(buf),f))
      {
        chomp(buf);
        n=10;
        if(re.Match(buf,m,n))
        {
          break;
        }
        line++;
      }
      if(feof(f))
      {
        NotFound(file,info->lineno);
        fclose(f);
        return;
      }
    }
    fclose(f);
    I.Editor(ToString(file).c_str(), L"", 0, 0, -1, -1, EF_NONMODAL, line + 1, 1, CP_DEFAULT);
    return;
  }
  EditorSetPosition esp;
  ei = GetCurrentEditorInfo();

  esp.CurPos=-1;
  esp.CurTabPos=-1;
  esp.TopScreenLine=-1;
  esp.LeftPos=-1;
  esp.Overtype=-1;


  int line=info->lineno-1;
  if(line!=-1)
  {
    EditorGetString egs = {sizeof(EditorGetString)};
    egs.StringNumber=line;
    I.EditorControl(ei.EditorID, ECTL_GETSTRING, 0, &egs);
    SMatch m[10];
    int n=10;

    std::string strLine = ToStdString(egs.StringText);
    if(havere && !re.Match(strLine.c_str(),strLine.c_str() + strLine.length(),m,n))
    {
      line=-1;
    }
  }
  if(line==-1)
  {
    if(!havere)
    {
      esp.CurLine=ei.CurLine;
      esp.TopScreenLine=ei.TopScreenLine;
      I.EditorControl(ei.EditorID, ECTL_SETPOSITION, 0, &esp);
      NotFound(file,info->lineno);
      return;
    }
    line=0;
    SMatch m[10];
    int n=10;
    EditorGetString egs;
    while(line<ei.TotalLines)
    {
      esp.CurLine=line;
      I.EditorControl(ei.EditorID, ECTL_SETPOSITION, 0, &esp);
      egs.StringNumber=-1;
      I.EditorControl(ei.EditorID, ECTL_GETSTRING, 0, &egs);
      n=10;
      std::string strLine = ToStdString(egs.StringText);
      if(re.Match(strLine.c_str(),strLine.c_str()+strLine.length(),m,n))
      {
        break;
      }
      line++;
    }
    if(line==ei.TotalLines)
    {
      esp.CurLine=info->lineno==-1?ei.CurLine:info->lineno-1;
      esp.TopScreenLine=ei.TopScreenLine;
      I.EditorControl(ei.EditorID, ECTL_SETPOSITION, 0, &esp);
      NotFound(file,info->lineno);
      return;
    }
  }

  esp.CurLine=line;
  esp.TopScreenLine=esp.CurLine-1;
  if(esp.TopScreenLine==-1)esp.TopScreenLine=0;
  if(ei.TotalLines<ei.WindowSizeY)esp.TopScreenLine=0;
  esp.LeftPos=0;
  I.EditorControl(ei.EditorID, ECTL_SETPOSITION, 0, &esp);
  I.EditorControl(ei.EditorID, ECTL_REDRAW, 0, nullptr);
}

int SetPos(const char *filename,int line,int col,int top,int left)
{
  if(!GotoOpenedFile(filename))
  {
    I.Editor(ToString(filename).c_str(), L"", 0, 0, -1, -1,  EF_NONMODAL, line, col, CP_DEFAULT);
    return 0;
  }

  EditorInfo ei = GetCurrentEditorInfo();
  EditorSetPosition esp = {sizeof(EditorSetPosition)};
  esp.CurLine=line;
  esp.CurPos=col;
  esp.CurTabPos=-1;
  esp.TopScreenLine=top;
  esp.LeftPos=left;
  esp.Overtype=-1;
  I.EditorControl(ei.EditorID, ECTL_SETPOSITION, 0, &esp);
  I.EditorControl(ei.EditorID, ECTL_REDRAW, 0, nullptr);
  return 1;
}

String TrimFilename(const String& file,int maxlength)
{
  if(file.Length()<=maxlength)return file;
  int ri=file.RIndex("\\")+1;
  if(file.Length()-ri+3>maxlength)
  {
    return "..."+file.Substr(file.Length()-maxlength-3);
  }
  return file.Substr(0,3)+"..."+file.Substr(file.Length()-(maxlength-7));
}

static TagInfo* TagsMenu(PTagArray pta)
{
  EditorInfo ei = GetCurrentEditorInfo();
  MenuList sm;
  String s;
  TagArray& ta=*pta;
  int maxid=0,maxinfo=0;
  int i;
  for(i=0;i<ta.Count();i++)
  {
    TagInfo *ti=ta[i];
    if(ti->name.Length()>maxid)maxid=ti->name.Length();
    if(ti->info.Length()>maxinfo)maxinfo=ti->info.Length();
    //if(ti->file.Length()>maxfile)
  }
  int maxfile=ei.WindowSizeX-8-maxid-maxinfo-1-1-1;
  for(i=0;i<ta.Count();i++)
  {
    TagInfo *ti=ta[i];
    s.Sprintf("%c:%s%*s %s%*s %s",ti->type,ti->name.Str(),maxid-ti->name.Length(),"",
      ti->info.Length()?ti->info.Str():"",maxinfo-ti->info.Length(),"",
      TrimFilename(ti->file,maxfile).Str()
    );
    sm<<MI(s.Str(),i);
  }
  int sel=Menu(GetMsg(MSelectSymbol),sm,0,MF_FILTER|MF_SHOWCOUNT);
  if(sel==-1)return NULL;
  return ta[sel];
}

static void FreeTagsArray(PTagArray ta)
{
  for(int i=0;i<ta->Count();i++)
  {
    delete (*ta)[i];
  }
  delete ta;
}

static void UpdateConfig()
{
  Registry r(HKEY_CURRENT_USER);
  if(!r.Open(rootKey))
  {
    config.exe="ctags.exe";
    config.opt="--c++-types=+px --c-types=+px --fields=+n";
    config.autoload="";
    memset(wordChars,0,sizeof(wordChars));
    for(int i=0;i<256;i++)
    {
      if(isalnum(i) || i=='$' || i=='_' || i=='~')
      {
        wordChars[i]=1;
      }
    }
    return;
  }
  char buf[512];
  if(r.Get("pathtoexe",buf,sizeof(buf)))
  {
    config.exe=buf;
  }
  if(r.Get("commandline",buf,sizeof(buf)))
  {
    config.opt=buf;
  }
  if(r.Get("autoload",buf,sizeof(buf)))
  {
    config.autoload=buf;
  }
  if(r.Get("wordchars",buf,sizeof(buf)))
  {
    memset(wordChars,0,sizeof(wordChars));
    int i=0;
    while(buf[i])
    {
      wordChars[(unsigned char)buf[i]]=1;
      i++;
    }
  }else
  {
    memset(wordChars,0,sizeof(wordChars));
    for(int i=0;i<256;i++)
    {
      if(isalnum(i) || i=='$' || i=='_' || i=='~')
      {
        wordChars[i]=1;
      }
    }
  }
  if(r.Get("casesensfilt",buf,sizeof(buf)))
  {
    config.casesens=!stricmp(buf,"true");
  }else
  {
    config.casesens=true;
  }
}

WideString GetPanelDir(HANDLE hPanel = PANEL_ACTIVE)
{
  size_t sz = I.PanelControl(hPanel, FCTL_GETPANELDIRECTORY, 0, nullptr);
  std::vector<wchar_t> buffer(sz);
  FarPanelDirectory* dir = reinterpret_cast<FarPanelDirectory*>(&buffer[0]);
  dir->StructSize = sizeof(FarPanelDirectory);
  sz = I.PanelControl(hPanel, FCTL_GETPANELDIRECTORY, sz, &buffer[0]);
  return dir->Name;
}

WideString GetCurFile(HANDLE hPanel = PANEL_ACTIVE)
{
  PanelInfo pi;
  I.PanelControl(hPanel, FCTL_GETPANELINFO, 0, &pi);
  size_t sz = I.PanelControl(hPanel, FCTL_GETPANELITEM, pi.CurrentItem, nullptr);
  std::vector<wchar_t> buffer(sz);
  FarGetPluginPanelItem* item = reinterpret_cast<FarGetPluginPanelItem*>(&buffer[0]);
  item->StructSize = sizeof(FarGetPluginPanelItem);
  item->Size = sz;
  item->Item = reinterpret_cast<PluginPanelItem*>(item + 1);
  sz = I.PanelControl(hPanel, FCTL_GETPANELITEM, pi.CurrentItem, &buffer[0]);
  return item->Item->FileName;
}

HANDLE WINAPI OpenW(const struct OpenInfo *info)
{
  OPENFROM OpenFrom = info->OpenFrom;
  UpdateConfig();
  if(OpenFrom==OPEN_EDITOR)
  {
    //DebugBreak();
    auto ei = GetCurrentEditorInfo();
    std::string fileName = GetFileNameFromEditor(ei.EditorID); // TODO: auto
    Autoload(fileName.c_str());
    if(Count()==0)
    {
      Msg(MENotLoaded);
      return nullptr;
    }
    MenuList ml;
    enum{
      miFindSymbol,miUndo,miResetUndo,
      miComplete,miBrowseFile,miBrowseClass,
    };
    ml<<MI(MFindSymbol,miFindSymbol)
      <<MI(MCompleteSymbol,miComplete)
      <<MI(MUndoNavigation,miUndo)
      <<MI(MResetUndo,miResetUndo)
      <<MI(MBrowseSymbolsInFile,miBrowseFile)
      <<MI(MBrowseClass,miBrowseClass);
    int res=Menu(GetMsg(MPlugin),ml,0);
    if(res==-1)return nullptr;
    switch(res)
    {
      case miFindSymbol:
      {
        String word=GetWord();
        if(word.Length()==0)return nullptr;
        //Msg(word);
        PTagArray ta = Find(word, fileName.c_str());
        if(!ta)
        {
          Msg(GetMsg(MNotFound));
          return nullptr;
        }
        TagInfo *ti;
        if(ta->Count()==1)
        {
          ti=(*ta)[0];
        }else
        {
          ti=TagsMenu(ta);
        }
        if(ti)NavigateTo(ti);
        FreeTagsArray(ta);
      }break;
      case miUndo:
      {
        if(UndoArray.Count()==0)return nullptr;
        /*char b[32];
        sprintf(b,"%d",ei.CurState);
        Msg(b);*/
        if(ei.CurState==ECSTATE_SAVED)
        {
          I.EditorControl(ei.EditorID, ECTL_QUIT, 0, nullptr);
          I.AdvControl(&PluginGuid, ACTL_COMMIT, 0, nullptr);
        }
        SUndoInfo ui;
        UndoArray.Pop(ui);
        SetPos(ui.file,ui.line,ui.pos,ui.top,ui.left);
      }break;
      case miResetUndo:
      {
        UndoArray.Clean();
      }break;
      case miComplete:
      {
        String word=GetWord(1);
        if(word.Length()==0)return nullptr;
        StrList lst;
        FindParts(fileName.c_str(),word,lst);
        if(lst.Count()==0)
        {
          Msg(MNothingFound);
          return nullptr;
        }
        int res;
        if(lst.Count()>1)
        {
          MenuList ml;
          for(int i=0;i<lst.Count();i++)
          {
            ml<<MI(lst[i],i);
          }
          res=Menu(GetMsg(MSelectSymbol),ml,0,MF_FILTER|MF_SHOWCOUNT,(void*)word.Str());
          if(res==-1)return nullptr;
        }else
        {
          res=0;
        }
        EditorGetString egs;
        egs.StringNumber=-1;
        I.EditorControl(ei.EditorID, ECTL_GETSTRING, 0, &egs);
        while(isident(egs.StringText[ei.CurPos]))ei.CurPos++;
        EditorSetPosition esp;
        esp.CurLine=-1;
        esp.CurPos=ei.CurPos;
        esp.CurTabPos=-1;
        esp.TopScreenLine=-1;
        esp.LeftPos=-1;
        esp.Overtype=-1;
        I.EditorControl(ei.EditorID, ECTL_SETPOSITION, 0, &esp);
        WideString newText(ToString(lst[res].Substr(word.Length()).Str()));
        I.EditorControl(ei.EditorID, ECTL_INSERTTEXT, 0, const_cast<wchar_t*>(newText.c_str()));
      }break;
      case miBrowseFile:
      {
        PTagArray ta = FindFileSymbols(fileName.c_str());
        if(!ta)
        {
          Msg(MNothingFound);
          return nullptr;
        }
        TagInfo *ti=TagsMenu(ta);
        if(ti)NavigateTo(ti);
        FreeTagsArray(ta);
      }break;
      case miBrowseClass:
      {
#ifdef DEBUG
        //DebugBreak();
#endif
        String word=GetWord();
        if(word.Length()==0)
        {
          wchar_t buf[256] = L"";
          if(!I.InputBox(&PluginGuid, &InputBoxGuid, GetMsg(MBrowseClassTitle),GetMsg(MInputClassToBrowse),nullptr,
                      L"",buf,sizeof(buf)/sizeof(buf[0]),nullptr,0))return nullptr;
          word=ToStdString(buf).c_str();
        }
        PTagArray ta = FindClassSymbols(fileName.c_str(), word);
        if(!ta)
        {
          Msg(MNothingFound);
          return nullptr;
        }
        TagInfo *ti=TagsMenu(ta);
        if(ti)NavigateTo(ti);
        FreeTagsArray(ta);
      }break;
    }
  }
  else
  {
    int load=OpenFrom==OPEN_COMMANDLINE;
    if(OpenFrom==OPEN_PLUGINSMENU)
    {
      MenuList ml;
      enum {miLoadTagsFile,miUnloadTagsFile,
            miUpdateTagsFile,miCreateTagsFile};
      ml<<MI(MLoadTagsFile,miLoadTagsFile)
        <<MI(MUnloadTagsFile,miUnloadTagsFile)
        <<MI(MCreateTagsFile,miCreateTagsFile)
        <<MI(MUpdateTagsFile,miUpdateTagsFile);
      int rc=Menu(GetMsg(MPlugin),ml,0);
      switch(rc)
      {
        case miLoadTagsFile:
        {
          load=1;
        }break;
        case miUnloadTagsFile:
        {
          ml.Clean();
          ml<<MI(MAll,0);
          StrList l;
          GetFiles(l);
          for(int i=0;i<l.Count();i++)
          {
            ml<<MI(l[i],i+1);
          }
          int rc=Menu(GetMsg(MUnloadTagsFile),ml,0);
          if(rc==-1)return nullptr;
          UnloadTags(rc-1);
        }break;
        case miCreateTagsFile:
        {
          HANDLE hScreen=I.SaveScreen(0,0,-1,-1);
          WideString msg = WideString(GetMsg(MPlugin)) + L"\n" + GetMsg(MTagingCurrentDirectory);
          I.Message(&PluginGuid, &InfoMessageGuid, FMSG_ALLINONE, nullptr, reinterpret_cast<const wchar_t* const*>(msg.c_str()), 0, 1);
          int rc=TagCurrentDir();
          I.RestoreScreen(hScreen);
        }break;
        case miUpdateTagsFile:
        {
          HANDLE hScreen=I.SaveScreen(0,0,-1,-1);
          WideString msg = WideString(GetMsg(MPlugin)) + L"\n" + GetMsg(MUpdatingTagsFile);
          StrList changed;
          String file = ToStdString(JoinPath(GetPanelDir(), GetCurFile())).c_str();
          I.Message(&PluginGuid, &InfoMessageGuid, FMSG_ALLINONE, nullptr, reinterpret_cast<const wchar_t* const*>(msg.c_str()), 0, 1);
          if(!UpdateTagsFile(file))
          {
            I.RestoreScreen(hScreen);
            Msg(MUnableToUpdate);
            return nullptr;
          }
          I.RestoreScreen(hScreen);
        }break;
      }
    }
    if(load)
    {
      //DebugBreak();
      if(OpenFrom==OPEN_PLUGINSMENU)
      {
        tagfile = ToStdString(JoinPath(GetPanelDir(), GetCurFile())).c_str();
      }else
      if(OpenFrom==OPEN_COMMANDLINE)
      {
        OpenCommandLineInfo const* cmdInfo = reinterpret_cast<OpenCommandLineInfo const*>(info->Data);
        WideString cmd(cmdInfo->CommandLine);
        if(cmd[1]==':')
        {
          tagfile=ToStdString(cmd).c_str();
        }else
        {
          if(cmd[0]=='\\')
          {
            cmd = cmd.substr(1);
          }
          tagfile = ToStdString(JoinPath(GetPanelDir(), cmd)).c_str();
        }
      }
      int rc=Load(tagfile,"",true);
      if(rc>1)
      {
        Msg(GetMsg(rc));
        return nullptr;
      }
      String msg;
      msg.Sprintf("%s:%d",GetMsg(MLoadOk),Count());
      Msg(ToString(msg.Str()).c_str());
    }
  }
  return nullptr;
}


void WINAPI GetPluginInfoW(struct PluginInfo *pi)
{
  static const wchar_t *PluginMenuStrings[1];
  PluginMenuStrings[0] = GetMsg(MPlugin);
  pi->StructSize = sizeof(*pi);
  pi->Flags = PF_EDITOR;
  pi->PluginMenu.Guids = &MenuGuid;
  pi->PluginMenu.Strings = PluginMenuStrings;
  pi->PluginMenu.Count = sizeof(PluginMenuStrings) / sizeof(PluginMenuStrings[0]);
  pi->PluginConfig.Guids = &MenuGuid;
  pi->PluginConfig.Strings = PluginMenuStrings;
  pi->PluginConfig.Count = sizeof(PluginMenuStrings) / sizeof(PluginMenuStrings[0]);
  pi->CommandPrefix = L"tag";
}

struct InitDialogItem
{
  unsigned char Type;
  unsigned char X1,Y1,X2,Y2;
  unsigned char Focus;
  unsigned int Selected;
  unsigned int Flags;
  unsigned char DefaultButton;
  char *Data;
};

void InitDialogItems(struct InitDialogItem *Init,struct FarDialogItem *Item,
                    int ItemsNumber)
{
  FarDialogItem empty = {};
  for (int I=0;I<ItemsNumber;I++)
  {
    Item[I] = empty;
    Item[I].Type=static_cast<FARDIALOGITEMTYPES>(Init[I].Type);
    Item[I].X1=Init[I].X1;
    Item[I].Y1=Init[I].Y1;
    Item[I].X2=Init[I].X2;
    Item[I].Y2=Init[I].Y2;
//    Item[I].Focus=Init[I].Focus;
    Item[I].Selected=Init[I].Selected;
    Item[I].Flags=Init[I].Flags;
//    Item[I].DefaultButton=Init[I].DefaultButton;
    if ((unsigned int)Init[I].Data<2000)
      Item[I].Data = ::I.GetMsg(&PluginGuid, (intptr_t)Init[I].Data);
    else
      Item[I].Data = L""; // TODO: Init[I].Data?
  }
}

//TODO: rework
std::vector<WideString> dialogStrings;
//TODO: rework
HANDLE ConfigureDialog = 0;

WideString get_text(unsigned ctrl_id) {
  FarDialogItemData item = { sizeof(FarDialogItemData) };
  item.PtrLength = I.SendDlgMessage(ConfigureDialog, DM_GETTEXT, ctrl_id, 0);
  std::vector<wchar_t> buf(item.PtrLength + 1);
  item.PtrData = buf.data();
  I.SendDlgMessage(ConfigureDialog, DM_GETTEXT, ctrl_id, &item);
  return WideString(item.PtrData, item.PtrLength);
}

intptr_t ConfigureDlgProc(
    HANDLE   hDlg,
    intptr_t Msg,
    intptr_t Param1,
    void* Param2)
{
  if (Msg == DN_EDITCHANGE)
  {
    dialogStrings[Param1 / 2 - 1] = get_text(Param1);
  }

  return I.DefDlgProc(hDlg, Msg, Param1, Param2);
}


intptr_t WINAPI ConfigureW(const struct ConfigureInfo *Info)
{
  struct InitDialogItem InitItems[]={
        /*Type         X1 Y2 X2 Y2  F S           Flags D Data */
/*00*/    DI_DOUBLEBOX, 3, 1,64,13, 0,0,              0,0,(char*)MPlugin,
/*01*/    DI_TEXT,      5, 2, 0, 0, 0,0,              0,0,(char*)MPathToExe,
/*02*/    DI_EDIT,      5, 3,62, 3, 1,0,              0,0,"",
/*03*/    DI_TEXT,      5, 4, 0, 0, 0,0,              0,0,(char*)MCmdLineOptions,
/*04*/    DI_EDIT,      5, 5,62, 5, 1,0,              0,0,"",
/*05*/    DI_TEXT,      5, 6, 0, 0, 0,0,              0,0,(char*)MAutoloadFile,
/*06*/    DI_EDIT,      5, 7,62, 7, 1,0,              0,0,"",
/*07*/    DI_TEXT,      5, 8, 0, 0, 0,0,              0,0,(char*)MWordChars,
/*08*/    DI_EDIT,      5, 9,62, 9, 1,0,              0,0,"",
/*09*/    DI_CHECKBOX,  5, 10,62,10,1,0,              0,0,(char*)MCaseSensFilt,
/*10*/    DI_TEXT,      5,11,62,10, 1,0,DIF_SEPARATOR|DIF_BOXCOLOR,0,"",
/*11*/    DI_BUTTON,    0,12, 0, 0, 0,0,DIF_CENTERGROUP,1,(char *)MOk,
/*12*/    DI_BUTTON,    0,12, 0, 0, 0,0,DIF_CENTERGROUP,0,(char *)MCancel
  };

  struct FarDialogItem DialogItems[sizeof(InitItems)/sizeof(InitItems[0])];
  InitDialogItems(InitItems,DialogItems,sizeof(InitItems)/sizeof(InitItems[0]));
  Registry r(HKEY_CURRENT_USER);
  if(!r.Open(rootKey))
  {
    Msg(MRegFailed);
    return FALSE;
  }
  char buf[512];
  dialogStrings.clear();
  if(r.Get("pathtoexe",buf,sizeof(buf)))
  {
    dialogStrings.push_back(ToString(std::string(buf)));
    DialogItems[2].Data = dialogStrings.back().c_str();
  }else
  {
    DialogItems[2].Data = L"ctags.exe";
  }
  if(r.Get("commandline",buf,sizeof(buf)))
  {
    dialogStrings.push_back(ToString(std::string(buf)));
    DialogItems[4].Data = dialogStrings.back().c_str();
  }else
  {
    dialogStrings.push_back(L"--c++-types=+px --c-types=+px --fields=+n -R *");
    DialogItems[4].Data = dialogStrings.back().c_str();
  }
  if(r.Get("autoload",buf,sizeof(buf)))
  {
    dialogStrings.push_back(ToString(std::string(buf)));
    DialogItems[6].Data = dialogStrings.back().c_str();
  }
  if(r.Get("wordchars",buf,sizeof(buf)))
  {
    dialogStrings.push_back(ToString(std::string(buf)));
    DialogItems[8].Data = dialogStrings.back().c_str();
  }else
  {
    String s;
    for(int i=0;i<256;i++)
    {
      if(isalnum(i) || i=='$' || i=='_' || i=='~')
      {
        s+=(char)i;
      }
    }
    dialogStrings.push_back(ToString(std::string(s.Str())));
    DialogItems[8].Data = dialogStrings.back().c_str();
  }
  if(r.Get("casesensfilt",buf,sizeof(buf)))
  {
    DialogItems[9].Selected=!stricmp(buf,"true");
  }else
  {
    DialogItems[9].Selected=1;
  }
  auto handle = I.DialogInit(
               &PluginGuid,
               &InteractiveDialogGuid,
               -1,
               -1,
               68,
               15,
               L"ctagscfg",
               DialogItems,
               sizeof(DialogItems)/sizeof(DialogItems[0]),
               0,
               FDLG_NONE,
               &ConfigureDlgProc,
               nullptr);

  if (handle == INVALID_HANDLE_VALUE)
    return FALSE;

  ConfigureDialog = handle;
  std::shared_ptr<void> handleHolder(handle, [](void* h){I.DialogFree(h);});
  auto ExitCode = I.DialogRun(handle);
  if(ExitCode!=11)return FALSE;
  r.Set("pathtoexe",ToStdString(dialogStrings[0]).c_str());
  r.Set("commandline", ToStdString(dialogStrings[1]).c_str());
  r.Set("autoload", ToStdString(dialogStrings[2]).c_str());
  r.Set("wordchars", ToStdString(dialogStrings[3]).c_str());
  //TODO: support
  r.Set("casesensfilt",DialogItems[9].Selected?"True":"False");
  return TRUE;
}

void WINAPI GetGlobalInfoW(struct GlobalInfo *info)
{
  info->StructSize = sizeof(*info);
  info->MinFarVersion = MAKEFARVERSION(3, 0, 0, 0, VS_RELEASE);
  info->Version = MAKEFARVERSION(1, 0, 0, 1, VS_RELEASE);
  info->Guid = PluginGuid;
  info->Title = L"TODO: fix";
  info->Description = L"TODO: fix description";
  info->Author = L"TODO: fix author";
}
