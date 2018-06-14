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
#include <sys/stat.h>
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
#include <functional>
#include <iterator>
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

//TODO: Synchronization must be reworked
class FileSynchronizer
{
public:
  using CallbackType = std::function<void(std::string const&)>;

  FileSynchronizer(std::string const& fname, CallbackType cb)
    : FileName(fname)
    , ModTime(0)
    , Callback(cb)
  {
  }

  void Synchronize()
  {
    struct stat st;
    if (stat(FileName.c_str(), &st) != -1 && ModTime != st.st_mtime)
      Callback(FileName);

    ModTime = st.st_mtime;
  }

  //TODO: rework force synchronization
  void Synchronize(std::string const& fileName, bool force)
  {
    ModTime = force || fileName != FileName ? 0 : ModTime;
    FileName = fileName;
    Synchronize();
  }

private:
  std::string FileName;
  time_t ModTime;
  CallbackType Callback;
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
int const MaxMenuWidth = 120;

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

template <typename CallType>
auto SafeCall(CallType call, decltype(call()) errorResult) ->decltype(call())
{
  try
  {
    return call();
  }
  catch (std::exception const& e)
  {
    Msg(ToString(e.what()).c_str());
  }
  catch(Error const& err)
  {
    Msg(err.GetCode());
  }

  return errorResult;
}

template <typename CallType>
void SafeCall(CallType call)
{
  SafeCall([call]() { call(); return 0; }, 0);
}

std::shared_ptr<void> LongOperationMessage(WideString const& msg)
{
  auto hScreen=I.SaveScreen(0,0,-1,-1);
  auto message = WideString(GetMsg(MPlugin)) + L"\n" + msg;
  I.Message(&PluginGuid, &InfoMessageGuid, FMSG_LEFTALIGN | FMSG_ALLINONE, nullptr, reinterpret_cast<const wchar_t* const*>(message.c_str()), 0, 0);
  return std::shared_ptr<void>(hScreen, [](void* h)
  { 
     I.RestoreScreen(h); 
  });
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

static void SetPanelDir(WideString const& dir, HANDLE hPanel)
{
  if (dir.empty())
    return; 

  FarPanelDirectory farDir = {sizeof(FarPanelDirectory), dir.c_str()};
  I.PanelControl(hPanel, FCTL_SETPANELDIRECTORY, 0, &farDir);
}

static void SelectFile(WideString const& fileName, HANDLE hPanel = PANEL_ACTIVE)
{
  auto pos = fileName.rfind('\\');
  if (pos == WideString::npos)
    return;

  SetPanelDir(fileName.substr(0, pos), hPanel);
  PanelInfo pi = {sizeof(PanelInfo)};
  I.PanelControl(hPanel, FCTL_GETPANELINFO, 0, &pi);
  size_t foundItem = 0;
  for(; foundItem < pi.ItemsNumber; ++foundItem)
  {
    auto sz = I.PanelControl(hPanel, FCTL_GETPANELITEM, foundItem, 0);
    if (!sz)
      continue;

    std::vector<char> buf(sz);
    PluginPanelItem *item = reinterpret_cast<PluginPanelItem*>(&buf[0]);
    FarGetPluginPanelItem FGPPI = {sizeof(FarGetPluginPanelItem), buf.size(), item};
    I.PanelControl(hPanel, FCTL_GETPANELITEM, foundItem, &FGPPI);
    if (FSF.ProcessName(item->FileName, const_cast<wchar_t*>(fileName.c_str()), 0, PN_CMPNAME | PN_SKIPPATH) != 0)
      break;
  }

  if (foundItem == pi.ItemsNumber)
    return;

  PanelRedrawInfo redrawInfo = { sizeof(PanelRedrawInfo), foundItem, pi.TopPanelItem };
  I.PanelControl(hPanel, FCTL_REDRAWPANEL, 0, &redrawInfo);
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

static int GetFarWidth()
{
  SMALL_RECT rect = {};
  I.AdvControl(&PluginGuid, ACTL_GETFARRECT, 0, &rect);
  return rect.Right - rect.Left;
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

static std::string GetClipboardText()
{
  if (!OpenClipboard(nullptr))
    return std::string();

  std::shared_ptr<void> clipboardCloser(0, [](void*) {CloseClipboard();});
  auto h = GetClipboardData(CF_TEXT);
  if (h == nullptr)
    return std::string();

  auto text = static_cast<char const*>(GlobalLock(h));
  std::shared_ptr<void> dataUnlocker(0, [&](void*) {GlobalUnlock(h);});
  const size_t maxClipboardLen = 128;
  size_t len = 0;
  for (; len < maxClipboardLen && text[len]; ++len);
  return text != nullptr ? std::string(text, text + len) : std::string();
}

static void SetClipboardText(std::string const& text)
{
  const size_t len = text.length() + 1;
  HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
  memcpy(GlobalLock(hMem), text.c_str(), len);
  GlobalUnlock(hMem);
  OpenClipboard(0);
  EmptyClipboard();
  SetClipboardData(CF_TEXT, hMem);
  CloseClipboard();
}

static std::string GetNormalizedClipboardText()
{
  auto text = GetClipboardText();
  text.resize(std::remove_if(text.begin(), text.end(), [](char c) {return !isident(c);}) - text.begin());
  return text;
}

static bool IsEscPressed()
{
  HANDLE h_con = GetStdHandle(STD_INPUT_HANDLE);
  INPUT_RECORD rec;
  DWORD read_cnt;
  while (true) 
  {
    PeekConsoleInputW(h_con, &rec, 1, &read_cnt);
    if (read_cnt == 0)
      break;

    ReadConsoleInputW(h_con, &rec, 1, &read_cnt);
    if (rec.EventType == KEY_EVENT)
    {
      const KEY_EVENT_RECORD& key_event = rec.Event.KeyEvent;
      if (key_event.wVirtualKeyCode == VK_ESCAPE)
        return true;
    }
  }

  return false;
}

static bool InteractiveWaitProcess(void* hProcess, WideString const& message)
{
  DWORD const tick = 1000;
  auto msg = message + L"\n" + GetMsg(MPressEscToCancel);
  auto messageHolder = LongOperationMessage(msg);
  while (WaitForSingleObject(hProcess, tick) == WAIT_TIMEOUT)
  {
    if (!IsEscPressed())
      continue;

    if (YesNoCalncelDialog(GetMsg(MAskCancel)) == YesNoCancel::Yes)
      return true;

    messageHolder.reset();
    messageHolder = LongOperationMessage(msg);
  }

  return false;
}

static void ExecuteScript(WideString const& script, WideString const& args, WideString workingDirectory, WideString const& message = WideString())
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

  DWORD exitCode = 0;
  if (InteractiveWaitProcess(ShExecInfo.hProcess, message.empty() ? L"Running: " + script + L"\nArgs:" + args : message))
  {
    TerminateProcess(ShExecInfo.hProcess, ERROR_CANCELLED);
    auto messageHolder = LongOperationMessage(GetMsg(MCanceling));
    WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
  }

  if (!GetExitCodeProcess(ShExecInfo.hProcess, &exitCode))
    throw std::runtime_error("Failed to get exit code of process: " + std::to_string(GetLastError()));

  if (exitCode == ERROR_CANCELLED)
    throw Error(MCanceled);

  if (exitCode)
    throw std::runtime_error("External utility failed with code " + std::to_string(exitCode));
}

static WideString GetSelectedItem(WideString const& DotDotSubst = L".")
{
  WideString selected = GetCurFile();
  return JoinPath(GetPanelDir(), selected == L".." ? DotDotSubst : selected);
}

int TagDirectory(WideString const& dir)
{
  if (!(GetFileAttributesW(dir.c_str()) & FILE_ATTRIBUTE_DIRECTORY))
    throw std::runtime_error("Selected item is not a direcory");

  ExecuteScript(ToString(ExpandEnvString(config.exe.Str())), ToString(config.opt.Str()), dir, WideString(GetMsg(MTagingCurrentDirectory)) + L"\n" + dir);
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

static WideString GetTempFilename()
{
  auto tempPath = GenerateTempPath();
  auto pos = tempPath.rfind('\\');
  return pos == WideString::npos ? tempPath : tempPath.substr(pos, WideString::npos);
}

static void RenameFile(WideString const& originalFile, WideString const& newFile)
{
  if (!::MoveFileExW(originalFile.c_str(), newFile.c_str(), MOVEFILE_REPLACE_EXISTING))
    throw std::runtime_error("Failed to rename file:\n" + ToStdString(originalFile) + 
                             "\nto file:\n" + ToStdString(newFile) + 
                             "\nwith error: " + std::to_string(GetLastError()));
}

static void RemoveFile(WideString const& file)
{
  if (!::DeleteFileW(file.c_str()))
    throw std::runtime_error("Failed to delete file:\n" + ToStdString(file) +
                             "\nwith error: " + std::to_string(GetLastError()));
}

static WideString RenameToTempFilename(WideString const& originalFile)
{
  auto pos = originalFile.rfind('\\');
  auto dirOfFile = pos == WideString::npos ? WideString() : originalFile.substr(0, pos);
  auto newName = JoinPath(dirOfFile, GetTempFilename());
  RenameFile(originalFile, newName);
  return newName;
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

static void LoadHistory(std::string const& fileName)
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

static void SynchronizeTagsHistory(bool force)
{
  static FileSynchronizer historySynchronizer("", &LoadHistory);
  if (config.history_len > 0)
    historySynchronizer.Synchronize(ExpandEnvString(config.history_file.Str()), force);
}

static void VisitTags(WideString const& tagsFile)
{
  SynchronizeTagsHistory(false);
  VisitedTags.Access(tagsFile);
  SaveHistory(ExpandEnvString(config.history_file.Str()));
}

static void LoadConfig(std::string const& fileName)
{
  SetDefaultConfig();
  config.autoload_changed = true;
  std::ifstream file;
  file.exceptions(std::ifstream::goodbit);
  file.open(fileName);
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
    else if(key == "tagsmask")
    {
      config.tagsmask = val.c_str();
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
  SynchronizeTagsHistory(true);
}

static void SynchronizeConfig()
{
  static FileSynchronizer configSynchronizer(ToStdString(GetConfigFilePath()), &LoadConfig);
  configSynchronizer.Synchronize();
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
  char const* ClipboardData;
  MI()
    : data(-1)
    , Disabled(false)
    , ClipboardData(nullptr)
  {
  }
  MI(WideString const& str,int value,bool disabled=false,char const* clipdata=nullptr):item(str),data(value),Disabled(disabled),ClipboardData(clipdata){}
  MI(const char* str,int value,bool disabled=false,char const* clipdata=nullptr):item(ToString(str)),data(value),Disabled(disabled),ClipboardData(clipdata){}
  MI(int msgid,int value,bool disabled=false,char const* clipdata=nullptr):item(GetMsg(msgid)),data(value),Disabled(disabled),ClipboardData(clipdata){}
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
  fk.push_back({VK_INSERT, SHIFT_PRESSED});
  fk.push_back({0x56, LEFT_CTRL_PRESSED});
  fk.push_back({0x56, RIGHT_CTRL_PRESSED});
  fk.push_back({ VK_INSERT, LEFT_CTRL_PRESSED });
  fk.push_back({ VK_INSERT, RIGHT_CTRL_PRESSED });
  fk.push_back({ 0x43, LEFT_CTRL_PRESSED });
  fk.push_back({ 0x43, RIGHT_CTRL_PRESSED });
  fk.push_back(FarKey());
  return fk;
}

bool IsCtrlC(FarKey const& key)
{
  return (key.VirtualKeyCode == VK_INSERT && key.ControlKeyState == LEFT_CTRL_PRESSED)
      || (key.VirtualKeyCode == VK_INSERT && key.ControlKeyState == RIGHT_CTRL_PRESSED)
      || (key.VirtualKeyCode == 0x43 && key.ControlKeyState == LEFT_CTRL_PRESSED)
      || (key.VirtualKeyCode == 0x43 && key.ControlKeyState == RIGHT_CTRL_PRESSED);
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
        menu[j++] = { MIF_NONE, i.second->item.c_str(), {}, reinterpret_cast<intptr_t>(i.second) };
      }
      intptr_t bkey;
      WideString ftitle = filter.Length() > 0 ? L"[Filter: " + ToString(filter.Str()) + L"]" : WideString(L" [") + title + L"]";
      WideString bottomText = flags&MF_SHOWCOUNT ? GetMsg(MItemsCount) + ToString(std::to_string(j)) : L"";
      int res = I.Menu(&PluginGuid, &CtagsMenuGuid,-1,-1,0,FMENU_WRAPMODE|FMENU_SHOWAMPERSAND,ftitle.c_str(),
                       bottomText.c_str(),L"content",&fk[0],&bkey,&menu[0],j);
      if(res==-1 && bkey==-1)return -1;
      if(bkey==-1)
      {
        return reinterpret_cast<MI const *>(menu[res].UserData)->data;
      }
      if (IsCtrlC(fk[bkey]))
      {
        if (res >= 0 && reinterpret_cast<MI const *>(menu[res].UserData)->ClipboardData)
          SetClipboardText(reinterpret_cast<MI const *>(menu[res].UserData)->ClipboardData);

        return -1;
      }
      if (bkey >= filterkeys.length())
      {
        filter += GetNormalizedClipboardText().c_str();
        continue;
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
  size_t const prefixLen = 3;
  String const junction = "...";
  size_t const beginingLen = prefixLen + junction.Length();
  return maxlength > beginingLen ? file.Substr(0, prefixLen) + junction + file.Substr(file.Length() - maxlength + beginingLen)
                                 : file.Substr(file.Length() - maxlength);
}

//TODO: rework
WideString FormatTagInfo(TagInfo const& ti, int maxid, int maxDeclaration, int maxfile, bool displayFile)
{
  std::string declaration = ti.declaration.Substr(0, maxDeclaration).Str();
  String s;
  s.Sprintf("%c:%s%*s %s%*s",ti.type,ti.name.Str(),maxid-ti.name.Length(),"",
    declaration.c_str(),maxDeclaration-declaration.length(),""
  );

  if (displayFile || ti.lineno >= 0)
  {
    auto lineNumber = ti.lineno >= 0 ? ":" + std::to_string(ti.lineno + 1) : std::string();
    s += String(" ") + TrimFilename((displayFile ? ti.file : String("Line")) + lineNumber.c_str(),maxfile);
  }

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
  auto title = GetMsg(MSelectSymbol);
  std::string filterkeys = "1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_$\\\x08-_=|;':\",./<>?[]*&^%#@!~";
  std::vector<FarKey> fk = GetFarKeys(filterkeys);
  const int currentWidth = std::min(GetFarWidth(), MaxMenuWidth);
  const int maxInfoWidth = currentWidth / 5;
  while(true)
  {
    auto tags = FindPartiallyMatchedTags(tagsFile, filter.Str(), maxCount);
//TODO: rework
    int maxid = 0;
    int maxinfo = 0;
    GetMaxParams(tags, maxid, maxinfo);
    maxinfo = std::min(maxinfo, maxInfoWidth);
    int maxfile=currentWidth-8-maxid-maxinfo-1-1-1-1;
    std::vector<FarMenuItem> menu;
    std::list<WideString> menuStrings;
    for (auto const& i : tags)
    {
      menuStrings.push_back(FormatTagInfo(i, maxid, maxinfo, maxfile, true));
      FarMenuItem item = {MIF_NONE,menuStrings.back().c_str()};
      menu.push_back(item);
    }
    intptr_t bkey;
    WideString ftitle = filter.Length() > 0 ? L"[Filter: " + ToString(filter.Str()) + L"]" : WideString(L" [") + title + L"]";
    WideString bottomText = L"";
    int res = I.Menu(&PluginGuid, &CtagsMenuGuid,-1,-1,0,FMENU_WRAPMODE|FMENU_SHOWAMPERSAND,ftitle.c_str(),
                     bottomText.c_str(),L"content",&fk[0],&bkey, menu.empty() ? nullptr : &menu[0],menu.size());
    if(res==-1 && bkey==-1)return false;
    if(bkey==-1)
    {
      tag = tags[res];
      return true;
    }
    if (IsCtrlC(fk[bkey]))
    {
      if (res != -1)
        SetClipboardText(tags[res].name.Str());

      return false;
    }
    if (bkey >= filterkeys.length())
    {
      filter += GetNormalizedClipboardText().c_str();
      continue;
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
  SynchronizeConfig();
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

static void NavigateTo(TagInfo* info, bool setPanelDir = false)
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
    int line= info->lineno < 0 ? -1 : info->lineno - 1;
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
    if (setPanelDir)
      SelectFile(ToString(info->file.Str()));

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
  line = line >= ei.TotalLines ? -1 : line;
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
    EditorGetString egs = {sizeof(EditorGetString)};
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

  if (setPanelDir)
    SelectFile(ToString(info->file.Str()));

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

static TagInfo* TagsMenu(PTagArray pta, bool displayFile = true)
{
  MenuList sm;
  String s;
  TagArray& ta=*pta;
  int maxid=0,maxDeclaration=0;
  int i;
  const int currentWidth = std::min<intptr_t>(GetFarWidth(), MaxMenuWidth);
  const int maxDeclarationWidth = currentWidth / 5;
  for(i=0;i<ta.Count();i++)
  {
    TagInfo *ti=ta[i];
    if(ti->name.Length()>maxid)maxid=ti->name.Length();
    if(ti->declaration.Length()>maxDeclaration)maxDeclaration=ti->declaration.Length();
    //if(ti->file.Length()>maxfile)
  }
  maxDeclaration = std::min(maxDeclaration, maxDeclarationWidth);
  int maxfile=currentWidth-8-maxid-maxDeclaration-1-1-1-1;
  for(i=0;i<ta.Count();i++)
  {
    sm.push_back(MI(FormatTagInfo(*ta[i], maxid, maxDeclaration, maxfile, displayFile), i, false, ta[i]->name));
  }
  int sel=FilterMenu(GetMsg(MSelectSymbol),sm,0,MF_SHOWCOUNT);
  if(sel==-1)return NULL;
  return ta[sel];
}

static WideString SelectFromHistory()
{
  SynchronizeTagsHistory(false);
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

static std::vector<WideString> GetDirectoryTags(WideString const& dir)
{
  std::vector<WideString> result;
  WIN32_FIND_DATAW fileData;
  auto pattern = JoinPath(dir, L"*");
  std::shared_ptr<void> handle(::FindFirstFileW(pattern.c_str(), &fileData), ::FindClose);
  if (handle.get() == INVALID_HANDLE_VALUE)
    return result;

  auto tagsMask = ToString(config.tagsmask.Str());
  while (FindNextFileW(handle.get(), &fileData))
  {
    auto curFile = JoinPath(dir, fileData.cFileName);
    auto test = FSF.ProcessName(tagsMask.c_str(), fileData.cFileName, 0, PN_CMPNAMELIST);
    auto atts = GetFileAttributesW(curFile.c_str());
    if (!(GetFileAttributesW(curFile.c_str()) & FILE_ATTRIBUTE_DIRECTORY) && !!FSF.ProcessName(tagsMask.c_str(), fileData.cFileName, 0, PN_CMPNAMELIST))
      result.push_back(curFile);
  }

  return result;
}

static std::string SearchTagsFile(std::string const& fileName)
{
  auto str = fileName;
  std::vector<WideString> foundTags;
  while(!str.empty())
  {
    auto pos = str.rfind('\\');
    if (pos == std::string::npos)
      break;

    str = str.substr(0, pos);
    auto tags = GetDirectoryTags(ToString(str));
    foundTags.insert(foundTags.end(), tags.begin(), tags.end());
  }

  if (foundTags.empty())
    throw Error(MTagsNotFound);

  MenuList lst;
  for (auto i = foundTags.begin(); i != foundTags.end(); ++i)
    lst.push_back(MI(*i, std::distance(foundTags.begin(), i)));

  auto res = Menu(GetMsg(MSelectTags), lst, 0, 0);
  if (res < 0)
    return std::string();

  auto tagsFile = ToStdString(foundTags[res]);
  if (tagsFile.empty() || !IsTagFile(tagsFile.c_str()))
    throw Error(MNotTagFile);

  return tagsFile;
}

static bool EnsureTagsLoaded(std::string const& fileName)
{
  if (TagsLoadedForFile(fileName.c_str()))
    return true;

  auto tagsFile = SearchTagsFile(fileName);
  if (tagsFile.empty() || !!Load(tagsFile.c_str()))
    throw Error(MENotLoaded);

  return true;
}

static WideString ReindexRepository(std::string const& fileName)
{
  auto tagsFile = ToString(SearchTagsFile(fileName));
  if (tagsFile.empty())
    return WideString();

  auto pos = tagsFile.find_last_of('\\');
  if (pos == WideString::npos)
    throw Error(MNotTagFile);

  auto reposDir = tagsFile.substr(0, pos);
  if (YesNoCalncelDialog(WideString(GetMsg(MAskReindex)) + L"\n" + reposDir + L"\n" + GetMsg(MProceed)) != YesNoCancel::Yes)
    return WideString();

  auto tempName = RenameToTempFilename(tagsFile);
  auto res = SafeCall(std::bind(TagDirectory, reposDir), 0);
  if (!res)
    RenameFile(tempName, tagsFile);
  else
    SafeCall(std::bind(RemoveFile, tempName));

  return tagsFile;
}

static void LookupSymbol(std::string const& file, bool setPanelDir)
{
  EnsureTagsLoaded(file);
  size_t const maxMenuItems = 10;
  TagInfo selectedTag;
  if (LookupTagsMenu(file.c_str(), maxMenuItems, selectedTag))
    NavigateTo(&selectedTag, setPanelDir);
}

static void FreeTagsArray(PTagArray ta)
{
  for(int i=0;i<ta->Count();i++)
  {
    delete (*ta)[i];
  }
  delete ta;
}

static std::vector<std::string> TagsToStrings(std::vector<TagInfo> const& tags)
{
  std::vector<std::string> result;
  std::transform(tags.begin(), tags.end(), std::back_inserter(result), [](TagInfo const& tag) {return tag.name.Str();});
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

HANDLE WINAPI OpenW(const struct OpenInfo *info)
{
  OPENFROM OpenFrom = info->OpenFrom;
  WideString tagfile;
  SynchronizeConfig();
  if(OpenFrom==OPEN_EDITOR)
  {
    //DebugBreak();
    auto ei = GetCurrentEditorInfo();
    std::string fileName = GetFileNameFromEditor(ei.EditorID); // TODO: auto
    LazyAutoload();
    enum{
      miFindSymbol,miUndo,miResetUndo,miReindexRepo,
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
      , MI::Separator()
      , MI(MReindexRepo, miReindexRepo)
    };
    int res=Menu(GetMsg(MPlugin),ml,0);
    if(res==-1)return nullptr;
    if ((res == miFindSymbol
      || res == miComplete
      || res == miBrowseClass
      || res == miBrowseFile
        ) && !SafeCall(std::bind(EnsureTagsLoaded, fileName), false))
    {
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
        auto lst = TagsToStrings(FindPartiallyMatchedTags(fileName.c_str(), word, 0));
        if(lst.empty())
        {
          Msg(MNothingFound);
          return nullptr;
        }
        int res = 0;
        if(lst.size()>1)
        {
          MenuList ml;
          int i = 0;
          for(auto const& name : lst)
          {
            ml.push_back(MI(name.c_str(), i++, false, name.c_str()));
          }
          res=FilterMenu(GetMsg(MSelectSymbol),ml,0,MF_SHOWCOUNT,(void*)word.Str());
          if(res==-1)return nullptr;
        }
        EditorGetString egs = {sizeof(EditorGetString)};
        egs.StringNumber=-1;
        I.EditorControl(ei.EditorID, ECTL_GETSTRING, 0, &egs);
        while(isident(egs.StringText[ei.CurPos]))ei.CurPos++;
        EditorSetPosition esp = {sizeof(EditorSetPosition)};
        esp.CurLine=-1;
        esp.CurPos=ei.CurPos;
        esp.CurTabPos=-1;
        esp.TopScreenLine=-1;
        esp.LeftPos=-1;
        esp.Overtype=-1;
        I.EditorControl(ei.EditorID, ECTL_SETPOSITION, 0, &esp);
        WideString newText(ToString(lst[res].substr(word.Length())));
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
        TagInfo *ti=TagsMenu(ta, false);
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
        TagInfo *ti=TagsMenu(ta, false);
        if(ti)NavigateTo(ti);
        FreeTagsArray(ta);
      }break;
      case miLookupSymbol:
      {
        SafeCall(std::bind(LookupSymbol, fileName, false));
      }break;
      case miReindexRepo:
      {
        tagfile = SafeCall(std::bind(ReindexRepository, fileName), WideString());
      }break;
    }
  }
  else
  {
    if(OpenFrom==OPEN_PLUGINSMENU)
    {
      enum {miLoadFromHistory,miLoadTagsFile,miUnloadTagsFile, miReindexRepo,
            miCreateTagsFile,miAddTagsToAutoload, miLookupSymbol};
      MenuList ml = {
           MI(MLookupSymbol, miLookupSymbol)
         , MI::Separator()
         , MI(MLoadTagsFile, miLoadTagsFile)
         , MI(MLoadFromHistory, miLoadFromHistory, !config.history_len)
         , MI(MUnloadTagsFile, miUnloadTagsFile)
         , MI(MAddTagsToAutoload, miAddTagsToAutoload)
         , MI::Separator()
         , MI(MCreateTagsFile, miCreateTagsFile)
         , MI(MReindexRepo, miReindexRepo)
      };
      int rc=Menu(GetMsg(MPlugin),ml,0);
      switch(rc)
      {
        case miLoadFromHistory:
        {
          tagfile = SelectFromHistory();
        }break;
        case miLoadTagsFile:
        {
          tagfile = GetSelectedItem();
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
          WideString selectedDir = GetSelectedItem(WideString());
          int rc = SafeCall(std::bind(TagDirectory, selectedDir), 0);
          if (rc)
          {
            tagfile = JoinPath(selectedDir, L"tags");
          }
        }break;
        case miAddTagsToAutoload:
        {
          auto err = AddToAutoload(ToStdString(GetSelectedItem()));
          if (err)
          {
            Msg(err);
          }
        }break;
        case miLookupSymbol:
        {
          SafeCall(std::bind(LookupSymbol, ToStdString(GetSelectedItem()), true));
        }break;
        case miReindexRepo:
        {
          tagfile = SafeCall(std::bind(ReindexRepository, ToStdString(GetSelectedItem())), WideString());
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
    if (OpenFrom == OPEN_ANALYSE)
    {
      auto fileName = reinterpret_cast<const OpenAnalyseInfo*>(info->Data)->Info->FileName;
      tagfile = fileName ? fileName : L"";
    }
  }

  if(!tagfile.empty())
  {
    size_t symbolsLoaded = 0;
    int rc=Load(ToStdString(tagfile).c_str(), symbolsLoaded);
    if(rc>1)
    {
      Msg(GetMsg(rc));
      return OpenFrom == OPEN_ANALYSE ? PANEL_STOP : nullptr;
    }
    InfoMessage(GetMsg(MLoadOk) + WideString(L":") + ToString(std::to_string(symbolsLoaded)));
    VisitTags(tagfile);
    return OpenFrom == OPEN_ANALYSE ? PANEL_STOP : nullptr;
  }

  return nullptr;
}

HANDLE WINAPI AnalyseW(const AnalyseInfo* info)
{
  return info->FileName &&
         FSF.ProcessName(ToString(config.tagsmask.Str()).c_str(), const_cast<wchar_t*>(info->FileName), 0, PN_CMPNAMELIST | PN_SKIPPATH) != 0 &&
         IsTagFile(ToStdString(info->FileName).c_str()) ? INVALID_HANDLE_VALUE : nullptr;
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
  SynchronizeConfig();
  unsigned char y = 0;
  struct InitDialogItem initItems[]={
//    Type        X1  Y2  X2 Y2  F S           Flags D Data
    DI_DOUBLEBOX, 3, ++y, 64,20, 0,0,              0,0,MPlugin,L"",{},
    DI_TEXT,      5, ++y,  0, 0, 0,0,              0,0,MPathToExe,L"",{},
    DI_EDIT,      5, ++y, 62, 3, 1,0,              0,0,-1,ToString(config.exe.Str()),{"pathtoexe", true},
    DI_TEXT,      5, ++y,  0, 0, 0,0,              0,0,MCmdLineOptions,L"",{},
    DI_EDIT,      5, ++y, 62, 5, 1,0,              0,0,-1,ToString(config.opt.Str()),{"commandline"},
    DI_TEXT,      5, ++y,  0, 0, 0,0,              0,0,MWordChars,L"",{},
    DI_EDIT,      5, ++y, 62, 9, 1,0,              0,0,-1,ToString(config.GetWordchars()),{"wordchars", true},
    DI_CHECKBOX,  5, ++y, 62,10, 1,config.casesens,0,0,MCaseSensFilt,L"",{"casesensfilt", false, true},
    DI_TEXT,      5, ++y, 62,10, 1,0,DIF_SEPARATOR|DIF_BOXCOLOR,0,-1,L"",{},
    DI_TEXT,      5, ++y,  0, 0, 0,0,              0,0,MTagsMask,L"",{},
    DI_EDIT,      5, ++y, 62, 9, 1,0,              0,0,-1,ToString(config.tagsmask.Str()),{"tagsmask", true},
    DI_TEXT,      5, ++y,  0, 0, 0,0,              0,0,MHistoryFile,L"",{},
    DI_EDIT,      5, ++y, 62, 9, 1,0,              0,0,-1,ToString(config.history_file.Str()),{"historyfile"},
    DI_TEXT,      5, ++y,  0, 0, 0,0,              0,0,MHistoryLength,L"",{},
    DI_EDIT,      5, ++y, 62, 9, 1,0,              0,0,-1,ToString(std::to_string(config.history_len)),{"historylen", true},
    DI_TEXT,      5, ++y,  0, 0, 0,0,              0,0,MAutoloadFile,L"",{},
    DI_EDIT,      5, ++y, 62, 7, 1,0,              0,0,-1,ToString(config.autoload.Str()),{"autoload"},
    DI_TEXT,      5, ++y, 62,10, 1,0,DIF_SEPARATOR|DIF_BOXCOLOR,0,-1,L"",{},
    DI_BUTTON,    0, ++y,  0, 0, 0,0,DIF_CENTERGROUP,1,MOk,L"",{},
    DI_BUTTON,    0,   y,  0, 0, 0,0,DIF_CENTERGROUP,0,MCancel,L"",{}
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
               y + 3,
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
  if(ExitCode!=18)return FALSE;
  if (SaveConfig(initItems, itemsCount))
    SynchronizeConfig();

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
}
