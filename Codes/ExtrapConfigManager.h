#pragma once

#include "BaseConfigManager.h"

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

    if (!isLoaded || !document.HasMember(particle.c_str())) {
      std::cerr << "[ERROR] " << managerName << ": Particle section not found in JSON: " << particle << std::endl;
      return config;
    }

    const rapidjson::Value& partNode = document[particle.c_str()];

    // Local Lambda function to populate the config struct dynamically
    // Every key here is optional because this lambda runs twice: once on "default"
    // and once on the bin override, which only carries what it changes. Absent
    // therefore means "keep what the previous pass set", never "reset".
    auto loadNodeIntoConfig = [&](const rapidjson::Value& node) {
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
    std::string binKey = "multBin" + std::to_string(multBin);
    // The outer key is fixed and must be an object; the inner one is built from the
    // bin index, so its absence is the normal case and HasMember is the right tool.
    const rapidjson::Value* bins = JsonConfig::TryObject(partNode, "bins", managerName);
    if (bins && bins->HasMember(binKey.c_str())) {
      std::cout << "   [INFO] " << managerName << ": Found custom parameters for bin: " << binKey << std::endl;
      loadNodeIntoConfig((*bins)[binKey.c_str()]);
    }

    return config;
  }
};
