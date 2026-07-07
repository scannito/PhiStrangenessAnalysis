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
    auto loadNodeIntoConfig = [&](const rapidjson::Value& node) {
      if (node.HasMember("model"))
        config.model = node["model"].GetString();

      if (node.HasMember("domain_range") && node["domain_range"].IsArray() && node["domain_range"].Size() == 2) {
        config.domainRange.first = node["domain_range"][0].GetDouble();
        config.domainRange.second = node["domain_range"][1].GetDouble();
      }

      if (node.HasMember("fit_range") && node["fit_range"].IsArray() && node["fit_range"].Size() == 2) {
        config.fitRange.first = node["fit_range"][0].GetDouble();
        config.fitRange.second = node["fit_range"][1].GetDouble();
      }

      // DYNAMICALLY loop through all items inside the "params" object
      if (node.HasMember("params") && node["params"].IsObject()) {
        for (auto& m : node["params"].GetObject()) {
          std::string paramName = m.name.GetString();
          LoadParam(m.value, config.params[paramName]);
        }
      }
    };

    // 1. First, apply all "default" mathematical parameters
    if (partNode.HasMember("default")) {
      loadNodeIntoConfig(partNode["default"]);
    }

    // 2. Override with specific bin settings if they exist
    std::string binKey = "multBin" + std::to_string(multBin);
    if (partNode.HasMember("bins") && partNode["bins"].HasMember(binKey.c_str())) {
      std::cout << "   [INFO] " << managerName << ": Found custom parameters for bin: " << binKey << std::endl;
      loadNodeIntoConfig(partNode["bins"][binKey.c_str()]);
    }

    return config;
  }
};
