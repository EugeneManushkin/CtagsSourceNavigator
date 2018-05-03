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

#define _WINCON_
#define NOMINMAX
#include <windows.h>
#undef _WINCON_
#pragma pack(push,4)
#include <wincon.h>
#pragma pack(pop)

#include <fstream>
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
#include "tags.h"
#include "resource.h"

#include <algorithm>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <unordered_map>

using std::auto_ptr;

static struct PluginStartupInfo I;
FarStandardFunctions FSF;

static const wchar_t* APPNAME = CTAGS_PRODUCT_NAME;

static const wchar_t* ConfigFileName=L"config";

static String targetFile;

RegExp RegexInstance;

Config config;

struct SUndoInfo{
  String file;
  int line;
  int pos;
  int top;
  int left;
};

Array<SUndoInfo> UndoArray;

using WideString = std::basic_string<wchar_t>;

struct VisitedTagsLru
{
  VisitedTagsLru(size_t max)
    : MaxVisited(max)
  {
  }

  using PathList = std::list<WideString>;
  PathList::const_iterator begin() const
  {
    return OrderedTagPaths.begin();
  }

  PathList::const_iterator end() const
  {
    return OrderedTagPaths.end();
  }

  PathList::const_reverse_iterator rbegin() const
  {
    return OrderedTagPaths.rbegin();
  }

  PathList::const_reverse_iterator rend() const
  {
    return OrderedTagPaths.rend();
  }

  void Access(WideString const& str)
  {
    if (!MaxVisited)
      return;

    auto i = Index.find(str);
    if (i == Index.end())
    {
      Add(str);
    }
    else
    {
      OrderedTagPaths.splice(OrderedTagPaths.begin(), OrderedTagPaths, i->second);
    }
  }

private:
  void Add(WideString const& str)
  {
    if (OrderedTagPaths.size() == MaxVisited)
    {
      Index.erase(OrderedTagPaths.back());
      OrderedTagPaths.pop_back();
    }

    OrderedTagPaths.push_front(str);
    Index[str] = OrderedTagPaths.begin();
  }

  size_t MaxVisited;
  PathList OrderedTagPaths;
  std::unordered_map<WideString, PathList::iterator> Index;
};

VisitedTagsLru VisitedTags(0);

//TODO: Make additional fields, replace throw std::exception with throw Error(code)
class Error
{
public:
  Error(int code)
    : Code(code)
  {
  }

  int GetCode() const
  {
    return Code;
  }

private:
  int Code;
};

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

::GUID ErrorMessageGuid = StringToGuid("{03cceb3e-20ba-438a-9972-85a48b0d28e4}");
::GUID InfoMessageGuid = StringToGuid("{58a20c1d-44e2-40ba-9223-5f96d31d8c09}");
::GUID InteractiveDialogGuid = StringToGuid("{fcd1e1b9-4060-4696-9e40-11f055c2909e}");
::GUID InputBoxGuid = StringToGuid("{6ac0c4bb-b907-43c6-8c7a-642e4a34ee35}");
::GUID PluginGuid = StringToGuid("{2e34b611-3df1-463f-8711-74b0f21558a5}");
::GUID CtagsMenuGuid = StringToGuid("{7f125c0d-5e18-4b7f-a6df-1caae013c48f}");
::GUID MenuGuid = StringToGuid("{a5b1037e-2f54-4609-b6dd-70cd47bd222b}");
//TODO: determine MaxMenuWidth depending on max Far Manager window width
intptr_t const MaxMenuWidth = 120;

WideString ToString(std::string const& str, UINT codePage = CP_ACP)
{
  auto sz = MultiByteToWideChar(codePage, 0, str.c_str(), str.length(), nullptr, 0);
  if (!sz)
    return WideString();

  std::vector<wchar_t> buffer(sz);
  MultiByteToWideChar(codePage, 0, str.c_str(), str.length(), &buffer[0], sz);
  return WideString(buffer.begin(), buffer.end());
}

std::string ToStdString(WideString const& str, UINT codePage = CP_ACP)
{
  auto sz = WideCharToMultiByte(codePage, 0, str.c_str(), str.length(), nullptr, 0, nullptr, nullptr);
  if (!sz)
    return std::string();

  std::vector<char> buffer(sz);
  WideCharToMultiByte(codePage, 0, str.c_str(), str.length(), &buffer[0], sz, nullptr, nullptr);
  return std::string(buffer.begin(), buffer.end());
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

YesNoCancel YesNoCalncelDialog(WideString const& what)
{
  WideString msg(WideString(APPNAME) + L"\n" + what);
  auto result = I.Message(&PluginGuid, &InfoMessageGuid, FMSG_MB_YESNOCANCEL | FMSG_ALLINONE, nullptr, reinterpret_cast<const wchar_t* const*>(msg.c_str()), 0, 1);
  return static_cast<YesNoCancel>(result);
}

int Msg(wchar_t const* err)
{
  WideString msg = WideString(APPNAME) + L"\n";
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

void InfoMessage(WideString const& str)
{
  WideString msg = WideString(APPNAME) + L"\n" + str;
  I.Message(&PluginGuid, &ErrorMessageGuid, FMSG_MB_OK | FMSG_ALLINONE, nullptr, reinterpret_cast<const wchar_t* const*>(msg.c_str()), 0, 1);
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
  EditorInfo ei = {sizeof(EditorInfo)};
  I.EditorControl(-1, ECTL_GETINFO, 0, &ei);
  return ei;
}

std::string GetFileNameFromEditor(intptr_t editorID)
{
  size_t sz = I.EditorControl(editorID, ECTL_GETFILENAME, 0, nullptr);
  if (!sz)
    return std::string();

  std::vector<wchar_t> buffer(sz);
  I.EditorControl(editorID, ECTL_GETFILENAME, buffer.size(), &buffer[0]);
  return ToStdString(WideString(buffer.begin(), buffer.end() - 1));
}

WideString GetPanelDir(HANDLE hPanel = PANEL_ACTIVE)
{
  size_t sz = I.PanelControl(hPanel, FCTL_GETPANELDIRECTORY, 0, nullptr);
  if (!sz)
    return WideString();

  std::vector<wchar_t> buffer(sz);
  FarPanelDirectory* dir = reinterpret_cast<FarPanelDirectory*>(&buffer[0]);
  dir->StructSize = sizeof(FarPanelDirectory);
  sz = I.PanelControl(hPanel, FCTL_GETPANELDIRECTORY, sz, &buffer[0]);
  return dir->Name;
}

WideString GetCurFile(HANDLE hPanel = PANEL_ACTIVE)
{
  PanelInfo pi = {sizeof(PanelInfo)};
  I.PanelControl(hPanel, FCTL_GETPANELINFO, 0, &pi);
  size_t sz = I.PanelControl(hPanel, FCTL_GETPANELITEM, pi.CurrentItem, nullptr);
  if (!sz)
    return WideString();

  std::vector<wchar_t> buffer(sz);
  FarGetPluginPanelItem* item = reinterpret_cast<FarGetPluginPanelItem*>(&buffer[0]);
  item->StructSize = sizeof(FarGetPluginPanelItem);
  item->Size = sz;
  item->Item = reinterpret_cast<PluginPanelItem*>(item + 1);
  sz = I.PanelControl(hPanel, FCTL_GETPANELITEM, pi.CurrentItem, &buffer[0]);
  return item->Item->FileName;
}

static std::string ExpandEnvString(std::string const& str)
{
  auto sz = ::ExpandEnvironmentStringsA(str.c_str(), nullptr, 0);
  if (!sz)
    return std::string();

  std::vector<char> buffer(sz);
  ::ExpandEnvironmentStringsA(str.c_str(), &buffer[0], sz);
  return std::string(buffer.begin(), buffer.end());
}

void ExecuteScript(WideString const& script, WideString const& args, WideString workingDirectory)
{
  SHELLEXECUTEINFOW ShExecInfo = {};
  ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
  ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
  ShExecInfo.hwnd = nullptr;
  ShExecInfo.lpVerb = nullptr;
  ShExecInfo.lpFile = script.c_str();
  ShExecInfo.lpParameters = args.c_str();
  ShExecInfo.lpDirectory = workingDirectory.c_str();
  ShExecInfo.nShow = 0;
  ShExecInfo.hInstApp = nullptr;
  if (!::ShellExecuteExW(&ShExecInfo))
    throw std::runtime_error("Failed to run external utility: " + std::to_string(GetLastError()));

  WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
  DWORD exitCode = 0;
  if (!GetExitCodeProcess(ShExecInfo.hProcess, &exitCode))
    throw std::runtime_error("Failed to get exit code of process: " + std::to_string(GetLastError()));

  if (exitCode)
    throw std::runtime_error("External utility failed with code " + std::to_string(exitCode));
}

static WideString GetSelectedDirectory()
{
  WideString selected = GetCurFile();
  return selected == L".." ? GetPanelDir() : JoinPath(GetPanelDir(), selected);
}

int TagDirectory(WideString const& dir, std::string& errorMessage)
{
  try
  {
    if (!(GetFileAttributesW(dir.c_str()) & FILE_ATTRIBUTE_DIRECTORY))
      throw std::runtime_error("Selected item is not a direcory");

    ExecuteScript(ToString(ExpandEnvString(config.exe.Str())), ToString(config.opt.Str()), dir);
  }
  catch(std::exception const& e)
  {
    errorMessage = e.what();
    return 0;
  }

  return 1;
}

static WideString GenerateTempPath()
{
  WideString const prefix = L"TMPDIR";
  auto sz = FSF.MkTemp(0, 0, prefix.c_str());
  if (!sz)
    return WideString();

  std::vector<wchar_t> buffer(sz);
  FSF.MkTemp(&buffer[0], buffer.size(), prefix.c_str());
  return WideString(buffer.begin(), --buffer.end());
}

static std::shared_ptr<void> MakeTemp(WideString const& dirPath)
{
  if (!::CreateDirectoryW(dirPath.c_str(), nullptr))
    return std::shared_ptr<void>();

  return std::shared_ptr<void>(0, [&](void*){
    //TODO: remove dir recursive
  });
}

static bool UpdateTagsFile(const char* file)
{
  WideString tempPath = GenerateTempPath();
  if (tempPath.empty())
    return false;

  auto dirHolder = MakeTemp(tempPath);
  WideString changesFile = JoinPath(tempPath, L"tags.changes");
  if (!SaveChangedFiles(file, ToStdString(changesFile).c_str()))
    return false;

  String opt=config.opt;
  opt.Replace("-R","");
  RegExp re("/\\*(\\.\\S*)?/");
  SMatch m[4];
  int n=4;
  while(re.Search(opt,m,n))
  {
    opt.Delete(m[0].start,m[0].end-m[0].start);
  }

  WideString updateFile = JoinPath(tempPath, L"tags.update");
  WideString arguments = ToString(opt.Str()) + L" -f " + updateFile + L" -L " + changesFile;
  try
  {
    ExecuteScript(ToString(ExpandEnvString(config.exe.Str())), arguments, GetPanelDir());
  }
  catch(std::exception const&)
  {
    return false;
  }

  if (!MergeFiles(file, ToStdString(updateFile).c_str()))
    return false;

  return !Load(file);
}

static void SetDefaultConfig()
{
  config = Config();
}

static WideString GetModulePath()
{
  WideString path(I.ModuleName);
  auto pos = path.rfind('\\');
  pos = pos == WideString::npos ? path.rfind('/') : pos;
  if (!pos || pos == WideString::npos)
    throw std::invalid_argument("Invalid module path");

  return path.substr(0, pos);
}

static WideString GetConfigFilePath()
{
  return JoinPath(GetModulePath(), ConfigFileName);
}

static DWORD ToFarControlState(WORD controlState)
{
  switch (controlState)
  {
  case 1:
    return SHIFT_PRESSED;
  case 2:
    return LEFT_CTRL_PRESSED;
  case 4:
    return LEFT_ALT_PRESSED;
  }

  return 0;
}

static FarKey ToFarKey(WORD virtualKey)
{
  WORD key = virtualKey & 0xff;
  WORD controlState = (virtualKey & 0xff00) >> 8;
  return {key, ToFarControlState(controlState)};
}

static void LoadHistory(std::string fileName)
{
  std::ifstream file;
  file.exceptions(std::ifstream::goodbit);
  file.open(fileName);
  std::shared_ptr<void> fileCloser(0, [&](void*) { file.close(); });
  std::string buf;
  while (std::getline(file, buf))
  {
    if (IsTagFile(buf.c_str()))
      VisitedTags.Access(ToString(buf));
  }
}

static void SaveHistory(std::string fileName)
{
  std::ofstream file;
  file.exceptions(std::ifstream::goodbit);
  file.open(fileName);
  std::shared_ptr<void> fileCloser(0, [&](void*) { file.close(); });
  for (auto i = VisitedTags.rbegin(); i != VisitedTags.rend(); ++i)
  {
    file << ToStdString(*i) << std::endl;
  }
}

static void LoadConfig()
{
  SetDefaultConfig();
  config.autoload_changed = true;
  std::ifstream file;
  file.exceptions(std::ifstream::goodbit);
  file.open(ToStdString(GetConfigFilePath()));
  std::shared_ptr<void> fileCloser(0, [&](void*) { file.close(); });
  std::string buf;
  while (std::getline(file, buf))
  {
    auto pos = buf.find('=');
    if (pos == std::string::npos)
      continue;

    auto key = buf.substr(0, pos);
    auto val = buf.substr(pos + 1);
    if(key == "pathtoexe")
    {
      config.exe=val.c_str();
    }
    else if(key == "commandline")
    {
      config.opt=val.c_str();
    }
    else if(key == "autoload")
    {
      config.autoload=val.c_str();
    }
    else if(key == "casesensfilt")
    {
      config.casesens = val == "true";
    }
    else if(key == "wordchars")
    {
      config.SetWordchars(val);
    }
    else if(key == "historyfile")
    {
      config.history_file = val.c_str();
    }
    else if(key == "historylen")
    {
      unsigned len = 0;
      if (sscanf(val.c_str(), "%u", &len) == 1)
        config.history_len = std::min(static_cast<size_t>(len), Config::max_history_len);
    }
  }

  config.history_len = !config.history_file.Length() ? 0 : config.history_len;
  VisitedTags = VisitedTagsLru(config.history_len);
  if (config.history_len > 0)
    LoadHistory(ExpandEnvString(config.history_file.Str()).c_str());
}

static void LazyAutoload()
{
  if (config.autoload_changed)
  {
    Autoload(ExpandEnvString(config.autoload.Str()).c_str());
  }

  config.autoload_changed = false;
}

static int AddToAutoload(std::string const& fname)
{
  if (!IsTagFile(fname.c_str()))
    return MNotTagFile;

  auto autoloadFilename = ExpandEnvString(config.autoload.Str());
  StrList sl;
  sl.LoadFromFile(autoloadFilename.c_str());
  for (int i = 0; i<sl.Count(); i++)
  {
    if (!fname.compare(sl[i].Str()))
      return 0;
  }
  sl.Insert(fname.c_str());
  if (!sl.SaveToFile(autoloadFilename.c_str()))
    return MFailedSaveAutoload;

  config.autoload_changed = true;
  return 0;
}

struct MI{
  WideString item;
  int data;
  bool Disabled;
  MI()
    : data(-1)
    , Disabled(false)
  {
  }
  MI(WideString const& str,int value,bool disabled=false):item(str),data(value),Disabled(disabled){}
  MI(const char* str,int value,bool disabled=false):item(ToString(str)),data(value),Disabled(disabled){}
  MI(int msgid,int value,bool disabled=false):item(GetMsg(msgid)),data(value),Disabled(disabled){}
  bool IsSeparator() const
  {
    return item.empty();
  }
  bool IsDisabled() const
  {
    return Disabled;
  }
  static MI Separator()
  {
    return MI(L"", -1);
  }
};

using MenuList = std::list<MI>;

#define MF_LABELS 1
#define MF_SHOWCOUNT 4

//TODO: consider pass const& lst
int Menu(const wchar_t *title,MenuList& lst,int sel,int flags=MF_LABELS,const void* param=NULL)
{
  Vector<FarMenuItem> menu;
  size_t const lstSize = lst.size();
  menu.Init(lstSize);
  wchar_t const labels[]=L"1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  int const labelsCount=sizeof(labels)-1;
  ZeroMemory(&menu[0],sizeof(FarMenuItem)*lst.size());
  int i = 0;
  int curLabel = 0;
  for (auto& lstItem : lst)
  {
    if (!lstItem.IsSeparator())
    {
      if((flags&MF_LABELS))
      {
        WideString buf = curLabel<labelsCount ? WideString(L"&") + labels[curLabel] + L" " : L"  ";
        lstItem.item = buf + lstItem.item;
      }
      lstItem.item = lstItem.item.substr(0, MaxMenuWidth);
      menu[i].Text = lstItem.item.c_str();
      if(sel==i)menu[i].Flags |= MIF_SELECTED;
      ++curLabel;
    }
    menu[i].Flags |= lstItem.IsSeparator() ? MIF_SEPARATOR : 0;
    menu[i].Flags |= lstItem.IsDisabled() ? MIF_DISABLE | MIF_GRAYED : 0;
    ++i;
  }
    WideString bottomText = flags&MF_SHOWCOUNT ? GetMsg(MItemsCount) + ToString(std::to_string(lstSize)) : L"";
    int res=I.Menu(&PluginGuid, &CtagsMenuGuid, -1, -1, 0, FMENU_WRAPMODE, title, bottomText.c_str(),
                   L"content",NULL,NULL,&menu[0],lstSize);
    if (res == -1)
      return -1;

    auto iter = lst.begin();
    std::advance(iter, res);
    return iter->data;
}

HKL GetAsciiLayout()
{
  auto sz = GetKeyboardLayoutList(0, nullptr);
  if (!sz)
    return 0;

  std::vector<HKL> layouts(sz);
  GetKeyboardLayoutList(sz, &layouts[0]);
  uintptr_t const englishLanguage = 0x09;
  for (auto const& layout : layouts)
  {
    if ((reinterpret_cast<uintptr_t>(layout) & 0xff) == englishLanguage)
      return layout;
  }

  return 0;
}

std::vector<FarKey> GetFarKeys(std::string const& filterkeys)
{
  std::vector<FarKey> fk;
  auto const asciiLayout = GetAsciiLayout();
  //TODO: consider using static virtual key code array
  for(auto filterKey : filterkeys)
  {
    auto virtualKey = VkKeyScanExA(filterKey, asciiLayout);
    if (virtualKey != 0xffff)
      fk.push_back(ToFarKey(virtualKey));
  }
  fk.push_back(FarKey());
  return fk;
}

int FilterMenu(const wchar_t *title,MenuList const& lst,int sel,int flags=MF_LABELS,const void* param=NULL)
{
  Vector<FarMenuItem> menu;
  size_t const lstSize = lst.size();
  menu.Init(lstSize);

    String filter=param?(char*)param:"";
    std::string filterkeys = "1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_$\\\x08-_=|;':\",./<>?[]*&^%#@!~";
    std::vector<FarKey> fk = GetFarKeys(filterkeys);
#ifdef DEBUG
    //DebugBreak();
#endif
    for(;;)
    {
      int j=0;
      String match="";
      int minit=0;
      int fnd=-1;
      std::multimap<int, MI const*> idx;
      for (auto& lstItem : lst)
      {
        String current = ToStdString(lstItem.item).c_str();
        current.SetNoCase(!config.casesens);
        if(filter.Length() && (fnd=current.Index(filter))==-1)continue;
        if(!minit && fnd!=-1)
        {
          match=current.Substr(fnd);
          minit=1;
        }
        idx.insert(std::make_pair(fnd, &lstItem));
      }
      for (auto const& i : idx)
      {
        menu[j++] = { MIF_NONE, i.second->item.c_str(), {}, i.second->data };
      }
      intptr_t bkey;
      WideString ftitle = filter.Length() > 0 ? L"[Filter: " + ToString(filter.Str()) + L"]" : WideString(L" [") + title + L"]";
      WideString bottomText = flags&MF_SHOWCOUNT ? GetMsg(MItemsCount) + ToString(std::to_string(j)) : L"";
      int res = I.Menu(&PluginGuid, &CtagsMenuGuid,-1,-1,0,FMENU_WRAPMODE|FMENU_SHOWAMPERSAND,ftitle.c_str(),
                       bottomText.c_str(),L"content",&fk[0],&bkey,&menu[0],j);
      if(res==-1 && bkey==-1)return -1;
      if(bkey==-1)
      {
        return menu[res].UserData;
      }
      int key=filterkeys[bkey];
      if(key==8)
      {
        filter.Delete(-1);
        continue;
      }
      filter+=(char)key;
      sel=res;
    }

  return -1;
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

//TODO: rework
WideString FormatTagInfo(TagInfo const& ti, int maxid, int maxDeclaration, int maxfile)
{
  std::string declaration = ti.declaration.Substr(0, maxDeclaration).Str();
  String s;
  s.Sprintf("%c:%s%*s %s%*s %s",ti.type,ti.name.Str(),maxid-ti.name.Length(),"",
    declaration.c_str(),maxDeclaration-declaration.length(),"",
    TrimFilename(ti.file,maxfile).Str()
  );

  return ToString(s.Str());
}

//TODO: rework this awful stuff
void GetMaxParams(std::vector<TagInfo> const& ta, int& maxid, int& maxDeclaration)
{
  maxid=0;
  maxDeclaration=0;
  for (auto const& ti : ta)
  {
    if(ti.name.Length()>maxid)maxid=ti.name.Length();
    if(ti.declaration.Length()>maxDeclaration)maxDeclaration=ti.declaration.Length();
    //if(ti->file.Length()>maxfile)
  }
}

bool LookupTagsMenu(char const* tagsFile, size_t maxCount, TagInfo& tag)
{
  String filter;
  std::string filterkeys = "1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_$\\\x08-_=|;':\",./<>?[]*&^%#@!~";
  std::vector<FarKey> fk = GetFarKeys(filterkeys);
  //TODO: what if opened in editor?
  const int currentWidth = MaxMenuWidth;
  const int maxInfoWidth = currentWidth / 5;
  while(true)
  {
    auto tags = FindPartiallyMatchedTags(tagsFile, filter.Str(), maxCount);
//TODO: rework
    int maxid = 0;
    int maxinfo = 0;
    GetMaxParams(tags, maxid, maxinfo);
    maxinfo = std::min(maxinfo, maxInfoWidth);
    int maxfile=currentWidth-8-maxid-maxinfo-1-1-1;
    std::vector<FarMenuItem> menu;
    std::list<WideString> menuStrings;
    for (auto const& i : tags)
    {
      menuStrings.push_back(FormatTagInfo(i, maxid, maxinfo, maxfile));
      FarMenuItem item = {MIF_NONE,menuStrings.back().c_str()};
      menu.push_back(item);
    }
    intptr_t bkey;
    WideString ftitle = L"[Search: " + ToString(filter.Length() ? filter.Str() : "") + L"]";
    WideString bottomText = L"";
    int res = I.Menu(&PluginGuid, &CtagsMenuGuid,-1,-1,0,FMENU_WRAPMODE|FMENU_SHOWAMPERSAND,ftitle.c_str(),
                     bottomText.c_str(),L"content",&fk[0],&bkey, menu.empty() ? nullptr : &menu[0],menu.size());
    if(res==-1 && bkey==-1)return false;
    if(bkey==-1)
    {
      tag = tags[res];
      return true;
    }
    int key=filterkeys[bkey];
    if(key==8)
    {
      filter.Delete(-1);
      continue;
    }
    filter+=(char)key;
  }
  return false;
}

void WINAPI SetStartupInfoW(const struct PluginStartupInfo *Info)
{
  RegexInstance.InitLocale();
  I=*Info;
  FSF = *Info->FSF;
  I.FSF = &FSF;
  LoadConfig();
}

int isident(int chr)
{
  return config.isident(chr);
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
  if(YesNoCalncelDialog(GetMsg(MNotFoundAsk)) == YesNoCancel::Yes)
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
    if (!wi.NameSize)
      continue;

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

  String file=info->file.Str();
  file.Replace('/', '\\');
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
    I.Editor(ToString(file.Str()).c_str(), L"", 0, 0, -1, -1, EF_NONMODAL, line + 1, 1, CP_DEFAULT);
    return;
  }
  EditorSetPosition esp = {sizeof(EditorSetPosition)};
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

static TagInfo* TagsMenu(PTagArray pta)
{
  MenuList sm;
  String s;
  TagArray& ta=*pta;
  int maxid=0,maxDeclaration=0;
  int i;
  const int currentWidth = std::min(GetCurrentEditorInfo().WindowSizeX, MaxMenuWidth);
  const int maxDeclarationWidth = currentWidth / 5;
  for(i=0;i<ta.Count();i++)
  {
    TagInfo *ti=ta[i];
    if(ti->name.Length()>maxid)maxid=ti->name.Length();
    if(ti->declaration.Length()>maxDeclaration)maxDeclaration=ti->declaration.Length();
    //if(ti->file.Length()>maxfile)
  }
  maxDeclaration = std::min(maxDeclaration, maxDeclarationWidth);
  int maxfile=currentWidth-8-maxid-maxDeclaration-1-1-1;
  for(i=0;i<ta.Count();i++)
  {
    sm.push_back(MI(FormatTagInfo(*ta[i], maxid, maxDeclaration, maxfile), i));
  }
  int sel=FilterMenu(GetMsg(MSelectSymbol),sm,0,MF_SHOWCOUNT);
  if(sel==-1)return NULL;
  return ta[sel];
}

static WideString SelectFromHistory()
{
  if (VisitedTags.begin() == VisitedTags.end())
  {
    InfoMessage(GetMsg(MHistoryEmpty));
    return WideString();
  }

  size_t i = 0;
  MenuList menuList;
  for (auto const& file : VisitedTags)
  {
    menuList.push_back(MI(ToStdString(file).c_str(), i));
    ++i;
  }

  auto rc = Menu(GetMsg(MTitleHistory), menuList, 0);
  if (rc < 0)
    return WideString();

  auto selected = VisitedTags.begin();
  std::advance(selected, rc);
  return *selected;
}

static std::string SearchTagsFile(std::string const& fileName)
{
  if(YesNoCalncelDialog(GetMsg(MAskSearchTags)) != YesNoCancel::Yes)
    return std::string();

  auto str = fileName;
  std::string tagsFile;
  while(tagsFile.empty() && !str.empty())
  {
    auto pos = str.rfind('\\');
    if (pos == std::string::npos)
      break;

    str = str.substr(0, pos);
    auto tags = str + "\\tags";
    if (GetFileAttributesA(tags.c_str()) != INVALID_FILE_ATTRIBUTES)
      tagsFile.swap(tags);
  }

  return !IsTagFile(tagsFile.c_str()) ? std::string() : tagsFile;
}

static bool EnsureTagsLoaded(std::string const& fileName)
{
  if (!GetTagsFile(fileName).empty())
    return true;

  auto tagsFile = SearchTagsFile(fileName);
  return !tagsFile.empty() && !Load(tagsFile.c_str());
}

static void LookupSymbolImpl(std::string const& file)
{
  auto tagsFile = IsTagFile(file.c_str()) ? file : GetTagsFile(file);
  tagsFile = tagsFile.empty() ? SearchTagsFile(file) : tagsFile;
  if (tagsFile.empty())
    throw Error(MENotLoaded);

  int rc=Load(tagsFile.c_str());
  if(rc>1)
    throw Error(rc);

  size_t const maxMenuItems = 10;
  TagInfo selectedTag;
  if (LookupTagsMenu(tagsFile.c_str(), maxMenuItems, selectedTag))
    NavigateTo(&selectedTag);
}

static void LookupSymbol(std::string const& file)
{
  try
  {
    LookupSymbolImpl(file);
  }
  catch(Error const& err)
  {
    Msg(err.GetCode());
  }
}

static void FreeTagsArray(PTagArray ta)
{
  for(int i=0;i<ta->Count();i++)
  {
    delete (*ta)[i];
  }
  delete ta;
}

HANDLE WINAPI OpenW(const struct OpenInfo *info)
{
  OPENFROM OpenFrom = info->OpenFrom;
  if(OpenFrom==OPEN_EDITOR)
  {
    //DebugBreak();
    auto ei = GetCurrentEditorInfo();
    std::string fileName = GetFileNameFromEditor(ei.EditorID); // TODO: auto
    LazyAutoload();
    enum{
      miFindSymbol,miUndo,miResetUndo,
      miComplete,miBrowseClass,miBrowseFile,miLookupSymbol,
    };
    MenuList ml = {
        MI(MFindSymbol,miFindSymbol)
      , MI(MCompleteSymbol,miComplete)
      , MI(MUndoNavigation,miUndo,UndoArray.Count() == 0)
      , MI::Separator()
      , MI(MBrowseClass,miBrowseClass)
      , MI(MBrowseSymbolsInFile,miBrowseFile)
      , MI(MLookupSymbol,miLookupSymbol)
    };
    int res=Menu(GetMsg(MPlugin),ml,0);
    if(res==-1)return nullptr;
    if ((res == miFindSymbol
      || res == miComplete
      || res == miBrowseClass
      || res == miBrowseFile
        ) && !EnsureTagsLoaded(fileName))
    {
      Msg(MENotLoaded);
      return nullptr;
    }

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
            ml.push_back(MI(lst[i],i));
          }
          res=FilterMenu(GetMsg(MSelectSymbol),ml,0,MF_SHOWCOUNT,(void*)word.Str());
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
      case miLookupSymbol:
      {
        LookupSymbol(fileName);
      }break;
    }
  }
  else
  {
    WideString tagfile;
    if(OpenFrom==OPEN_PLUGINSMENU)
    {
      enum {miLoadFromHistory,miLoadTagsFile,miUnloadTagsFile,
            miCreateTagsFile,miAddTagsToAutoload, miUpdateTagsFile, miLookupSymbol};
      MenuList ml = {
           MI(MLookupSymbol, miLookupSymbol)
         , MI::Separator()
         , MI(MLoadFromHistory, miLoadFromHistory)
         , MI(MLoadTagsFile, miLoadTagsFile)
         , MI(MUnloadTagsFile, miUnloadTagsFile)
         , MI(MCreateTagsFile, miCreateTagsFile)
         , MI(MAddTagsToAutoload, miAddTagsToAutoload)
      };
      //TODO: fix UpdateTagsFile operation and include in menu
      int rc=Menu(GetMsg(MPlugin),ml,0);
      switch(rc)
      {
        case miLoadFromHistory:
        {
          tagfile = SelectFromHistory();
        }break;
        case miLoadTagsFile:
        {
          tagfile = JoinPath(GetPanelDir(), GetCurFile());
        }break;
        case miUnloadTagsFile:
        {
          ml.clear();
          ml.push_back(MI(MAll, 0));
          StrList l;
          GetFiles(l);
          for(int i=0;i<l.Count();i++)
          {
            ml.push_back(MI(l[i], i + 1));
          }
          int rc=Menu(GetMsg(MUnloadTagsFile),ml,0);
          if(rc==-1)return nullptr;
          UnloadTags(rc-1);
        }break;
        case miCreateTagsFile:
        {
          WideString selectedDir = GetSelectedDirectory();
          HANDLE hScreen=I.SaveScreen(0,0,-1,-1);
          WideString msg = WideString(GetMsg(MPlugin)) + L"\n" + GetMsg(MTagingCurrentDirectory);
          I.Message(&PluginGuid, &InfoMessageGuid, FMSG_LEFTALIGN | FMSG_ALLINONE, nullptr, reinterpret_cast<const wchar_t* const*>(msg.c_str()), 0, 0);
          std::string errorMessage;
          int rc= TagDirectory(selectedDir, errorMessage);
          I.RestoreScreen(hScreen);
          if (!rc)
          {
            Msg(ToString(errorMessage).c_str());
          }
          else
          {
            tagfile = JoinPath(selectedDir, L"tags");
          }
        }break;
        case miUpdateTagsFile:
        {
          HANDLE hScreen=I.SaveScreen(0,0,-1,-1);
          WideString msg = WideString(GetMsg(MPlugin)) + L"\n" + GetMsg(MUpdatingTagsFile);
          StrList changed;
          String file = ToStdString(JoinPath(GetPanelDir(), GetCurFile())).c_str();
          I.Message(&PluginGuid, &InfoMessageGuid, FMSG_LEFTALIGN | FMSG_ALLINONE, nullptr, reinterpret_cast<const wchar_t* const*>(msg.c_str()), 0, 0);
          if(!UpdateTagsFile(file))
          {
            I.RestoreScreen(hScreen);
            Msg(MUnableToUpdate);
            return nullptr;
          }
          I.RestoreScreen(hScreen);
        }break;
        case miAddTagsToAutoload:
        {
          auto err = AddToAutoload(ToStdString(JoinPath(GetPanelDir(), GetCurFile())));
          if (err)
          {
            Msg(err);
          }
        }break;
        case miLookupSymbol:
        {
          LookupSymbol(ToStdString(JoinPath(GetPanelDir(), GetCurFile())));
        }break;
      }
    }
    if (OpenFrom == OPEN_COMMANDLINE)
    {
      OpenCommandLineInfo const* cmdInfo = reinterpret_cast<OpenCommandLineInfo const*>(info->Data);
      WideString cmd(cmdInfo->CommandLine);
      if (cmd[1] == ':')
      {
        tagfile = cmd;
      }
      else
      {
        if (cmd[0] == '\\')
        {
          cmd = cmd.substr(1);
        }
        tagfile = JoinPath(GetPanelDir(), cmd);
      }
    }
    if(!tagfile.empty())
    {
      auto strTagFile = ToStdString(tagfile);
      int rc=Load(strTagFile.c_str());
      if(rc>1)
      {
        Msg(GetMsg(rc));
        return nullptr;
      }
      InfoMessage(GetMsg(MLoadOk) + WideString(L":") + ToString(std::to_string(Count(strTagFile.c_str()))));
      VisitedTags.Access(tagfile);
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

//TODO: rework
struct InitDialogItem
{
  unsigned char Type;
  unsigned char X1,Y1,X2,Y2;
  unsigned char Focus;
  unsigned int Selected;
  unsigned int Flags;
  unsigned char DefaultButton;
  intptr_t MessageID;
  WideString MessageText;
  struct SaveInfo
  {
    std::string KeyName;
    bool NotNull;
    bool IsCheckbox;
  } Save;
};

static void InitDialogItems(struct InitDialogItem *Init,struct FarDialogItem *Item,
                    int ItemsNumber)
{
  FarDialogItem empty = {};
  for (int i=0;i<ItemsNumber;i++)
  {
    auto& item = Item[i];
    auto const& init = Init[i];
    item = empty;
    item.Type=static_cast<FARDIALOGITEMTYPES>(init.Type);
    item.X1=init.X1;
    item.Y1=init.Y1;
    item.X2=init.X2;
    item.Y2=init.Y2;
    item.Selected=init.Selected;
    item.Flags=init.Flags;
    item.Flags |= init.Focus ? DIF_FOCUS : 0;
    item.Flags |= init.DefaultButton ? DIF_DEFAULTBUTTON : 0;
    item.Data = init.MessageID < 0 ? init.MessageText.c_str() : I.GetMsg(&PluginGuid, init.MessageID);
  }
}

std::string SelectedToString(int selected)
{
  return !!selected ? "true" : "false";
}

static bool SaveConfig(InitDialogItem const* dlgItems, size_t count)
{
  try
  {
    std::ofstream file;
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    file.open(ToStdString(GetConfigFilePath()));
    std::shared_ptr<void> fileCloser(0, [&](void*) { file.close(); });
    for (size_t i = 0; i < count; ++i)
    {
      auto const& item = dlgItems[i];
      if (!item.Save.KeyName.empty() && (!item.Save.NotNull || !item.MessageText.empty()))
        file << item.Save.KeyName << "=" << (item.Save.IsCheckbox ? SelectedToString(item.Selected) : ToStdString(item.MessageText)) << std::endl;
    }
  }
  catch(std::exception const&)
  {
    Msg((L"Failed to save configuration to file: " + GetConfigFilePath()).c_str());
    return false;
  }

  return true;
}

//TODO: rework
HANDLE ConfigureDialog = 0;
//TODO: rework
InitDialogItem* DlgItems = 0;

WideString get_text(unsigned ctrl_id) {
  FarDialogItemData item = { sizeof(FarDialogItemData) };
  item.PtrLength = I.SendDlgMessage(ConfigureDialog, DM_GETTEXT, ctrl_id, 0);
  std::vector<wchar_t> buf(item.PtrLength + 1);
  item.PtrData = buf.data();
  I.SendDlgMessage(ConfigureDialog, DM_GETTEXT, ctrl_id, &item);
  return WideString(item.PtrData, item.PtrLength);
}

intptr_t WINAPI ConfigureDlgProc(
    HANDLE   hDlg,
    intptr_t Msg,
    intptr_t Param1,
    void* Param2)
{
  if (Msg == DN_EDITCHANGE)
  {
    DlgItems[Param1].MessageText = get_text(Param1);
  }

  if (Msg == DN_BTNCLICK)
  {
      DlgItems[Param1].Selected = reinterpret_cast<intptr_t>(Param2);
  }

  return I.DefDlgProc(hDlg, Msg, Param1, Param2);
}

intptr_t WINAPI ConfigureW(const struct ConfigureInfo *Info)
{
  struct InitDialogItem initItems[]={
//    Type        X1 Y2 X2 Y2  F S           Flags D Data
    DI_DOUBLEBOX, 3, 1, 64,18, 0,0,              0,0,MPlugin,L"",{},
    DI_TEXT,      5, 2,  0, 0, 0,0,              0,0,MPathToExe,L"",{},
    DI_EDIT,      5, 3, 62, 3, 1,0,              0,0,-1,ToString(config.exe.Str()),{"pathtoexe", true},
    DI_TEXT,      5, 4,  0, 0, 0,0,              0,0,MCmdLineOptions,L"",{},
    DI_EDIT,      5, 5, 62, 5, 1,0,              0,0,-1,ToString(config.opt.Str()),{"commandline"},
    DI_TEXT,      5, 6,  0, 0, 0,0,              0,0,MWordChars,L"",{},
    DI_EDIT,      5, 7, 62, 9, 1,0,              0,0,-1,ToString(config.GetWordchars()),{"wordchars", true},
    DI_CHECKBOX,  5, 8, 62,10,1,config.casesens, 0,0,MCaseSensFilt,L"",{"casesensfilt", false, true},
    DI_TEXT,      5, 9, 62,10, 1,0,DIF_SEPARATOR|DIF_BOXCOLOR,0,-1,L"",{},
    DI_TEXT,      5, 10, 0, 0, 0,0,              0,0,MHistoryFile,L"",{},
    DI_EDIT,      5, 11,62, 9, 1,0,              0,0,-1,ToString(config.history_file.Str()),{"historyfile"},
    DI_TEXT,      5, 12, 0, 0, 0,0,              0,0,MHistoryLength,L"",{},
    DI_EDIT,      5, 13,62, 9, 1,0,              0,0,-1,ToString(std::to_string(config.history_len)),{"historylen", true},
    DI_TEXT,      5, 14, 0, 0, 0,0,              0,0,MAutoloadFile,L"",{},
    DI_EDIT,      5, 15,62, 7, 1,0,              0,0,-1,ToString(config.autoload.Str()),{"autoload"},
    DI_TEXT,      5, 16,62,10, 1,0,DIF_SEPARATOR|DIF_BOXCOLOR,0,-1,L"",{},
    DI_BUTTON,    0, 17, 0, 0, 0,0,DIF_CENTERGROUP,1,MOk,L"",{},
    DI_BUTTON,    0, 17, 0, 0, 0,0,DIF_CENTERGROUP,0,MCancel,L"",{}
  };

  constexpr size_t itemsCount = sizeof(initItems)/sizeof(initItems[0]);
  struct FarDialogItem DialogItems[itemsCount];
  InitDialogItems(initItems,DialogItems,itemsCount);
  DlgItems = initItems;
  auto handle = I.DialogInit(
               &PluginGuid,
               &InteractiveDialogGuid,
               -1,
               -1,
               68,
               20,
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
  if(ExitCode!=16)return FALSE;
  if (SaveConfig(initItems, itemsCount))
    LoadConfig();

  return TRUE;
}

void WINAPI GetGlobalInfoW(struct GlobalInfo *info)
{
  info->StructSize = sizeof(*info);
  info->MinFarVersion = MAKEFARVERSION(3, 0, 0, 0, VS_RELEASE);
  info->Version = MAKEFARVERSION(CTAGS_VERSION_MAJOR, CTAGS_VERSION_MINOR, 0, CTAGS_BUILD, VS_RELEASE);
  info->Guid = PluginGuid;
  info->Title = APPNAME;
  info->Description = CTAGS_FILE_DESCR;
  info->Author = L"Eugene Manushkin";
}

void WINAPI ExitFARW(const struct ExitInfo *info)
{
  if (config.history_file.Length() > 0)
    SaveHistory(ExpandEnvString(config.history_file.Str()));
}
