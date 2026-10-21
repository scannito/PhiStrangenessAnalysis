#include "CCDB/BasicCCDBManager.h"
#include "Framework/Logger.h"

#include <TFile.h>
#include <TH2D.h>

#include <cctype>
#include <cmath>
#include <format>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct DatasetInfo {
  std::string name, prefix, dir, category;
  std::map<std::string, std::pair<unsigned int, unsigned int>> runRanges;
};

DatasetInfo datasetInfo2023{
  .name = "LHC23_pass4_Thin_medium",
  .prefix = "2023",
  .dir = "pp23",
  .category = "full"};

DatasetInfo datasetInfo2023StrInj{
  .name = "LHC23_pass4_Thin_medium_strinj",
  .prefix = "2023_StrInj",
  .dir = "pp23strinj",
  .category = "full"};

DatasetInfo dataset2026PerPeriod{
  .name = "LHC26_pass1_Thin_medium_per_period",
  .prefix = "2026",
  .dir = "pp26",
  .category = "per_period",
  .runRanges = {{"HighIR", {569945, 570608}},
                {"LowIRad", {570609, 570703}},
                {"LowIRae", {570733, 570872}},
                {"LowIRaf", {571002, 571211}},
                {"LowIRag", {571234, 571290}},
                {"LowIRai", {571424, 571787}}}};

DatasetInfo dataset2026PerIR{
  .name = "LHC26_pass1_Thin_medium_per_IR",
  .prefix = "2026",
  .dir = "pp26",
  .category = "per_IR",
  .runRanges = {{"HighIR", {569945, 570608}},
                {"LowIR", {570609, 571787}}}};

DatasetInfo dataset2026{
  .name = "LHC26_pass1_Thin_medium",
  .prefix = "2026",
  .dir = "pp26",
  .category = "full"};

DatasetInfo dataset2026StrInjPerPeriod{
  .name = "LHC26_pass1_Thin_medium_strinj_per_period",
  .prefix = "2026_StrInj",
  .dir = "pp26",
  .category = "per_period",
  .runRanges = {{"HighIR", {569945, 570608}},
                {"LowIRad", {570609, 570703}},
                {"LowIRae", {570733, 570872}},
                {"LowIRaf", {571002, 571211}},
                {"LowIRag", {571234, 571290}},
                {"LowIRai", {571424, 571787}}}};

DatasetInfo dataset2026StrInjPerIR{
  .name = "LHC26_pass1_Thin_medium_strinj_per_IR",
  .prefix = "2026_StrInj",
  .dir = "pp26",
  .category = "per_IR",
  .runRanges = {{"HighIR", {569945, 570608}},
                {"LowIR", {570609, 571787}}}};

DatasetInfo getDatasetInfo(std::string datasetName)
{
  if (datasetName == datasetInfo2023.name) {
    return datasetInfo2023;
  } else if (datasetName == datasetInfo2023StrInj.name) {
    return datasetInfo2023StrInj;
  } else if (datasetName == dataset2026PerPeriod.name) {
    return dataset2026PerPeriod;
  } else if (datasetName == dataset2026PerIR.name) {
    return dataset2026PerIR;
  } else if (datasetName == dataset2026.name) {
    return dataset2026;
  } else if (datasetName == dataset2026StrInjPerPeriod.name) {
    return dataset2026StrInjPerPeriod;
  } else if (datasetName == dataset2026StrInjPerIR.name) {
    return dataset2026StrInjPerIR;
  } else {
    throw std::runtime_error("Unknown dataset name: " + datasetName);
  }
}

void uploadToCCDB(std::string datasetName = "LHC26_pass1_Thin_medium", bool perPeriodFullValidity = false)
{
  std::cout << "Uploader starting..." << std::endl;
  o2::ccdb::CcdbApi ccdb_api;
  ccdb_api.init("https://alice-ccdb.cern.ch");

  DatasetInfo dataset = getDatasetInfo(datasetName);
  std::cout << "Dataset to process: " << dataset.name << std::endl;

  // Interface with local objects to be uploaded
  std::cout << "Performing dedicated object upload..." << std::endl;

  if (datasetName == dataset2026PerPeriod.name || datasetName == dataset2026PerIR.name || datasetName == dataset2026StrInjPerPeriod.name || datasetName == dataset2026StrInjPerIR.name) {
    for (const auto& [periodName, runRange] : dataset.runRanges) {
      std::map<std::string, std::string> metadataRCTFirstRun, metadataRCTLastRun, headersFirstRun, headersLastRun;

      headersFirstRun = ccdb_api.retrieveHeaders(std::format("RCT/Info/RunInformation/{}", runRange.first), metadataRCTFirstRun, -1);
      long firstRunStartTime = std::stol(headersFirstRun["SOR"]) - 300;
      headersLastRun = ccdb_api.retrieveHeaders(std::format("RCT/Info/RunInformation/{}", runRange.second), metadataRCTLastRun, -1);
      long lastRunEndTime = std::stol(headersLastRun["EOR"]) + 300;

      std::cout << "Processing period: " << periodName << std::endl;

      for (const auto& particleName : {"Phi", "K0S", "Xi", "Omega", "Pi"}) {
        std::string inputFileName = std::format("../DataFile/pp/DeltaY/Corrections/MoreFullDifferential/{}_{}_updated_h2EffMap{}.root", dataset.prefix, periodName, particleName);
        TFile* file = new TFile(inputFileName.c_str(), "READ");
        TH2D* effMap = (TH2D*)file->Get("ccdb_object");

        // Create stuff to send to CCDB
        // std::cout << "Defining metadata for this run..." << std::endl;
        std::map<std::string, std::string> metadata; // can be empty
        // metadata.insert(std::pair<std::string, std::string>{"Description", "Efficiency map"});
        // metadata.insert(std::pair<std::string, std::string>{"Author", "Stefano Cannito"});

        std::string periodNameLower = periodName;
        periodNameLower[0] = std::tolower(periodNameLower[0]);
        // std::string perPeriodFullValidityStr = perPeriodFullValidity ? "fullvalidity" : "";

        try {
          std::string outputPath;
          if (perPeriodFullValidity) {
            std::cout << "Uploading efficiency map for particle " << particleName << " for period " << periodName << " with full validity..." << std::endl;

            outputPath = std::format("Users/s/scannito/Efficiencies/{}_{}_updated/h2EffMap{}", dataset.dir, periodNameLower, particleName);
          } else {
            std::cout << "Uploading efficiency map for particle " << particleName << " for period " << periodName << " with run range validity..." << std::endl;

            outputPath = std::format("Users/s/scannito/Efficiencies/{}/perIR_Updated/h2EffMap{}", dataset.dir, particleName);
          }

          long startValidity = perPeriodFullValidity ? 1 : firstRunStartTime;        // 1 Jan 1970 or first run start time
          long endValidity = perPeriodFullValidity ? 2524604400000 : lastRunEndTime; // 1 Jan 2050 or last run end time

          ccdb_api.storeAsTFileAny(effMap, outputPath, metadata, startValidity, endValidity);
        } catch (std::exception const& e) {
          LOG(fatal) << "Failed at CCDB submission!";
        }

        delete file;
      }
    }
  } else {
    for (const auto& particleName : {"Phi", "K0S", "Xi", "Omega", "Pi"}) {
      std::string inputFileName = std::format("../DataFile/pp/DeltaY/Corrections/MoreFullDifferential/{}_updated_h2EffMap{}.root", dataset.prefix, particleName);
      TFile* file = new TFile(inputFileName.c_str(), "READ");
      TH2D* effMap = (TH2D*)file->Get("ccdb_object");

      // Create stuff to send to CCDB
      // std::cout << "Defining metadata for this run..." << std::endl;
      std::map<std::string, std::string> metadata; // can be empty
      // metadata.insert(std::pair<std::string, std::string>{"Description", "Efficiency map"});
      // metadata.insert(std::pair<std::string, std::string>{"Author", "Stefano Cannito"});

      try {
        std::cout << "Uploading efficiency map for particle " << particleName << "..." << std::endl;

        std::string outputPath = std::format("Users/s/scannito/Efficiencies/{}_updated/h2EffMap{}", dataset.dir, particleName);

        long startValidity = 1;           // 1 Jan 1970
        long endValidity = 2524604400000; // 1 Jan 2050

        ccdb_api.storeAsTFileAny(effMap, outputPath, metadata, startValidity, endValidity);
      } catch (std::exception const& e) {
        LOG(fatal) << "Failed at CCDB submission!";
      }

      delete file;
    }
  }

  std::cout << "Finished with upload! " << std::endl;
  std::cout << "Done!" << std::endl;
}
