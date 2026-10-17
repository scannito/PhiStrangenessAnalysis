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
    JsonConfig::RequireKnownKeys(partNode, {"default", "bins"}, std::format("{}: section '{}'", managerName, particle));

    // Local Lambda function to populate the config struct dynamically
    // Every key here is optional because this lambda runs twice: once on "default"
    // and once on the bin override, which only carries what it changes. Absent
    // therefore means "keep what the previous pass set", never "reset".
    auto loadNodeIntoConfig = [&](const rapidjson::Value& node) {
      JsonConfig::RequireKnownKeys(node, {"model", "domain_range", "fit_range", "params"}, std::format("{}: section '{}'", managerName, particle));
      config.model = JsonConfig::OptionalString(node, "model", config.model, managerName);

      if (auto range = JsonConfig::TryRange(node, "domain_range", managerName))
        config.domainRange = *range;

      if (auto range = JsonConfig::TryRange(node, "fit_range", managerName))
        config.fitRange = *range;

      // DYNAMICALLY loop through all items inside the "params" object
      if (const rapidjson::Value* params = JsonConfig::TryObject(node, "params", managerName)) {
        for (auto& m : params->GetObject()) {
          std::string paramName = m.name.GetString();
          LoadParam(m.value, paramName, config.params[paramName]);
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
