#pragma once

#include <plugin/config.h>

#include <istream>
#include <memory>

namespace Plugin
{
  class ConfigDataMapper;
}

namespace Far3
{
  class ConfigStreamReader
  {
  public:
    static std::unique_ptr<const ConfigStreamReader> Create(
      size_t maxKeyLen = 128
    , size_t maxValueLen = 65536
    , unsigned maxErrors = static_cast<int>(Plugin::ConfigFieldId::MaxFieldId)
    , unsigned maxLines = static_cast<int>(Plugin::ConfigFieldId::MaxFieldId) * 8
    );
    virtual Plugin::Config Read(std::istream& stream, Plugin::ConfigDataMapper const& dataMapper) const = 0;
  };
}
