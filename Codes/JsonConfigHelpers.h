#pragma once

#include "rapidjson/document.h"

#include <format>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace JsonConfig
{
// ---------------------------------------------------------------------------
// Member lookup
// ---------------------------------------------------------------------------
// The two entry points below differ in what "absent" means, and everything
// else in this header is built on top of them.
//
//   RequireMember  - absent is a fatal error, so it can return a reference.
//   OptionalMember - absent is legitimate, so it must return a pointer, with
//                    nullptr standing for "not there, use your default".
//
// Neither takes a decision about the TYPE of the value: that belongs to the
// typed wrappers, which is where the error message can say what was expected.

inline const rapidjson::Value& RequireMember(const rapidjson::Value& node, const char* key, std::string_view errCtx)
{
  auto it = node.FindMember(key);
  if (it == node.MemberEnd())
    throw std::runtime_error(std::format("[FATAL] {}: '{}' missing in JSON!", errCtx, key));
  return it->value;
}

// No context argument: an absent key is not an error, so there is nothing to report.
inline const rapidjson::Value* OptionalMember(const rapidjson::Value& node, const char* key)
{
  auto it = node.FindMember(key);
  return it != node.MemberEnd() ? &it->value : nullptr;
}

// ---------------------------------------------------------------------------
// Required keys
// ---------------------------------------------------------------------------

inline std::string RequireString(const rapidjson::Value& node, const char* key, std::string_view errCtx)
{
  const auto& val = RequireMember(node, key, errCtx);
  if (!val.IsString())
    throw std::runtime_error(std::format("[FATAL] {}: key '{}' exists but is NOT a string!", errCtx, key));
  return val.GetString();
}

inline bool RequireBool(const rapidjson::Value& node, const char* key, std::string_view errCtx)
{
  const auto& val = RequireMember(node, key, errCtx);
  if (!val.IsBool())
    throw std::runtime_error(std::format("[FATAL] {}: key '{}' exists but is NOT a boolean!", errCtx, key));
  return val.GetBool();
}

inline auto RequireArray(const rapidjson::Value& node, const char* key, std::string_view errCtx)
{
  const auto& arr = RequireMember(node, key, errCtx);
  if (!arr.IsArray())
    throw std::runtime_error(std::format("[FATAL] {}: '{}' exists but is not an ARRAY!", errCtx, key));
  return arr.GetArray();
}

// ---------------------------------------------------------------------------
// Optional keys
// ---------------------------------------------------------------------------
// An absent key means "use the default" and is not an error. A key that IS
// present but carries the wrong type is: the old "HasMember(k) && k.IsBool()"
// idiom skipped it silently, so a JSON asking for one thing quietly got
// another. These throw instead.

inline std::string OptionalString(const rapidjson::Value& node, const char* key, std::string fallback, std::string_view errCtx)
{
  const rapidjson::Value* val = OptionalMember(node, key);
  if (!val)
    return fallback;
  if (!val->IsString())
    throw std::runtime_error(std::format("[FATAL] {}: key '{}' is present but is NOT a string, so it would be silently ignored.", errCtx, key));
  return val->GetString();
}

inline bool OptionalBool(const rapidjson::Value& node, const char* key, bool fallback, std::string_view errCtx)
{
  const rapidjson::Value* val = OptionalMember(node, key);
  if (!val)
    return fallback;
  if (!val->IsBool())
    throw std::runtime_error(std::format("[FATAL] {}: key '{}' is present but is NOT a boolean, so it would be silently ignored.", errCtx, key));
  return val->GetBool();
}

inline int OptionalInt(const rapidjson::Value& node, const char* key, int fallback, std::string_view errCtx)
{
  const rapidjson::Value* val = OptionalMember(node, key);
  if (!val)
    return fallback;
  if (!val->IsInt())
    throw std::runtime_error(std::format("[FATAL] {}: key '{}' is present but is NOT an integer, so it would be silently ignored.", errCtx, key));
  return val->GetInt();
}

inline double OptionalDouble(const rapidjson::Value& node, const char* key, double fallback, std::string_view errCtx)
{
  const rapidjson::Value* val = OptionalMember(node, key);
  if (!val)
    return fallback;
  // IsNumber, not IsDouble: rapidjson stores "2" as an int, and refusing it
  // would make the JSON depend on whether a decimal point was typed.
  if (!val->IsNumber())
    throw std::runtime_error(std::format("[FATAL] {}: key '{}' is present but is NOT a number, so it would be silently ignored.", errCtx, key));
  return val->GetDouble();
}

// Empty when the key is absent; throws when it is there but is not an array.
// The alias is needed because a deduced return type has to agree across every
// return statement, and a bare std::nullopt would not match the array branch.
inline auto OptionalArray(const rapidjson::Value& node, const char* key, std::string_view errCtx)
{
  using Result = std::optional<rapidjson::Value::ConstArray>;

  const rapidjson::Value* val = OptionalMember(node, key);
  if (!val)
    return Result{};
  if (!val->IsArray())
    throw std::runtime_error(std::format("[FATAL] {}: key '{}' is present but is NOT an array, so it would be silently ignored.", errCtx, key));
  return Result{val->GetArray()};
}

// ---------------------------------------------------------------------------
// Optional enumerations
// ---------------------------------------------------------------------------
// Reads a key whose legal values are a fixed, small set of names and maps it
// onto an enum. The list of accepted spellings is the SAME object that builds
// the error message, so a new entry cannot leave a stale "Available: ..." text
// behind. Aliases are just two entries pointing at the same enumerator.
//
// The default is looked up in the table as well: a typo in the default given
// at the call site fails here rather than silently resolving to it.

template <typename E>
inline E OptionalEnum(const rapidjson::Value& node, const char* key, std::string_view defaultName,
                      std::initializer_list<std::pair<std::string_view, E>> options, std::string_view errCtx)
{
  const std::string requested = OptionalString(node, key, std::string(defaultName), errCtx);

  std::string available;
  for (const auto& [name, value] : options) {
    if (name == requested)
      return value;
    if (!available.empty())
      available += ", ";
    available += name;
  }

  throw std::runtime_error(std::format("[FATAL] {}: key '{}' has unknown value '{}'. Available: {}",
                                       errCtx, key, requested, available));
}

// ---------------------------------------------------------------------------
// Present-or-not, with the type still checked
// ---------------------------------------------------------------------------
// The third case, between Require and Optional. Require makes an absent key an
// error, Optional replaces it with a default; these report whether it was there,
// for callers whose default is not a constant but follows from something else.
// Unlike OptionalMember they still refuse a value of the wrong type: absent and
// misspelled must not look the same.

inline std::optional<std::string> TryString(const rapidjson::Value& node, const char* key, std::string_view errCtx)
{
  const rapidjson::Value* val = OptionalMember(node, key);
  if (!val)
    return std::nullopt;
  if (!val->IsString())
    throw std::runtime_error(std::format("[FATAL] {}: key '{}' is present but is NOT a string, so it would be silently ignored.", errCtx, key));
  return val->GetString();
}

} // namespace JsonConfig
