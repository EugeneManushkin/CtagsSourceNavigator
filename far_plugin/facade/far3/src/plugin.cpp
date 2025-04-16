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

#include "text.h"
#include "resource.h"

#include <facade/safe_call.h>
#include <far3/current_editor_impl.h>
#include <far3/error.h>
#include <far3/plugin_sdk/api.h>
#include <far3/wide_string.h>

#include <plugin/navigator.h>
#include <tags.h>
#include <tags_repository_storage.h>
#include <tags_selector.h>
#include <tags_viewer.h>

#include <shellapi.h>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <bitset>
#include <deque>
#include <fstream>
#include <functional>
#include <iomanip>
#include <regex>
#include <sstream> 
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

using WideString = Far3::WideString;
using Far3::ToString;
using Far3::ToStdString;
using Far3::Error;

static struct PluginStartupInfo I;
FarStandardFunctions FSF;

static std::shared_ptr<Plugin::CurrentEditor> CurrentEditor;

static std::unique_ptr<Plugin::Navigator> NavigatorInstance;

static const wchar_t* APPNAME = CTAGS_PRODUCT_NAME;

static const wchar_t* const DefaultTagsFilename = L"tags";

static const bool FlushTagsCache = true;

//TODO: remove dependency
std::bitset<256> GetCharsMap(std::string const& str);

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
  std::string permanents;
  std::string tagsmask;
  std::string history_file;
  size_t history_len;
  bool casesens;
  size_t max_results;
  size_t threshold;
  size_t threshold_filter_len;
  bool cur_file_first;
  bool cached_tags_on_top;
  bool index_edited_file;
  bool sort_class_members_by_name;
  static const size_t max_history_len;
  bool use_built_in_ctags;
  size_t reset_cache_counters_timeout_hours;
  bool restore_last_visited_on_load;

private:
  std::string wordchars;
  std::bitset<256> wordCharsMap;
};

const size_t Config::max_history_len = 100;

Config::Config()
  : exe("ctags.exe")
  , opt("--c++-types=+px --c-types=+px --fields=+n -R *")
  , permanents("%USERPROFILE%\\.tags-autoload")
  , tagsmask("tags,*.tags")
  , history_file("%USERPROFILE%\\.tags-history")
  , history_len(10)
  , casesens(true)
  , max_results(10)
  , threshold(1500)
  , threshold_filter_len(2)
  , cur_file_first(true)
  , cached_tags_on_top(true)
  , index_edited_file(true)
  , sort_class_members_by_name(false)
  , use_built_in_ctags(true)
  , reset_cache_counters_timeout_hours(12)
  , restore_last_visited_on_load(true)
{
  SetWordchars("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz~$_");
}

void Config::SetWordchars(std::string const& str)
{
  wordchars = str;
  wordCharsMap = GetCharsMap(wordchars);
}

Config config;

static const wchar_t* GetMsg(int MsgId);

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

void ErrorMessage(WideString const& str)
{
  WideString msg = WideString(APPNAME) + L"\n" + str;
  I.Message(&PluginGuid, &ErrorMessageGuid, FMSG_WARNING | FMSG_MB_OK | FMSG_ALLINONE, nullptr, reinterpret_cast<const wchar_t* const*>(msg.c_str()), 0, 1);
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

static WideString ToString(Error const& e)
{
  auto message = WideString(GetMsg(e.Code));
  return e.Field.first.empty() ? message : message + L"\n" + ToString(e.Field.first) + L": " + ToString(e.Field.second);
}

void Err(std::exception_ptr e)
try
{
  std::rethrow_exception(e);
}
catch (std::exception const& e)
{
  ErrorMessage(ToString(e.what()));
}
catch(Error const& err)
{
  ErrorMessage(ToString(err));
}

using Facade::SafeCall;

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

intptr_t GetCurrentEditorID()
{
  EditorInfo ei = {sizeof(EditorInfo)};
  return !!I.EditorControl(-1, ECTL_GETINFO, 0, &ei) ? ei.EditorID : -1;
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
  size_t const borderLen = 8;
  size_t const width = std::min(GetFarWidth(), MaxMenuWidth);
  return borderLen > width ? 0 : width - borderLen;
}

static bool FindFarWindow(WideString const& file, intptr_t id, WindowInfo& info)
{
  auto i = I.AdvControl(&PluginGuid, ACTL_GETWINDOWCOUNT, 0, nullptr);
  WindowInfo wi = {sizeof(WindowInfo)};
  for (; i > 0; --i)
  {
    wi = {sizeof(WindowInfo)};
    wi.Pos = i - 1;
    I.AdvControl(&PluginGuid, ACTL_GETWINDOWINFO, 0, &wi);
    if (wi.Id == id || id < 0 && file.empty() && !!(wi.Flags & WIF_CURRENT))
      break;

    if (file.empty() || wi.Type != WTYPE_EDITOR || !wi.NameSize)
      continue;

    std::vector<wchar_t> name(wi.NameSize);
    wi.Name = &name[0];
    I.AdvControl(&PluginGuid, ACTL_GETWINDOWINFO, 0, &wi);
    if (!!FSF.ProcessName(file.c_str(), wi.Name, 0, PN_CMPNAME))
      break;
  }

  if (!i)
    return false;

  info = wi;
  info.Name = nullptr;
  return true;
}

//TODO: refactor duplications
static int CountEditors(WideString const& file)
{
  int result = 0;
  auto i = I.AdvControl(&PluginGuid, ACTL_GETWINDOWCOUNT, 0, nullptr);
  WindowInfo wi = {sizeof(WindowInfo)};
  for (; i > 0; --i)
  {
    wi = {sizeof(WindowInfo)};
    wi.Pos = i - 1;
    I.AdvControl(&PluginGuid, ACTL_GETWINDOWINFO, 0, &wi);
    if (wi.Type != WTYPE_EDITOR)
      continue;

    std::vector<wchar_t> name(wi.NameSize);
    wi.Name = &name[0];
    I.AdvControl(&PluginGuid, ACTL_GETWINDOWINFO, 0, &wi);
    result += wi.Name == file ? 1 : 0;
  }

  return result;
}

static intptr_t FindEditorID(WideString const& file)
{
  WindowInfo found = {sizeof(WindowInfo)};
  FindFarWindow(WideString(), -1, found);
  intptr_t currentID = found.Id;
  if (!FindFarWindow(file, -1, found))
    return -1;

  I.AdvControl(&PluginGuid, ACTL_SETCURRENTWINDOW, found.Pos, nullptr);
  I.AdvControl(&PluginGuid, ACTL_COMMIT, 0, nullptr);
  auto editorInfo = GetCurrentEditorInfo();
  FindFarWindow(WideString(), currentID, found);
  I.AdvControl(&PluginGuid, ACTL_SETCURRENTWINDOW, found.Pos, nullptr);
  I.AdvControl(&PluginGuid, ACTL_COMMIT, 0, nullptr);
  return editorInfo.EditorID;
}

static bool LineMatches(int line, std::wregex const& re, intptr_t editorID)
{
  EditorGetString egs = {sizeof(EditorGetString)};
  egs.StringNumber = line;
  I.EditorControl(editorID, ECTL_GETSTRING, 0, &egs);
  return std::regex_match(egs.StringText, re);
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

std::string GetErrorText(DWORD error)
{
  char* buffer = 0;
  size_t sz = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, 
                             MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<char*>(&buffer), 0, nullptr);
  std::string result(buffer, sz);
  LocalFree(buffer);
  result.erase(std::remove(result.begin(), result.end(), '\r'), result.end());
  for (; !result.empty() && result.back() == '\n'; result.erase(--result.end()));
  return std::move(result);
}

static void ExecuteScript(WideString const& script, WideString const& args, WideString workingDirectory, WideString const& message = WideString())
{
  SHELLEXECUTEINFOW ShExecInfo = {};
  ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
  ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
  ShExecInfo.hwnd = nullptr;
  ShExecInfo.lpVerb = nullptr;
  ShExecInfo.lpFile = script.c_str();
  ShExecInfo.lpParameters = args.c_str();
  ShExecInfo.lpDirectory = workingDirectory.c_str();
  ShExecInfo.nShow = 0;
  ShExecInfo.hInstApp = nullptr;
  if (!::ShellExecuteExW(&ShExecInfo))
    throw std::runtime_error("Failed to run external utility: " + GetErrorText(GetLastError()));

  DWORD exitCode = 0;
  if (InteractiveWaitProcess(ShExecInfo.hProcess, message.empty() ? L"Running: " + script + L"\nArgs: " + args : message))
  {
    TerminateProcess(ShExecInfo.hProcess, ERROR_CANCELLED);
    auto messageHolder = LongOperationMessage(GetMsg(MCanceling));
    WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
  }

  if (!GetExitCodeProcess(ShExecInfo.hProcess, &exitCode))
    throw std::runtime_error("Failed to get exit code of process: " + GetErrorText(GetLastError()));

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

static WideString GetCtagsUtilityPath();
static bool IsLocalDirectory(WideString const& directory);

void TagDirectory(WideString const& dir)
{
  if (!IsLocalDirectory(dir))
    throw std::runtime_error("Selected item is not a direcory");

  ExecuteScript(GetCtagsUtilityPath(), ToString(config.opt), dir, WideString(GetMsg(MTagingCurrentDirectory)) + L"\n" + dir);
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

static bool IsTempDirectory(WideString const& directory)
{
  auto const tempDir = GetDirOfFile(GenerateTempPath());
  return !tempDir.empty() && !directory.empty() && !!FSF.ProcessName(JoinPath(tempDir, L"*").c_str(), const_cast<wchar_t*>(directory.c_str()), 0, PN_CMPNAME);
}

static bool IsAsciiAlpha(WideString::value_type c)
{
  return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z');
}

static bool IsLocalDirectory(WideString const& directory)
{
  return directory.length() > 2
      && IsAsciiAlpha(directory[0]) && directory[1] == ':'
      && !!(GetFileAttributesW(directory.c_str()) & FILE_ATTRIBUTE_DIRECTORY);
}

static bool FileExists(WideString const& file)
{
  auto attrs = GetFileAttributesW(file.c_str());
  return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static void Copy(WideString const& originalFile, WideString const& newFile)
{
  if (!::CopyFileW(originalFile.c_str(), newFile.c_str(), 0))
    throw std::runtime_error("Failed to copy file:\n" + ToStdString(originalFile) +
                             "\nto file:\n" + ToStdString(newFile) +
                             "\nwith error: " + GetErrorText(GetLastError()));
}

static void RenameFile(WideString const& originalFile, WideString const& newFile)
{
  if (!::MoveFileExW(originalFile.c_str(), newFile.c_str(), MOVEFILE_REPLACE_EXISTING))
    throw std::runtime_error("Failed to rename file:\n" + ToStdString(originalFile) + 
                             "\nto file:\n" + ToStdString(newFile) + 
                             "\nwith error: " + GetErrorText(GetLastError()));
}

static void RemoveFile(WideString const& file)
{
  if (!::DeleteFileW(file.c_str()))
    throw std::runtime_error("Failed to delete file:\n" + ToStdString(file) +
                             "\nwith error: " + GetErrorText(GetLastError()));
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
                             "\nwith error: " + GetErrorText(GetLastError()));
}

static void MkDir(WideString const& dir)
{
  if (!::CreateDirectoryW(dir.c_str(), nullptr))
    throw std::runtime_error("Failed to create directory:\n" + ToStdString(dir) +
                             "\nwith error: " + GetErrorText(GetLastError()));
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
  return ExpandEnvString(L"%USERPROFILE%\\.tags-config");
}

static void MigrateConfig()
{
  auto obsoletConfig = JoinPath(GetModulePath(), L"config");
  auto actualConfig = GetConfigFilePath();
  if (!FileExists(actualConfig) && FileExists(obsoletConfig))
  {
    SafeCall(Copy, Facade::ExceptionHandler(), obsoletConfig, actualConfig);
    SafeCall(RemoveFile, Facade::ExceptionHandler(), obsoletConfig);
  }
}

static WideString GetCtagsUtilityPath()
{
  return config.use_built_in_ctags ? JoinPath(JoinPath(GetModulePath(), ToString(CTAGS_UTIL_DIR)), L"ctags.exe") : ExpandEnvString(ToString(config.exe));
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

using Strings = std::deque<std::string>;

static void SaveStrings(Strings const& strings, std::string const& fileName, std::ios_base::iostate exceptions = std::ifstream::goodbit)
{
  std::ofstream file;
  file.exceptions(exceptions);
  file.open(fileName);
  std::shared_ptr<void> fileCloser(0, [&](void*) { if (file.good()) file.close(); });
  for (auto const& str : strings)
    file << str << std::endl;
}

static Strings LoadStrings(std::string const& fileName)
{
  Strings result;
  std::ifstream file;
  file.exceptions(std::ifstream::goodbit);
  file.open(fileName);
  std::shared_ptr<void> fileCloser(0, [&](void*) { file.close(); });
  std::string buf;
  while (std::getline(file, buf))
  {
    if (!buf.empty())
      result.push_back(buf);
  }

  return std::move(result);
}

static Strings LoadHistory()
{
  auto result = config.history_len > 0 ? LoadStrings(ExpandEnvString(config.history_file)) : Strings();
  result.erase(std::remove_if(result.begin(), result.end(), [](const std::string &file){ return !FileExists(ToString(file)); }), result.end());
  for(; result.size() > config.history_len; result.pop_front());
  return std::move(result);
}

static void SaveHistory(Strings const& history)
{
  SaveStrings(history, ExpandEnvString(config.history_file));
}

static void VisitTags(std::string const& tagsFile)
{
  auto history = LoadHistory();
  history.erase(std::remove(history.begin(), history.end(), tagsFile), history.end());
  history.push_back(tagsFile);
  for(; history.size() > config.history_len; history.pop_front());
  SaveHistory(history);
}

static Config LoadConfig(std::string const& fileName)
{
  Config config;
  for (auto const& buf : LoadStrings(fileName))
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
      config.permanents=val.c_str();
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
    else if(key == "threshold")
    {
      int threshold = 0;
      if (sscanf(val.c_str(), "%d", &threshold) == 1 && threshold >= 0)
        config.threshold = threshold;
    }
    else if(key == "thresholdfilterlen")
    {
      int thresholdfilterlen = 0;
      if (sscanf(val.c_str(), "%d", &thresholdfilterlen) == 1 && thresholdfilterlen >= 0)
        config.threshold_filter_len = thresholdfilterlen;
    }
    else if(key == "curfilefirst")
    {
      config.cur_file_first = val == "true";
    }
    else if(key == "cachedtagsontop")
    {
      config.cached_tags_on_top = val == "true";
    }
    else if(key == "indexeditedfile")
    {
      config.index_edited_file = val == "true";
    }
    else if(key == "sortclassmembersbyname")
    {
      config.sort_class_members_by_name = val == "true";
    }
    else if(key == "usebuiltinctags")
    {
      config.use_built_in_ctags = val == "true";
    }
    else if(key == "resetcachecounterstimeouthours")
    {
      int timeout = 0;
      if (sscanf(val.c_str(), "%d", &timeout) == 1 && timeout >= 0)
        config.reset_cache_counters_timeout_hours = timeout;
    }
    else if(key == "restorelastvisitedonload")
    {
      config.restore_last_visited_on_load = val == "true";
    }
  }

  config.history_len = config.history_file.empty() ? 0 : config.history_len;
  return std::move(config);
}

static auto Storage = Tags::RepositoryStorage::Create();

using Tags::SortingOptions;

inline SortingOptions GetSortOptions(Config const& config)
{
  return SortingOptions::SortByName
       | (config.cur_file_first ? SortingOptions::CurFileFirst : SortingOptions::Default)
       | (config.cached_tags_on_top ? SortingOptions::CachedTagsOnTop : SortingOptions::Default)
  ;
}

static std::unique_ptr<Tags::Selector> GetSelector(std::string const& file)
{
  return Storage->GetSelector(file.c_str(), !config.casesens, GetSortOptions(config), config.max_results);
}

static size_t LoadTagsImpl(std::string const& tagsFile, Tags::RepositoryType type = Tags::RepositoryType::Regular)
{
  size_t symbolsLoaded = 0;
  auto message = LongOperationMessage(GetMsg(MLoadingTags));
  if (auto err = Storage->Load(tagsFile.c_str(), type, symbolsLoaded))
    throw Error(err == ENOENT ? MEFailedToOpen : MFailedToWriteIndex, "Tags file", tagsFile);

  return symbolsLoaded;
}

static void LoadTags(std::string const& tagsFile, bool silent)
{
  size_t symbolsLoaded = LoadTagsImpl(tagsFile);
  if (!silent)
    InfoMessage(GetMsg(MLoadOk) + WideString(L":") + ToString(std::to_string(symbolsLoaded)));

  VisitTags(tagsFile);
}

static void ManualResetCacheCounters(TagInfo const& tag)
{
  auto info = Storage->GetInfo(tag.Owner.TagsFile.c_str());
  std::stringstream elapsed;
  elapsed << std::setfill('0') << std::setw(2) << info.ElapsedSinceCached / 3600 << std::setw(0) << ":"
                               << std::setw(2) << (info.ElapsedSinceCached / 60) % 60 << std::setw(0) << ":"
                               << std::setw(2) << info.ElapsedSinceCached % 60;
  if (YesNoCalncelDialog(GetMsg(MAskResetCounters) + ToString(info.Root) + GetMsg(MCacheNotModified) + ToString(elapsed.str()) + L")") == YesNoCancel::Yes)
    Storage->ResetCacheCounters(info.TagsPath.c_str(), FlushTagsCache);
}

static void ResetCacheCountersOnTimeout(TagInfo const& tag)
{
  time_t timeout = config.reset_cache_counters_timeout_hours * 3600;
  if (!!timeout && Storage->GetInfo(tag.Owner.TagsFile.c_str()).ElapsedSinceCached > timeout)
    Storage->ResetCacheCounters(tag.Owner.TagsFile.c_str(), FlushTagsCache);
}

static WideString LabelToStr(char label)
{
  return !label ? WideString() : label == ' ' ? WideString(L"  ") : WideString(L"&") + wchar_t(label) + L" ";
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
  MI(WideString const& str,int value):item(str),data(value),Disabled(false){}
  MI(int msgid,int value,char label=0,bool disabled=false):item((LabelToStr(label) + GetMsg(msgid)).substr(0, MaxMenuWidth)),data(value),Disabled(disabled){}
  bool IsSeparator() const
  {
    return data == -2;
  }
  FarMenuItem GetFarItem(int selected) const
  {
    return
    {
      (IsSeparator() ? MIF_SEPARATOR : 0) | (Disabled ? MIF_DISABLE | MIF_GRAYED : 0) | (data == selected ? MIF_SELECTED : 0)
    , item.c_str()
    };
  }
  static MI Separator(int msgid = -1)
  {
    return MI(msgid == -1 ? L"" : GetMsg(msgid), -2);
  }
};

using MenuList = std::vector<MI>;

std::pair<int, FarKey> Menu(const wchar_t *title, MenuList const& lst, int sel, std::vector<FarKey> const& stopKeys)
{
  std::vector<FarMenuItem> menu(lst.size());
  std::transform(lst.begin(), lst.end(), menu.begin(), [sel](MI const& mi) {return mi.GetFarItem(sel);});
  intptr_t bkey = -1;
  auto res=I.Menu(&PluginGuid, &CtagsMenuGuid, -1, -1, 0, FMENU_WRAPMODE, title, L"",
                   L"content",stopKeys.empty() ? nullptr : &stopKeys[0],&bkey,&menu[0],lst.size());
  return std::make_pair(res == -1 ? -1 : lst.at(res).data, bkey == -1 ? FarKey() : stopKeys.at(bkey));
}

int Menu(const wchar_t *title, MenuList const& lst, int sel = 0)
{
  return Menu(title, lst, sel, std::vector<FarKey>()).first;
}

std::vector<HKL> GetKeyboardLayouts()
{
  auto sz = GetKeyboardLayoutList(0, nullptr);
  if (!sz)
    throw Error(MFailedGetKeyboardLayoutListSize, "GetLastError", std::to_string(GetLastError()));

  std::vector<HKL> result(sz);
  if (!GetKeyboardLayoutList(sz, &result[0]))
    throw Error(MFailedGetKeyboardLayoutList, "GetLastError", std::to_string(GetLastError()));

  return result;
}

std::vector<uint16_t> GetVirtualKeys(std::string const& filterkeys, HKL layout)
{
  std::vector<uint16_t> result;
  for(auto filterKey : filterkeys)
  {
    auto virtualKey = VkKeyScanExA(filterKey, layout);
    if (virtualKey == -1)
      break;

    result.push_back(virtualKey);
  }

  return result;
}

std::vector<uint16_t> GetVirtualKeys(std::string const& filterkeys)
{
  for (auto const& layout : GetKeyboardLayouts())
  {
    auto result = GetVirtualKeys(filterkeys, layout);
    if (result.size() == filterkeys.size())
      return result;
  }
  throw Error(MNoKeyboardLayout, "filter_keys", filterkeys);
}

std::vector<FarKey> GetFarKeys(std::string const& filterkeys)
{
  const auto virtualKeys = GetVirtualKeys(filterkeys);
  std::vector<FarKey> fk(virtualKeys.size());
  std::transform(virtualKeys.begin(), virtualKeys.end(), fk.begin(), ToFarKey);

  fk.push_back({VK_INSERT, SHIFT_PRESSED});
  fk.push_back({0x56, LEFT_CTRL_PRESSED});
  fk.push_back({0x56, RIGHT_CTRL_PRESSED});
  fk.push_back({ VK_INSERT, LEFT_CTRL_PRESSED });
  fk.push_back({ VK_INSERT, RIGHT_CTRL_PRESSED });
  fk.push_back({ 0x43, LEFT_CTRL_PRESSED });
  fk.push_back({ 0x43, RIGHT_CTRL_PRESSED });
  fk.push_back({VK_F4});
  fk.push_back({VK_DELETE, LEFT_CTRL_PRESSED});
  fk.push_back({VK_DELETE, RIGHT_CTRL_PRESSED});
  fk.push_back({0x5A, LEFT_CTRL_PRESSED});
  fk.push_back({0x5A, RIGHT_CTRL_PRESSED});
  fk.push_back({0x52, LEFT_CTRL_PRESSED});
  fk.push_back({0x52, RIGHT_CTRL_PRESSED});
  fk.push_back({VK_RETURN, LEFT_CTRL_PRESSED});
  fk.push_back({VK_RETURN, RIGHT_CTRL_PRESSED});
  fk.push_back({VK_TAB});
  fk.push_back({VK_BACK});
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

bool IsCtrlV(FarKey const& key)
{
  return (key.VirtualKeyCode == VK_INSERT && key.ControlKeyState == SHIFT_PRESSED)
      || (key.VirtualKeyCode == 0x56 && key.ControlKeyState == LEFT_CTRL_PRESSED)
      || (key.VirtualKeyCode == 0x56 && key.ControlKeyState == RIGHT_CTRL_PRESSED);
}

bool IsF4(FarKey const& key)
{
  return key.VirtualKeyCode == VK_F4;
}

bool IsCtrlDel(FarKey const& key)
{
  return key.VirtualKeyCode == VK_DELETE && (key.ControlKeyState == LEFT_CTRL_PRESSED || key.ControlKeyState == RIGHT_CTRL_PRESSED);
}

bool IsCtrlP(FarKey const& key)
{
  return (key.VirtualKeyCode == 0x50 && key.ControlKeyState == LEFT_CTRL_PRESSED)
      || (key.VirtualKeyCode == 0x50 && key.ControlKeyState == RIGHT_CTRL_PRESSED);
}

bool IsCtrlZ(FarKey const& key)
{
  return (key.VirtualKeyCode == 0x5A && key.ControlKeyState == LEFT_CTRL_PRESSED)
      || (key.VirtualKeyCode == 0x5A && key.ControlKeyState == RIGHT_CTRL_PRESSED);
}

bool IsCtrlR(FarKey const& key)
{
  return key.VirtualKeyCode == 0x52 && (key.ControlKeyState == LEFT_CTRL_PRESSED || key.ControlKeyState == RIGHT_CTRL_PRESSED);
}

bool IsCtrlEnter(FarKey const& key)
{
  return key.VirtualKeyCode == VK_RETURN && (key.ControlKeyState == LEFT_CTRL_PRESSED || key.ControlKeyState == RIGHT_CTRL_PRESSED);
}

bool IsTab(FarKey const& key)
{
  return key.VirtualKeyCode == VK_TAB;
}

bool IsBackspace(FarKey const& key)
{
  return key.VirtualKeyCode == VK_BACK;
}

using Tags::FormatTagFlag;

static std::vector<WideString> GetMenuStrings(Tags::TagsView const& tagsView, size_t menuWidth, FormatTagFlag formatFlag)
{
  std::string const separator = " ";
  auto colLengths = Tags::ShrinkColumnLengths(tagsView.GetMaxColumnLengths(formatFlag), separator.length(), menuWidth);
  std::vector<WideString> result;
  for (size_t i = 0; i < tagsView.Size(); result.push_back(ToString(tagsView[i].GetRaw(separator, formatFlag, colLengths))), ++i);
  return std::move(result);
}

static void OpenInNewWindow(TagInfo const& tag);

using Tags::TagsViewer;

enum class LookupResult
{
  Ok = 0,
  Goto,
  Cancel,
  Exit,
};

static std::string JoinFilters(std::string const& left, std::string const& right, bool neverEmpty = false)
{
  auto result = (left.empty() ? left : left + "|") + right;
  return result.empty() && neverEmpty ? "\"\"" : std::move(result);
}

std::vector<TagInfo> GetTagsOnTop(Tags::Selector const& selector, bool getFiles = false)
{
  return config.cached_tags_on_top ? selector.GetCachedTags(getFiles) : std::vector<TagInfo>();
}

static bool LookupOk(LookupResult result)
{
  return result == LookupResult::Ok || result == LookupResult::Goto;
}

static std::vector<FarMenuItem> MakeMenuItems(const std::vector<WideString>& menuStrings, intptr_t separatorPos)
{
  std::vector<FarMenuItem> result;
  for (size_t i = 0; i < menuStrings.size(); ++i)
  {
    result.push_back(FarMenuItem({MIF_NONE, menuStrings[i].c_str()}));
    result.back().UserData = i;
    if (i == separatorPos)
      result.push_back(FarMenuItem({MIF_SEPARATOR}));
  }

  return result;
}

static void ResetSelected(std::vector<FarMenuItem>& menu, intptr_t selected)
{
  for (size_t i = 0; i < menu.size(); ++i)
  {
    auto& flags = menu[i].Flags;
    flags = i == selected ? MIF_SELECTED : (flags & ~MIF_SELECTED);
  }
}

static LookupResult LookupTagsMenu(TagsViewer const& viewer, TagInfo& tag, std::vector<TagInfo> const& tagsOnTop, FormatTagFlag formatFlag = FormatTagFlag::Default, intptr_t separatorPos = -1, std::string&& prevFilter = "", bool throttle_search = false)
{
  std::string filter;
  auto title = GetMsg(MSelectSymbol);
//TODO: Support platform path chars
  static std::string filterkeys = "1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_$\\-_=|;':\",./<>?[]()+*&^%#@!~";
  static std::vector<FarKey> fk = GetFarKeys(filterkeys);
  while(true)
  {
    size_t threshold_filter_len = config.threshold_filter_len > 0 ? config.threshold_filter_len : std::numeric_limits<size_t>::max();
    size_t threshold = throttle_search && !!filter.length() && filter.length() <= threshold_filter_len ? config.threshold : 0;
    bool thresholdReached = false;
    auto tagsView = viewer.GetView(filter.c_str(), formatFlag, threshold, thresholdReached);
    auto menuStrings = GetMenuStrings(tagsView, GetMenuWidth(), formatFlag);
    std::vector<FarMenuItem> menu = MakeMenuItems(menuStrings, filter.empty() ? separatorPos : -1);
    auto displayFilter = JoinFilters(prevFilter, filter);
    WideString ftitle = !displayFilter.empty() ? L"[Filter: " + ToString(displayFilter) + L"]" : WideString(L" [") + title + L"]";
    ftitle += L": " + ToString(std::to_string(tagsView.Size()));
    ftitle += thresholdReached ? (WideString(L" ") + GetMsg(MAndMore)) : WideString();
    intptr_t selected = -1;
    while(true)
    {
      ResetSelected(menu, selected);
      intptr_t bkey = -1;
      selected = I.Menu(&PluginGuid, &CtagsMenuGuid,-1,-1,0,FMENU_WRAPMODE|FMENU_SHOWAMPERSAND,ftitle.c_str(),
                       GetMsg(MLookupMenuBottom),L"content",&fk[0],&bkey, menu.empty() ? nullptr : &menu[0],menu.size());
      if(selected == -1 && bkey == -1) return LookupResult::Cancel;
      auto selectedTag = selected >= 0 ? tagsView[menu[selected].UserData].GetTag() : nullptr;
      if(bkey==-1)
      {
        tag = *selectedTag;
        return LookupResult::Ok;
      }
      if (selectedTag && IsCtrlEnter(fk[bkey]))
      {
        tag = *selectedTag;
        return LookupResult::Goto;
      }
      if (IsCtrlC(fk[bkey]))
      {
        if (selectedTag)
          SetClipboardText(selectedTag->name.empty() ? selectedTag->file : selectedTag->name);

        return LookupResult::Exit;
      }
      if (selectedTag && IsF4(fk[bkey]))
      {
        OpenInNewWindow(*selectedTag);
      }
      if (selectedTag && IsCtrlDel(fk[bkey]))
      {
        ResetCacheCountersOnTimeout(*selectedTag);
        Storage->EraseCachedTag(*selectedTag, FlushTagsCache);
      }
      if (selectedTag && IsCtrlR(fk[bkey]))
      {
        ManualResetCacheCounters(*selectedTag);
      }
      if (IsCtrlZ(fk[bkey]))
      {
        return LookupResult::Exit;
      }
      if(IsTab(fk[bkey]))
      {
        LookupResult result = LookupResult::Ok;
        if ((prevFilter.empty() || !filter.empty()) &&
            (result = LookupTagsMenu(*Tags::GetFilterTagsViewer(tagsView, !config.casesens, tagsOnTop), tag, tagsOnTop, formatFlag, -1, JoinFilters(prevFilter, filter, true))) != LookupResult::Cancel)
          return result;
      }
      if (IsCtrlV(fk[bkey]))
      {
        filter += GetClipboardText();
        break;
      }
      if(IsBackspace(fk[bkey]))
      {
        if (!prevFilter.empty() && filter.empty())
          return LookupResult::Cancel;

        filter.resize(filter.empty() ? 0 : filter.length() - 1);
        break;
      }
      if (bkey < static_cast<intptr_t>(filterkeys.length()))
      {
        filter+=filterkeys[bkey];
        break;
      }
    }
  }
  return LookupResult::Cancel;
}

static MenuList GetRepoMenuList(std::vector<Tags::RepositoryInfo> const& repositories, Strings const& history)
{
  MenuList menuList;
  if (repositories.empty() || repositories.front().Type == Tags::RepositoryType::Permanent)
    menuList.push_back(MI(MNoRegularRepositories, -1, 0, true));

  int i = 0;
  Tags::RepositoryType currentType = Tags::RepositoryType::Regular;
  for (auto const& r : repositories)
  {
    if (currentType != r.Type)
      menuList.push_back(MI::Separator(r.Type == Tags::RepositoryType::Permanent ? MPermanentRepositories : -1));

    menuList.push_back(MI(ToString(r.Root), i++));
    currentType = r.Type;
  }

  menuList.push_back(MI::Separator(MTitleHistory));
  for (auto const& h : history)
  {
    menuList.push_back(MI(ToString(h), i++));
  }

  if (history.empty())
    menuList.push_back(MI(MHistoryEmpty, -1, 0, true));

  return std::move(menuList);
}

static WideString GetRepoLastVisitedDir(Tags::RepositoryInfo const& info)
{
  auto path = ToString(info.LastVisited);
  path = FileExists(path) ? GetDirOfFile(path) : path;
  auto attrs = GetFileAttributesW(path.c_str());
  return attrs != INVALID_FILE_ATTRIBUTES ? path : ToString(info.Root);
}

static void SavePermanents();
static void LoadPermanents();
static void AddPermanent(std::string const& tagsFile);

static void ManageRepositories()
{
  std::vector<FarKey> const stopKeys = {{VK_DELETE, LEFT_CTRL_PRESSED}, {VK_DELETE, RIGHT_CTRL_PRESSED}, {0x50, LEFT_CTRL_PRESSED}, {0x50, RIGHT_CTRL_PRESSED}};
  Tags::RepositoryInfo repo;
  int selected = 0;
  while(repo.TagsPath.empty())
  {
    LoadPermanents();
    auto repositories = Storage->GetByType(~Tags::RepositoryType::Temporary);
    auto regulars_count = std::distance(repositories.begin(), std::stable_partition(repositories.begin(), repositories.end(), [](Tags::RepositoryInfo const& repo) {return repo.Type == Tags::RepositoryType::Regular;}));
    auto history = LoadHistory();
    for (size_t i = 0; i < history.size() / 2; std::swap(history[i], history[history.size() - 1 - i]), ++i);
    auto res = Menu(GetMsg(MUnloadTagsFile), GetRepoMenuList(repositories, history), selected, stopKeys);
    if (res.first == -1)
      return;

    bool repository_selected = res.first < repositories.size();
    size_t index = repository_selected ? res.first : res.first - repositories.size();
    if (IsCtrlDel(res.second) && repository_selected)
    {
      selected = index > 0 && (index + 1 == regulars_count || index + 1 == repositories.size()) ? res.first - 1 : res.first;
      Storage->Remove(repositories.at(index).TagsPath.c_str());
      SavePermanents();
    }
    else if (IsCtrlDel(res.second))
    {
      selected = index > 0 && index + 1 == history.size() ? res.first - 1 : res.first;
      history.erase(history.begin() + index);
      for (size_t i = 0; i < history.size() / 2; std::swap(history[i], history[history.size() - 1 - i]), ++i);
      SaveHistory(history);
    }
    else if (IsCtrlP(res.second))
    {
      selected = 0;
      auto tagsPath = repository_selected ? repositories.at(index).TagsPath : history.at(index);
      AddPermanent(tagsPath);
    }
    else if (repository_selected)
    {
      auto selected = repositories.at(index);
      repo = SafeCall(LoadTags, Err, selected.TagsPath, true).first ? selected : repo;
    }
    else
    {
      auto tagsFile = history.at(index);
      repo = SafeCall(LoadTags, Err, tagsFile, true).first ? Storage->GetInfo(tagsFile.c_str()) : repo;
    }
  }

  auto dir = config.restore_last_visited_on_load ? GetRepoLastVisitedDir(repo) : ToString(repo.Root);
  SetPanelDir(dir, PANEL_ACTIVE);
}

static void SetLastVisited(intptr_t editorID)
{
  auto file = GetFileNameFromEditor(editorID);
  auto dir = ToStdString(GetDirOfFile(file));
  auto owners = Storage->GetOwners(ToStdString(file).c_str());
  for (auto const& owner : owners)
  {
    Storage->SetLastVisited(owner.TagsPath.c_str(), dir.c_str(), FlushTagsCache);
  }
}

void WINAPI SetStartupInfoW(const struct PluginStartupInfo *Info)
{
  I=*Info;
  FSF = *Info->FSF;
  I.FSF = &FSF;
  MigrateConfig();
  config = LoadConfig(ToStdString(GetConfigFilePath()));
  CurrentEditor = Far3::CreateCurrentEditor(I, PluginGuid); //TODO: prevent loading plugin if failed
  NavigatorInstance = Plugin::Navigator::Create(CurrentEditor); //TODO: prevent loading plugin if failed
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

//TODO: remove
static std::string GetCppInclude(std::string const& str)
{
  std::smatch sm;
  return std::regex_match(str, sm, std::regex("^\\s*#\\s*include\\s*<(.+)>\\s*")) ? sm[1] : std::string();
}

static std::string GetQuotedString(std::string const& line, size_t pos)
{
  auto IsQuotes = [](char c) { return c == '"' || c == '\''; };
  auto middle = line.begin() + std::min(pos, line.size());
  auto left = line.begin();
  for (auto right = left == line.end() ? left : left + 1; left < middle && right != line.end(); ++right)
  {
    if (IsQuotes(*right) && *left == *right && left < middle && middle < right)
      return std::string(left + 1, right);

    left = !IsQuotes(*left) || (left != right - 1 && *left == *(right - 1)) ? right : left;
  }

  return std::string();
}

static std::string GetStringLiteral()
{
  EditorInfo ei = GetCurrentEditorInfo();
  EditorGetString egs = {sizeof(EditorGetString)};
  egs.StringNumber = -1;
  I.EditorControl(ei.EditorID, ECTL_GETSTRING, 0, &egs);
  if(ei.CurPos > egs.StringLength)
     return std::string();

  auto pos = static_cast<size_t>(ei.CurPos < 0 ? 0 : ei.CurPos);
  auto line = ToStdString(egs.StringText);
  auto result = GetQuotedString(line, pos);
//TODO: remove language specific logic
  return !result.empty() ? result : GetCppInclude(line);
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

static int EnsureLineInFile(int line, std::string const& file, std::string const& regex)
{
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

static int EnsureLineInEditor(int line, intptr_t editorID, std::string const& regex)
{
  auto re = std::wregex(ToString(regex));
  EditorInfo ei = {sizeof(EditorInfo)};
  I.EditorControl(editorID, ECTL_GETINFO, 0, &ei);
  line = line >= ei.TotalLines || !LineMatches(line - 1, re, editorID) ? -1 : line;
  if (line < 0)
  {
    auto messageHolder = LongOperationMessage(L"not found in place, searching");
    for (line = 0; line < ei.TotalLines && !LineMatches(line, re, editorID); ++line);
    line = line >= ei.TotalLines ? -1 : line + 1;
  }

  return line;
}

int EnsureLine(int line, std::string const& file, std::string const& regex)
{
  if (regex.empty())
    return line;

  auto id = !CurrentEditor->IsModal() ? FindEditorID(ToString(file))
          : CurrentEditor->IsOpened(file.c_str()) ? GetCurrentEditorID()
          : -1;
  return id < 0 ? EnsureLineInFile(line, file, regex) : EnsureLineInEditor(line, id, regex);
}

static Plugin::EditorPosition MakeEditorPosition(std::string const& file, int line)
{
  return {
      file
    , line < 0 ? Plugin::EditorPosition::DefaultLine : line - 1
    , line < 0 ? Plugin::EditorPosition::DefaultPos : 0
    , line < 0 ? Plugin::EditorPosition::DefaultLine : std::max(line, 2) - 2
  };
}

static void OpenInNewWindow(TagInfo const& tag)
{
  auto ensured = tag.name.empty() ? std::make_pair(true, -1) : SafeCall(EnsureLine, Err, tag.lineno, tag.file, tag.re);
  auto line = ensured.first ? ensured.second : -1;
  if (tag.name.empty() || line >= 0)
    CurrentEditor->OpenModal(MakeEditorPosition(tag.file, line));
}

static void CacheTag(TagInfo const& tag)
{
  ResetCacheCountersOnTimeout(tag);
  if (!tag.name.empty())
    Storage->CacheTag(Tags::MakeFileTag(TagInfo(tag)), config.max_results, FlushTagsCache);

  Storage->CacheTag(tag, config.max_results, FlushTagsCache);
}

static void NavigateTo(TagInfo const& tag, bool setPanelDir = false)
{
  int line = tag.lineno;
  if (!tag.name.empty())
  {
    line = EnsureLine(tag.lineno, tag.file, tag.re);
    line = line < 0 && YesNoCalncelDialog(GetMsg(MNotFoundAsk)) == YesNoCancel::Yes ? tag.lineno : line;
    if (line < 0)
      return;
  }

  CacheTag(tag);
  NavigatorInstance->Open(MakeEditorPosition(tag.file, line));
  if (!CurrentEditor->IsModal() && setPanelDir)
    SelectFile(ToString(tag.file));
}

static void NavigateBack()
{
  NavigatorInstance->GoBack();
}

static void NavigateForward()
{
  NavigatorInstance->GoForward();
}

static void NavigationHistory(bool setPanelDir)
{
  MenuList menuList;
  auto size = NavigatorInstance->HistorySize();
  for (Plugin::Navigator::Index i = size; i > 0; --i)
  {
    auto pos = NavigatorInstance->GetHistoryPosition(i - 1);
    auto num = size - i + 1;
    char label = num < 10 ? '0' + static_cast<char>(num) : ' ';
    menuList.push_back(MI(LabelToStr(label) + ToString(pos.File) + WideString(L":") + ToString(std::to_string(pos.Line + 1)), static_cast<int>(i - 1)));
  }

  auto selected = NavigatorInstance->CurrentHistoryIndex();
  selected = selected == size && size > 0 ? size - 1 : selected;
  auto index = Menu(GetMsg(MNavigationHistoryMenuTitle), menuList, static_cast<int>(selected));
  if (index < 0)
    return;

  auto pos = NavigatorInstance->GetHistoryPosition(index);
  NavigatorInstance->Open(pos);
  if (!CurrentEditor->IsModal() && setPanelDir)
    SelectFile(ToString(pos.File));
}

void OnNewEditor()
{
  NavigatorInstance->OnNewEditor();
}

void OnCloseEditor()
{
  NavigatorInstance->OnCloseEditor();
}

static std::string RemoveFileMask(std::string const& args)
{
  auto fileMaskEndPos = config.opt.find_last_not_of(" ");
  if (fileMaskEndPos == std::string::npos)
    return args;

  auto beforeMaskPos = config.opt.rfind(' ', fileMaskEndPos);
  return config.opt.substr(0, beforeMaskPos == std::string::npos ? 0 : beforeMaskPos + 1);
}

//TODO: handle '-f tagfile' ctags flag
static void IndexSingleFile(WideString const& fileFullPath, WideString const& tagsDirectoryPath)
{
  auto args = ToString(RemoveFileMask(config.opt));
  args += args.empty() || args.back() == ' ' ? L" " : L"";
  args += L"\"" + fileFullPath + L"\"";
  ExecuteScript(GetCtagsUtilityPath(), args, tagsDirectoryPath);
}

static bool CreateTemporaryTags(WideString const& fileFullPath)
{
  auto tempDirPath = GenerateTempPath();
  MkDir(tempDirPath);
  auto tagsFile = ToStdString(JoinPath(tempDirPath, DefaultTagsFilename));
  if (SafeCall(IndexSingleFile, Err, fileFullPath, tempDirPath).first &&
      SafeCall(LoadTagsImpl, Err, tagsFile, Tags::RepositoryType::Temporary).second > 0)
    return true;

  Storage->Remove(tagsFile.c_str());
  RemoveDirWithFiles(tempDirPath);
  return false;
}

static void ClearTemporaryTagsByFile(WideString const& file)
{
  for (auto const& owner : Storage->GetOwners(ToStdString(file).c_str()))
  {
    if (owner.Type == Tags::RepositoryType::Temporary)
    {
      Storage->Remove(owner.TagsPath.c_str());
      SafeCall(RemoveDirWithFiles, Err, GetDirOfFile(ToString(owner.TagsPath)));
    }
  }
}

using SearchPattern = std::pair<DWORD, WideString>;
using SearchRequest = std::unordered_map<int, SearchPattern>;
using SearchResults = std::unordered_map<int, std::vector<WideString>>;

static bool FilenameMatches(WideString const& filename, SearchPattern const& pattern)
{
  auto filenameOffset = filename.find_last_of(L"\\/") + 1; // std::string::npos == (size_t)-1 => std::string::npos + 1 == 0
  return !!(GetFileAttributesW(filename.c_str()) & pattern.first) && !!FSF.ProcessName(pattern.second.c_str(), const_cast<WideString::value_type*>(filename.c_str() + filenameOffset), 0, PN_CMPNAMELIST);
}

static void SearchInDirectory(WideString const& dir, SearchRequest const& request, SearchResults& results)
{
  std::vector<WideString> result;
  WIN32_FIND_DATAW fileData;
  auto pattern = JoinPath(dir, L"*");
  std::shared_ptr<void> handle(::FindFirstFileW(pattern.c_str(), &fileData), ::FindClose);
  while (handle.get() != INVALID_HANDLE_VALUE && FindNextFileW(handle.get(), &fileData))
  {
    auto curFile = JoinPath(dir, fileData.cFileName);
    auto found = std::find_if(request.begin(), request.end(), [&curFile](SearchRequest::value_type const& r){return FilenameMatches(curFile.c_str(), r.second);});
    if (found != request.end())
      results[found->first].push_back(curFile);
  }
}

static SearchResults SearchInParents(WideString const& fileName, SearchRequest const& request)
{
  auto const dirOfFile = GetDirOfFile(fileName);
  bool isRegular = IsLocalDirectory(dirOfFile) && !IsTempDirectory(dirOfFile);
  SearchResults results;
  for (auto dir = GetDirOfFile(fileName); isRegular && !dir.empty(); dir = GetDirOfFile(dir))
    SearchInDirectory(dir, request, results);

  return std::move(results);
}

enum WhatSearch
{
  TAGS_FILES = 1 << 0,
  SCM_DIRS = 1 << 1,
};

static bool Found(int what, SearchResults const& searched)
{
  return searched.count(what) > 0 && !searched.at(what).empty();
}

static SearchResults SearchInParents(WideString const& fileName, int what)
{
  SearchRequest request;
  if (!!(what & TAGS_FILES)) request[TAGS_FILES] = {~FILE_ATTRIBUTE_DIRECTORY, ToString(config.tagsmask)};
  if (!!(what & SCM_DIRS)) request[SCM_DIRS] = {FILE_ATTRIBUTE_DIRECTORY, L".git,.svn"};
  return SearchInParents(fileName, request);
}

static WideString SelectTags(std::vector<WideString> const& foundTags)
{
  MenuList lst;
  int i = 0;
  for (auto const& tagsFile : foundTags)
    lst.push_back(MI(tagsFile, i++));

  auto res = Menu(GetMsg(MSelectTags), lst);
  if (res < 0)
    return WideString();

  auto const& tagsFile = foundTags[res];
  if (!Tags::IsTagFile(ToStdString(tagsFile).c_str()))
    throw Error(MNotTagFile);

  return tagsFile;
}

static std::vector<WideString> FilterLoadedTags(std::vector<WideString>&& tagsFiles)
{
  auto isLoaded = [](WideString const& tagsFile){return !Storage->GetInfo(ToStdString(tagsFile).c_str()).TagsPath.empty();};
  tagsFiles.erase(std::remove_if(tagsFiles.begin(), tagsFiles.end(), isLoaded), tagsFiles.end());
  return std::move(tagsFiles);
}

static WideString IndexSelectedRepository(std::vector<WideString> const& repositories)
{
  MenuList lst;
  int i = 0;
  for (auto const& str : repositories)
    lst.push_back(MI(str, i++));

  auto res = Menu(GetMsg(MAskIndexFoundRepositories), lst);
  if (res < 0)
    return WideString();

  auto dir = GetDirOfFile(repositories.at(res));
  TagDirectory(dir);
//TODO: handle '-f tagfile' ctags flag
  return JoinPath(dir, DefaultTagsFilename);
}

static bool LoadMultipleTags(Strings const& tags, Tags::RepositoryType type = Tags::RepositoryType::Regular)
{
  auto errCount = static_cast<size_t>(0);
  for (auto const& tag : tags)
    errCount += SafeCall(LoadTagsImpl, Err, tag, type).first ? 0 : 1;

  return errCount < tags.size();
}

static Strings RepositoriesToTagsPaths(std::vector<Tags::RepositoryInfo> const& repositories)
{
  Strings result;
  std::transform(repositories.begin(), repositories.end(), std::back_inserter(result), [](Tags::RepositoryInfo const& r){ return r.TagsPath; });
  return std::move(result);
}

static void RemoveNotOf(Strings const& permanents)
{
  std::unordered_set<std::string> loaded;
  for (auto const& r : permanents)
  {
    auto info = Storage->GetInfo(r.c_str());
    if (info.Type == Tags::RepositoryType::Permanent)
      loaded.insert(std::move(info.TagsPath));
    else if (!info.TagsPath.empty())
      Storage->Remove(info.TagsPath.c_str());
  }

  for (auto const& r : Storage->GetByType(Tags::RepositoryType::Permanent))
  {
    if (!loaded.count(r.TagsPath))
      Storage->Remove(r.TagsPath.c_str());
  }
}

static std::string GetPermanentsFilePath()
{
  return ExpandEnvString(config.permanents);
}

static void SavePermanents()
{
  SaveStrings(RepositoriesToTagsPaths(Storage->GetByType(Tags::RepositoryType::Permanent)), GetPermanentsFilePath());
}

static void LoadPermanents()
{
  auto permanents = LoadStrings(GetPermanentsFilePath());
  RemoveNotOf(permanents);
  LoadMultipleTags(permanents, Tags::RepositoryType::Permanent);
  SavePermanents();
}

static void AddPermanent(std::string const& tagsFile)
{
  Storage->Remove(tagsFile.c_str());
  LoadPermanents();
  LoadTagsImpl(tagsFile, Tags::RepositoryType::Permanent);
  SavePermanents();
  VisitTags(tagsFile);
}

static void AddPermanentRepository(WideString const& fileName)
{
  auto searched = SearchInParents(fileName, TAGS_FILES | SCM_DIRS);
  auto selected = Found(TAGS_FILES, searched) ? SelectTags(searched[TAGS_FILES])
                : Found(SCM_DIRS, searched) ? IndexSelectedRepository(searched[SCM_DIRS])
                : WideString();
  if (!selected.empty())
    AddPermanent(ToStdString(selected));
}

static bool EnsureOwnersLoaded(WideString const& fileName, bool createTempTags)
{
  auto tags = RepositoriesToTagsPaths(Storage->GetOwners(ToStdString(fileName).c_str()));
  if (!tags.empty())
    return LoadMultipleTags(tags);

  auto searched = SearchInParents(fileName, TAGS_FILES | SCM_DIRS);
  searched[TAGS_FILES] = FilterLoadedTags(std::move(searched[TAGS_FILES]));
  auto selected = Found(TAGS_FILES, searched) ? SelectTags(searched[TAGS_FILES])
                : Found(SCM_DIRS, searched) ? IndexSelectedRepository(searched[SCM_DIRS])
                : WideString();
  if (!selected.empty())
    LoadTags(ToStdString(selected).c_str(), Found(TAGS_FILES, searched));
  else if (createTempTags)
    CreateTemporaryTags(fileName);

  return !Storage->GetOwners(ToStdString(fileName).c_str()).empty();
}

static bool EnsureTagsLoaded(WideString const& fileName, bool createTempTags)
{
  LoadPermanents();
  return EnsureOwnersLoaded(fileName, createTempTags);
}

static WideString ReindexTagsFile(WideString const& tagsFile)
{
  auto reposDir = GetDirOfFile(tagsFile);
  if (reposDir.empty())
    return WideString();

  if (YesNoCalncelDialog(WideString(GetMsg(MAskReindex)) + L"\n" + reposDir + L"\n" + GetMsg(MProceed)) != YesNoCancel::Yes)
    return WideString();

  auto tempName = RenameToTempFilename(tagsFile);
  if (!SafeCall(TagDirectory, Err, reposDir).first)
    RenameFile(tempName, tagsFile);
  else
    SafeCall(RemoveFile, Err, tempName);

  return tagsFile;
}

static WideString ReindexRepository(WideString const& fileName)
{
  auto searched = SearchInParents(fileName, TAGS_FILES | SCM_DIRS);
  return Found(TAGS_FILES, searched) ? ReindexTagsFile(SelectTags(searched[TAGS_FILES]))
       : Found(SCM_DIRS, searched) ? IndexSelectedRepository(searched[SCM_DIRS])
       : WideString();
}

static Tags::RepositoryInfo SelectRepository(std::vector<Tags::RepositoryInfo>&& repositories)
{
  if (repositories.size() <= 1)
    return repositories.empty() ? Tags::RepositoryInfo() : Tags::RepositoryInfo(std::move(repositories.back()));

  std::vector<WideString> tagsFiles;
  std::transform(repositories.begin(), repositories.end(), std::back_inserter(tagsFiles), [](Tags::RepositoryInfo const& r){ return ToString(r.TagsPath); });
  auto selected = SelectTags(tagsFiles);
  return selected.empty() ? Tags::RepositoryInfo() : Storage->GetInfo(ToStdString(selected).c_str());
}

static void UpdateFileInRepositoryImpl(WideString const& fileName, WideString const& tempDirectory, Tags::RepositoryInfo const& repo)
{
  auto message = LongOperationMessage(GetMsg(MIndexingFile));
  IndexSingleFile(fileName, tempDirectory);
  auto commit = Storage->UpdateTagsByFile(repo.TagsPath.c_str(), ToStdString(fileName).c_str(), ToStdString(JoinPath(tempDirectory, DefaultTagsFilename)).c_str());
  try
  {
    commit();
  }
  catch (std::exception const& e)
  {
    throw Error(MTagsCorrupted, "Error", e.what());
  }
}

static bool UpdateFileInRepository(WideString const& fileName, Tags::RepositoryInfo const& repo)
{
  auto tempDirectory = GenerateTempPath();
  MkDir(tempDirectory);
  bool success = SafeCall(UpdateFileInRepositoryImpl, Err, fileName, tempDirectory, repo).first;
  SafeCall(RemoveDirWithFiles, Err, tempDirectory);
  return success;
}

static WideString ReindexFile(WideString const& fileName)
{
  auto owners = Storage->GetOwners(ToStdString(fileName).c_str());
  if (owners.empty())
  {
    auto searched = SearchInParents(fileName, TAGS_FILES | SCM_DIRS);
    searched[TAGS_FILES] = FilterLoadedTags(std::move(searched[TAGS_FILES]));
    if (!Found(TAGS_FILES, searched))
      return Found(SCM_DIRS, searched) ? IndexSelectedRepository(searched[SCM_DIRS]) : WideString();

    auto selected = SelectTags(searched[TAGS_FILES]);
    if (!selected.empty())
      LoadTags(ToStdString(selected).c_str(), true);

    owners = Storage->GetOwners(ToStdString(fileName).c_str());
  }

  auto repo = SelectRepository(std::move(owners));
  bool updated = false;
  if (repo.Type == Tags::RepositoryType::Temporary)
    updated = (IndexSingleFile(fileName, GetDirOfFile(ToString(repo.TagsPath))), true);
  else if (repo.Type != Tags::RepositoryInfo().Type)
    updated = UpdateFileInRepository(fileName, repo);

  return updated ? ToString(repo.TagsPath) : WideString();
}

static void Lookup(WideString const& file, bool getFiles, bool setPanelDir, bool createTempTags)
{
  if (!EnsureTagsLoaded(file, createTempTags))
    return;

  TagInfo selectedTag;
  auto selector = GetSelector(ToStdString(file));
  auto tagsOnTop = GetTagsOnTop(*selector, getFiles);
  auto menuResult = LookupTagsMenu(*Tags::GetPartiallyMatchedViewer(std::move(selector), getFiles), selectedTag, tagsOnTop, FormatTagFlag::Default, -1, "", true);
  if (setPanelDir && menuResult == LookupResult::Goto)
    SelectFile(ToString(selectedTag.file));
  else if (LookupOk(menuResult))
    NavigateTo(selectedTag, setPanelDir);
}

static void NavigateToTag(std::vector<TagInfo>&& ta, intptr_t separatorPos, std::vector<TagInfo> const& tagsOnTop, FormatTagFlag formatFlag = FormatTagFlag::Default)
{
  TagInfo tag;
  if (!ta.empty() && LookupOk(LookupTagsMenu(*Tags::GetFilterTagsViewer(Tags::TagsView(std::move(ta)), !config.casesens, tagsOnTop), tag, tagsOnTop, formatFlag, separatorPos)))
    NavigateTo(tag);
}

static void NavigateToTag(std::vector<TagInfo>&& ta, std::vector<TagInfo> const& tagsOnTop, FormatTagFlag formatFlag)
{
  NavigateToTag(std::move(ta), -1, tagsOnTop, formatFlag);
}

static std::vector<TagInfo>::const_iterator AdjustToContext(std::vector<TagInfo>& tags, char const* fileName)
{
  EditorInfo ei = GetCurrentEditorInfo();
  EditorGetString egs = {sizeof(EditorGetString)};
  egs.StringNumber=-1;
  if (!I.EditorControl(ei.EditorID, ECTL_GETSTRING, 0, &egs))
    return tags.cbegin();

  auto iter = Tags::FindContextTag(tags, fileName, static_cast<int>(ei.CurLine), ToStdString(egs.StringText).c_str());
  if (iter == tags.end())
    return tags.cbegin();

  auto context = *iter;
  tags.erase(iter);
  return Tags::Reorder(context, tags);
}

static void GotoDeclaration(char const* fileName, std::string word)
{
  auto selector = GetSelector(fileName);
  auto tags = selector->GetByName(word.c_str());
  if (tags.empty())
    return;

  auto border = AdjustToContext(tags, fileName);
  if (tags.empty())
    return;

  if (tags.size() == 1)
    NavigateTo(tags.back());
  else
    NavigateToTag(std::move(tags), static_cast<intptr_t>(std::distance(tags.cbegin(), border)), GetTagsOnTop(*selector));
}

static void GotoFile(char const* fileName, std::string path)
{
  auto selector = GetSelector(fileName);
  auto tags = selector->GetFiles(path.c_str());
  if (tags.size() == 1)
    NavigateTo(tags.back());
  else if (!tags.empty())
    NavigateToTag(std::move(tags), tags.size(), GetTagsOnTop(*selector, true));
}

static void FindSymbol(char const* fileName)
{
  std::string str;
  if (!(str = GetStringLiteral()).empty())
    GotoFile(fileName, str);
  else if (!(str = GetWord()).empty())
    GotoDeclaration(fileName, str);
}

static void CompleteName(char const* fileName, EditorInfo const& ei)
{
  auto word=GetWord(1);
  if(word.empty())
    return;

  auto selector = GetSelector(fileName);
  auto tags = selector->GetByPart(word.c_str(), false, true);
  if(tags.empty())
    return;

  std::sort(tags.begin(), tags.end(), [](TagInfo const& left, TagInfo const& right) {return left.name < right.name;});
  tags.erase(std::unique(tags.begin(), tags.end(), [](TagInfo const& left, TagInfo const& right) {return left.name == right.name;}), tags.end());
  TagInfo tag = tags.back();
  auto tagsOnTop = tags.size() > 1 ? GetTagsOnTop(*selector) : std::vector<TagInfo>();
  if (tags.size() > 1 && !LookupOk(LookupTagsMenu(*Tags::GetFilterTagsViewer(Tags::TagsView(std::move(tags)), !config.casesens, tagsOnTop), tag, tagsOnTop, FormatTagFlag::DisplayOnlyName)))
    return;

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
  config = LoadConfig(ToStdString(GetConfigFilePath()));
  if(OpenFrom==OPEN_EDITOR)
  {
    auto ei = GetCurrentEditorInfo();
    auto fileName = GetFileNameFromEditor(ei.EditorID);
    enum{
      miFindSymbol,miGoBack,miGoForward,miReindexRepo,miReindexFile,
      miComplete,miBrowseClass,miBrowseFile,miLookupSymbol,miSearchFile,
      miPluginConfiguration,miNavigationHistory,
    };
    MenuList ml = {
        MI(MFindSymbol,miFindSymbol, '1')
      , MI(MCompleteSymbol,miComplete, '2')
      , MI(MUndoNavigation,miGoBack, '3', !NavigatorInstance->CanGoBack())
      , MI(MRepeatNavigation,miGoForward, 'F', !NavigatorInstance->CanGoForward())
      , MI(MNavigationHistory,miNavigationHistory, 'H', !NavigatorInstance->HistorySize())
      , MI::Separator()
      , MI(MBrowseClass,miBrowseClass, '4')
      , MI(MBrowseSymbolsInFile,miBrowseFile, '5')
      , MI(MLookupSymbol,miLookupSymbol, '6')
      , MI(MSearchFile,miSearchFile, '7')
      , MI::Separator()
      , MI(MReindexRepo, miReindexRepo, '8')
      , MI(MReindexFile, miReindexFile, 'R')
      , MI::Separator()
      , MI(MPluginConfiguration, miPluginConfiguration, 'C')
    };
    int res=Menu(GetMsg(MPlugin),ml,miReindexFile);
    if(res==-1)return nullptr;
    if ((res == miFindSymbol
      || res == miComplete
      || res == miBrowseClass
      || res == miBrowseFile
        ) && !SafeCall(EnsureTagsLoaded, Err, fileName, config.index_edited_file).second)
    {
      return nullptr;
    }

    switch(res)
    {
      case miFindSymbol:
      {
        SafeCall(FindSymbol, Err, ToStdString(fileName).c_str());
      }break;
      case miGoBack:
      {
        SafeCall(NavigateBack, Err);
      }break;
      case miGoForward:
      {
        SafeCall(NavigateForward, Err);
      }break;
      case miNavigationHistory:
      {
        SafeCall(NavigationHistory, Err, false);
      }break;
      case miComplete:
      {
        SafeCall(CompleteName, Err, ToStdString(fileName).c_str(), ei);
      }break;
      case miBrowseFile:
      {
        SafeCall([&fileName]{
          auto selector = GetSelector(ToStdString(fileName).c_str());
          NavigateToTag(selector->GetByFile(ToStdString(fileName).c_str()), GetTagsOnTop(*selector), FormatTagFlag::NotDisplayFile);
        }, Err);
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
        auto options = GetSortOptions(config) & static_cast<SortingOptions>(~static_cast<int>(SortingOptions::SortByName));
        options = options | (config.sort_class_members_by_name ? SortingOptions::SortByName : SortingOptions::Default);
        SafeCall([&fileName, &word]{
          auto selector = GetSelector(ToStdString(fileName).c_str());
          NavigateToTag(selector->GetClassMembers(word.c_str()), GetTagsOnTop(*selector), FormatTagFlag::Default);
        }, Err);
      }break;
      case miLookupSymbol:
      {
        SafeCall(Lookup, Err, fileName, false, false, config.index_edited_file);
      }break;
      case miSearchFile:
      {
        SafeCall(Lookup, Err, fileName, true, false, config.index_edited_file);
      }break;
      case miReindexRepo:
      {
        tagfile = SafeCall(ReindexRepository, Err, fileName).second;
      }break;
      case miReindexFile:
      {
        tagfile = SafeCall(ReindexFile, Err, fileName).second;
      }break;
      case miPluginConfiguration:
      {
        SafeCall(ConfigurePlugin, Err);
      }break;
    }
  }
  else
  {
    if(OpenFrom==OPEN_PLUGINSMENU)
    {
      enum {miLoadFromHistory,miLoadTagsFile,miUnloadTagsFile, miReindexRepo,
            miCreateTagsFile,miAddPermanentRepository, miLookupSymbol, miSearchFile,
            miPluginConfiguration, miNavigationHistory,
      };
      MenuList ml = {
           MI(MLookupSymbol, miLookupSymbol, '1')
         , MI(MSearchFile, miSearchFile, '2')
         , MI(MNavigationHistory, miNavigationHistory, 'H', !NavigatorInstance->HistorySize())
         , MI::Separator()
         , MI(MLoadTagsFile, miLoadTagsFile, '3')
         , MI(MLoadFromHistory, miLoadFromHistory, '4', !config.history_len)
         , MI(MManageRepositories, miUnloadTagsFile, '5')
         , MI(MAddPermanentRepository, miAddPermanentRepository, '6')
         , MI::Separator()
         , MI(MCreateTagsFile, miCreateTagsFile, '7')
         , MI(MReindexRepo, miReindexRepo, '8')
         , MI::Separator()
         , MI(MPluginConfiguration, miPluginConfiguration, 'C')
      };
      int rc=Menu(GetMsg(MPlugin),ml,miReindexRepo);
      switch(rc)
      {
        case miLoadFromHistory:
        {
          InfoMessage(GetMsg(MLoadTagsFromHistoryDeprecated));
          SafeCall(ManageRepositories, Err);
        }break;
        case miLoadTagsFile:
        {
          tagfile = GetSelectedItem();
        }break;
        case miUnloadTagsFile:
        {
          SafeCall(ManageRepositories, Err);
        }break;
        case miCreateTagsFile:
        {
          WideString selectedDir = GetSelectedItem(WideString());
          if (SafeCall(TagDirectory, Err, selectedDir).first)
          {
//TODO: handle '-f tagfile' ctags flag
            tagfile = JoinPath(selectedDir, DefaultTagsFilename);
          }
        }break;
        case miAddPermanentRepository:
        {
          SafeCall(AddPermanentRepository, Err, GetSelectedItem());
        }break;
        case miLookupSymbol:
        {
          SafeCall(Lookup, Err, GetSelectedItem(), false, true, false);
        }break;
        case miSearchFile:
        {
          SafeCall(Lookup, Err, GetSelectedItem(), true, true, false);
        }break;
        case miNavigationHistory:
        {
          SafeCall(NavigationHistory, Err, true);
        }break;
        case miReindexRepo:
        {
          tagfile = SafeCall(ReindexRepository, Err, GetSelectedItem()).second;
        }break;
        case miPluginConfiguration:
        {
          SafeCall(ConfigurePlugin, Err);
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
    SafeCall(LoadTags, Err, ToStdString(tagfile), false);
    return OpenFrom == OPEN_ANALYSE ? PANEL_STOP : nullptr;
  }

  return nullptr;
}

HANDLE WINAPI AnalyseW(const AnalyseInfo* info)
{
  return info->FileName &&
         FSF.ProcessName(ToString(config.tagsmask).c_str(), const_cast<wchar_t*>(info->FileName), 0, PN_CMPNAMELIST | PN_SKIPPATH) != 0 &&
         Tags::IsTagFile(ToStdString(info->FileName).c_str()) ? INVALID_HANDLE_VALUE : nullptr;
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

static void SaveConfig(InitDialogItem const* dlgItems, size_t count)
{
  Strings strings;
  for (size_t i = 0; i < count; ++i)
  {
    auto const& item = dlgItems[i];
    if (!item.Save.KeyName.empty() && (!item.Save.NotNull || !item.MessageText.empty()))
      strings.push_back(item.Save.KeyName + "=" + (item.Save.IsCheckbox ? SelectedToString(item.Selected) : ToStdString(item.MessageText)));
  }

  SaveStrings(strings, ToStdString(GetConfigFilePath()), std::ifstream::badbit);
}

static WideString PluginVersionString()
{
  return ToString(std::to_string(CTAGS_VERSION_MAJOR) + "." +
                  std::to_string(CTAGS_VERSION_MINOR) + "." +
                  std::to_string(CTAGS_VERSION_REVISION) + "." +
                  std::to_string(CTAGS_VERSION_BUILD));
}

WideString get_text(HANDLE hDlg, intptr_t ctrl_id) {
  FarDialogItemData item = { sizeof(FarDialogItemData) };
  item.PtrLength = I.SendDlgMessage(hDlg, DM_GETTEXT, ctrl_id, 0);
  std::vector<wchar_t> buf(item.PtrLength + 1);
  item.PtrData = buf.data();
  I.SendDlgMessage(hDlg, DM_GETTEXT, ctrl_id, &item);
  return WideString(item.PtrData, item.PtrLength);
}

static FarDialogItem GetItem(HANDLE hDlg, intptr_t id)
{
  FarDialogItem item;
  I.SendDlgMessage(hDlg, DM_GETDLGITEMSHORT, id, &item);
  return std::move(item);
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

  I.SendDlgMessage(hDlg, DM_ENABLE , 2, reinterpret_cast<void*>(!GetItem(hDlg, 3).Selected));
  return I.DefDlgProc(hDlg, Msg, Param1, Param2);
}

static intptr_t ConfigurePlugin()
{
  config = LoadConfig(ToStdString(GetConfigFilePath()));
  unsigned char y = 0;
  WideString menuTitle = WideString(GetMsg(MPlugin)) + L" " + PluginVersionString();
  struct InitDialogItem initItems[]={
//    Type        X1  Y2  X2 Y2  F S           Flags D Data
    DI_DOUBLEBOX, 3, ++y, 64,36, 0,0,              0,0,-1,menuTitle.c_str(),{},
    DI_TEXT,      5, ++y,  0, 0, 0,0,              0,0,MPathToExe,L"",{},
    DI_EDIT,      5, ++y, 62, 3, 1,0,              0,0,-1,ToString(config.exe),{"pathtoexe", true},
    DI_CHECKBOX,  5, ++y, 62,10, 1,config.use_built_in_ctags,0,0,MUseBuiltInCtags,L"",{"usebuiltinctags", false, true},
    DI_TEXT,      5, ++y,  0, 0, 0,0,              0,0,MCmdLineOptions,L"",{},
    DI_EDIT,      5, ++y, 62, 5, 1,0,              0,0,-1,ToString(config.opt),{"commandline"},
    DI_TEXT,      5, ++y, 62,10, 1,0,DIF_SEPARATOR|DIF_BOXCOLOR,0,-1,L"",{},
    DI_TEXT,      5, ++y,  0, 0, 0,0,              0,0,MMaxResults,L"",{},
    DI_EDIT,      5, ++y, 62, 9, 1,0,              0,0,-1,ToString(std::to_string(config.max_results)),{"maxresults", true},
    DI_TEXT,      5, ++y,  0, 0, 0,0,              0,0,MThreshold,L"",{},
    DI_EDIT,      5, ++y, 62, 9, 1,0,              0,0,-1,ToString(std::to_string(config.threshold)),{"threshold", true},
    DI_TEXT,      5, ++y,  0, 0, 0,0,              0,0,MThresholdFilterLen,L"",{},
    DI_EDIT,      5, ++y, 62, 9, 1,0,              0,0,-1,ToString(std::to_string(config.threshold_filter_len)),{"thresholdfilterlen", true},
    DI_TEXT,      5, ++y, 62,10, 1,0,DIF_SEPARATOR|DIF_BOXCOLOR,0,-1,L"",{},
    DI_TEXT,      5, ++y,  0, 0, 0,0,              0,0,MResetCountersAfter,L"",{},
    DI_EDIT,      5, ++y, 62, 9, 1,0,              0,0,-1,ToString(std::to_string(config.reset_cache_counters_timeout_hours)),{"resetcachecounterstimeouthours", true},
    DI_CHECKBOX,  5, ++y, 62,10, 1,config.casesens,0,0,MCaseSensFilt,L"",{"casesensfilt", false, true},
    DI_CHECKBOX,  5, ++y, 62,10, 1,config.sort_class_members_by_name,0,0,MSortClassMembersByName,L"",{"sortclassmembersbyname", false, true},
    DI_CHECKBOX,  5, ++y, 62,10, 1,config.cur_file_first,0,0,MCurFileFirst,L"",{"curfilefirst", false, true},
    DI_CHECKBOX,  5, ++y, 62,10, 1,config.cached_tags_on_top,0,0,MCachedTagsOnTop,L"",{"cachedtagsontop", false, true},
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
    DI_TEXT,      5, ++y,  0, 0, 0,0,              0,0,MPermanentsFile,L"",{},
    DI_EDIT,      5, ++y, 62, 7, 1,0,              0,0,-1,ToString(config.permanents),{"autoload"},
    DI_CHECKBOX,  5, ++y, 62,10, 1,config.restore_last_visited_on_load,0,0,MRestoreLastVisitedOnLoad,L"",{"restorelastvisitedonload", false, true},
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
  if(ExitCode!=itemsCount-2)return FALSE;
  if (SafeCall(SaveConfig, Err, initItems, itemsCount).first)
    config = LoadConfig(ToStdString(GetConfigFilePath()));

  return TRUE;
}

intptr_t WINAPI ConfigureW(const struct ConfigureInfo *)
{
    return SafeCall(ConfigurePlugin, Err).second;
}

void WINAPI GetGlobalInfoW(struct GlobalInfo *info)
{
  info->StructSize = sizeof(*info);
  info->MinFarVersion = MAKEFARVERSION(3, 0, 0, 0, VS_RELEASE);
  info->Version = MAKEFARVERSION(CTAGS_VERSION_MAJOR, CTAGS_VERSION_MINOR, CTAGS_VERSION_REVISION, CTAGS_VERSION_BUILD, VS_RELEASE);
  info->Guid = PluginGuid;
  info->Title = APPNAME;
  info->Description = CTAGS_FILE_DESCR;
  info->Author = L"Eugene Manushkin";
}

intptr_t WINAPI ProcessEditorEventW(const struct ProcessEditorEventInfo *info)
{
  if (info->Event == EE_READ)
  {
    SafeCall(OnNewEditor, Err);
  }
  else if (info->Event == EE_CLOSE)
  {
    auto file = GetFileNameFromEditor(info->EditorID);
    if (CountEditors(file) == 1)
      SafeCall(ClearTemporaryTagsByFile, Err, file);

    SafeCall(OnCloseEditor, Err);
  }
  else if (info->Event == EE_GOTFOCUS)
  {
    static intptr_t LastFocusedID = -1;
    auto prevID = LastFocusedID;
    if ((LastFocusedID = info->EditorID) != prevID && !CurrentEditor->IsModal())
      SafeCall(SetLastVisited, Facade::ExceptionHandler(), info->EditorID); // No error handler since I.Message is forbidden in EE_GOTFOCUS
  }

  return 0;
}

void WINAPI ExitFARW(const struct ExitInfo *info)
{
}
