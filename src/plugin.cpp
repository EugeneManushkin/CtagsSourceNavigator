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
#include "tags.h"
#include "text.h"
#include "resource.h"

#include <algorithm>
#include <bitset>
#include <deque>
#include <functional>
#include <iterator>
#include <regex>
#include <sstream> 
#include <stack>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <unordered_map>

static struct PluginStartupInfo I;
FarStandardFunctions FSF;

static const wchar_t* APPNAME = CTAGS_PRODUCT_NAME;

static const wchar_t* ConfigFileName=L"config";

static const wchar_t* const DefaultTagsFilename = L"tags";

struct Config{
  Config();
  void SetWordchars(std::string const& str);
  std::string GetWordchars() const
  {
    return wordchars;
  }

  bool isident(int chr) const
  {
    return wordCharsMap[(unsigned char)chr];
  }

  std::string exe;
  std::string opt;
  std::string autoload;
  std::string tagsmask;
  std::string history_file;
  size_t history_len;
  bool casesens;
  bool autoload_changed;
  size_t max_results;
  bool cur_file_first;
  bool index_edited_file;
  bool sort_class_members_by_name;
  static const size_t max_history_len;

private:
  std::string wordchars;
  std::bitset<256> wordCharsMap;
};

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
  , max_results(10)
  , cur_file_first(true)
  , index_edited_file(true)
  , sort_class_members_by_name(false)
{
  SetWordchars("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz~$_");
}

void Config::SetWordchars(std::string const& str)
{
  wordchars = str;
  wordCharsMap.reset();
  for (auto c : str)
  {
    wordCharsMap.set((unsigned char)c, true);
  }
}

Config config;

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
size_t const MaxMenuWidth = 120;

WideString ToString(std::string const& str, UINT codePage = CP_ACP)
{
  auto sz = MultiByteToWideChar(codePage, 0, str.c_str(), static_cast<int>(str.length()), nullptr, 0);
  if (!sz)
    return WideString();

  std::vector<wchar_t> buffer(sz);
  MultiByteToWideChar(codePage, 0, str.c_str(), static_cast<int>(str.length()), &buffer[0], sz);
  return WideString(buffer.begin(), buffer.end());
}

std::string ToStdString(WideString const& str, UINT codePage = CP_ACP)
{
  auto sz = WideCharToMultiByte(codePage, 0, str.c_str(), static_cast<int>(str.length()), nullptr, 0, nullptr, nullptr);
  if (!sz)
    return std::string();

  std::vector<char> buffer(sz);
  WideCharToMultiByte(codePage, 0, str.c_str(), static_cast<int>(str.length()), &buffer[0], sz, nullptr, nullptr);
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

WideString GetFileNameFromEditor(intptr_t editorID)
{
  size_t sz = I.EditorControl(editorID, ECTL_GETFILENAME, 0, nullptr);
  if (!sz)
    return WideString();

  std::vector<wchar_t> buffer(sz);
  I.EditorControl(editorID, ECTL_GETFILENAME, buffer.size(), &buffer[0]);
  return WideString(buffer.begin(), buffer.end() - 1);
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

inline WideString GetDirOfFile(WideString const& fileName)
{
  auto pos = fileName.find_last_of(L"\\/");
  return pos == WideString::npos ? WideString() : fileName.substr(0, pos);
}

static void SelectFile(WideString const& fileName, HANDLE hPanel = PANEL_ACTIVE)
{
  auto const dirOfFile = GetDirOfFile(fileName);
  if (dirOfFile.empty())
    return;

  SetPanelDir(dirOfFile, hPanel);
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

static size_t GetFarWidth()
{
  SMALL_RECT rect = {};
  I.AdvControl(&PluginGuid, ACTL_GETFARRECT, 0, &rect);
  return rect.Right > rect.Left ? rect.Right - rect.Left : 0;
}

static size_t GetMenuWidth()
{
  size_t const borderLen = 8 + 1 + 1 + 1 + 1;
  size_t const width = std::min(GetFarWidth(), MaxMenuWidth);
  return borderLen > width ? 0 : width - borderLen;
}

static WideString ExpandEnvString(WideString const& str)
{
  auto sz = ::ExpandEnvironmentStringsW(str.c_str(), nullptr, 0);
  if (!sz)
    return WideString();

  std::vector<wchar_t> buffer(sz);
  ::ExpandEnvironmentStringsW(str.c_str(), &buffer[0], sz);
  return WideString(buffer.begin(), buffer.end() - 1);
}

static std::string ExpandEnvString(std::string const& str)
{
  return ToStdString(ExpandEnvString(ToString(str)));
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
  if (InteractiveWaitProcess(ShExecInfo.hProcess, message.empty() ? L"Running: " + script + L"\nArgs: " + args : message))
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

  ExecuteScript(ExpandEnvString(ToString(config.exe)), ToString(config.opt), dir, WideString(GetMsg(MTagingCurrentDirectory)) + L"\n" + dir);
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

static bool IsInTempDirectory(WideString const& fileName)
{
  auto const tempDir = GetDirOfFile(GenerateTempPath());
  auto const dirOfFile = GetDirOfFile(fileName);
  return !tempDir.empty() && !dirOfFile.empty() && !!FSF.ProcessName(JoinPath(tempDir, L"*").c_str(), const_cast<wchar_t*>(dirOfFile.c_str()), 0, PN_CMPNAME);
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

static void RemoveDirWithFiles(WideString const& dir)
{
  if (!(GetFileAttributesW(dir.c_str()) & FILE_ATTRIBUTE_DIRECTORY))
    return;

  WIN32_FIND_DATAW fileData;
  auto pattern = JoinPath(dir, L"*");
  std::shared_ptr<void> handle(::FindFirstFileW(pattern.c_str(), &fileData), ::FindClose);
  while (handle.get() != INVALID_HANDLE_VALUE && FindNextFileW(handle.get(), &fileData))
  {
    auto file = JoinPath(dir, fileData.cFileName);
    if (!(GetFileAttributesW(file.c_str()) & FILE_ATTRIBUTE_DIRECTORY))
      RemoveFile(file);
  }

  if (!::RemoveDirectoryW(dir.c_str()))
    throw std::runtime_error("Failed to remove directory:\n" + ToStdString(dir) +
                             "\nwith error: " + std::to_string(GetLastError()));
}

static void MkDir(WideString const& dir)
{
  if (!::CreateDirectoryW(dir.c_str(), nullptr))
    throw std::runtime_error("Failed to create directory:\n" + ToStdString(dir) +
                             "\nwith error: " + std::to_string(GetLastError()));
}

static WideString RenameToTempFilename(WideString const& originalFile)
{
  auto const newName = JoinPath(GetDirOfFile(originalFile), GetTempFilename());
  RenameFile(originalFile, newName);
  return newName;
}

static void SetDefaultConfig()
{
  config = Config();
}

static WideString GetModulePath()
{
  auto const path = GetDirOfFile(I.ModuleName);
  if (path.empty())
    throw std::invalid_argument("Invalid module path");

  return path;
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

static void SaveHistory(std::string const& fileName)
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
    historySynchronizer.Synchronize(ExpandEnvString(config.history_file), force);
}

static void VisitTags(WideString const& tagsFile)
{
  SynchronizeTagsHistory(false);
  VisitedTags.Access(tagsFile);
  SaveHistory(ExpandEnvString(config.history_file));
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
      int len = 0;
      if (sscanf(val.c_str(), "%d", &len) == 1 && len >= 0)
        config.history_len = std::min(static_cast<size_t>(len), Config::max_history_len);
    }
    else if(key == "maxresults")
    {
      int maxresults = 0;
      if (sscanf(val.c_str(), "%d", &maxresults) == 1 && maxresults > 0)
        config.max_results = maxresults;
    }
    else if(key == "curfilefirst")
    {
      config.cur_file_first = val == "true";
    }
    else if(key == "indexeditedfile")
    {
      config.index_edited_file = val == "true";
    }
    else if(key == "sortclassmembersbyname")
    {
      config.sort_class_members_by_name = val == "true";
    }
  }

  config.history_len = config.history_file.empty() ? 0 : config.history_len;
  VisitedTags = VisitedTagsLru(config.history_len);
  SynchronizeTagsHistory(true);
}

static void SynchronizeConfig()
{
  static FileSynchronizer configSynchronizer(ToStdString(GetConfigFilePath()), &LoadConfig);
  configSynchronizer.Synchronize();
}

//TODO: remove
static void SaveStrings(std::vector<std::string> const& strings, std::string const& fileName)
{
  std::ofstream file;
  file.exceptions(std::ifstream::goodbit);
  file.open(fileName);
  std::shared_ptr<void> fileCloser(0, [&](void*) { file.close(); });
  for (auto const& str : strings)
    file << str << std::endl;
}

//TODO: remove
static std::vector<std::string> LoadStrings(std::string const& fileName)
{
  std::ifstream file;
  file.exceptions(std::ifstream::goodbit);
  file.open(fileName);
  std::shared_ptr<void> fileCloser(0, [&](void*) { file.close(); });
  std::string buf;
  std::vector<std::string> result;
  while (std::getline(file, buf))
  {
    if (!buf.empty())
      result.push_back(buf);
  }

  return result;
}

//TODO: remove
static void Autoload(std::string const& fileName)
{
  for(auto const& str : LoadStrings(fileName))
  {
    if (str.length() > 1 || str[1] == ':')
    {
      size_t symbolsLoaded;
      Load(str.c_str(), false, symbolsLoaded);
    }
  }
}

//TODO: remove
static void LazyAutoload()
{
  if (config.autoload_changed)
  {
    Autoload(ExpandEnvString(config.autoload));
  }

  config.autoload_changed = false;
}

//TODO: remove
static int AddToAutoload(std::string const& fname)
{
  if (!IsTagFile(fname.c_str()))
    return MNotTagFile;

  auto autoloadFilename = ExpandEnvString(config.autoload);
  auto strings = LoadStrings(autoloadFilename);
  for (auto const& str : strings)
  {
    if (!fname.compare(str))
      return 0;
  }
  strings.push_back(fname.c_str());
  SaveStrings(strings, autoloadFilename);
  config.autoload_changed = true;
  return 0;
}

static size_t LoadTagsImpl(std::string const& tagsFile, bool singleFileRepos = false)
{
  size_t symbolsLoaded = 0;
  auto message = LongOperationMessage(GetMsg(MLoadingTags));
  if (auto err = Load(tagsFile.c_str(), singleFileRepos, symbolsLoaded))
    throw Error(err == ENOENT ? MEFailedToOpen : MFailedToWriteIndex);

  return symbolsLoaded;
}

static void LoadTags(std::string const& tagsFile, bool silent)
{
  size_t symbolsLoaded = LoadTagsImpl(tagsFile);
  if (!silent)
    InfoMessage(GetMsg(MLoadOk) + WideString(L":") + ToString(std::to_string(symbolsLoaded)));

  VisitTags(ToString(tagsFile));
}

struct MI{
  WideString item;
  int data;
  bool Disabled;
  char Label;
  MI()
    : data(-1)
    , Disabled(false)
  {
  }
  MI(WideString const& str,int value):item(str),data(value),Disabled(false),Label(0){}
  MI(int msgid,int value,bool disabled=false,char label=0):item(GetMsg(msgid)),data(value),Disabled(disabled),Label(label){}
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
  std::vector<FarMenuItem> menu(lst.size());
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
        WideString buf;
        buf = buf.empty() && !!lstItem.Label ? WideString(L"&") + wchar_t(lstItem.Label) + L" " : buf;
        buf = buf.empty() && curLabel < labelsCount ? WideString(L"&") + labels[curLabel] + L" " : buf;
        buf = buf.empty() ? L"  " : buf;
        lstItem.item = buf + lstItem.item;
        curLabel += !!lstItem.Label ? 0 : 1;
      }
      lstItem.item = lstItem.item.substr(0, MaxMenuWidth);
      menu[i].Text = lstItem.item.c_str();
      if(sel==i)menu[i].Flags |= MIF_SELECTED;
    }
    menu[i].Flags |= lstItem.IsSeparator() ? MIF_SEPARATOR : 0;
    menu[i].Flags |= lstItem.IsDisabled() ? MIF_DISABLE | MIF_GRAYED : 0;
    ++i;
  }
    WideString bottomText = flags&MF_SHOWCOUNT ? GetMsg(MItemsCount) + ToString(std::to_string(lst.size())) : L"";
    auto res=I.Menu(&PluginGuid, &CtagsMenuGuid, -1, -1, 0, FMENU_WRAPMODE, title, bottomText.c_str(),
                   L"content",NULL,NULL,&menu[0],lst.size());
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

inline std::string GetFilterKeys()
{
  return "1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_$\\\x08-_=|;':\",./<>?[]()+*&^%#@!~";
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
  fk.push_back({VK_F4});
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

bool IsF4(FarKey const& key)
{
  return key.VirtualKeyCode == VK_F4;
}

static bool GetRegex(WideString const& filter, bool caseInsensitive, std::wregex& result)
{
  try
  {
    std::regex_constants::syntax_option_type regexFlags = std::regex_constants::ECMAScript;
    regexFlags = caseInsensitive ? regexFlags | std::regex_constants::icase : regexFlags;
    result = std::wregex(filter, regexFlags);
    return true;
  }
  catch(std::exception const&)
  {
  }

  return false;
}

std::string TrimFilename(const std::string& file,size_t maxlength)
{
  if(file.length()<=maxlength)return file;
  size_t const prefixLen = 3;
  std::string const junction = "...";
  size_t const beginingLen = prefixLen + junction.length();
  return maxlength > beginingLen ? file.substr(0, prefixLen) + junction + file.substr(file.length() - maxlength + beginingLen)
                                 : file.substr(file.length() - maxlength);
}

enum class FormatTagFlag
{
  DisplayFile,
  NotDisplayFile,
  DisplayOnlyName,
};

//TODO: rework
WideString FormatTagInfo(TagInfo const& ti, size_t maxid, size_t maxDeclaration, size_t maxfile, FormatTagFlag formatFlag)
{
  std::string name = ti.name.substr(0, maxid);
  if (formatFlag == FormatTagFlag::DisplayOnlyName)
    return ToString(name);

  std::string declaration = ti.declaration.substr(0, maxDeclaration);
  std::stringstream str;
  str << ti.type << ":" << name << std::string(maxid - name.length(), ' ') << " " << declaration << std::string(maxDeclaration - declaration.length(), ' ');
  if (formatFlag == FormatTagFlag::DisplayFile || ti.lineno >= 0)
  {
    auto lineNumber = ti.lineno >= 0 ? ":" + std::to_string(ti.lineno + 1) : std::string();
    str << " " << TrimFilename((formatFlag == FormatTagFlag::DisplayFile ? ti.file : std::string("Line")) + lineNumber, maxfile);
  }

  return ToString(str.str());
}

//TODO: rework
std::vector<WideString> GetMenuStrings(std::vector<TagInfo const*> const& tags, FormatTagFlag formatFlag)
{
  size_t maxid = 0;
  size_t maxinfo = 0;
  for (auto const& ti : tags)
  {
    if (ti->name.length() > maxid)maxid = ti->name.length();
    if (ti->declaration.length() > maxinfo)maxinfo = ti->declaration.length();
  }

  const size_t currentWidth = GetMenuWidth();
  maxid = std::min(maxid, currentWidth);
  maxinfo = std::min(maxinfo, (currentWidth - maxid) / 2);
  size_t len = maxid + maxinfo;
  size_t maxfile = len > currentWidth ? 0 : currentWidth - len;
  std::vector<WideString> result;
  for (auto const& i : tags)
  {
    result.push_back(FormatTagInfo(*i, maxid, maxinfo, maxfile, formatFlag));
  }

  return result;
}

std::vector<WideString> GetMenuStrings(std::vector<TagInfo> const& tags, FormatTagFlag formatFlag = FormatTagFlag::DisplayFile)
{
  std::vector<TagInfo const*> tagsPtrs;
  std::transform(tags.begin(), tags.end(), std::back_inserter(tagsPtrs), [](TagInfo const& tag) {return &tag;});
  return GetMenuStrings(tagsPtrs, formatFlag);
}

inline int GetSortOptions(Config const& config)
{
  return SortOptions::SortByName | (config.cur_file_first ? SortOptions::CurFileFirst : 0);
}

class LookupMenuVisitor
{
public:
  virtual ~LookupMenuVisitor() = default;
  virtual std::vector<WideString> ApplyFilter(char const* filter) = 0;
  virtual std::string GetClipboardText(intptr_t index) const = 0;
  virtual TagInfo GetTag(intptr_t index) const = 0;
};

class LookupTagsVisitor : public LookupMenuVisitor
{
public:
  LookupTagsVisitor(std::string const& file)
    : File(file)
  {
  }

  std::vector<WideString> ApplyFilter(char const* filter) override
  {
    Tags = FindPartiallyMatchedTags(File.c_str(), filter, config.max_results, !config.casesens, GetSortOptions(config));
    return GetMenuStrings(Tags);
  }

  virtual std::string GetClipboardText(intptr_t index) const override
  {
    return Tags.at(index).name;
  }

  virtual TagInfo GetTag(intptr_t index) const override
  {
    return Tags.at(index);
  }

private:
  std::string const File;
  std::vector<TagInfo> Tags;
};

class SearchFileVisitor : public LookupMenuVisitor
{
public:
  SearchFileVisitor(std::string const& file)
    : File(file)
  {
  }

  std::vector<WideString> ApplyFilter(char const* filter) override
  {
    Paths = FindPartiallyMatchedFile(File.c_str(), filter, config.max_results);
    auto maxfile = GetMenuWidth();
    std::vector<WideString> menuStrings;
    std::transform(Paths.begin(), Paths.end(), std::back_inserter(menuStrings), [=](std::string const& path) {return ToString(TrimFilename(path, maxfile));});
    return menuStrings;
  }

  virtual std::string GetClipboardText(intptr_t index) const override
  {
    return Paths.at(index);
  }

  virtual TagInfo GetTag(intptr_t index) const override
  {
    TagInfo result = {};
    result.file = Paths.at(index);
    return result;
  }

private:
  std::string const File;
  std::vector<std::string> Paths;
};

class FilterMenuVisitor : public LookupMenuVisitor
{
public:
  FilterMenuVisitor(std::vector<TagInfo>&& tags, bool caseInsensitive, FormatTagFlag formatFlag)
    : Tags(std::move(tags))
    , CaseInsensitive(caseInsensitive)
    , FormatFlag(formatFlag)
  {
  }

  std::vector<WideString> ApplyFilter(char const* filter) override
  {
    FilteredTags.clear();
    std::wregex regexFilter;
    if (!*filter || !GetRegex(ToString(filter), CaseInsensitive, regexFilter))
    {
      std::transform(Tags.begin(), Tags.end(), std::back_inserter(FilteredTags), [](TagInfo const& val){return &val;});
      return GetMenuStrings(FilteredTags, FormatFlag);
    }

    std::multimap<WideString::difference_type, TagInfo const*> idx;
    for (auto const& tag : Tags)
    {
      std::wsmatch matchResult;
      auto str = FormatTagInfo(tag, tag.name.length(), tag.declaration.length(), tag.file.length(), FormatFlag);
      if (std::regex_search(str, matchResult, regexFilter) && !matchResult.empty())
        idx.insert(std::make_pair(matchResult.position(), &tag));
    }

    std::transform(idx.begin(), idx.end(), std::back_inserter(FilteredTags), [](std::multimap<WideString::difference_type, TagInfo const*>::value_type const& val){return val.second;});
    return GetMenuStrings(FilteredTags, FormatFlag);
  }

  virtual std::string GetClipboardText(intptr_t index) const override
  {
    return FilteredTags.at(index)->name;
  }

  virtual TagInfo GetTag(intptr_t index) const override
  {
    return *FilteredTags.at(index);
  }

private:
  std::vector<TagInfo> const Tags;
  bool const CaseInsensitive;
  FormatTagFlag const FormatFlag;
  std::vector<TagInfo const*> FilteredTags;
};

static void OpenInNewWindow(TagInfo const& tag);

static bool LookupTagsMenu(LookupMenuVisitor& visitor, TagInfo& tag, intptr_t separatorPos = -1)
{
  std::string filter;
  auto title = GetMsg(MSelectSymbol);
//TODO: Support platform path chars
  std::string filterkeys = GetFilterKeys();
  std::vector<FarKey> fk = GetFarKeys(filterkeys);
  intptr_t selected = -1;
  while(true)
  {
    auto menuStrings = visitor.ApplyFilter(filter.c_str());
    std::vector<FarMenuItem> menu;
    intptr_t counter = 0;
    for (auto const& i : menuStrings)
    {
      FarMenuItem item = {counter == selected ? MIF_SELECTED : MIF_NONE, i.c_str()};
      item.UserData = counter++;
      menu.push_back(item);
    }

    if (filter.empty() && separatorPos > 0 && static_cast<size_t>(separatorPos) < menu.size())
      menu.insert(menu.begin() + separatorPos, FarMenuItem({MIF_SEPARATOR}));

    intptr_t bkey;
    WideString ftitle = !filter.empty() ? L"[Filter: " + ToString(filter) + L"]" : WideString(L" [") + title + L"]";
    WideString bottomText = L"";
    selected = -1;
    auto res = I.Menu(&PluginGuid, &CtagsMenuGuid,-1,-1,0,FMENU_WRAPMODE|FMENU_SHOWAMPERSAND,ftitle.c_str(),
                     bottomText.c_str(),L"content",&fk[0],&bkey, menu.empty() ? nullptr : &menu[0],menu.size());
    if(res==-1 && bkey==-1)return false;
    if(bkey==-1)
    {
      tag = visitor.GetTag(menu[res].UserData);
      return true;
    }
    if (IsCtrlC(fk[bkey]))
    {
      if (res != -1)
        SetClipboardText(visitor.GetClipboardText(menu[res].UserData));

      return false;
    }
    if (IsF4(fk[bkey]))
    {
      OpenInNewWindow(visitor.GetTag(menu[res].UserData));
      selected = res;
      continue;
    }
    if (static_cast<size_t>(bkey) >= filterkeys.length())
    {
      filter += GetClipboardText();
      continue;
    }
    int key=filterkeys[bkey];
    if(key==8)
    {
      filter.resize(filter.empty() ? 0 : filter.length() - 1);
      continue;
    }
    filter+=(char)key;
  }
  return false;
}

void WINAPI SetStartupInfoW(const struct PluginStartupInfo *Info)
{
  I=*Info;
  FSF = *Info->FSF;
  I.FSF = &FSF;
  SynchronizeConfig();
}

inline int isident(int chr)
{
  return config.isident(chr);
}

static std::string GetWord(int offset=0)
{
//  DebugBreak();
  EditorInfo ei = GetCurrentEditorInfo();
  EditorGetString egs = {sizeof(EditorGetString)};
  egs.StringNumber=-1;
  I.EditorControl(ei.EditorID, ECTL_GETSTRING, 0, &egs);
  intptr_t pos=ei.CurPos-offset;
  if(pos<0)pos=0;
  if(pos>egs.StringLength)return "";
  if(!isident(egs.StringText[pos]))return "";
  intptr_t start=pos,end=pos;
  while(start>0 && isident(egs.StringText[start-1]))start--;
  while(end<egs.StringLength-1 && isident(egs.StringText[end+1]))end++;
  if(start==end || (!isident(egs.StringText[start])))return "";
  return ToStdString(egs.StringText).substr(start, end-start+1);
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

static bool FileExists(WideString const& filename)
{
  auto attributes = GetFileAttributesW(filename.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

static void SetPos(WideString const& filename, intptr_t line, intptr_t col, intptr_t top, intptr_t left)
{
  if (!FileExists(filename))
    throw Error(MEFailedToOpen);

  I.Editor(filename.c_str(), L"", 0, 0, -1, -1,  EF_NONMODAL | EF_IMMEDIATERETURN | EF_OPENMODE_USEEXISTING, -1, -1, CP_DEFAULT);
  if (line < 0)
    return;

  EditorInfo ei = GetCurrentEditorInfo();
  EditorSetPosition esp = {sizeof(EditorSetPosition)};
  esp.CurLine = line;
  esp.CurPos = col;
  esp.CurTabPos = -1;
  esp.TopScreenLine = top;
  esp.LeftPos = left;
  esp.Overtype = -1;
  I.EditorControl(ei.EditorID, ECTL_SETPOSITION, 0, &esp);
  I.EditorControl(ei.EditorID, ECTL_REDRAW, 0, nullptr);
}

int EnsureLine(int line, std::string const& file, std::string const& regex)
{
  if (regex.empty())
    return line;

  std::regex re = std::regex(regex);
  std::ifstream f;
  f.exceptions(std::ifstream::goodbit);
  f.open(file);
  std::shared_ptr<void> fileCloser(0, [&](void*) { f.close(); });
  if(!f.is_open())
    throw Error(MEFailedToOpen);

  std::string buf;
  if (line >= 0)
  {
    for (int i = 0; i < line && std::getline(f, buf); ++i);
    line = !std::regex_match(buf, re) ? -1 : line;
  }

  if (line < 0)
  {
    auto messageHolder = LongOperationMessage(L"not found in place, searching");
    f.seekg(0, f.beg);
    for (line = 1; std::getline(f, buf) && !std::regex_match(buf, re); ++line);
    line = f.eof() || f.fail() ? -1 : line;
  }

  return line;
}

static void OpenInNewWindow(TagInfo const& tag)
{
  int line = tag.name.empty() ? -1 : SafeCall(std::bind(EnsureLine, tag.lineno, tag.file, tag.re), -1);
  if (!tag.name.empty() && line < 0)
    return;

  auto file = ToString(tag.file);
  if (FileExists(file))
    I.Editor(file.c_str(), L"", 0, 0, -1, -1,  EF_OPENMODE_NEWIFOPEN, line, line < 0 ? -1 : 1, CP_DEFAULT);
}

static bool IsModalMode()
{
  WindowInfo info = {sizeof(WindowInfo)};
  for (auto numWindows = I.AdvControl(&PluginGuid, ACTL_GETWINDOWCOUNT, 0, nullptr); numWindows > 0 && !(info.Flags & WIF_CURRENT); --numWindows)
  {
    info.Pos = numWindows - 1;
    info.Flags = WIF_NONE;
    I.AdvControl(&PluginGuid, ACTL_GETWINDOWINFO, 0, &info);
  }

  return !!(info.Flags & WIF_CURRENT) && !!(info.Flags & WIF_MODAL);
}

static bool IsOpenedInCurrentEditor(WideString const& file)
{
  EditorInfo ei = {sizeof(EditorInfo)};
  if (!I.EditorControl(-1, ECTL_GETINFO, 0, &ei))
    return false;

  auto const editorFile = GetFileNameFromEditor(ei.EditorID);
  return FSF.ProcessName(file.c_str(), const_cast<wchar_t*>(editorFile.c_str()), 0, PN_CMPNAME) != 0;
}

class Navigator
{
public:
  using Index = size_t;

  virtual ~Navigator() = default;
  virtual void Goto(TagInfo const& tag, bool setPanelDir) = 0;
  virtual void Goto(Index index) = 0;
  virtual void GoBack() = 0;
  virtual bool CanGoBack() const = 0;
  virtual void GoForward() = 0;
  virtual bool CanGoForward() const = 0;
  virtual Index NumPositions() const = 0;
  virtual Index CurrentPosition() const = 0;
  virtual std::pair<WideString, intptr_t> GetPosition(Index index) const = 0;
  virtual void OnNewModalWindow() = 0;
  virtual void OnCloseModalWindow() = 0;
};

class InvalidNavigator : public Navigator
{
public:
  using Navigator::Index;

  virtual void Goto(TagInfo const&, bool) override
  {
    throw std::logic_error("Navigation is disabled");
  }

  virtual void Goto(Index) override
  {
    throw std::logic_error("Navigation is disabled");
  }

  virtual void GoBack() override
  {
    throw std::logic_error("Navigation is disabled");
  }

  virtual bool CanGoBack() const override
  {
    return false;
  }

  virtual void GoForward() override
  {
    throw std::logic_error("Navigation is disabled");
  }

  virtual bool CanGoForward() const override
  {
    return false;
  }

  virtual Index NumPositions() const override
  {
    return 0;
  }

  virtual Index CurrentPosition() const override
  {
    return 0;
  }

  virtual std::pair<WideString, intptr_t> GetPosition(Index) const override
  {
    throw std::logic_error("Navigation is disabled");
  }

  virtual void OnNewModalWindow() override
  {
  }

  virtual void OnCloseModalWindow() override
  {
  }
};

class PlainNavigator
{
public:
  using Index = Navigator::Index;

  PlainNavigator(bool modal);
  void Goto(TagInfo const& tag, bool setPanelDir);
  void Goto(Index index);
  void GoBack();
  bool CanGoBack() const;
  void GoForward();
  bool CanGoForward() const;
  Index NumPositions() const;
  Index CurrentPosition() const;
  std::pair<WideString, intptr_t> GetPosition(Index index) const;

private:
  struct Position
  {
    WideString File;
    intptr_t Line;
    intptr_t Pos;
    intptr_t Top;
    intptr_t Left;
  };

  bool GetPosition(Position& pos) const;
  void SavePosition(Position&& pos);
  void Move(WideString const& file, int line, bool setPanelDir);

  std::deque<Position> Stack;
  Navigator::Index Current;
  bool Modal;
};

PlainNavigator::PlainNavigator(bool modal)
  : Current(Stack.size())
  , Modal(modal)
{
}

void PlainNavigator::Goto(TagInfo const& tag, bool setPanelDir)
{
  int line = -1;
  if (!tag.name.empty())
  {
    line = EnsureLine(tag.lineno, tag.file, tag.re);
    line = line < 0 && YesNoCalncelDialog(GetMsg(MNotFoundAsk)) == YesNoCancel::Yes ? tag.lineno : line;
    if (line < 0)
      return;
  }

  Move(ToString(tag.file), line, setPanelDir);
}

void PlainNavigator::Goto(Index index)
{
  auto const& newPosition = Stack.at(index);
  Position position;
  bool havePosition = Current == Stack.size() && GetPosition(position); 
  SetPos(newPosition.File, newPosition.Line, newPosition.Pos, newPosition.Top, newPosition.Left);
  if (havePosition)
    SavePosition(std::move(position));

  Current = index;
}

void PlainNavigator::GoBack()
{
  if (CanGoBack())
    Goto(Current - 1);
}

bool PlainNavigator::CanGoBack() const
{
  return !!Current;
}

void PlainNavigator::GoForward()
{
  if (CanGoForward())
    Goto(Current + 1 == Stack.size() ? Current : Current + 1);
}

bool PlainNavigator::CanGoForward() const
{
  return Current != Stack.size();
}

PlainNavigator::Index PlainNavigator::NumPositions() const
{
  return Stack.size();
}

PlainNavigator::Index PlainNavigator::CurrentPosition() const
{
  return Current;
}

std::pair<WideString, intptr_t> PlainNavigator::GetPosition(Index index) const
{
  auto const& pos = Stack.at(index);
  return std::make_pair(pos.File, pos.Line);
}

bool PlainNavigator::GetPosition(Position& pos) const
{
  EditorInfo ei = {sizeof(EditorInfo)};
  if (!I.EditorControl(-1, ECTL_GETINFO, 0, &ei))
    return false;

  pos = {GetFileNameFromEditor(ei.EditorID), ei.CurLine, ei.CurPos, ei.TopScreenLine, ei.LeftPos};
  return true;
}

void PlainNavigator::SavePosition(PlainNavigator::Position&& pos)
{
  Stack.erase(Stack.begin() + Current, Stack.end());
  Stack.push_back(std::move(pos));
  Current = Stack.size();
}

void PlainNavigator::Move(WideString const& file, int line, bool setPanelDir)
{
  Position position;
  bool openedInCurrentEditor = IsOpenedInCurrentEditor(file);
  bool havePosition = (!Modal || openedInCurrentEditor) && GetPosition(position);
  if (Modal && !openedInCurrentEditor)
//TODO: handle open error
    I.Editor(file.c_str(), L"", 0, 0, -1, -1,  EF_OPENMODE_NEWIFOPEN, line, line < 0 ? -1 : 1, CP_DEFAULT);
  else
    SetPos(file, line > 0 ? line - 1 : line, 0, line > 1 ? line - 2 : line, 0);

  if (!Modal && setPanelDir)
    SelectFile(file);

  if (havePosition)
    SavePosition(std::move(position));
}

class NavigatorImpl : public Navigator
{
public:
  using Navigator::Index;

  NavigatorImpl()
  {
    Navigators.push(PlainNavigator(false));
  }

  virtual void Goto(TagInfo const& tag, bool setPanelDir) override
  {
    Navigators.top().Goto(tag, setPanelDir);
  }

  virtual void Goto(Index index) override
  {
    Navigators.top().Goto(index);
  }

  virtual void GoBack() override
  {
    Navigators.top().GoBack();
  }

  virtual bool CanGoBack() const override
  {
    return Navigators.top().CanGoBack();
  }

  virtual void GoForward() override
  {
    Navigators.top().GoForward();
  }

  virtual bool CanGoForward() const override
  {
    return Navigators.top().CanGoForward();
  }

  virtual Index NumPositions() const override
  {
    return Navigators.top().NumPositions();
  }

  virtual Index CurrentPosition() const override
  {
    return Navigators.top().CurrentPosition();
  }

  virtual std::pair<WideString, intptr_t> GetPosition(Index index) const override
  {
    return Navigators.top().GetPosition(index);
  }

  virtual void OnNewModalWindow() override
  {
    Navigators.push(PlainNavigator(true));
  }

  virtual void OnCloseModalWindow() override
  {
    if (Navigators.size() <= 1)
      throw std::logic_error("No more modal navigators");

    Navigators.pop();
  }

private:
  std::stack<PlainNavigator> Navigators;
};

std::unique_ptr<Navigator> NavigatorInstance(new NavigatorImpl);

static void NavigateTo(TagInfo const* info, bool setPanelDir = false)
{
  NavigatorInstance->Goto(*info, setPanelDir);
}

static void NavigateBack()
{
  NavigatorInstance->GoBack();
}

static void NavigateForward()
{
  NavigatorInstance->GoForward();
}

static void NavigationHistory()
{
  MenuList menuList;
  for (Navigator::Index i = 0; i < NavigatorInstance->NumPositions(); ++i)
  {
    auto pos = NavigatorInstance->GetPosition(i);
    menuList.push_back(MI(pos.first + WideString(L":") + ToString(std::to_string(pos.second + 1)), static_cast<int>(i)));
  }

  auto selected = static_cast<int>(NavigatorInstance->CurrentPosition());
  selected = selected == NavigatorInstance->NumPositions() ? selected - 1 : selected;
  auto index = Menu(GetMsg(MNavigationHistoryMenuTitle), menuList, selected, 0);
  if (index >= 0)
    NavigatorInstance->Goto(static_cast<Navigator::Index>(index));
}

bool OnNewModalWindow()
{
  NavigatorInstance->OnNewModalWindow();
  return true;
}

bool OnCloseModalWindow()
{
  NavigatorInstance->OnCloseModalWindow();
  return true;
}

static WideString SelectFromHistory()
{
  SynchronizeTagsHistory(false);
  if (VisitedTags.begin() == VisitedTags.end())
  {
    InfoMessage(GetMsg(MHistoryEmpty));
    return WideString();
  }

  int i = 0;
  MenuList menuList;
  for (auto const& file : VisitedTags)
  {
    menuList.push_back(MI(file, i));
    ++i;
  }

  auto rc = Menu(GetMsg(MTitleHistory), menuList, 0);
  if (rc < 0)
    return WideString();

  auto selected = VisitedTags.begin();
  std::advance(selected, rc);
  return *selected;
}

class TemprepoStorage
{
public:
  void Register(WideString const& fileFullPath, WideString const& tempDirectory)
  {
    if (!FileToTempdir.insert(std::make_pair(fileFullPath, tempDirectory)).second)
      throw std::logic_error("Internal error: file already indexed " + ToStdString(fileFullPath));

    if (!TempdirToFile.insert(std::make_pair(tempDirectory, fileFullPath)).second)
    {
      FileToTempdir.erase(fileFullPath);
      throw std::logic_error("Internal error: temporary directory already used " + ToStdString(tempDirectory));
    }
  }

  void ClearByFile(WideString const& fileFullPath)
  {
    auto const file = FileToTempdir.find(fileFullPath);
    auto const tempDir = file == FileToTempdir.end() ? TempdirToFile.end() : TempdirToFile.find(file->second);
    Clear(file, tempDir);
  }

  void ClearByTags(WideString const& tagsFile)
  {
    auto const tempDir = TempdirToFile.find(GetDirOfFile(tagsFile));
    auto const file = tempDir == TempdirToFile.end() ? FileToTempdir.end() : FileToTempdir.find(tempDir->second);
    Clear(file, tempDir);
  }

  void ClearAll()
  {
    for (auto const& i : FileToTempdir)
      SafeCall(std::bind(&TemprepoStorage::ClearTempRepository, i.second));

    FileToTempdir.clear();
    TempdirToFile.clear();
  }

private:
  using PathToPathMap = std::unordered_map<WideString, WideString>;

  std::logic_error IndexMissedError(PathToPathMap::iterator iter) const
  {
    return std::logic_error("Internal error: index missed for " + ToStdString(iter->first) + ", " + ToStdString(iter->second));
  }

  void Clear(PathToPathMap::iterator file, PathToPathMap::iterator tempDir)
  {
    if (file == FileToTempdir.end() && tempDir == TempdirToFile.end())
      return;

    if (file == FileToTempdir.end() || tempDir == TempdirToFile.end())
        throw IndexMissedError(file == FileToTempdir.end() ? tempDir : file);

    auto tempDirPath = std::move(file->second);
    FileToTempdir.erase(file);
    TempdirToFile.erase(tempDir);
    ClearTempRepository(tempDirPath);
  }

  static void ClearTempRepository(WideString const& tempDirectory)
  {
    UnloadTags(ToStdString(JoinPath(tempDirectory, DefaultTagsFilename)).c_str());
    RemoveDirWithFiles(tempDirectory);
  }

  PathToPathMap FileToTempdir;
  PathToPathMap TempdirToFile;
};

TemprepoStorage TemporaryRepositories;

static std::string RemoveFileMask(std::string const& args)
{
  auto fileMaskEndPos = config.opt.find_last_not_of(" ");
  if (fileMaskEndPos == std::string::npos)
    return args;

  auto beforeMaskPos = config.opt.rfind(' ', fileMaskEndPos);
  return config.opt.substr(0, beforeMaskPos == std::string::npos ? 0 : beforeMaskPos + 1);
}

//TODO: handle '-f tagfile' ctags flag
static bool IndexSingleFile(WideString const& fileFullPath, WideString const& tagsDirectoryPath)
{
  auto args = ToString(RemoveFileMask(config.opt));
  args += args.empty() || args.back() == ' ' ? L" " : L"";
  args += L"\"" + fileFullPath + L"\"";
  ExecuteScript(ExpandEnvString(ToString(config.exe)), args, tagsDirectoryPath);
  return !!LoadTagsImpl(ToStdString(JoinPath(tagsDirectoryPath, DefaultTagsFilename)), true);
}

static bool CreateTemporaryTags(WideString const& fileFullPath)
{
  auto tempDirPath = GenerateTempPath();
  MkDir(tempDirPath);
  TemporaryRepositories.Register(fileFullPath, tempDirPath);
  if (SafeCall(std::bind(IndexSingleFile, fileFullPath, tempDirPath), false))
    return true;

  TemporaryRepositories.ClearByFile(fileFullPath);
  return false;
}

static void ClearTemporaryTags(WideString const& tagsFile)
{
  TemporaryRepositories.ClearByTags(tagsFile);
}

static void ClearTemporaryTagsByFile(WideString const& file)
{
  TemporaryRepositories.ClearByFile(file);
}

static std::vector<WideString> GetDirectoryTags(WideString const& dir)
{
  std::vector<WideString> result;
  WIN32_FIND_DATAW fileData;
  auto pattern = JoinPath(dir, L"*");
  std::shared_ptr<void> handle(::FindFirstFileW(pattern.c_str(), &fileData), ::FindClose);
  if (handle.get() == INVALID_HANDLE_VALUE)
    return result;

  auto tagsMask = ToString(config.tagsmask);
  while (FindNextFileW(handle.get(), &fileData))
  {
    auto curFile = JoinPath(dir, fileData.cFileName);
    auto atts = GetFileAttributesW(curFile.c_str());
    if (!(GetFileAttributesW(curFile.c_str()) & FILE_ATTRIBUTE_DIRECTORY) && !!FSF.ProcessName(tagsMask.c_str(), fileData.cFileName, 0, PN_CMPNAMELIST))
      result.push_back(curFile);
  }

  return result;
}

static WideString SearchTagsFile(WideString const& fileName)
{
  auto str = fileName;
  std::vector<WideString> foundTags;
  while(!str.empty())
  {
    str = GetDirOfFile(str);
    if (str.empty())
      break;

    auto tags = GetDirectoryTags(str);
    foundTags.insert(foundTags.end(), tags.begin(), tags.end());
  }

  if (foundTags.empty())
    return WideString();

  MenuList lst;
  int i = 0;
  for (auto const& tagsFile : foundTags)
    lst.push_back(MI(tagsFile, i++));

  auto res = Menu(GetMsg(MSelectTags), lst, 0, 0);
  if (res < 0)
    return WideString();

  auto const& tagsFile = foundTags[res];
  if (!IsTagFile(ToStdString(tagsFile).c_str()))
    throw Error(MNotTagFile);

  return tagsFile;
}

static bool EnsureTagsLoaded(WideString const& fileName, bool createTempTags)
{
  if (TagsLoadedForFile(ToStdString(fileName).c_str()))
    return true;

  auto tagsFile = IsInTempDirectory(fileName) ? WideString() : SearchTagsFile(fileName);
  if (tagsFile.empty())
    return createTempTags && CreateTemporaryTags(fileName);

  LoadTags(ToStdString(tagsFile).c_str(), true);
  return true;
}

static WideString ReindexRepository(WideString const& fileName)
{
  auto tagsFile = SearchTagsFile(fileName);
  if (tagsFile.empty())
    return WideString();

  auto reposDir = GetDirOfFile(tagsFile);
  if (reposDir.empty())
    throw Error(MNotTagFile);

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

static void Lookup(WideString const& file, bool setPanelDir, LookupMenuVisitor& visitor, bool createTempTags)
{
  if (!EnsureTagsLoaded(file, createTempTags))
    return;

  TagInfo selectedTag;
  if (LookupTagsMenu(visitor, selectedTag))
    NavigateTo(&selectedTag, setPanelDir);
}

static void LookupSymbol(WideString const& file, bool setPanelDir, bool createTempTags)
{
  LookupTagsVisitor visitor(ToStdString(file));
  Lookup(file, setPanelDir, visitor, createTempTags);
}

static void LookupFile(WideString const& file, bool setPanelDir)
{
  SearchFileVisitor visitor(ToStdString(file));
  Lookup(file, setPanelDir, visitor, false);
}

static void NavigateToTag(std::vector<TagInfo>&& ta, intptr_t separatorPos, FormatTagFlag formatFlag)
{
  if (ta.empty())
    throw Error(MNothingFound);

  TagInfo tag;
  FilterMenuVisitor visitor(std::move(ta), !config.casesens, formatFlag);
  if (LookupTagsMenu(visitor, tag, separatorPos))
    NavigateTo(&tag);
}

static void NavigateToTag(std::vector<TagInfo>&& ta, FormatTagFlag formatFlag)
{
  NavigateToTag(std::move(ta), -1, formatFlag);
}

static std::vector<TagInfo>::const_iterator AdjustToContext(std::vector<TagInfo>& tags, char const* fileName)
{
  EditorInfo ei = GetCurrentEditorInfo();
  EditorGetString egs = {sizeof(EditorGetString)};
  egs.StringNumber=-1;
  if (!I.EditorControl(ei.EditorID, ECTL_GETSTRING, 0, &egs))
    return tags.cbegin();

  auto iter = FindContextTag(tags, fileName, static_cast<int>(ei.CurLine), ToStdString(egs.StringText).c_str());
  if (iter == tags.end())
    return tags.cbegin();

  auto context = *iter;
  tags.erase(iter);
  return Reorder(context, tags);
}

static void GotoDeclaration(char const* fileName)
{
  auto word = GetWord();
  if(word.empty())
    return;

  auto tags = Find(word.c_str(), fileName, GetSortOptions(config));
  if (tags.empty())
    throw Error(MNotFound);

  auto border = AdjustToContext(tags, fileName);
  if (tags.empty())
    return;

  if (tags.size() == 1)
    NavigateTo(&tags.back());
  else
    NavigateToTag(std::move(tags), static_cast<intptr_t>(std::distance(tags.cbegin(), border)), FormatTagFlag::DisplayFile);
}

static void CompleteName(char const* fileName, EditorInfo const& ei)
{
  auto word=GetWord(1);
  if(word.empty())
    return;

  auto tags = FindPartiallyMatchedTags(fileName, word.c_str(), 0, !config.casesens, SortOptions::DoNotSort);
  if(tags.empty())
    throw Error(MNothingFound);

  std::sort(tags.begin(), tags.end(), [](TagInfo const& left, TagInfo const& right) {return left.name < right.name;});
  tags.erase(std::unique(tags.begin(), tags.end(), [](TagInfo const& left, TagInfo const& right) {return left.name == right.name;}), tags.end());
  TagInfo tag = tags.back();
  if (tags.size() > 1)
  {
    FilterMenuVisitor visitor(std::move(tags), !config.casesens, FormatTagFlag::DisplayOnlyName);
    if (!LookupTagsMenu(visitor, tag))
      return;
  }

  EditorGetString egs = {sizeof(EditorGetString)};
  egs.StringNumber=-1;
  I.EditorControl(ei.EditorID, ECTL_GETSTRING, 0, &egs);
  auto pos = ei.CurPos;
  for(; isident(egs.StringText[pos]); pos++);
  EditorSetPosition esp = {sizeof(EditorSetPosition)};
  esp.CurLine=-1;
  esp.CurPos=pos;
  esp.CurTabPos=-1;
  esp.TopScreenLine=-1;
  esp.LeftPos=-1;
  esp.Overtype=-1;
  I.EditorControl(ei.EditorID, ECTL_SETPOSITION, 0, &esp);
  WideString newText(ToString(tag.name.substr(word.length())));
  I.EditorControl(ei.EditorID, ECTL_INSERTTEXT, 0, const_cast<wchar_t*>(newText.c_str()));
}

static intptr_t ConfigurePlugin();

HANDLE WINAPI OpenW(const struct OpenInfo *info)
{
  OPENFROM OpenFrom = info->OpenFrom;
  WideString tagfile;
  SynchronizeConfig();
  if(OpenFrom==OPEN_EDITOR)
  {
    auto ei = GetCurrentEditorInfo();
    auto fileName = GetFileNameFromEditor(ei.EditorID);
    LazyAutoload();
    enum{
      miFindSymbol,miGoBack,miGoForward,miReindexRepo,
      miComplete,miBrowseClass,miBrowseFile,miLookupSymbol,miSearchFile,
      miPluginConfiguration,miNavigationHistory,
    };
    MenuList ml = {
        MI(MFindSymbol,miFindSymbol)
      , MI(MCompleteSymbol,miComplete)
      , MI(MUndoNavigation,miGoBack,!NavigatorInstance->CanGoBack())
      , MI(MRepeatNavigation,miGoForward,!NavigatorInstance->CanGoForward(), 'F')
      , MI(MNavigationHistory,miNavigationHistory,!NavigatorInstance->NumPositions(), 'H')
      , MI::Separator()
      , MI(MBrowseClass,miBrowseClass)
      , MI(MBrowseSymbolsInFile,miBrowseFile)
      , MI(MLookupSymbol,miLookupSymbol)
      , MI(MSearchFile,miSearchFile)
      , MI::Separator()
      , MI(MReindexRepo, miReindexRepo)
      , MI::Separator()
      , MI(MPluginConfiguration, miPluginConfiguration, false, 'C')
    };
    int res=Menu(GetMsg(MPlugin),ml,0);
    if(res==-1)return nullptr;
    if ((res == miFindSymbol
      || res == miComplete
      || res == miBrowseClass
      || res == miBrowseFile
        ) && !SafeCall(std::bind(EnsureTagsLoaded, fileName, config.index_edited_file), false))
    {
      return nullptr;
    }

    switch(res)
    {
      case miFindSymbol:
      {
        SafeCall(std::bind(GotoDeclaration, ToStdString(fileName).c_str()));
      }break;
      case miGoBack:
      {
        SafeCall(NavigateBack);
      }break;
      case miGoForward:
      {
        SafeCall(NavigateForward);
      }break;
      case miNavigationHistory:
      {
        SafeCall(NavigationHistory);
      }break;
      case miComplete:
      {
        SafeCall(std::bind(CompleteName, ToStdString(fileName).c_str(), std::cref(ei)));
      }break;
      case miBrowseFile:
      {
        SafeCall([&]{ NavigateToTag(FindFileSymbols(ToStdString(fileName).c_str()), FormatTagFlag::NotDisplayFile); });
      }break;
      case miBrowseClass:
      {
#ifdef DEBUG
        //DebugBreak();
#endif
        auto word=GetWord();
        if(word.empty())
        {
          wchar_t buf[256] = L"";
          if(!I.InputBox(&PluginGuid, &InputBoxGuid, GetMsg(MBrowseClassTitle),GetMsg(MInputClassToBrowse),nullptr,
                      L"",buf,sizeof(buf)/sizeof(buf[0]),nullptr,0))return nullptr;
          word=ToStdString(buf);
        }
        auto options = GetSortOptions(config) & ~SortOptions::SortByName;
        options |= config.sort_class_members_by_name ? SortOptions::SortByName : 0;
        SafeCall([&]{ NavigateToTag(FindClassMembers(ToStdString(fileName).c_str(), word.c_str(), options), FormatTagFlag::DisplayFile); });
      }break;
      case miLookupSymbol:
      {
        SafeCall(std::bind(LookupSymbol, fileName, false, config.index_edited_file));
      }break;
      case miSearchFile:
      {
        SafeCall(std::bind(LookupFile, fileName, false));
      }break;
      case miReindexRepo:
      {
        tagfile = SafeCall(std::bind(ReindexRepository, fileName), WideString());
      }break;
      case miPluginConfiguration:
      {
        SafeCall(ConfigurePlugin);
      }break;
    }
  }
  else
  {
    if(OpenFrom==OPEN_PLUGINSMENU)
    {
      enum {miLoadFromHistory,miLoadTagsFile,miUnloadTagsFile, miReindexRepo,
            miCreateTagsFile,miAddTagsToAutoload, miLookupSymbol, miSearchFile,
            miPluginConfiguration, miNavigationHistory,
      };
      MenuList ml = {
           MI(MLookupSymbol, miLookupSymbol)
         , MI(MSearchFile, miSearchFile)
         , MI(MNavigationHistory, miNavigationHistory, !NavigatorInstance->NumPositions(), 'H')
         , MI::Separator()
         , MI(MLoadTagsFile, miLoadTagsFile)
         , MI(MLoadFromHistory, miLoadFromHistory, !config.history_len)
         , MI(MUnloadTagsFile, miUnloadTagsFile)
         , MI(MAddTagsToAutoload, miAddTagsToAutoload)
         , MI::Separator()
         , MI(MCreateTagsFile, miCreateTagsFile)
         , MI(MReindexRepo, miReindexRepo)
         , MI::Separator()
         , MI(MPluginConfiguration, miPluginConfiguration, false, 'C')
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
          auto files = GetFiles();
          int i = 0;
          for(auto const& file : files)
          {
            ml.push_back(MI(ToString(file), ++i));
          }
          int rc=Menu(GetMsg(MUnloadTagsFile),ml,0);
          if(rc==-1)return nullptr;
          if (!rc)
          {
            for (auto const& file : files)
              SafeCall(std::bind(ClearTemporaryTags, ToString(file)));

            UnloadAllTags();
          }
          else
          {
            auto file = files.at(rc - 1);
            SafeCall(std::bind(ClearTemporaryTags, ToString(file)));
            UnloadTags(file.c_str());
          }
        }break;
        case miCreateTagsFile:
        {
          WideString selectedDir = GetSelectedItem(WideString());
          int rc = SafeCall(std::bind(TagDirectory, selectedDir), 0);
          if (rc)
          {
//TODO: handle '-f tagfile' ctags flag
            tagfile = JoinPath(selectedDir, DefaultTagsFilename);
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
          SafeCall(std::bind(LookupSymbol, GetSelectedItem(), true, false));
        }break;
        case miSearchFile:
        {
          SafeCall(std::bind(LookupFile, GetSelectedItem(), true));
        }break;
        case miNavigationHistory:
        {
          SafeCall(NavigationHistory);
        }break;
        case miReindexRepo:
        {
          tagfile = SafeCall(std::bind(ReindexRepository, GetSelectedItem()), WideString());
        }break;
        case miPluginConfiguration:
        {
          SafeCall(ConfigurePlugin);
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
    SafeCall(std::bind(LoadTags, ToStdString(tagfile), false));
    return OpenFrom == OPEN_ANALYSE ? PANEL_STOP : nullptr;
  }

  return nullptr;
}

HANDLE WINAPI AnalyseW(const AnalyseInfo* info)
{
  return info->FileName &&
         FSF.ProcessName(ToString(config.tagsmask).c_str(), const_cast<wchar_t*>(info->FileName), 0, PN_CMPNAMELIST | PN_SKIPPATH) != 0 &&
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
  intptr_t Selected;
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

std::string SelectedToString(intptr_t selected)
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

static WideString PluginVersionString()
{
  return ToString(std::to_string(CTAGS_VERSION_MAJOR) + "." + std::to_string(CTAGS_VERSION_MINOR) + ".0." + std::to_string(CTAGS_BUILD));
}

WideString get_text(HANDLE hDlg, intptr_t ctrl_id) {
  FarDialogItemData item = { sizeof(FarDialogItemData) };
  item.PtrLength = I.SendDlgMessage(hDlg, DM_GETTEXT, ctrl_id, 0);
  std::vector<wchar_t> buf(item.PtrLength + 1);
  item.PtrData = buf.data();
  I.SendDlgMessage(hDlg, DM_GETTEXT, ctrl_id, &item);
  return WideString(item.PtrData, item.PtrLength);
}

intptr_t WINAPI ConfigureDlgProc(
    HANDLE   hDlg,
    intptr_t Msg,
    intptr_t Param1,
    void* Param2)
{
  InitDialogItem* items = reinterpret_cast<InitDialogItem*>(I.SendDlgMessage(hDlg, DM_GETDLGDATA, 0, nullptr));

  if (Msg == DN_EDITCHANGE)
  {
    items[Param1].MessageText = get_text(hDlg, Param1);
  }

  if (Msg == DN_BTNCLICK)
  {
    items[Param1].Selected = reinterpret_cast<intptr_t>(Param2);
  }

  return I.DefDlgProc(hDlg, Msg, Param1, Param2);
}

static intptr_t ConfigurePlugin()
{
  SynchronizeConfig();
  unsigned char y = 0;
  WideString menuTitle = WideString(GetMsg(MPlugin)) + L" " + PluginVersionString();
  struct InitDialogItem initItems[]={
//    Type        X1  Y2  X2 Y2  F S           Flags D Data
    DI_DOUBLEBOX, 3, ++y, 64,26, 0,0,              0,0,-1,menuTitle.c_str(),{},
    DI_TEXT,      5, ++y,  0, 0, 0,0,              0,0,MPathToExe,L"",{},
    DI_EDIT,      5, ++y, 62, 3, 1,0,              0,0,-1,ToString(config.exe),{"pathtoexe", true},
    DI_TEXT,      5, ++y,  0, 0, 0,0,              0,0,MCmdLineOptions,L"",{},
    DI_EDIT,      5, ++y, 62, 5, 1,0,              0,0,-1,ToString(config.opt),{"commandline"},
    DI_TEXT,      5, ++y, 62,10, 1,0,DIF_SEPARATOR|DIF_BOXCOLOR,0,-1,L"",{},
    DI_TEXT,      5, ++y,  0, 0, 0,0,              0,0,MMaxResults,L"",{},
    DI_EDIT,      5, ++y, 62, 9, 1,0,              0,0,-1,ToString(std::to_string(config.max_results)),{"maxresults", true},
    DI_CHECKBOX,  5, ++y, 62,10, 1,config.casesens,0,0,MCaseSensFilt,L"",{"casesensfilt", false, true},
    DI_CHECKBOX,  5, ++y, 62,10, 1,config.sort_class_members_by_name,0,0,MSortClassMembersByName,L"",{"sortclassmembersbyname", false, true},
    DI_CHECKBOX,  5, ++y, 62,10, 1,config.cur_file_first,0,0,MCurFileFirst,L"",{"curfilefirst", false, true},
    DI_CHECKBOX,  5, ++y, 62,10, 1,config.index_edited_file,0,0,MIndexEditedFile,L"",{"indexeditedfile", false, true},
    DI_TEXT,      5, ++y,  0, 0, 0,0,              0,0,MWordChars,L"",{},
    DI_EDIT,      5, ++y, 62, 9, 1,0,              0,0,-1,ToString(config.GetWordchars()),{"wordchars", true},
    DI_TEXT,      5, ++y, 62,10, 1,0,DIF_SEPARATOR|DIF_BOXCOLOR,0,-1,L"",{},
    DI_TEXT,      5, ++y,  0, 0, 0,0,              0,0,MTagsMask,L"",{},
    DI_EDIT,      5, ++y, 62, 9, 1,0,              0,0,-1,ToString(config.tagsmask),{"tagsmask", true},
    DI_TEXT,      5, ++y,  0, 0, 0,0,              0,0,MHistoryFile,L"",{},
    DI_EDIT,      5, ++y, 62, 9, 1,0,              0,0,-1,ToString(config.history_file),{"historyfile"},
    DI_TEXT,      5, ++y,  0, 0, 0,0,              0,0,MHistoryLength,L"",{},
    DI_EDIT,      5, ++y, 62, 9, 1,0,              0,0,-1,ToString(std::to_string(config.history_len)),{"historylen", true},
    DI_TEXT,      5, ++y,  0, 0, 0,0,              0,0,MAutoloadFile,L"",{},
    DI_EDIT,      5, ++y, 62, 7, 1,0,              0,0,-1,ToString(config.autoload),{"autoload"},
    DI_TEXT,      5, ++y, 62,10, 1,0,DIF_SEPARATOR|DIF_BOXCOLOR,0,-1,L"",{},
    DI_BUTTON,    0, ++y,  0, 0, 0,0,DIF_CENTERGROUP,1,MOk,L"",{},
    DI_BUTTON,    0,   y,  0, 0, 0,0,DIF_CENTERGROUP,0,MCancel,L"",{}
  };

  constexpr size_t itemsCount = sizeof(initItems)/sizeof(initItems[0]);
  struct FarDialogItem DialogItems[itemsCount];
  InitDialogItems(initItems,DialogItems,itemsCount);
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
               initItems);

  if (handle == INVALID_HANDLE_VALUE)
    return FALSE;

  std::shared_ptr<void> handleHolder(handle, [](void* h){I.DialogFree(h);});
  auto ExitCode = I.DialogRun(handle);
  if(ExitCode!=24)return FALSE;
  if (SaveConfig(initItems, itemsCount))
    SynchronizeConfig();

  return TRUE;
}

intptr_t WINAPI ConfigureW(const struct ConfigureInfo *)
{
    return SafeCall(ConfigurePlugin, FALSE);
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

intptr_t WINAPI ProcessEditorEventW(const struct ProcessEditorEventInfo *info)
{
  bool modal = IsModalMode();
  if (info->Event == EE_READ)
  {
    if (modal && SafeCall(OnNewModalWindow, false) == false)
      NavigatorInstance.reset(new InvalidNavigator);
  }
  else if (info->Event == EE_CLOSE)
  {
    SafeCall(std::bind(ClearTemporaryTagsByFile, GetFileNameFromEditor(info->EditorID)));
    if (modal && SafeCall(OnCloseModalWindow, false) == false)
      NavigatorInstance.reset(new InvalidNavigator);
  }

  return 0;
}

void WINAPI ExitFARW(const struct ExitInfo *info)
{
  TemporaryRepositories.ClearAll();
}
