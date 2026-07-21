#include "CCDB/BasicCCDBManager.h"
#include "Framework/Logger.h"

#include <TFile.h>
#include <TH2F.h>

#include <cmath>
#include <iostream>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

void uploadToCCDB(TString lDataset = "New-ppref-small")
{
  std::cout << "Uploader starting..." << std::endl;
  o2::ccdb::CcdbApi ccdb_api;
  ccdb_api.init("https://alice-ccdb.cern.ch");

  std::cout << "Dataset to process: " << lDataset.Data() << std::endl;

  // Interface with local objects to be uploaded
  std::cout << "Performing dedicated object upload..." << std::endl;

  // Creating vertex Z dummies
  TFile* file = new TFile(".root", "READ");
  TH2D* effMap = (TH2D*)file->Get("ccdb_object");

  // Create stuff to send to CCDB
  std::cout << "Defining metadata for this run..." << std::endl;
  std::map<std::string, std::string> metadata; // can be empty
  // metadata.insert(std::pair<std::string, std::string>{"Description", "Efficiency map"});
  // metadata.insert(std::pair<std::string, std::string>{"Author", "Stefano Cannito"});

  try {
    ccdb_api.storeAsTFileAny(listCalibration, "Users/s/scannito/Efficiencies/Calibration", metadata);
  } catch (std::exception const& e) {
    LOG(fatal) << "Failed at CCDB submission!";
  }
  std::cout << "Finished with upload! " << std::endl;
  std::cout << "Done!" << std::endl;
}
