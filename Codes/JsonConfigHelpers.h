#pragma once

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace JsonConfig
{
// ---------------------------------------------------------------------------
// Serialisation
// ---------------------------------------------------------------------------

// The node back as JSON text, on one line. Used to record inside an output file
// the configuration that produced it: the merged block, so what the task actually
// received rather than what the file on disk says today. Compact rather than
// pretty because it is stored, not read directly - pipe it through a formatter
// when you need to look at it.
inline std::string Serialize(const rapidjson::Value& node)
{
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  node.Accept(writer);
  return buffer.GetString();
}

// The file as it is on disk, bytes and formatting included. Used to snapshot a
// referenced configuration - the fit or the extrapolation one - inside an output
// file: the path alone would say which file it was, not what was in it, and those
// are exactly the files that get edited while tuning.
//
// Never throws: this feeds documentation, and a missing file is not a reason to
// stop a run that is otherwise fine.
inline std::string ReadFileText(const std::string& path)
{
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    std::cerr << "[WARNING] JsonConfig::ReadFileText: cannot read '" << path
              << "', it will not be recorded." << std::endl;
    return "";
  }
  return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

// Numbers appear in the configurations both as numbers and as strings - 6.4 and
// "6.4" - so a value copied from one key to another must not change meaning. This
// accepts either, and names what it could not read: rapidjson's GetDouble() on a
// string does not throw, it asserts, and the process dies without a message.
//
// std::exception and not std::invalid_argument: stod also throws out_of_range, and
// "1e400" would otherwise escape as an unhandled exception rather than a diagnostic.
inline double ToNumber(const rapidjson::Value& value, std::string_view what, std::string_view errCtx)
{
  if (value.IsNumber())
    return value.GetDouble();

  if (value.IsString()) {
    try {
      return std::stod(value.GetString());
    } catch (const std::exception&) {
      throw std::runtime_error(std::format("[FATAL] {}: '{}' is the string \"{}\", which is not a number.",
                                           errCtx, what, value.GetString()));
    }
  }

  throw std::runtime_error(std::format("[FATAL] {}: '{}' is neither a number nor a string.", errCtx, what));
}

// The array that a value already in hand must be. Separate from RequireArray for
// the case of a loop over an object's members: there each value is already held,
// and looking it up again by name would search for something that was not lost.
inline rapidjson::Value::ConstArray ToArray(const rapidjson::Value& value, std::string_view what, std::string_view errCtx)
{
  if (!value.IsArray())
    throw std::runtime_error(std::format("[FATAL] {}: '{}' is not an array.", errCtx, what));
  return value.GetArray();
}

// Takes the array and not the node: whether the key exists and whether it is an
// array is what RequireArray and TryArray answer, and answering it twice would
// mean two error messages for one mistake.
inline std::vector<double> ReadNumberArray(rapidjson::Value::ConstArray array, std::string_view what, std::string_view errCtx)
{
  std::vector<double> values;
  values.reserve(array.Size());
  for (const auto& v : array)
    values.push_back(ToNumber(v, what, errCtx));
  return values;
}

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
  return ToArray(RequireMember(node, key, errCtx), key, errCtx);
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

// ---------------------------------------------------------------------------
// Optional enumerations
// ---------------------------------------------------------------------------
// Maps one of a fixed, small set of names onto an enum. The list of accepted
// spellings is the SAME object that builds the error message, so a new entry
// cannot leave a stale "Available: ..." text behind. Aliases are just two entries
// pointing at the same enumerator.
//
// Split from the reading below so that a caller who needs the name as well - to
// record it, to log it - gets it from the read and does not have to ask twice.

template <typename E>
inline E ResolveEnum(std::string_view requested, std::initializer_list<std::pair<std::string_view, E>> options,
                     std::string_view key, std::string_view errCtx)
{
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

// The usual case: read the name, then resolve it. The default is looked up in the
// table like any other value, so a typo in the default given at the call site
// fails here rather than silently resolving to it.
template <typename E>
inline E OptionalEnum(const rapidjson::Value& node, const char* key, std::string_view defaultName,
                      std::initializer_list<std::pair<std::string_view, E>> options, std::string_view errCtx)
{
  return ResolveEnum<E>(OptionalString(node, key, std::string(defaultName), errCtx), options, key, errCtx);
}

// ---------------------------------------------------------------------------
// Present-or-not, with the type still checked
// ---------------------------------------------------------------------------
// The third case, between Require and Optional:
//
//   Require*  absent is fatal, returns the value
//   Optional*  absent means the fallback you pass, returns the value
//   Try*       absent means an empty optional, and you decide
//
// Try is what a caller needs when its default is not a constant but follows from
// something else - AssocParticleConfig deriving dirName from the species name.
//
// Arrays and objects have no Optional form and cannot have one: ConstArray and
// ConstObject are views into the document, not values, so there is no fallback to
// hand over. They are Try by construction.
//
// Unlike OptionalMember these still refuse a value of the wrong type: absent and
// misspelled must not look the same.

// Empty when the key is absent; throws when it is there but is not an array.
// The alias is needed because a deduced return type has to agree across every
// return statement, and a bare std::nullopt would not match the array branch.
inline auto TryArray(const rapidjson::Value& node, const char* key, std::string_view errCtx)
{
  using Result = std::optional<rapidjson::Value::ConstArray>;

  const rapidjson::Value* val = OptionalMember(node, key);
  if (!val)
    return Result{};
  return Result{ToArray(*val, key, errCtx)};
}

// The same for an object, and for the same reason: a "params" or "observable"
// block that is present but written as an array or a string would otherwise be
// skipped by the HasMember + IsObject idiom without a word.
//
// Returns the node and not a ConstObject, unlike TryArray. Not because rapidjson
// gets in the way - GenericObject has operator[], HasMember and FindMember, and is
// as capable as GenericValue - but because the accessors in this header take a
// Value. Handing back the view would leave the caller unable to pass it to
// OptionalString and forced to look the key up again.
//
// Arrays escape this only because their one consumer here, ReadNumberArray, is
// ours and takes the view. The rule is pragmatic, not principled: each Try returns
// whatever its callers in this codebase can actually use.
inline const rapidjson::Value* TryObject(const rapidjson::Value& node, const char* key, std::string_view errCtx)
{
  const rapidjson::Value* val = OptionalMember(node, key);
  if (!val)
    return nullptr;
  if (!val->IsObject())
    throw std::runtime_error(std::format("[FATAL] {}: key '{}' is present but is NOT an object, so it would be silently ignored.", errCtx, key));
  return val;
}

// A range written as [lo, hi]. Present but not a pair of numbers is an error and
// not something to skip: "fit_range": 6.4 means someone meant something, and the
// old "HasMember && IsArray && Size()==2" idiom would have ignored it in silence.
inline std::optional<std::pair<double, double>> TryRange(const rapidjson::Value& node, const char* key, std::string_view errCtx)
{
  auto array = TryArray(node, key, errCtx);
  if (!array)
    return std::nullopt;

  const std::vector<double> values = ReadNumberArray(*array, key, errCtx);
  if (values.size() != 2)
    throw std::runtime_error(std::format("[FATAL] {}: '{}' must have exactly two entries, [low, high], but has {}.",
                                         errCtx, key, values.size()));

  return std::pair{values[0], values[1]};
}

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
