#include <far3/config_menu.h>
#include <far3/config_value_dialog.h>
#include <far3/guid.h>
#include <far3/plugin_sdk/api.h>
#include <far3/text.h>
#include <far3/wide_string.h>

#include <plugin/config.h>
#include <plugin/config_data_mapper.h>

#include <algorithm>
#include <iterator>
#include <vector>

namespace
{
  using Far3::Guid;
  using Far3::ToString;
  using Far3::WideString;
  using ID = Plugin::ConfigFieldId;
  using FieldText = std::pair<ID, int>;

  auto const NoId = Plugin::ConfigFieldId::MaxFieldId;
  auto const MenuGuid = Guid("{f2f663a3-f18f-4f18-8b15-8b5de8abe1a2}");

  std::vector<FieldText> CreateFieldsTexts()
  {
    auto const separator = std::make_pair(NoId, -1);
    return {
      {ID::exe, MPathToExe},
      {ID::use_built_in_ctags, MUseBuiltInCtags},
      {ID::opt, MCmdLineOptions},
      separator,
      {ID::max_results, MMaxResults},
      {ID::threshold, MThreshold},
      {ID::threshold_filter_len, MThresholdFilterLen},
      {ID::platform_language_lookup, MPlatformLanguageLookup},
      separator,
      {ID::reset_cache_counters_timeout_hours, MResetCountersAfter},
      {ID::casesens, MCaseSensFilt},
      {ID::sort_class_members_by_name, MSortClassMembersByName },
      {ID::cur_file_first, MCurFileFirst},
      {ID::cached_tags_on_top, MCachedTagsOnTop},
      {ID::index_edited_file, MIndexEditedFile},
      {ID::wordchars, MWordChars},
      separator,
      {ID::tagsmask, MTagsMask},
      {ID::history_file, MHistoryFile},
      {ID::history_len, MHistoryLength},
      {ID::permanents, MPermanentsFile},
      {ID::restore_last_visited_on_load, MRestoreLastVisitedOnLoad},
    };
  }

  std::string MakeMenuValue(ID id, Plugin::ConfigDataMapper const& dataMapper, Plugin::Config const& config)
  {
    auto const field = dataMapper.Get(id, config);
    return field.type == Plugin::ConfigFieldType::Flag ? "[" + std::string(field.value == "true" ? "x" : " ") + "]"
         : field.type == Plugin::ConfigFieldType::Size ? field.value
         : "...";
  }

  std::vector<FarMenuItem> MakeFarMenuItems(std::vector<WideString> const& menuStrings)
  {
    std::vector<FarMenuItem> result;
    std::transform(menuStrings.begin(), menuStrings.end(), std::back_inserter(result), [](WideString const& str) {
      return str.empty() ? FarMenuItem{MIF_SEPARATOR} : FarMenuItem{MIF_NONE, str.c_str()};
    });
    return result;
  }

  void ResetSelected(intptr_t selected, std::vector<FarMenuItem>& items)
  {
    for (size_t i = 0; i < items.size(); ++i)
      items[i].Flags = i == selected ? (items[i].Flags | MIF_SELECTED)
                                     : (items[i].Flags & (~MIF_SELECTED));
  }

  intptr_t IndexOf(ID field, std::vector<FieldText> const& fieldsTexts)
  {
    auto found = std::find_if(fieldsTexts.cbegin(), fieldsTexts.cend(), [field](FieldText const& val){return val.first == field;});
    return found == fieldsTexts.cend() ? -1 : std::distance(fieldsTexts.cbegin(), found);
  }

  WideString PluginVersionString()
  {
    return ToString(std::to_string(CTAGS_VERSION_MAJOR) + "." +
                    std::to_string(CTAGS_VERSION_MINOR) + "." +
                    std::to_string(CTAGS_VERSION_REVISION) + "." +
                    std::to_string(CTAGS_VERSION_BUILD));
  }

  class ConfigMenuImpl : public Far3::ConfigMenu
  {
  public:
    ConfigMenuImpl(PluginStartupInfo const& i, Guid const& pluginGuid);
    std::pair<ID, std::string> Show(ID selectField, Plugin::ConfigDataMapper const& dataMapper, Plugin::Config const& config) const override;

  private:
    std::vector<WideString> MakeMenuStrings(Plugin::ConfigDataMapper const& dataMapper, Plugin::Config const& config) const;

  private:
    std::vector<FieldText> const FieldsTexts;
    PluginStartupInfo const I;
    Guid const PluginGuid;
  };

  ConfigMenuImpl::ConfigMenuImpl(PluginStartupInfo const& i, Guid const& pluginGuid)
    : FieldsTexts(CreateFieldsTexts())
    , I(i)
    , PluginGuid(pluginGuid)
  {
  }

  std::vector<WideString> ConfigMenuImpl::MakeMenuStrings(Plugin::ConfigDataMapper const& dataMapper, Plugin::Config const& config) const
  {
    std::vector<WideString> result;
    size_t maxLen = 0;
    for (auto const& fieldText : FieldsTexts)
    {
      result.push_back(fieldText.first == NoId ? WideString() : ToString(MakeMenuValue(fieldText.first, dataMapper, config)));
      maxLen = std::max(maxLen, result.back().length());
    }

    for (size_t i = 0; i < FieldsTexts.size(); ++i)
    {
      if (FieldsTexts[i].first == NoId)
        continue;

      WideString prefix = result[i].empty() ? L"" : WideString(maxLen - result[i].length(), L' ');
      result[i] = prefix + result[i] + L" | ";
      result[i] += I.GetMsg(&PluginGuid, FieldsTexts[i].second);
    }

    return result;
  }

  std::pair<ID, std::string> ConfigMenuImpl::Show(ID selectField, Plugin::ConfigDataMapper const& dataMapper, Plugin::Config const& config) const
  {
    auto const menuStrings = MakeMenuStrings(dataMapper, config);
    auto farMenuItems = MakeFarMenuItems(menuStrings);
    if (config.use_built_in_ctags)
      farMenuItems[0].Flags |= MIF_DISABLE | MIF_GRAYED;

    WideString const menuTitle = WideString(I.GetMsg(&PluginGuid, MPlugin)) + L" " + PluginVersionString();
    intptr_t const defaultX = -1, defaultY = -1, defaultHeight = 0;
    auto const bottomText = nullptr;
    auto const breakKeys = nullptr;
    auto const configureValueDialog = Far3::ConfigValueDialog::Create(I, PluginGuid);
    intptr_t breakKey = -1;
    intptr_t selected = IndexOf(selectField, FieldsTexts);

    do
    {
      ResetSelected(selected, farMenuItems);
      selected = I.Menu(
        &PluginGuid,
        &MenuGuid,
        defaultX,
        defaultY,
        defaultHeight,
        FMENU_WRAPMODE,
        menuTitle.c_str(),
        bottomText,
        L"configmenu",
        breakKeys,
        &breakKey,
        farMenuItems.data(),
        farMenuItems.size()
      );
      if (selected < 0)
        continue;

      auto const selectedField = FieldsTexts.at(selected);
      auto const selectedFieldData = dataMapper.Get(selectedField.first, config);
      auto const defaultValue = dataMapper.Get(selectedField.first, Plugin::Config()).value;
      auto value = configureValueDialog->Show(selectedFieldData.value, defaultValue, selectedFieldData.type, selectedField.second);
      if (value.first)
        return {selectedField.first, value.second};
    }
    while (selected >= 0);

    return {NoId, ""};
  }
}

namespace Far3
{
  std::unique_ptr<const ConfigMenu> ConfigMenu::Create(PluginStartupInfo const& i, Guid const& pluginGuid)
  {
    return std::unique_ptr<const ConfigMenu>(new ConfigMenuImpl(i, pluginGuid));
  }
}
