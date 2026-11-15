#include <Riostream.h>
#include <TChain.h>
#include "TProof.h"

void Chain_pynew() {

  TChain *ch_new=new TChain("t","Pythia Ropes+QCD");
  //ch_new->Add("/home/etlar/stefano/pythia8310/examples/rootfiles/Data_pynew/pythia_new-*.root");

  //ch_new->Add("/home/etlar/stefano/pythia8310/examples/rootfiles/Data_pynew_omegatest/pythia_new-*.root");	
  
  ch_new->Add("/home/etlar/stefano/pythia8310/examples/rootfiles/Data_Pythia_Ropes/pythia_ropes-*.root");
  ch_new->Add("/home/etlar/stefano/pythia8310/examples/rootfiles/Data_Pythia_Ropes2/pythia_ropes-*.root");
  ch_new->Add("/home/etlar/stefano/pythia8310/examples/rootfiles/Data_Pythia_Ropes3/pythia_ropes-*.root");
  ch_new->Add("/home/etlar/stefano/pythia8310/examples/rootfiles/Data_Pythia_Ropes4/pythia_ropes-*.root");
  ch_new->Add("/home/etlar/stefano/pythia8310/examples/rootfiles/Data_Pythia_Ropes5/pythia_ropes-*.root");
  ch_new->Add("/home/etlar/stefano/pythia8310/examples/rootfiles/Data_Pythia_Ropes6/pythia_ropes-*.root");

  // Just a trivial check.
  int tot_entries_new = ch_new->GetEntries();
  cout << "Total entries in the chain for Ropes+QCD: " << tot_entries_new << endl;

  // Start Proof Lite on cores.
  //TProof::Open("workers=20");
  //ch_new->SetProof();
  //ch_new->Process("Calibration.C+", "pythia_new");
  //ch_new->Process("Analysis_minbias.C+", "pythia_new");
  //ch_new->Process("Analysis_phi2.C+", "pythia_new");
  //ch_new->Process("Analysis_phi2_new.C+", "pythia_new");
  //ch_new->Process("Analysis_phi_xi.C+", "pythia_new");

  //ch_new->Process("Calibration_O2.C+", "pythiaRopes");
  //ch_new->Process("Analysis_phi_O2.C+", "pythiaRopes");

  //ch_new->Process("Calibration_Run3.C+", "pythiaRopes");
  ch_new->Process("Analysis_phi_Run3.C+", "pythiaRopes");
 
  return;

}
