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
    return JsonConfig::TryObject(document, particle.c_str(), managerName) != nullptr;
  }

  // The multiplicity classes a task extrapolates for a particle that HasConfig, each by OutputNames::MultTag.
  // Declared once, before the extrapolations: an override of 'bins' that names none of them stops the run, since
  // it would never be applied; GetConfig refuses a class that was not declared, as in FitConfigManager.
  void DeclareCells(const std::string& particle, std::span<const double> multBinning)
  {
    std::set<std::string>& classes = declaredCells[particle];
    for (int multBin : BinningUtils::BinIndices(multBinning))
      classes.insert(OutputNames::MultTag(multBinning, multBin));
    if (const rapidjson::Value* bins = JsonConfig::TryObject(Section(particle), "bins", SectionCtx(particle)))
      JsonConfig::RequireKnownKeys(*bins, classes, std::format("{}.bins (the keys are the multiplicity classes)", SectionCtx(particle)));
    // Every class read once now, at Init, for the reason given at FitConfigManager::DeclareCells.
    for (const std::string& multTag : classes)
      (void)GetConfig(particle, multTag);
  }

  // The configuration of one class, 'multTag' as OutputNames::MultTag gives it.
  AnalysisStructures::ExtrapConfig GetConfig(const std::string& particle, const std::string& multTag) const
  {
    const auto declared = declaredCells.find(particle);
    if (declared == declaredCells.end() || !declared->second.contains(multTag))
      throw std::runtime_error(std::format("[FATAL] {}: GetConfig('{}', '{}') for a class that was not declared with DeclareCells, "
                                           "so the overrides were not checked against it.",
                                           managerName, particle, multTag));

    AnalysisStructures::ExtrapConfig config;
    const rapidjson::Value& partNode = Section(particle);
    // Closed sets of keys, as in FitConfigManager: a misspelled key is refused, not skipped.
    // Every error inside the section names it, the high fit included.
    const std::string sectionCtx = SectionCtx(particle);
    JsonConfig::RequireKnownKeys(partNode, {"default", "bins"}, sectionCtx);

    // The keys of one fit, read into 'fit' over what it already holds. Every key here is
    // optional because the main fit is read twice: once from "default" and once from the
    // bin override, which only carries what it changes. Absent therefore means "keep what
    // the previous pass set", never "reset".
    auto loadFit = [](const rapidjson::Value& node, AnalysisStructures::ExtrapFit& fit, std::string_view ctx) {
      fit.model = JsonConfig::OptionalString(node, "model", fit.model, ctx);

      if (auto range = JsonConfig::TryRange<AnalysisStructures::Range>(node, "fit_range", ctx))
        fit.fitRange = *range;

      MergeParams(node, fit.params, ctx);
    };

    // 'where' names the node in the messages: "default" or "bins['mult10-15']".
    auto loadNodeIntoConfig = [&](const rapidjson::Value& node, const std::string& where) {
      const std::string nodeCtx = std::format("{}.{}", sectionCtx, where);
      JsonConfig::RequireKnownKeys(node, {"model", "domain_range", "fit_range", "params", "high"}, nodeCtx);
      loadFit(node, config, nodeCtx);
      // The main fit is merged pass after pass, so its source is every node read so far.
      const std::string read = std::format("{}.{}", particle, where);
      config.source = config.source.empty() ? read : std::format("{} and {}", config.source, read);

      if (auto range = JsonConfig::TryRange<AnalysisStructures::Range>(node, "domain_range", nodeCtx))
        config.domainRange = *range;

      // The separate fit of the high side (ExtrapConfig::high). Replaced whole when a bin
      // override carries it, and switched off there with null: an override that only left
      // it out would keep the default's. Replaced whole, it has nothing to keep from a
      // previous pass, so its three keys are required.
      if (const rapidjson::Value* value = JsonConfig::OptionalMember(node, "high")) {
        const std::string ctx = std::format("{}.high", nodeCtx);
        config.high.reset();
        if (const rapidjson::Value* high = JsonConfig::ToObjectOrNull(*value, "high", ctx)) {
          for (const char* key : {"model", "fit_range", "params"})
            JsonConfig::RequireMember(*high, key, ctx);
          loadFit(*high, config.high.emplace(), ctx);
          config.high->source = std::format("{}.high", read);
        }
      }
    };

    // The default first, then the override of this bin over it; either may be absent.
    if (const rapidjson::Value* defaults = JsonConfig::TryObject(partNode, "default", sectionCtx))
      loadNodeIntoConfig(*defaults, "default");

    const rapidjson::Value* bins = JsonConfig::TryObject(partNode, "bins", sectionCtx);
    if (const rapidjson::Value* classNode = bins ? JsonConfig::TryObject(*bins, multTag.c_str(), std::format("{}.bins", sectionCtx)) : nullptr) {
      Logger::Debug("{}: Found custom parameters for bin: {}", managerName, multTag);
      loadNodeIntoConfig(*classNode, std::format("bins['{}']", multTag));
    }

    return config;
  }

 private:
  // A missing section is a legitimate state, and HasConfig is how a caller asks: it
  // skips the particle. Arriving here means a caller skipped that test - and an empty
  // ExtrapConfig would only fail later, with a message about a model that was never the
  // problem.
  const rapidjson::Value& Section(const std::string& particle) const
  {
    const rapidjson::Value* node = JsonConfig::TryObject(document, particle.c_str(), managerName);
    if (!node)
      throw std::runtime_error(std::format("[FATAL] {}: section '{}' asked for, but the JSON has none: check HasConfig first.",
                                           managerName, particle));
    return *node;
  }

  // section -> the MultTags its task extrapolates ("mult10-15"); filled by DeclareCells
  std::map<std::string, std::set<std::string>> declaredCells;
};
