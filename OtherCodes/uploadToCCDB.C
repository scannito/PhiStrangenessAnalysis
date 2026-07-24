#include "CCDB/BasicCCDBManager.h"
#include "Framework/Logger.h"

#include <TFile.h>
#include <TH2F.h>

#include <cmath>
#include <format>
#include <iostream>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

void uploadToCCDB(TString lDataset = "LHC26ac_pass1_Thin_medium")
{
  std::cout << "Uploader starting..." << std::endl;
  o2::ccdb::CcdbApi ccdb_api;
  ccdb_api.init("https://alice-ccdb.cern.ch");

  std::cout << "Dataset to process: " << lDataset.Data() << std::endl;

  // Interface with local objects to be uploaded
  std::cout << "Performing dedicated object upload..." << std::endl;

  for (const auto& particleName : {"Phi", "K0S", "Xi", "Pi"}) {
    std::string inputFileName = std::format("../DataFile/pp_ref/DeltaY/Corrections/MoreFullDifferential/StrInj_h2EffMap{}.root", particleName);
    TFile* file = new TFile(inputFileName.c_str(), "READ");
    TH2D* effMap = (TH2D*)file->Get("ccdb_object");

    // Create stuff to send to CCDB
    std::cout << "Defining metadata for this run..." << std::endl;
    std::map<std::string, std::string> metadata; // can be empty
    // metadata.insert(std::pair<std::string, std::string>{"Description", "Efficiency map"});
    // metadata.insert(std::pair<std::string, std::string>{"Author", "Stefano Cannito"});

    try {
      std::string outputPath = std::format("Users/s/scannito/Efficiencies/ppref24strinj/h2EffMap{}", particleName);
      ccdb_api.storeAsTFileAny(effMap, outputPath, metadata);
    } catch (std::exception const& e) {
      LOG(fatal) << "Failed at CCDB submission!";
    }

    delete effMap;
    delete file;
  }

  std::cout << "Finished with upload! " << std::endl;
  std::cout << "Done!" << std::endl;
}
