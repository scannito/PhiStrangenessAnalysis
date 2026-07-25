#pragma once

#include "rapidjson/document.h"

#include <stdexcept>
#include <string>

inline const rapidjson::Value& RequireMember(const rapidjson::Value& node, const char* key, const std::string& errCtx)
{
  if (!node.HasMember(key))
    throw std::runtime_error("[FATAL] " + errCtx + ": '" + key + "' missing in JSON!");
  return node[key];
}

inline std::string RequireString(const rapidjson::Value& node, const char* key, const std::string& errCtx)
{
  const auto& val = RequireMember(node, key, errCtx);
  if (!val.IsString()) {
    throw std::runtime_error("[FATAL] " + errCtx + ": Key '" + key + "' exists but is NOT a string!");
  }
  return val.GetString();
}

inline bool RequireBool(const rapidjson::Value& node, const char* key, const std::string& errCtx)
{
  const auto& val = RequireMember(node, key, errCtx);
  if (!val.IsBool()) {
    throw std::runtime_error("[FATAL] " + errCtx + ": Key '" + key + "' exists but is NOT a boolean!");
  }
  return val.GetBool();
}

inline auto RequireArray(const rapidjson::Value& node, const char* key, const std::string& errCtx)
{
  const auto& arr = RequireMember(node, key, errCtx);
  if (!arr.IsArray())
    throw std::runtime_error("[FATAL] " + errCtx + ": '" + key + "' exists but is not an ARRAY!");
  return arr.GetArray();
}
