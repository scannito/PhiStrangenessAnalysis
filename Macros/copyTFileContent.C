#include <TDirectory.h>
#include <TFile.h>
#include <TKey.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

// Recursive function to copy contents while excluding a vector of target directories
void copy_recursive(TDirectory* source, TDirectory* destination, const std::vector<std::string>& dirs_to_exclude)
{
  TKey* key;
  TIter next(source->GetListOfKeys());

  while ((key = (TKey*)next())) {
    std::string obj_name = key->GetName();
    std::string obj_class = key->GetClassName();

    // Check if the current object name exists inside our exclusion vector
    bool should_exclude = (std::ranges::find(dirs_to_exclude, obj_name) != dirs_to_exclude.end());

    // If it's a directory and it matches one of the targets, skip it completely
    if (obj_class == "TDirectoryFile" && should_exclude) {
      std::cout << "-> Excluding directory: " << obj_name << std::endl;
      continue;
    }

    // If it's a directory we want to keep, recreate its structure and dig deeper
    if (obj_class == "TDirectoryFile") {
      source->cd(obj_name.c_str());
      TDirectory* source_subdir = gDirectory;

      destination->cd();
      TDirectory* dest_subdir = destination->mkdir(obj_name.c_str());

      copy_recursive(source_subdir, dest_subdir, dirs_to_exclude);
    } else {
      // Read the object (TTree, TH1, etc.) and copy it to the new file
      source->cd();
      TObject* obj = key->ReadObj();
      destination->cd();
      obj->Write(obj_name.c_str());
      delete obj;
    }
  }
}

void copyTFileContent()
{
  /*std::string original_file = "../DataFile/pp_ref/DeltaY/Data/AnalysisResults_536_PtMod_Bneg_5agosto.root";
  std::string new_file = "../DataFile/pp_ref/DeltaY/Data/AnalysisResults_536_PtMod_Bneg_strinj_5agosto.root";

  // Simply list all the directories you want to wipe out here
  std::vector<std::string> dirs_to_delete = {
    "flow-generic-framework",
    "flow-generic-framework_rbr",
    "kstar892-light-ion_FT0C",
    "kstar892-light-ion_FV0A",
    "phi-strange-correlation_id52483",
    "phi-strange-correlation_OnlineEfficiency_id52483",
    "phi-strange-correlation_OnlineEfficiencyNoPhi_id52483"};*/

  std::string original_file = "../DataFile/pp_ref/DeltaY/Data/AnalysisResults_536_PtMod_Bpos_5agosto.root";
  std::string new_file = "../DataFile/pp_ref/DeltaY/Data/AnalysisResults_536_PtMod_Bpos_strinj_5agosto.root";

  // Simply list all the directories you want to wipe out here
  std::vector<std::string> dirs_to_delete = {
    "nch-studypp",
    "nucleitpc-pb-pb",
    "phi-strange-correlation_id45540",
    "phi-strange-correlation_OnlineEfficiency_id45540",
    "phi-strange-correlation_OnlineEfficiencyNoPhi_id45540"};

  TFile* f_in = new TFile(original_file.c_str(), "READ");
  if (!f_in || f_in->IsZombie()) {
    std::cerr << "Error opening source file!" << std::endl;
    return;
  }

  TFile* f_out = new TFile(new_file.c_str(), "RECREATE");

  std::cout << "Starting copy process and freeing up disk space..." << std::endl;

  copy_recursive(f_in, f_out, dirs_to_delete);

  f_out->Write();
  f_out->Close();
  f_in->Close();

  delete f_in;
  delete f_out;

  std::cout << "Done! The file '" << new_file << "' has been created successfully with optimized size." << std::endl;
}
