#pragma once

#include "BaseConfigManager.h"
#include "BinningUtils.h"
#include "Logger.h"
#include "OutputNames.h"

#include <format>
#include <map>
#include <set>
#include <span>

class ExtrapConfigManager : public BaseConfigManager
{
 public:
  // Calls the Base constructor to handle file I/O
  ExtrapConfigManager(const std::string& jsonPath = "extrapConfig.json") : BaseConfigManager(jsonPath, "ExtrapConfigManager") {}

  bool HasConfig(const std::string& particle) const
  {
    return isLoaded && document.HasMember(particle.c_str());
  }

  // The multiplicity classes a task extrapolates for a particle that HasConfig, each by OutputNames::MultTag.
  // Declared once, before the extrapolations: an override of 'bins' that names none of them stops the run, since
  // it would never be applied; GetConfig refuses a class that was not declared, as in FitConfigManager.
  void DeclareCells(const std::string& particle, std::span<const double> multBinning)
  {
    std::set<std::string>& classes = declaredCells[particle];
    for (int i = 0; i < BinningUtils::NBins(multBinning); i++)
      classes.insert(OutputNames::MultTag(multBinning, i));
    if (const rapidjson::Value* bins = JsonConfig::TryObject(Section(particle), "bins", managerName))
      JsonConfig::RequireKnownKeys(*bins, classes, std::format("{}: section '{}'.bins (the keys are the multiplicity classes)", managerName, particle));
  }

  // The configuration of one class, 'multTag' as OutputNames::MultTag gives it.
  ExtrapConfig GetConfig(const std::string& particle, const std::string& multTag) const
  {
    const auto declared = declaredCells.find(particle);
    if (declared == declaredCells.end() || !declared->second.contains(multTag))
      throw std::runtime_error(std::format("[FATAL] {}: GetConfig('{}', '{}') for a class that was not declared with DeclareCells, "
                                           "so the overrides were not checked against it.",
                                           managerName, particle, multTag));

    ExtrapConfig config;
    const rapidjson::Value& partNode = Section(particle);
    // Closed sets of keys, as in FitConfigManager: a misspelled key is refused, not skipped.
    // Every error inside the section names it, the high fit included.
    const std::string sectionCtx = std::format("{}: section '{}'", managerName, particle);
    JsonConfig::RequireKnownKeys(partNode, {"default", "bins"}, sectionCtx);

    // The keys of one fit, read into 'fit' over what it already holds. Every key here is
    // optional because the main fit is read twice: once from "default" and once from the
    // bin override, which only carries what it changes. Absent therefore means "keep what
    // the previous pass set", never "reset".
    auto loadFit = [](const rapidjson::Value& node, ExtrapFit& fit, std::string_view ctx) {
      fit.model = JsonConfig::OptionalString(node, "model", fit.model, ctx);

      if (auto range = JsonConfig::TryRange(node, "fit_range", ctx))
        fit.fitRange = *range;

      MergeParams(node, fit.params, ctx);
    };

    auto loadNodeIntoConfig = [&](const rapidjson::Value& node) {
      JsonConfig::RequireKnownKeys(node, {"model", "domain_range", "fit_range", "params", "high"}, sectionCtx);
      loadFit(node, config, sectionCtx);

      if (auto range = JsonConfig::TryRange(node, "domain_range", sectionCtx))
        config.domainRange = *range;

      // The separate fit of the high side (ExtrapConfig::high). Replaced whole when a bin
      // override carries it, and switched off there with null: an override that only left
      // it out would keep the default's. Replaced whole, it has nothing to keep from a
      // previous pass, so its three keys are required.
      if (const rapidjson::Value* value = JsonConfig::OptionalMember(node, "high")) {
        const std::string ctx = sectionCtx + ", high";
        config.high.reset();
        if (const rapidjson::Value* high = JsonConfig::ToObjectOrNull(*value, "high", ctx)) {
          for (const char* key : {"model", "fit_range", "params"})
            JsonConfig::RequireMember(*high, key, ctx);
          loadFit(*high, config.high.emplace(), ctx);
        }
      }
    };

    // 1. First, apply all "default" mathematical parameters
    if (const rapidjson::Value* defaults = JsonConfig::TryObject(partNode, "default", managerName)) {
      loadNodeIntoConfig(*defaults);
    }

    // 2. Override with specific bin settings if they exist
    // The outer key is fixed and must be an object; the inner one is the class, so
    // its absence is the normal case and HasMember is the right tool.
    const rapidjson::Value* bins = JsonConfig::TryObject(partNode, "bins", managerName);
    if (bins && bins->HasMember(multTag.c_str())) {
      Logger::Debug("{}: Found custom parameters for bin: {}", managerName, multTag);
      loadNodeIntoConfig((*bins)[multTag.c_str()]);
    }

    return config;
  }

 private:
  // A missing section is a legitimate state, and HasConfig is how a caller asks: it
  // skips the particle. Arriving here means a caller skipped that test - and an empty
  // ExtrapConfig would only fail later, with a message about a model that was never the
  // problem. Checked before the lookup: rapidjson's operator[] on a missing key asserts.
  const rapidjson::Value& Section(const std::string& particle) const
  {
    if (!HasConfig(particle))
      throw std::runtime_error(std::format("[FATAL] {}: section '{}' asked for, but the JSON has none: check HasConfig first.",
                                           managerName, particle));
    return document[particle.c_str()];
  }

  // section -> the MultTags its task extrapolates ("mult10-15"); filled by DeclareCells
  std::map<std::string, std::set<std::string>> declaredCells;
};
