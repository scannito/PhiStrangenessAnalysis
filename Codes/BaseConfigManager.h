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

  // The overrides of a section are keyed by tag: a cell named by its interval (OutputNames). They sit under "bins"
  // in the node that also holds the section's "default" ('layered': fit_setup for the fits, the section itself for
  // the extrapolation). Every manager declares the tags its task will read, in its DeclareCells, before reading any:
  // - every key of "bins" must be a declared tag: a typo, or a key written for another binning, would never be
  //   applied, so it stops the run;
  // - LoadLayered, the only reader of "bins", refuses a tag that was not declared, so the check cannot be skipped;
  // - DeclareCells reads every declared tag once, at Init: a mistake in a node then stops the run before the task
  //   has cleared its output, and not in Run, after the previous production was already gone.
  // Declared tags add up over calls; the keys are checked against all of them, which this returns. 'keysAre' says in
  // the message what the keys are.
  const std::set<std::string>& DeclareTags(const std::string& section, const std::set<std::string>& tags,
                                           const rapidjson::Value* layered, std::string_view layeredCtx, std::string_view keysAre)
  {
    std::set<std::string>& declared = declaredTags[section];
    declared.insert(tags.begin(), tags.end());
    if (const rapidjson::Value* bins = layered ? JsonConfig::TryObject(*layered, "bins", layeredCtx) : nullptr)
      JsonConfig::RequireKnownKeys(*bins, declared, std::format("{}.bins (the keys are {})", layeredCtx, keysAre));
    return declared;
  }

  // The two layers of 'tag' in 'section': the "default" of 'layered', then its "bins"[tag] over it, each handed to
  // load(node, where), 'where' naming it ("default", "bins['mult10-15']"); either may be absent, and the second
  // carries only what it changes. The keys of 'layered' must be exactly those two, and 'tag' must have been declared.
  // 'layered' may be null: a section without overrides, nothing to read. Returns whether 'tag' had an override.
  template <class Load>
  bool LoadLayered(const std::string& section, const rapidjson::Value* layered, std::string_view layeredCtx,
                   const std::string& tag, Load&& load) const
  {
    const auto declared = declaredTags.find(section);
    if (declared == declaredTags.end() || !declared->second.contains(tag))
      throw std::runtime_error(std::format("[FATAL] {}: '{}' of section '{}' read without being declared with DeclareCells, "
                                           "so the overrides were not checked against it.",
                                           managerName, tag, section));
    if (!layered)
      return false;

    JsonConfig::RequireKnownKeys(*layered, {"default", "bins"}, layeredCtx);
    if (const rapidjson::Value* defaults = JsonConfig::TryObject(*layered, "default", layeredCtx))
      load(*defaults, std::string("default"));

    const rapidjson::Value* bins = JsonConfig::TryObject(*layered, "bins", layeredCtx);
    const rapidjson::Value* node = bins ? JsonConfig::TryObject(*bins, tag.c_str(), std::format("{}.bins", layeredCtx)) : nullptr;
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
