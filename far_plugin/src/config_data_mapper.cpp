#include <plugin/config_data_mapper.h>

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <unordered_map>

namespace
{
  using Plugin::Config;
  using Plugin::ConfigFieldData;
  using Plugin::ConfigFieldId;
  using Plugin::ConfigFieldType;
  using Plugin::ThreeStateFlag;

  bool SetValue(std::string const& source, std::string& dest)
  {
    dest = source;
    return true;
  }

  bool SetNotEmpty(std::string const& source, std::string& dest)
  {
    dest = !source.empty() ? source : dest;
    return !source.empty();
  }

  bool SetValue(std::string const& source, bool& dest)
  {
    if (source != "true" && source != "false") return false;
    dest = source == "true" ? true : false;
    return true;
  }

  bool SetValue(std::string const& source, ThreeStateFlag& dest)
  {
    bool result = false;
    if (!SetValue(source, result)) return false;
    dest = result ? ThreeStateFlag::Enabled : ThreeStateFlag::Disabled;
    return true;
  }

  bool SetValue(std::string const& source, size_t& dest, size_t limit = std::numeric_limits<size_t>::max()) try
  {
    auto val = std::stoul(source);
    dest = std::min(static_cast<size_t>(val), limit);
    return true;
  }
  catch(...)
  {
    return false;
  }

  bool SetNonzero(std::string const& source, size_t& dest)
  {
    size_t result = 0;
    if (!SetValue(source, result) || !result) return false;
    dest = result;
    return true;
  }

  std::string ToString(std::string const& value)
  {
    return value;
  }

  std::string ToString(bool value)
  {
    return value ? "true" : "false";
  }

  std::string ToString(ThreeStateFlag value)
  {
    return value == ThreeStateFlag::Enabled ? "true" : "false";
  }

  std::string ToString(size_t value)
  {
    return std::to_string(value);
  }

  using GetterCallback = std::function<std::string(Config const&)>;
  using SetterCallback = std::function<bool(std::string const&, Config&)>;

  struct ConfigFieldMeta
  {
    std::string key;
    ConfigFieldType type;
    GetterCallback getter;
    SetterCallback setter;
  };

  class ConfigDataMapperImpl : public Plugin::ConfigDataMapper
  {
  public:
    ConfigDataMapperImpl();

    ConfigFieldData Get(int fieldId, Config const& config) const override;
    bool Set(std::string const& key, std::string const& value, Config& config) const override;
    bool Set(int fieldId, std::string const& value, Config& config) const override;

  private:
    void InitField(ConfigFieldId fieldId, std::string&& key, ConfigFieldType type, GetterCallback&& getter, SetterCallback&& setter)
    {
      FieldsMeta[static_cast<int>(fieldId)] = {
        std::move(key),
        type,
        std::move(getter),
        std::move(setter),
      };
    }

    std::unordered_map<int, ConfigFieldMeta> FieldsMeta;
  };

  #define DEFINE_GETTER(field) [](Config const& config){return ToString(config.##field);}
  #define DEFINE_SETTER(field, func) [](std::string const& value, Config& config){return (##func(value, config.##field));}
  #define DEFINE_META_S(field, key, type, setter) \
    InitField(ConfigFieldId::##field, key, type, DEFINE_GETTER(field), ##setter)
  #define DEFINE_META_F(field, key, type, func) \
    DEFINE_META_S(field, key, type, DEFINE_SETTER(field, func))
  #define DEFINE_META(field, key, type) \
    DEFINE_META_F(field, key, type, SetValue)

  ConfigDataMapperImpl::ConfigDataMapperImpl()
  {
    using FT = ConfigFieldType;

    DEFINE_META_F(exe, "pathtoexe", FT::String, SetNotEmpty);
    DEFINE_META(opt, "commandline", FT::String);
    DEFINE_META(use_built_in_ctags, "usebuiltinctags", FT::Flag);
    DEFINE_META_F(max_results, "maxresults", FT::Size, SetNonzero);
    DEFINE_META(threshold, "threshold", FT::Size);
    DEFINE_META(threshold_filter_len, "thresholdfilterlen", FT::Size);
    DEFINE_META(platform_language_lookup, "platformlanguagelookup", FT::Flag);
    DEFINE_META(reset_cache_counters_timeout_hours, "resetcachecounterstimeouthours", FT::Size);
    DEFINE_META(casesens, "casesensfilt", FT::Flag);
    DEFINE_META(sort_class_members_by_name, "sortclassmembersbyname", FT::Flag);
    DEFINE_META(cur_file_first, "curfilefirst", FT::Flag);
    DEFINE_META(cached_tags_on_top, "cachedtagsontop", FT::Flag);
    DEFINE_META(index_edited_file, "indexeditedfile", FT::Flag);
    DEFINE_META_F(wordchars, "wordchars", FT::String, SetNotEmpty);
    DEFINE_META_F(tagsmask, "tagsmask", FT::String, SetNotEmpty);
    DEFINE_META_S(history_file, "historyfile", FT::String,
      [](std::string const& value, Config& conf) {
        conf.history_len = value.empty() ? 0 : conf.history_len;
        return SetValue(value, conf.history_file);}
    );
    DEFINE_META_S(history_len, "historylen", FT::Size,
      [](std::string const& value, Config& conf) {
        return SetValue(value, conf.history_len, Config::max_history_len);}
    );
    DEFINE_META(permanents, "autoload", FT::String);
    DEFINE_META(restore_last_visited_on_load, "restorelastvisitedonload", FT::Flag);
  }

  ConfigFieldData ConfigDataMapperImpl::Get(int fieldId, Config const& config) const
  {
    auto found = FieldsMeta.find(fieldId);
    if (found == FieldsMeta.end())
      throw std::logic_error("Field id=" + std::to_string(fieldId) + " not found");

    return {
      found->second.key,
      found->second.getter(config),
      found->second.type,
    };
  }

  bool ConfigDataMapperImpl::Set(std::string const& key, std::string const& value, Config& config) const
  {
    auto found = std::find_if(FieldsMeta.begin(), FieldsMeta.end(), [&key](auto const& meta){return meta.second.key == key;});
    if (found == FieldsMeta.end())
      return false;

    return found->second.setter(value, config);
  }

  bool ConfigDataMapperImpl::Set(int fieldId, std::string const& value, Config& config) const
  {
    auto found = FieldsMeta.find(fieldId);
    if (found == FieldsMeta.end())
      return false;

    return found->second.setter(value, config);
  }
}

namespace Plugin
{
  std::unique_ptr<ConfigDataMapper> ConfigDataMapper::Create()
  {
    return std::unique_ptr<ConfigDataMapper>(new ConfigDataMapperImpl);
  }
}
