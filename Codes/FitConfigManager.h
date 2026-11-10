#pragma once

#include "BaseConfigManager.h"
#include "BinningUtils.h"
#include "OutputNames.h"

#include <format>
#include <set>

class FitConfigManager : public BaseConfigManager
{
 public:
  // Calls the Base constructor to handle file I/O
  FitConfigManager(const std::string& jsonPath = "fitConfig.json") : BaseConfigManager(jsonPath, "FitConfigManager") {}

  // The cells a task fits in a section, each by OutputNames::CellTag: every multiplicity binning in 'multBinnings'
  // (PurityTask fits the classes and their union, OutputNames::ClassesAndUnion) crossed with every pT binning in 'ptBinnings' (PurityTask fits the
  // source and the analysis one). Declared once per section, before the fits, every binning in the same call: the
  // overrides are checked against the cells of that call, and a call per binning would refuse a key valid for
  // another one. The overrides are fit_setup.bins, checked and read as BaseConfigManager::DeclareTags says.
  void DeclareCells(const std::string& section, const std::vector<std::vector<double>>& multBinnings,
                    const std::vector<std::vector<double>>& ptBinnings)
  {
    std::set<std::string> cells;
    for (const std::vector<double>& multBinning : multBinnings)
      for (const std::vector<double>& ptBinning : ptBinnings)
        for (int multBin : BinningUtils::BinIndices(multBinning))
          for (int ptBin : BinningUtils::BinIndices(ptBinning))
            cells.insert(OutputNames::CellTag(multBinning, multBin, ptBinning, ptBin));
    const std::string fitCtx = std::format("{}.fit_setup", SectionCtx(section));
    DeclareTags(section, cells, JsonConfig::TryObject(Section(section), "fit_setup", SectionCtx(section)), fitCtx,
                "the cells this task fits");
    for (const std::string& cell : cells)
      (void)GetConfig(section, cell); // every cell read once, at Init (BaseConfigManager::DeclareTags)
  }

  // The configuration of one cell, 'cell' as OutputNames::CellTag gives it: the section's default, then the
  // override of that cell if there is one.
  AnalysisStructures::FitConfig GetConfig(const std::string& section, const std::string& cell) const
  {
    AnalysisStructures::FitConfig config;
    const rapidjson::Value& sectionNode = Section(section);

    // Every block below has a closed set of keys, so a misspelled one is refused instead
    // of skipped: a typo in n_sigma_integration would otherwise leave the previous window
    // in force and change the fit with no word (CLAUDE.md, Configuration).
    // One context for the section; every message below names the node under it.
    const std::string sectionCtx = SectionCtx(section);
    JsonConfig::RequireKnownKeys(sectionNode, {"observable", "fit_setup"}, sectionCtx);

    // LOAD GLOBAL OBSERVABLE PROPERTIES
    // This is done ONLY ONCE for the section, completely outside the override logic.
    // It prevents accidental bin-by-bin modifications of the physical X-axis.
    // The block, the name and the range are required: nothing sensible can stand in for
    // what is fitted or over which interval. The title only labels the axis.
    const rapidjson::Value& obs = JsonConfig::RequireObject(sectionNode, "observable", sectionCtx);
    const std::string obsCtx = std::format("{}.observable", sectionCtx);
    JsonConfig::RequireKnownKeys(obs, {"name", "title", "min", "max"}, obsCtx);
    config.obs.name = JsonConfig::RequireString(obs, "name", obsCtx);
    config.obs.title = JsonConfig::OptionalString(obs, "title", config.obs.title, obsCtx);
    config.obs.min = JsonConfig::RequireDouble(obs, "min", obsCtx);
    config.obs.max = JsonConfig::RequireDouble(obs, "max", obsCtx);
    if (!(config.obs.min < config.obs.max))
      throw std::runtime_error(std::format("[FATAL] {}: observable range is empty or inverted: min = {}, max = {}.",
                                           obsCtx, config.obs.min, config.obs.max));

    // LOAD FIT PARAMETERS (WITH OVERRIDE LOGIC)
    //  We now look strictly inside the "fit_setup" block for mathematical models.
    const rapidjson::Value* fitSetup = JsonConfig::TryObject(sectionNode, "fit_setup", sectionCtx);
    const std::string fitCtx = std::format("{}.fit_setup", sectionCtx);

    // Local Lambda function to populate the nested ModelConfig struct
    // Every key is optional because this runs twice, on "default" and then on the
    // bin override, which carries only what it changes. The current value is the
    // fallback, so absent means "keep the previous pass" and never "reset".
    // 'where' is the node read, "default" or "bins['mult10-15_pt1.2-1.6']", named under fit_setup in the messages.
    auto loadNodeIntoModel = [&](const rapidjson::Value& node, const std::string& where) {
      const std::string ctx = std::format("{}.{}", fitCtx, where);
      JsonConfig::RequireKnownKeys(node, {"sig_model", "bkg_model", "integration_range", "n_sigma_integration", "calculate_purity", "sideband", "params"},
                                   ctx);
      config.model.sigModel = JsonConfig::OptionalString(node, "sig_model", config.model.sigModel, ctx);
      config.model.bkgModel = JsonConfig::OptionalString(node, "bkg_model", config.model.bkgModel, ctx);


      // The two are alternatives, and an explicit range wins: reaching the second
      // branch means no range was given, not that n_sigma has priority.
      if (auto range = JsonConfig::TryRange<AnalysisStructures::Range>(node, "integration_range", ctx)) {
        config.integration.signalWindow = *range;
      } else if (auto nSigma = JsonConfig::TryDouble(node, "n_sigma_integration", ctx)) {
        config.integration.signalWindow = AnalysisStructures::NSigma{*nSigma};
      }

      config.integration.calculatePurity = JsonConfig::OptionalBool(node, "calculate_purity", config.integration.calculatePurity, ctx);

      if (const rapidjson::Value* sbNode = JsonConfig::TryObject(node, "sideband", ctx)) {
        const std::string sbCtx = std::format("{}.sideband", ctx);
        JsonConfig::RequireKnownKeys(*sbNode, {"from_fit", "range"}, sbCtx);
        AnalysisStructures::SidebandConfig sb = config.integration.sideband.value_or(AnalysisStructures::SidebandConfig{});
        sb.fromFit = JsonConfig::OptionalBool(*sbNode, "from_fit", sb.fromFit, sbCtx);
        if (auto r = JsonConfig::TryRange<AnalysisStructures::Range>(*sbNode, "range", sbCtx))
          sb.range = *r;
        config.integration.sideband = sb;
      }

      MergeParams(node, config.model.params, ctx);
    };

    // The default first, then the override of this cell over it.
    if (LoadLayered(section, fitSetup, fitCtx, cell, loadNodeIntoModel))
      config.binOverride = cell;

    // A sideband needs its range, after the default and the cell are merged: absent it stays {0, 0}, an empty range
    // that the histogram count refuses and the fit integrates to nothing, an R of 0 or infinity.
    if (const auto& sb = config.integration.sideband; sb && !(sb->range.min < sb->range.max))
      throw std::runtime_error(std::format("[FATAL] {}, cell '{}': 'sideband' has no 'range' [low, high].",
                                           sectionCtx, cell));

    return config;
  }

 private:
  // The sections of this file are asked for by name, by tasks whose own configuration
  // decides which ones: the purity task by each particle's 'purity_key', the trigger
  // fit by "phi". A section cannot be defaulted - there is no sensible model,
  // observable or range to invent - so its absence is always an error, unlike for
  // ExtrapConfigManager, where it means "skip this particle".
  const rapidjson::Value& Section(const std::string& section) const
  {
    const rapidjson::Value* node = JsonConfig::TryObject(document, section.c_str(), managerName);
    if (!node)
      throw std::runtime_error(std::format("[FATAL] {}: no section '{}' in the fit configuration.",
                                           managerName, section));
    return *node;
  }
};
