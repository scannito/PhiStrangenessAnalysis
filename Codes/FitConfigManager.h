#pragma once

#include "BaseConfigManager.h"
#include "Logger.h"

#include <format>

class FitConfigManager : public BaseConfigManager
{
 public:
  // Calls the Base constructor to handle file I/O
  FitConfigManager(const std::string& jsonPath = "fitConfig.json") : BaseConfigManager(jsonPath, "FitConfigManager") {}

  // Queries the RAM to get the configuration for a specific bin
  FitConfig GetConfig(const std::string& particle, int multBin, int ptBin) const
  {
    FitConfig config;

    // The sections of this file are asked for by name, by tasks whose own configuration
    // decides which ones: the purity task by each particle's 'purity_key', the trigger
    // fit by "phi". A section cannot be defaulted - there is no sensible model,
    // observable or range to invent - so its absence is always an error, unlike for
    // ExtrapConfigManager, where it means "skip this particle". Checked before the
    // lookup below: rapidjson's operator[] on a missing key asserts, and in a Release
    // build (where asserts are compiled out) returns a null value in silence.
    if (!isLoaded || !document.HasMember(particle.c_str()))
      throw std::runtime_error(std::format("[FATAL] {}: no section '{}' in the fit configuration.",
                                           managerName, particle));

    const rapidjson::Value& partNode = document[particle.c_str()];

    // LOAD GLOBAL OBSERVABLE PROPERTIES
    // This is done ONLY ONCE for the particle, completely outside the override logic.
    // It prevents accidental bin-by-bin modifications of the physical X-axis.
    // The block, the name and the range are required: nothing sensible can stand in for
    // what is fitted or over which interval. The title only labels the axis.
    const std::string obsCtx = std::format("{}: section '{}'", managerName, particle);
    const rapidjson::Value* obs = JsonConfig::TryObject(partNode, "observable", obsCtx);
    if (!obs)
      throw std::runtime_error(std::format("[FATAL] {}: 'observable' missing in JSON!", obsCtx));
    config.obs.name = JsonConfig::RequireString(*obs, "name", obsCtx);
    config.obs.title = JsonConfig::OptionalString(*obs, "title", config.obs.title, obsCtx);
    config.obs.min = JsonConfig::RequireDouble(*obs, "min", obsCtx);
    config.obs.max = JsonConfig::RequireDouble(*obs, "max", obsCtx);
    if (!(config.obs.min < config.obs.max))
      throw std::runtime_error(std::format("[FATAL] {}: observable range is empty or inverted: min = {}, max = {}.",
                                           obsCtx, config.obs.min, config.obs.max));

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
          config.integration.signalWindow = FixedRange{range->first, range->second};
        } else if (auto nSigma = JsonConfig::TryDouble(node, "n_sigma_integration", managerName)) {
          config.integration.signalWindow = NSigma{*nSigma};
        }

        config.integration.calculatePurity = JsonConfig::OptionalBool(node, "calculate_purity", config.integration.calculatePurity, managerName);

        if (const rapidjson::Value* sbNode = JsonConfig::TryObject(node, "sideband", managerName)) {
          SidebandConfig sb = config.integration.sideband.value_or(SidebandConfig{});
          sb.fromFit = JsonConfig::OptionalBool(*sbNode, "from_fit", sb.fromFit, managerName);
          if (auto r = JsonConfig::TryRange(*sbNode, "range", managerName))
            sb.range = *r;
          config.integration.sideband = sb;
        }

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
      std::string binKey = std::format("mult{}_pt{}", multBin, ptBin);
      // The outer key is fixed and must be an object; the inner one is built from
      // the bin indices, so its absence is normal and HasMember is the right tool.
      const rapidjson::Value* bins = JsonConfig::TryObject(fitNode, "bins", managerName);
      if (bins && bins->HasMember(binKey.c_str())) {
        Logger::Debug("{}: Found custom parameters for bin: {}", managerName, binKey);
        loadNodeIntoModel((*bins)[binKey.c_str()]);
      }
    }

    return config;
  }
};
