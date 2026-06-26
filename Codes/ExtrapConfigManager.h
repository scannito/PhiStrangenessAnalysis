#pragma once

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"

#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

// Structure to hold initial value and limits for a fit parameter
struct ExtrapParam {
  double val{0.0};
  double min{0.0};
  double max{0.0};
  bool isConstant{false}; // Flag to indicate if the parameter is fixed
};

// Complete configuration for a specific particle and bin
struct ExtrapConfig {
  std::string model;
  std::pair<double, double> domainRange{0.0, 15.0};
  std::pair<double, double> fitRange{0.0, 0.0};
  std::map<std::string, ExtrapParam> params;
  double mass{0.0}; // Injected dynamically at runtime by the Task
};

class ExtrapConfigManager
{
 public:
  // Constructor: opens and parses the JSON file ONLY ONCE, keeping the tree in RAM
  ExtrapConfigManager(const std::string& jsonPath = "extrapConfig.json")
  {
    FILE* fp = fopen(jsonPath.c_str(), "rb");
    if (!fp) {
      std::cerr << "[ERROR] ExtrapConfigManager: Cannot open configuration file: " << jsonPath << std::endl;
      return;
    }

    char readBuffer[65536];
    rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));

    document.ParseStream(is);
    fclose(fp);

    if (!document.HasParseError() && document.IsObject()) {
      isLoaded = true;
      std::cout << "[INFO] ExtrapConfigManager: JSON successfully loaded into RAM." << std::endl;
    } else {
      std::cerr << "[ERROR] ExtrapConfigManager: Syntax error or invalid object in the JSON file!" << std::endl;
    }
  }

  // Queries the RAM to get the configuration for a specific particle and multiplicity bin
  ExtrapConfig GetConfig(const std::string& particle, int multBin) const
  {
    ExtrapConfig config;

    if (!isLoaded || !document.HasMember(particle.c_str())) {
      std::cerr << "[ERROR] ExtrapConfigManager: Particle section not found in JSON: " << particle << std::endl;
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
      // std::cout << "   [INFO] Extrapolation: Found custom parameters for bin: " << binKey << std::endl;
      loadNodeIntoConfig(partNode["bins"][binKey.c_str()]);
    }

    return config;
  }

  /*// Constructor parses the JSON node representing the root of the extrap config file
  ExtrapConfigManager(const rapidjson::Value& rootNode)
  {
    if (!rootNode.IsObject()) {
      throw std::runtime_error("[FATAL] ExtrapConfigManager: Root node is not a valid JSON object.");
    }

    // Iterate over all particles defined in the JSON (e.g., "K0S", "Pi")
    for (auto& particleIt : rootNode.GetObject()) {
      std::string particleName = particleIt.name.GetString();
      const auto& particleNode = particleIt.value;

      ExtrapConfig defaultCfg;

      // 1. Parse 'default' settings if available
      if (particleNode.HasMember("default")) {
        defaultCfg = ParseConfigNode(particleNode["default"]);
      }
      configs[particleName]["default"] = defaultCfg;

      // 2. Parse specific 'bins' and overwrite default settings
      if (particleNode.HasMember("bins") && particleNode["bins"].IsObject()) {
        for (auto& binIt : particleNode["bins"].GetObject()) {
          std::string binName = binIt.name.GetString();

          // Inherit from default
          ExtrapConfig binCfg = defaultCfg;
          ExtrapConfig specificCfg = ParseConfigNode(binIt.value);

          // Override model if specified
          if (!specificCfg.model.empty()) {
            binCfg.model = specificCfg.model;
          }

          // Override domain range if specified
          if (specificCfg.domainRange.first != 0.0 || specificCfg.domainRange.second != 15.0) {
            binCfg.domainRange = specificCfg.domainRange;
          }

          // Override fit range if specified
          if (specificCfg.fitRange.first != 0.0 || specificCfg.fitRange.second != 0.0) {
            binCfg.fitRange = specificCfg.fitRange;
          }

          // Override specific parameters
          for (const auto& [pName, pVal] : specificCfg.params) {
            binCfg.params[pName] = pVal;
          }

          configs[particleName][binName] = binCfg;
        }
      }
    }
  }

  // Retrieve configuration for a specific particle and bin (fallback to default)
  ExtrapConfig GetConfig(const std::string& particle, const std::string& binName) const
  {
    if (configs.find(particle) == configs.end()) {
      throw std::runtime_error("[FATAL] ExtrapConfigManager: No setup found for particle " + particle);
    }

    const auto& particleConfigs = configs.at(particle);

    // Return specific bin config if exists, otherwise fallback to default
    if (particleConfigs.find(binName) != particleConfigs.end()) {
      return particleConfigs.at(binName);
    }
    return particleConfigs.at("default");
  }*/

 private:
  rapidjson::Document document;
  bool isLoaded{false};

  // Parses the numerical values for a parameter (handles both fixed and free params)
  void LoadParam(const rapidjson::Value& node, ExtrapParam& param) const
  {
    if (node.IsArray() && node.Size() == 3) {
      // Free parameter: [initial_value, min, max]
      param.val = node[0].GetDouble();
      param.min = node[1].GetDouble();
      param.max = node[2].GetDouble();
      param.isConstant = false;
    } else if (node.IsNumber()) {
      // Fixed parameter: single number
      param.val = node.GetDouble();
      param.isConstant = true;
    } else {
      throw std::runtime_error("[FATAL ERROR] ExtrapConfigManager: Invalid parameter format in JSON. Must be a Number or an Array of 3 Numbers.");
    }
  }

  /*// Data structure: configs[ParticleName][BinName] = ExtrapConfig
  std::map<std::string, std::map<std::string, ExtrapConfig>> configs;

  // Helper method to extract configuration from a rapidjson node
  ExtrapConfig ParseConfigNode(const rapidjson::Value& node)
  {
    ExtrapConfig cfg;

    if (node.HasMember("model") && node["model"].IsString()) {
      cfg.model = node["model"].GetString();
    }

    if (node.HasMember("domain_range") && node["domain_range"].IsArray() && node["domain_range"].Size() == 2) {
      cfg.domainRange.first = node["domain_range"][0].GetDouble();
      cfg.domainRange.second = node["domain_range"][1].GetDouble();
    }

    if (node.HasMember("fit_range") && node["fit_range"].IsArray() && node["fit_range"].Size() == 2) {
      cfg.fitRange.first = node["fit_range"][0].GetDouble();
      cfg.fitRange.second = node["fit_range"][1].GetDouble();
    }

    if (node.HasMember("params") && node["params"].IsObject()) {
      for (auto& paramIt : node["params"].GetObject()) {
        if (paramIt.value.IsArray() && paramIt.value.Size() == 3) {
          cfg.params[paramIt.name.GetString()] = {
            paramIt.value[0].GetDouble(),
            paramIt.value[1].GetDouble(),
            paramIt.value[2].GetDouble()};
        }
      }
    }
    return cfg;
  }*/
};
