#pragma once

#include "BaseConfigManager.h"

class FitConfigManager : public BaseConfigManager
{
 public:
  // Calls the Base constructor to handle file I/O
  FitConfigManager(const std::string& jsonPath = "fitConfig.json") : BaseConfigManager(jsonPath, "FitConfigManager") {}

  // Queries the RAM to get the configuration for a specific bin
  FitConfig GetConfig(const std::string& particle, int multBin, int ptBin) const
  {
    FitConfig config;

    // If JSON is not loaded or the particle section (e.g., "k0s") is missing, return an empty config
    if (!isLoaded || !document.HasMember(particle.c_str())) {
      std::cerr << "[ERROR] " << managerName << ": Particle section not found in JSON: " << particle << std::endl;
      return config;
    }

    const rapidjson::Value& partNode = document[particle.c_str()];

    // LOAD GLOBAL OBSERVABLE PROPERTIES
    // This is done ONLY ONCE for the particle, completely outside the override logic.
    // It prevents accidental bin-by-bin modifications of the physical X-axis.
    if (const rapidjson::Value* obs = JsonConfig::TryObject(partNode, "observable", managerName)) {
      config.obs.name = JsonConfig::OptionalString(*obs, "name", config.obs.name, managerName);
      config.obs.title = JsonConfig::OptionalString(*obs, "title", config.obs.title, managerName);
      config.obs.min = JsonConfig::OptionalDouble(*obs, "min", config.obs.min, managerName);
      config.obs.max = JsonConfig::OptionalDouble(*obs, "max", config.obs.max, managerName);
    }

    // LOAD FIT PARAMETERS (WITH OVERRIDE LOGIC)
    //  We now look strictly inside the "fit_setup" block for mathematical models.
    if (const rapidjson::Value* fitSetup = JsonConfig::TryObject(partNode, "fit_setup", managerName)) {
      const auto& fitNode = *fitSetup;

      // Local Lambda function to populate the nested ModelConfig struct
      // Every key is optional because this runs twice, on "default" and then on the
      // bin override, which carries only what it changes. The current value is the
      // fallback, so absent means "keep the previous pass" and never "reset".
      auto loadNodeIntoModel = [&](const rapidjson::Value& node) {
        config.model.sigModel = JsonConfig::OptionalString(node, "sig_model", config.model.sigModel, managerName);
        config.model.bkgModel = JsonConfig::OptionalString(node, "bkg_model", config.model.bkgModel, managerName);

        config.integration.snapToBin = JsonConfig::OptionalBool(node, "snap_to_bin", config.integration.snapToBin, managerName);

        // The two are alternatives, and an explicit range wins: reaching the second
        // branch means no range was given, not that n_sigma has priority.
        if (auto range = JsonConfig::TryRange(node, "integration_range", managerName)) {
          config.integration.range = *range;
          config.integration.useFixedRange = true;
        } else if (JsonConfig::OptionalMember(node, "n_sigma_integration")) {
          config.integration.nSigma = JsonConfig::OptionalDouble(node, "n_sigma_integration", config.integration.nSigma, managerName);
          config.integration.useFixedRange = false;
        }

        config.integration.calculatePurity = JsonConfig::OptionalBool(node, "calculate_purity", config.integration.calculatePurity, managerName);
        config.integration.calculateSideband = JsonConfig::OptionalBool(node, "calculate_sideband", config.integration.calculateSideband, managerName);
        config.integration.sidebandFromFit = JsonConfig::OptionalBool(node, "sideband_from_fit", config.integration.sidebandFromFit, managerName);

        if (auto range = JsonConfig::TryRange(node, "sideband_range", managerName))
          config.integration.sidebandRange = *range;

        // DYNAMICALLY loop through all items inside the "params" object
        if (const rapidjson::Value* params = JsonConfig::TryObject(node, "params", managerName)) {
          for (auto& m : params->GetObject()) {
            std::string paramName = m.name.GetString();
            // Notice we are saving into config.model.params now!
            LoadParam(m.value, paramName, config.model.params[paramName]);
          }
        }
      };

      // 1. First, apply all "default" mathematical parameters
      if (const rapidjson::Value* defaults = JsonConfig::TryObject(fitNode, "default", managerName))
        loadNodeIntoModel(*defaults);

      // 2. Override with specific bin settings if they exist
      std::string binKey = "mult" + std::to_string(multBin) + "_pt" + std::to_string(ptBin);
      // The outer key is fixed and must be an object; the inner one is built from
      // the bin indices, so its absence is normal and HasMember is the right tool.
      const rapidjson::Value* bins = JsonConfig::TryObject(fitNode, "bins", managerName);
      if (bins && bins->HasMember(binKey.c_str())) {
        std::cout << "   [INFO] " << managerName << ": Found custom parameters for bin: " << binKey << std::endl;
        loadNodeIntoModel((*bins)[binKey.c_str()]);
      }
    }

    return config;
  }
};
