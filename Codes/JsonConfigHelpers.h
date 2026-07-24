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
  return RequireMember(node, key, errCtx).GetString();
}

inline bool RequireBool(const rapidjson::Value& node, const char* key, const std::string& errCtx)
{
  return RequireMember(node, key, errCtx).GetBool();
}

inline decltype(auto) RequireArray(const rapidjson::Value& node, const char* key, const std::string& errCtx)
{
  decltype(auto) arr = RequireMember(node, key, errCtx);
  if (!arr.IsArray())
    throw std::runtime_error("[FATAL] " + errCtx + ": '" + key + "' is not an array in JSON!");
  return arr.GetArray();
}
