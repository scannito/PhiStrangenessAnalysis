#pragma once

#include "AnalysisStructures.h"
#include "JsonConfigHelpers.h"
#include "Logger.h"

#include "rapidjson/document.h"

#include <format>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

class BaseConfigManager
{
 protected:
  rapidjson::Document document;
  std::string managerName;

  // The head of every message about a section of the file; the node read is appended to it.
  std::string SectionCtx(const std::string& section) const { return std::format("{}: section '{}'", managerName, section); }

  BaseConfigManager(const std::string& jsonPath, const std::string& managerName) : managerName(managerName)
  {
    JsonConfig::LoadDocument(jsonPath, document, managerName);
    Logger::Info("{}: JSON successfully loaded into RAM.", managerName);
  }

  // The overrides of a section are keyed by cell, named by its interval (OutputNames): the task declares the cells it
  // will read before reading any, every key of the section's "bins" must be one of them (a typo, or a key written for
  // another binning, would never be applied), and RequireDeclared refuses a cell that was not declared, so the check
  // cannot be skipped. Declared cells add up over calls; the keys are checked against all of them, which this returns.
  const std::set<std::string>& DeclareTags(const std::string& section, const std::set<std::string>& tags,
                                           const rapidjson::Value* bins, std::string_view binsCtx)
  {
    std::set<std::string>& declared = declaredTags[section];
    declared.insert(tags.begin(), tags.end());
    if (bins)
      JsonConfig::RequireKnownKeys(*bins, declared, binsCtx);
    return declared;
  }

  // 'what' names a tag in the message: "cell", "class".
  void RequireDeclared(const std::string& section, const std::string& tag, std::string_view what) const
  {
    const auto declared = declaredTags.find(section);
    if (declared == declaredTags.end() || !declared->second.contains(tag))
      throw std::runtime_error(std::format("[FATAL] {}: GetConfig('{}', '{}') for a {} that was not declared with DeclareCells, "
                                           "so the overrides were not checked against it.",
                                           managerName, section, tag, what));
  }

  // The two layers of a node with overrides: its "default", then "bins"[tag] over it, each handed to
  // load(node, where), 'where' naming it ("default", "bins['mult10-15']"); either may be absent, and the second
  // carries only what it changes. Returns whether 'tag' had an override.
  template <class Load>
  bool LoadLayered(const rapidjson::Value& parent, std::string_view parentCtx, const std::string& tag, Load&& load) const
  {
    JsonConfig::RequireKnownKeys(parent, {"default", "bins"}, parentCtx);
    if (const rapidjson::Value* defaults = JsonConfig::TryObject(parent, "default", parentCtx))
      load(*defaults, std::string("default"));

    const rapidjson::Value* bins = JsonConfig::TryObject(parent, "bins", parentCtx);
    const rapidjson::Value* node = bins ? JsonConfig::TryObject(*bins, tag.c_str(), std::format("{}.bins", parentCtx)) : nullptr;
    if (!node)
      return false;
    Logger::Debug("{}: Found custom parameters for bin: {}", managerName, tag);
    load(*node, std::format("bins['{}']", tag));
    return true;
  }

  // The entries of the "params" object of 'node', merged by name into 'params': an entry
  // replaces the parameter of that name and the others stay, since a bin override carries
  // only what it changes. Absent "params" leaves them all.
  static void MergeParams(const rapidjson::Value& node, std::map<std::string, AnalysisStructures::MathParam>& params, std::string_view errCtx)
  {
    if (const rapidjson::Value* entries = JsonConfig::TryObject(node, "params", errCtx)) {
      for (auto& m : entries->GetObject()) {
        const char* name = m.name.GetString();
        params[name] = ReadParam(m.value, name, errCtx);
      }
    }
  }

 private:
  // [value, min, max] to let it float, a single value to fix it. Both forms accept numbers
  // written as strings, like every other number in these configurations. A whole parameter
  // every time: a fixed one has min and max at zero, not whatever an earlier pass left.
  static AnalysisStructures::MathParam ReadParam(const rapidjson::Value& value, std::string_view paramName, std::string_view errCtx)
  {
    if (value.IsArray()) {
      const std::vector<double> values = JsonConfig::ReadNumberArray(value.GetArray(), paramName, errCtx);
      if (values.size() != 3) {
        throw std::runtime_error(std::format("[FATAL] {}: parameter '{}' is an array of {} entries; a free parameter "
                                             "needs exactly three, [value, min, max].",
                                             errCtx, paramName, values.size()));
      }
      return {.val = values[0], .min = values[1], .max = values[2], .isConstant = false};
    }

    // Anything else must be a single number: ToNumber says so by name if it is not.
    return {.val = JsonConfig::ToNumber(value, paramName, errCtx), .isConstant = true};
  }

  // section -> the tags declared for it, every DeclareTags call together
  std::map<std::string, std::set<std::string>> declaredTags;
};
