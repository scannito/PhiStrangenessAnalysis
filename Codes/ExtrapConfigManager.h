#pragma once

#include "BaseConfigManager.h"
#include "Logger.h"

#include <format>

class ExtrapConfigManager : public BaseConfigManager
{
 public:
  // Calls the Base constructor to handle file I/O
  ExtrapConfigManager(const std::string& jsonPath = "extrapConfig.json") : BaseConfigManager(jsonPath, "ExtrapConfigManager") {}

  bool HasConfig(const std::string& particle) const
  {
    return isLoaded && document.HasMember(particle.c_str());
  }

  // Queries the RAM to get the configuration for a specific bin
  ExtrapConfig GetConfig(const std::string& particle, int multBin) const
  {
    ExtrapConfig config;

    // A missing section is a legitimate state, and HasConfig is how a caller asks: it
    // skips the particle. GetConfig is for particles that passed that test, so arriving
    // here means a caller skipped it - and an empty ExtrapConfig would only fail later,
    // with a message about a model that was never the problem.
    if (!isLoaded || !document.HasMember(particle.c_str())) {
      throw std::runtime_error(std::format(
        "[FATAL] {}: GetConfig('{}') called for a particle with no section in the JSON: check HasConfig first.",
        managerName, particle));
    }

    const rapidjson::Value& partNode = document[particle.c_str()];
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
    std::string binKey = std::format("multBin{}", multBin);
    // The outer key is fixed and must be an object; the inner one is built from the
    // bin index, so its absence is the normal case and HasMember is the right tool.
    const rapidjson::Value* bins = JsonConfig::TryObject(partNode, "bins", managerName);
    if (bins && bins->HasMember(binKey.c_str())) {
      Logger::Debug("{}: Found custom parameters for bin: {}", managerName, binKey);
      loadNodeIntoConfig((*bins)[binKey.c_str()]);
    }

    return config;
  }
};
