#include <Riostream.h>
#include <TChain.h>
#include "TProof.h"

void Chain_pydef() {

  TChain *ch_def=new TChain("t","Pythia Default");
  ch_def->Add("/home/etlar/stefano/pythia8310/examples/rootfiles/Data_pydef/pythia-*.root");
									  
  // Just a trivial check.
  int tot_entries_def = ch_def->GetEntries();
  cout << "Total entries in the chain for Default: " << tot_entries_def << endl;

  // Start Proof Lite on cores.
  //TProof::Open("workers=20");
  //ch_def->SetProof();
  //ch_def->Process("Calibration.C+", "pythia_def");
  //ch_def->Process("Analysis_minbias.C+", "pythia_def");
  //ch_def->Process("Analysis_phi2.C+", "pythia_def");

  //ch_def->Process("Calibration_O2.C+", "pythiaMonash");
  ch_def->Process("Analysis_phi_O2.C+", "pythiaMonash");

  return;

}
