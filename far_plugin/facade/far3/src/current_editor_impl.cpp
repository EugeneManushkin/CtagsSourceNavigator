#include "current_editor_impl.h"
#include "error.h"
#include "text.h"
#include "wide_string.h"

#include <plugin_sdk/plugin.hpp>

#include <plugin/current_editor.h>

#include <vector>

namespace
{
  using WideString = Far3::WideString;
  using Far3::ToString;
  using Far3::ToStdString;
  using Far3::Error;

  EditorSetPosition MakeEditorSetPosition(Plugin::EditorPosition const& position)
  {
    EditorSetPosition result = {sizeof(EditorSetPosition)};
    result.CurLine = position.Line;
    result.CurPos = position.Pos;
    result.CurTabPos = -1;
    result.TopScreenLine = position.Top;
    result.LeftPos = position.Left;
    result.Overtype = -1;
    return result;
  }

  Plugin::EditorPosition MakeEditorPosition(std::string const& file, EditorInfo const& ei)
  {
    return {
        file
      , static_cast<int>(ei.CurLine)
      , static_cast<int>(ei.CurPos)
      , static_cast<int>(ei.TopScreenLine)
      , static_cast<int>(ei.LeftPos)
    };
  }

  std::pair<bool, EditorInfo> GetInfo(PluginStartupInfo const& I)
  {
    EditorInfo result = {sizeof(EditorInfo)};
    bool success = !!I.EditorControl(-1, ECTL_GETINFO, 0, &result);
    return std::make_pair(success, result);
  }

  WideString GetEditorFileName(intptr_t editorID, PluginStartupInfo const& I)
  {
    size_t sz = I.EditorControl(editorID, ECTL_GETFILENAME, 0, nullptr);
    std::vector<wchar_t> buffer(sz);
    I.EditorControl(editorID, ECTL_GETFILENAME, buffer.size(), !buffer.empty() ? &buffer[0] : nullptr);
    return !buffer.empty() ? WideString(buffer.begin(), buffer.end() - 1) : WideString();
  }

  class CurrentEditorImpl : public Plugin::CurrentEditor
  {
  public:
    CurrentEditorImpl(PluginStartupInfo const& i, GUID const& pluginGuid)
      : I(i)
      , PluginGuid(pluginGuid)
    {
    }

    bool IsModal() const override
    {
      for (auto i = I.AdvControl(&PluginGuid, ACTL_GETWINDOWCOUNT, 0, nullptr); i > 0; --i)
      {
        WindowInfo wi = {sizeof(WindowInfo)};
        wi.Pos = i - 1;
        I.AdvControl(&PluginGuid, ACTL_GETWINDOWINFO, 0, &wi);
        if (!!(wi.Flags & WIF_CURRENT))
          return !!(wi.Flags & WIF_MODAL);
      }

      return false;
    }

    bool IsOpened(char const* file) const override
    {
      auto info = GetInfo(I);
      auto id = info.second.EditorID;
      return info.first ? I.FSF->ProcessName(ToString(file).c_str(), const_cast<wchar_t*>(GetEditorFileName(id, I).c_str()), 0, PN_CMPNAME) != 0
                        : false;
    }

    Plugin::EditorPosition GetPosition() const override
    {
      auto info = GetInfo(I);
      auto id = info.second.EditorID;
      return info.first ? MakeEditorPosition(ToStdString(GetEditorFileName(id, I)), info.second)
                        : Plugin::EditorPosition();
    }

    void OpenAsync(Plugin::EditorPosition const& position) override
    {
      auto filename = ToString(position.File);
      if (I.Editor(filename.c_str(), L"", 0, 0, -1, -1,  EF_NONMODAL | EF_IMMEDIATERETURN | EF_OPENMODE_USEEXISTING, -1, -1, CP_DEFAULT) == EEC_OPEN_ERROR)
        throw Error(MEFailedToOpen, "File", position.File);
    
      if (position.Line >= 0)
      {
        auto esp = MakeEditorSetPosition(position);
        auto id = GetInfo(I).second.EditorID;
        I.EditorControl(id, ECTL_SETPOSITION, 0, &esp);
        I.EditorControl(id, ECTL_REDRAW, 0, nullptr);
      }
    }

    void OpenModal(Plugin::EditorPosition const& position) override
    {
      auto filename = ToString(position.File);
      auto line = position.Line + (position.Line == Plugin::EditorPosition::DefaultLine ? 0 : 1);
      auto pos = position.Pos + (position.Pos == Plugin::EditorPosition::DefaultPos ? 0 : 1);
      if (I.Editor(filename.c_str(), L"", 0, 0, -1, -1,  EF_OPENMODE_NEWIFOPEN, line, pos, CP_DEFAULT) == EEC_OPEN_ERROR)
        throw Error(MEFailedToOpen, "File", position.File);
    }

  private:
    PluginStartupInfo I;
    GUID PluginGuid;
  };
}

namespace Far3
{
  std::unique_ptr<Plugin::CurrentEditor> CreateCurrentEditor(PluginStartupInfo const& i, GUID const& pluginGuid)
  {
    return std::unique_ptr<Plugin::CurrentEditor>(new CurrentEditorImpl(i, pluginGuid));
  }
}
