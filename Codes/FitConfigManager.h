#pragma once

#include "BaseConfigManager.h"
#include "BinningUtils.h"
#include "Logger.h"
#include "OutputNames.h"

#include <format>
#include <initializer_list>
#include <map>
#include <set>
#include <span>

class FitConfigManager : public BaseConfigManager
{
 public:
  // Calls the Base constructor to handle file I/O
  FitConfigManager(const std::string& jsonPath = "fitConfig.json") : BaseConfigManager(jsonPath, "FitConfigManager") {}

  // The cells a task fits in a section, each by OutputNames::CellTag: every pT binning in 'ptBinnings' (PurityTask
  // fits the source and the analysis one) crossed with 'multBinning'. Declared once per section, before the fits,
  // every binning in the same call: the overrides are checked against the union of their cells, and a call per
  // binning would refuse a key valid only for a binning not declared yet.
  // An override of fit_setup.bins that names none of them stops the run, since it would never be applied (a typo,
  // or a key written for another binning); GetConfig refuses a cell that was not declared, so a task cannot skip
  // the check, nor fit a binning it did not declare.
  void DeclareCells(const std::string& section, std::span<const double> multBinning,
                    std::initializer_list<std::span<const double>> ptBinnings)
  {
    std::set<std::string>& cells = declaredCells[section];
    for (std::span<const double> ptBinning : ptBinnings)
      for (int multBin : BinningUtils::BinIndices(multBinning))
        for (int ptBin : BinningUtils::BinIndices(ptBinning))
          cells.insert(OutputNames::CellTag(multBinning, multBin, ptBinning, ptBin));
    if (const rapidjson::Value* bins = Bins(section))
      JsonConfig::RequireKnownKeys(*bins, cells, std::format("{}: section '{}'.fit_setup.bins (the keys are the cells this task fits)", managerName, section));
  }

  // The configuration of one cell, 'cell' as OutputNames::CellTag gives it: the section's default, then the
  // override of that cell if there is one.
  FitConfig GetConfig(const std::string& section, const std::string& cell) const
  {
    const auto declared = declaredCells.find(section);
    if (declared == declaredCells.end() || !declared->second.contains(cell))
      throw std::runtime_error(std::format("[FATAL] {}: GetConfig('{}', '{}') for a cell that was not declared with DeclareCells, "
                                           "so the overrides were not checked against it.",
                                           managerName, section, cell));

    FitConfig config;
    const rapidjson::Value& sectionNode = Section(section);

    // Every block below has a closed set of keys, so a misspelled one is refused instead
    // of skipped: a typo in n_sigma_integration would otherwise leave the previous window
    // in force and change the fit with no word (CLAUDE.md, Configuration).
    JsonConfig::RequireKnownKeys(sectionNode, {"observable", "fit_setup"}, std::format("{}: section '{}'", managerName, section));

    // LOAD GLOBAL OBSERVABLE PROPERTIES
    // This is done ONLY ONCE for the section, completely outside the override logic.
    // It prevents accidental bin-by-bin modifications of the physical X-axis.
    // The block, the name and the range are required: nothing sensible can stand in for
    // what is fitted or over which interval. The title only labels the axis.
    const std::string obsCtx = std::format("{}: section '{}'", managerName, section);
    const rapidjson::Value* obs = JsonConfig::TryObject(sectionNode, "observable", obsCtx);
    if (!obs)
      throw std::runtime_error(std::format("[FATAL] {}: 'observable' missing in JSON!", obsCtx));
    JsonConfig::RequireKnownKeys(*obs, {"name", "title", "min", "max"}, obsCtx + ".observable");
    config.obs.name = JsonConfig::RequireString(*obs, "name", obsCtx);
    config.obs.title = JsonConfig::OptionalString(*obs, "title", config.obs.title, obsCtx);
    config.obs.min = JsonConfig::RequireDouble(*obs, "min", obsCtx);
    config.obs.max = JsonConfig::RequireDouble(*obs, "max", obsCtx);
    if (!(config.obs.min < config.obs.max))
      throw std::runtime_error(std::format("[FATAL] {}: observable range is empty or inverted: min = {}, max = {}.",
                                           obsCtx, config.obs.min, config.obs.max));

    // LOAD FIT PARAMETERS (WITH OVERRIDE LOGIC)
    //  We now look strictly inside the "fit_setup" block for mathematical models.
    if (const rapidjson::Value* fitSetup = JsonConfig::TryObject(sectionNode, "fit_setup", managerName)) {
      const auto& fitNode = *fitSetup;
      JsonConfig::RequireKnownKeys(fitNode, {"default", "bins"}, std::format("{}: section '{}'.fit_setup", managerName, section));

      // Local Lambda function to populate the nested ModelConfig struct
      // Every key is optional because this runs twice, on "default" and then on the
      // bin override, which carries only what it changes. The current value is the
      // fallback, so absent means "keep the previous pass" and never "reset".
      auto loadNodeIntoModel = [&](const rapidjson::Value& node) {
        JsonConfig::RequireKnownKeys(node, {"sig_model", "bkg_model", "snap_to_bin", "integration_range", "n_sigma_integration", "calculate_purity", "sideband", "params"},
                                     std::format("{}: section '{}'", managerName, section));
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
          JsonConfig::RequireKnownKeys(*sbNode, {"from_fit", "range"}, std::format("{}: section '{}'.sideband", managerName, section));
          SidebandConfig sb = config.integration.sideband.value_or(SidebandConfig{});
          sb.fromFit = JsonConfig::OptionalBool(*sbNode, "from_fit", sb.fromFit, managerName);
          if (auto r = JsonConfig::TryRange(*sbNode, "range", managerName))
            sb.range = *r;
          config.integration.sideband = sb;
        }

        MergeParams(node, config.model.params, managerName);
      };

      // 1. First, apply all "default" mathematical parameters
      if (const rapidjson::Value* defaults = JsonConfig::TryObject(fitNode, "default", managerName))
        loadNodeIntoModel(*defaults);

      // 2. Override with specific bin settings if they exist
      // The outer key is fixed and must be an object; the inner one is the cell, so
      // its absence is normal and HasMember is the right tool.
      const rapidjson::Value* bins = Bins(section);
      if (bins && bins->HasMember(cell.c_str())) {
        Logger::Debug("{}: Found custom parameters for bin: {}", managerName, cell);
        loadNodeIntoModel((*bins)[cell.c_str()]);
        config.binOverride = cell;
      }
    }

    return config;
  }

 private:
  // The sections of this file are asked for by name, by tasks whose own configuration
  // decides which ones: the purity task by each particle's 'purity_key', the trigger
  // fit by "phi". A section cannot be defaulted - there is no sensible model,
  // observable or range to invent - so its absence is always an error, unlike for
  // ExtrapConfigManager, where it means "skip this particle". Checked before the
  // lookup: rapidjson's operator[] on a missing key asserts, and in a Release
  // build (where asserts are compiled out) returns a null value in silence.
  const rapidjson::Value& Section(const std::string& section) const
  {
    if (!isLoaded || !document.HasMember(section.c_str()))
      throw std::runtime_error(std::format("[FATAL] {}: no section '{}' in the fit configuration.",
                                           managerName, section));
    return document[section.c_str()];
  }

  // The section's fit_setup.bins, or null if it has none.
  const rapidjson::Value* Bins(const std::string& section) const
  {
    const rapidjson::Value* fitSetup = JsonConfig::TryObject(Section(section), "fit_setup", managerName);
    return fitSetup ? JsonConfig::TryObject(*fitSetup, "bins", managerName) : nullptr;
  }

  // section -> the CellTags its task fits ("mult0-1_pt0.7-0.8"), every declared binning together; filled by DeclareCells
  std::map<std::string, std::set<std::string>> declaredCells;
};
