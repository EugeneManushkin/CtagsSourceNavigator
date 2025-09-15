#include <far3/config_stream_reader.h>
#include <far3/error.h>
#include <far3/text.h>

#include <plugin/config_data_mapper.h>

#include <iterator>

namespace
{
  bool IsLineEnd(char c)
  {
    return c == '\r' || c == '\n';
  }

  void SkipLine(std::istreambuf_iterator<char>& begin, std::istreambuf_iterator<char> const& end)
  {
    bool cr = begin != end && *begin == '\r';
    if (begin != end && IsLineEnd(*begin)) ++begin;
    if (cr && begin != end && *begin == '\n') ++begin;
  }

  bool ParseKey(std::istreambuf_iterator<char>& begin, std::istreambuf_iterator<char> const& end, size_t maxLen, std::string& result)
  {
    std::string key;
    for(; begin != end && *begin != '=' && key.length() < maxLen; key += *begin, ++begin);
    if (begin == end || *begin != '=') return false;
    ++begin;
    result = key;
    return true;
  }

  bool ParseValue(std::istreambuf_iterator<char>& begin, std::istreambuf_iterator<char> const& end, size_t maxLen, std::string& result)
  {
    std::string value;
    for (; begin != end && !IsLineEnd(*begin) && value.length() < maxLen; value += *begin, ++begin);
    if (begin != end && !IsLineEnd(*begin)) return false;
    SkipLine(begin, end);
    result = value;
    return true;
  }

  class ConfigStreamReaderImpl : public Far3::ConfigStreamReader
  {
  public:
    ConfigStreamReaderImpl(size_t maxKeyLen, size_t maxValueLen, unsigned maxErrors, unsigned maxLines);
    Plugin::Config Read(std::istream& stream, Plugin::ConfigDataMapper const& dataMapper) override;

  private:
    size_t const MaxKeyLen;
    size_t const MaxValueLen;
    unsigned const MaxErrors;
    unsigned const MaxLines;
  };

  ConfigStreamReaderImpl::ConfigStreamReaderImpl(size_t maxKeyLen, size_t maxValueLen, unsigned maxErrors, unsigned maxLines)
    : MaxKeyLen(maxKeyLen)
    , MaxValueLen(maxValueLen)
    , MaxErrors(maxErrors)
    , MaxLines(maxLines)
  {
  }

  Plugin::Config ConfigStreamReaderImpl::Read(std::istream& stream, Plugin::ConfigDataMapper const& dataMapper)
  {
    Plugin::Config result;
    std::istreambuf_iterator<char> begin(stream);
    std::istreambuf_iterator<char> const end;
    unsigned lines = 0;
    unsigned errors = 0;
    for (; begin != end; ++lines)
    {
      for (; lines < MaxLines && begin != end && IsLineEnd(*begin); SkipLine(begin, end), ++lines);
      if (begin == end)
        break;

      if (lines == MaxLines)
        throw Far3::Error(MTooManyLinesInConfigFile, "line", std::to_string(lines + 1));

      std::string key;
      if (!ParseKey(begin, end, MaxKeyLen, key))
        throw Far3::Error(MInvalidConfigFileFormat, "line", std::to_string(lines + 1));

      std::string value;
      if (!ParseValue(begin, end, MaxValueLen, value))
        throw Far3::Error(MInvalidConfigFileFormat, "line", std::to_string(lines + 1));

      errors += dataMapper.Set(key, value, result) ? 0 : 1;
      if (errors > MaxErrors)
        throw Far3::Error(MTooManyErrorsInConfigFile, "line", std::to_string(lines + 1));
    }

    return result;
  }
}

namespace Far3
{
  std::unique_ptr<ConfigStreamReader> ConfigStreamReader::Create(size_t maxKeyLen, size_t maxValueLen, unsigned maxErrors, unsigned maxLines)
  {
    return std::unique_ptr<ConfigStreamReader>(new ConfigStreamReaderImpl(maxKeyLen, maxValueLen, maxErrors, maxLines));
  }
}
