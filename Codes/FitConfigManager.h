#pragma once

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"

#include <cstdio>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

// Data Domain Struct: Handles X-axis and physical boundaries.
struct ObservableConfig {
  std::string name;
  std::string title;
  double min;
  double max;
};

// Parameter Struct: Holds numerical bounds for MINUIT.
struct FitParam {
  double val{0.0};
  double min{0.0};
  double max{0.0};

  bool isConstant{false};
};

// Mathematical Model Struct: Handles functions and numeric parameters.
struct ModelConfig {
  std::string sigModel;
  std::string bkgModel;
  std::map<std::string, FitParam> params;
};

// Integration Struct: Handles the post-fit signal extraction logic.
struct IntegrationConfig {
  bool useFixedRange{true};
  std::pair<double, double> range{0.0, 0.0};
  double nSigma{3.0};
  bool snapToBin{false};

  bool calculatePurity{false};

  bool calculateSideband{false};
  bool sidebandFromFit{true};
  std::pair<double, double> sidebandRange{0.0, 0.0};
};

// The Master Configuration: Glues the data domain and the mathematical model together.
struct FitConfig {
  ObservableConfig obs;
  ModelConfig model;
  IntegrationConfig integration;
};

class FitConfigManager
{
 public:
  // Constructor: opens and parses the JSON file ONLY ONCE
  FitConfigManager(const std::string& jsonPath = "fitConfig.json")
  {
    FILE* fp = fopen(jsonPath.c_str(), "rb"); // Open file in binary mode (faster)
    if (!fp) {
      std::cerr << "[ERROR] Cannot open configuration file: " << jsonPath << std::endl;
      return;
    }

    // Create a memory buffer (64KB) to read the file in chunks.
    // This makes reading significantly faster than reading line-by-line.
    char readBuffer[65536];
    rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));

    document.ParseStream(is); // Builds the JSON tree in memory
    fclose(fp);               // Close the file on disk, we don't need it anymore!

    if (!document.HasParseError()) {
      isLoaded = true;
      std::cout << "[INFO] FitConfigManager: JSON successfully loaded into RAM." << std::endl;
    } else {
      std::cerr << "[ERROR] Syntax error in the JSON file!" << std::endl;
    }
  }

  // Queries the RAM to get the configuration for a specific bin
  FitConfig GetConfig(const std::string& particle, int multBin, int ptBin) const
  {
    FitConfig config;

    // If JSON is not loaded or the particle section (e.g., "k0s") is missing, return an empty config
    if (!isLoaded || !document.HasMember(particle.c_str())) {
      std::cerr << "[ERROR] Particle section not found in JSON: " << particle << std::endl;
      return config;
    }

    const rapidjson::Value& partNode = document[particle.c_str()];

    // LOAD GLOBAL OBSERVABLE PROPERTIES
    // This is done ONLY ONCE for the particle, completely outside the override logic.
    // It prevents accidental bin-by-bin modifications of the physical X-axis.
    if (partNode.HasMember("observable")) {
      const auto& obsNode = partNode["observable"];
      if (obsNode.HasMember("name"))
        config.obs.name = obsNode["name"].GetString();
      if (obsNode.HasMember("title"))
        config.obs.title = obsNode["title"].GetString();
      if (obsNode.HasMember("min"))
        config.obs.min = obsNode["min"].GetDouble();
      if (obsNode.HasMember("max"))
        config.obs.max = obsNode["max"].GetDouble();
    }

    // LOAD FIT PARAMETERS (WITH OVERRIDE LOGIC)
    //  We now look strictly inside the "fit_setup" block for mathematical models.
    if (partNode.HasMember("fit_setup")) {
      const auto& fitNode = partNode["fit_setup"];

      // Local Lambda function to populate the nested ModelConfig struct
      auto loadNodeIntoModel = [&](const rapidjson::Value& node) {
        // Parse mathematical models
        if (node.HasMember("sig_model"))
          config.model.sigModel = node["sig_model"].GetString();
        if (node.HasMember("bkg_model"))
          config.model.bkgModel = node["bkg_model"].GetString();

        // Parse integration logic
        if (node.HasMember("snap_to_bin"))
          config.integration.snapToBin = node["snap_to_bin"].GetBool();

        if (node.HasMember("integration_range") && node["integration_range"].IsArray() && node["integration_range"].Size() == 2) {
          config.integration.range.first = node["integration_range"][0].GetDouble();
          config.integration.range.second = node["integration_range"][1].GetDouble();
          config.integration.useFixedRange = true;
        } else if (node.HasMember("n_sigma_integration")) {
          config.integration.nSigma = node["n_sigma_integration"].GetDouble();
          config.integration.useFixedRange = false;
        }

        // Parse purity calculation flag
        if (node.HasMember("calculate_purity"))
          config.integration.calculatePurity = node["calculate_purity"].GetBool();

        // Parse sideband calculation flags and ranges
        if (node.HasMember("calculate_sideband"))
          config.integration.calculateSideband = node["calculate_sideband"].GetBool();
        if (node.HasMember("sideband_from_fit"))
          config.integration.sidebandFromFit = node["sideband_from_fit"].GetBool();
        if (node.HasMember("sideband_range") && node["sideband_range"].IsArray() && node["sideband_range"].Size() == 2) {
          config.integration.sidebandRange.first = node["sideband_range"][0].GetDouble();
          config.integration.sidebandRange.second = node["sideband_range"][1].GetDouble();
        }

        // DYNAMICALLY loop through all items inside the "params" object
        if (node.HasMember("params") && node["params"].IsObject()) {
          for (auto& m : node["params"].GetObject()) {
            std::string paramName = m.name.GetString();
            // Notice we are saving into config.model.params now!
            LoadParam(m.value, config.model.params[paramName]);
          }
        }
      };

      // 1. First, apply all "default" mathematical parameters
      if (fitNode.HasMember("default"))
        loadNodeIntoModel(fitNode["default"]);

      // 2. Override with specific bin settings if they exist
      std::string binKey = "mult" + std::to_string(multBin) + "_pt" + std::to_string(ptBin);
      if (fitNode.HasMember("bins") && fitNode["bins"].HasMember(binKey.c_str())) {
        std::cout << "   [INFO] Found custom parameters for bin: " << binKey << std::endl;
        loadNodeIntoModel(fitNode["bins"][binKey.c_str()]);
      }
    }

    return config;
  }

 private:
  rapidjson::Document document;
  bool isLoaded{false};

  void LoadParam(const rapidjson::Value& node, FitParam& param) const
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
      throw std::runtime_error("[FATAL ERROR] FitConfigManager: Invalid parameter format in JSON. Must be a Number or an Array of 3 Numbers.");
    }
  }
};
