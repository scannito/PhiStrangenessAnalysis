#include <Riostream.h>
#include <TChain.h>
#include "TProof.h"

void Chain_pynew2() {

  TChain *ch_new=new TChain("t","Pythia Rope+QCD");
  ch_new->Add("/home/etlar/stefano/pythia8310/examples/rootfiles/Data_pynew3/pythia_new-*.root");
  //ch_new->Add("/home/etlar/stefano/pythia8310/examples/rootfiles/Data_pynew2/test3.root");
										  
  // Just a trivial check.
  int tot_entries_new = ch_new->GetEntries();
  cout << "Total entries in the chain for Rope+QCD: " << tot_entries_new << endl;

  // Start Proof Lite on cores.
  TProof::Open("workers=20");
  ch_new->SetProof();
  //ch_new->Process("Calibration.C+", "pythia_new");
  //ch_new->Process("Analysis_minbias.C+", "pythia_new");
  //ch_new->Process("Analysis_phi2.C+", "pythia_new");
  //ch_new->Process("Analysis_phi_xi.C+", "pythia_new");
  ch_new->Process("Analysis_phi2_kappa.C+", "pythia_new");

  return;

}
