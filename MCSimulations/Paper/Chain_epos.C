#include <Riostream.h>
#include <TChain.h>
#include "TProof.h"

void Chain_epos() {
 
  TChain *ch_epos=new TChain("t","EPOS4");
  /*ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_proc_run1/EPOS4-*.root");
  ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_proc_run2/EPOS4-*.root");
  ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_proc_run3/EPOS4-*.root");
  ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_proc_run4/EPOS4-*.root");
  ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_proc_run5/EPOS4-*.root");
  ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_proc_run6/EPOS4-*.root");
  ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_proc_run7/EPOS4-*.root");
  ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_proc_run8/EPOS4-*.root");
  ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_proc_run9/EPOS4-*.root");
  ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_proc_run10/EPOS4-*.root");
  ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_proc_run11/EPOS4-*.root");	
  ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_proc_run12/EPOS4-*.root");
  ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_proc_run13/EPOS4-*.root");
  ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_proc_run14/EPOS4-*.root");
  ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_proc_run15/EPOS4-*.root");
  ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_proc_run16/EPOS4-*.root");
  ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_proc_run17/EPOS4-*.root");
  ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_proc_run18/EPOS4-*.root");
  ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_proc_run19/EPOS4-*.root");*/
  ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_proc_run20/EPOS4-*.root");
  ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_proc_run21/EPOS4-*.root");
  ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_proc_run22/EPOS4-*.root");
  ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_proc_run23/EPOS4-*.root");
													
  //ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_proc2_run21/EPOS4-*.root");
 												
  //ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_nocore_proc_run1/EPOS4-*.root");

  //ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_pfe_proc_run1/EPOS4-*.root");
 
  //ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_1fr_proc_run1/EPOS4-*.root");
  //ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_1fr_proc_run2/EPOS4-*.root");
  //ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_1fr_proc_run3/EPOS4-*.root");
  //ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_1fr_proc_run4/EPOS4-*.root");
  //ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_1fr_proc_run5/EPOS4-*.root");
  //ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_1fr_proc_run6/EPOS4-*.root");
  //ch_epos->Add("/home/etlar/stefano/epos4.0.0/rootfiles_run/data_1fr_proc_run7/EPOS4-*.root");

  ////////ch_epos->Add("/home/etlar/stefano/epos4.0.0/Data_EPOS4/EPOS4-*.root");

  // Just a trivial check.
  int tot_entries_epos = ch_epos->GetEntries();
  cout << "Total entries in the chain for EPOS4: " << tot_entries_epos << endl;

  // Start Proof Lite on cores.
  //TProof::Open("workers=20");
  //ch_epos->SetProof();
  //ch_epos->Process("Calibration.C+", "epos4");
  //ch_epos->Process("Analysis_minbias.C+", "epos4");
  //ch_epos->Process("Analysis_phi2.C+", "epos4");
  //ch_epos->Process("Analysis_phi2_new.C+", "epos4");
  //ch_epos->Process("Analysis_Xcore.C+");
  //ch_epos->Process("Analysis_Xcore2.C+");
  //ch_epos->Process("Analysis_phi_xi.C+", "epos4");

  //ch_epos->Process("Calibration_O2.C+", "epos4");
  //ch_epos->Process("Analysis_phi_O2.C+", "epos4");

  //ch_epos->Process("Calibration_Run3.C+", "epos4");
  //ch_epos->Process("Analysis_phi_Run3.C+", "epos4");

  return;

}
