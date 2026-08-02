#pragma once

#include "rapidjson/document.h"

#include <format>
#include <stdexcept>
#include <string>
#include <string_view>

namespace JsonConfig
{
inline const rapidjson::Value& RequireMember(const rapidjson::Value& node, const char* key, std::string_view errCtx)
{
  if (!node.HasMember(key))
    throw std::runtime_error(std::format("[FATAL] {}: '{}' missing in JSON!", errCtx, key));
  return node[key];
}

inline std::string RequireString(const rapidjson::Value& node, const char* key, std::string_view errCtx)
{
  const auto& val = RequireMember(node, key, errCtx);
  if (!val.IsString()) {
    throw std::runtime_error(std::format("[FATAL] {}: key '{}' exists but is NOT a string!", errCtx, key));
  }
  return val.GetString();
}

inline bool RequireBool(const rapidjson::Value& node, const char* key, std::string_view errCtx)
{
  const auto& val = RequireMember(node, key, errCtx);
  if (!val.IsBool()) {
    throw std::runtime_error(std::format("[FATAL] {}: key '{}' exists but is NOT a boolean!", errCtx, key));
  }
  return val.GetBool();
}

inline auto RequireArray(const rapidjson::Value& node, const char* key, std::string_view errCtx)
{
  const auto& arr = RequireMember(node, key, errCtx);
  if (!arr.IsArray())
    throw std::runtime_error(std::format("[FATAL] {}: '{}' exists but is not an ARRAY!", errCtx, key));
  return arr.GetArray();
}

} // namespace JsonConfig
