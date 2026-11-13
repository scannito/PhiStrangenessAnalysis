#define Analysis_phi_O2_cxx
// The class definition in Selector.h has been generated automatically
// by the ROOT utility TTree::MakeSelector(). This class is derived
// from the ROOT class TSelector. For more information on the TSelector
// framework see $ROOTSYS/README/README.SELECTOR or the ROOT User Manual.

// The following methods are defined in this file:
//    Begin():        called every time a loop on the tree starts,
//                    a convenient place to create your histograms.
//    SlaveBegin():   called after Begin(), when on PROOF called only on the
//                    slave servers.
//    Process():      called for each event, in this function you decide what
//                    to read and fill your histograms.
//    SlaveTerminate: called at the end of the loop on the tree, when on PROOF
//                    called only on the slave servers.
//    Terminate():    called at the end of the loop on the tree,
//                    a convenient place to draw/fit your histograms.
//
// To use this file, try the following session on your Tree T:
//
// root> T->Process("Analysis_phi_O2.C")
// root> T->Process("Analysis_phi_O2.C","some options")
// root> T->Process("Analysis_phi_O2.C+")
//

#include <vector>
#include <array>
#include <Riostream.h>

#include <TStyle.h>
#include <TH2.h>
#include <TH3.h>
#include <TCanvas.h>

#include "Analysis_phi_O2.h"

using namespace std;

void Analysis_phi_O2::Begin(TTree * /*tree*/)
{
  // The Begin() function is called at the start of the query.
  // When running with PROOF Begin() is only called on the client.
  // The tree argument is deprecated (on PROOF 0 is passed).

  TString option = GetOption();
  Info("Begin", "starting analysis with process option: %s", option.Data());

  if (option.Contains("pythiaMonash")) {
    pythiaMonash = kTRUE;
    xBin = {0., 33., 49., 57., 66., 76., 82., 90., 101., 114., 123., 129., 188.};
  }
  else if (option.Contains("pythiaRopes")) {
    pythiaRopes = kTRUE;
    xBin = {0., 34., 48., 56., 64., 74., 80., 87., 97., 109., 117., 123., 182.};
    //xBin = {-0.5, 16.5, 24.5, 30.5, 38.5, 49.5, 56.5, 65.5, 78.5, 92.5, 101.5, 108.5, 185.5};
  }
  else if (option.Contains("epos4")) {
    epos4 = kTRUE;
    xBin = {0., 39., 50., 56., 62., 70., 75., 82., 94., 108., 118., 126., 186.};
    //xBin = {-0.5, 19.5, 29.5, 36.5, 43.5, 51.5, 56.5, 63.5, 74.5, 86.5, 97.5, 105.5, 190.5};
  }
  else {};
}

void Analysis_phi_O2::SlaveBegin(TTree *tree)
{
  // The SlaveBegin() function is called after the Begin() function.
  // When running with PROOF SlaveBegin() is called on each slave server.
  // The tree argument is deprecated (on PROOF 0 is passed).

  // Initialize the Tree branch adresses.
  Init(tree);    		

  TString option = GetOption();

  BookHisto();
}

Bool_t Analysis_phi_O2::Process(Long64_t entry)
{
  // The Process() function is called for each entry in the tree (or possibly
  // keyed object in the case of PROOF) to be processed. The entry argument
  // specifies which entry in the currently loaded tree is to be processed.
  // When processing keyed objects with PROOF, the object is already loaded
  // and is available via the fObject pointer.
  //
  // This function should contain the \"body\" of the analysis. It can contain
  // simple or elaborate selection criteria, run algorithms on the data
  // of the event and typically fill histograms.
  //
  // The processing can be stopped by calling Abort().
  //
  // Use fStatus to set the return value of TTree::Process().
  //
  // The return value is currently not used.

  fReader.SetLocalEntry(entry);

  if (!AcceptEvent()) return kTRUE;

  Double_t NChargedTrig = 0, NCharged = 0;
                                          
  // Count the number of charged particles and phi mesons
  vector<size_t> Phitag;
  for (size_t p=0; p<tracks_charge.GetSize(); ++p) {
    if (tracks_charge[p] && ((tracks_eta[p]>-3.7 && tracks_eta[p]<-1.7) || (tracks_eta[p]>2.8 && tracks_eta[p]<5.1))) ++NChargedTrig;
    if (abs(tracks_y[p])<=2.5 && tracks_pid[p] == 333) Phitag.push_back(p);
  }
  hNTriggerPart->Fill(NChargedTrig, Phitag.size());

  // Charged particles, strange hadrons and pions per class 
  for (size_t p=0; p<tracks_charge.GetSize(); ++p) {
    if (tracks_charge[p] && abs(tracks_eta[p])<0.5) ++NCharged;
    if (abs(tracks_y[p])>2.5) continue;
    ParticleType::Type part = ParticleType::FromPDG(tracks_pid[p]);
    array<Bool_t, NDeltaYClasses> isPhiCounted = {kFALSE, kFALSE};
    for (size_t tag : Phitag) {
      if (p==tag) continue;
      if (!isPhiCounted.at(0)) {
        hNPart->Fill(NChargedTrig, 0., part);
        /*if (abs(tracks_pid[p])==310) hNK0S->Fill(NChargedTrig, 0);
        if (abs(tracks_pid[p])==3312) hNXi->Fill(NChargedTrig, 0);
        if (abs(tracks_pid[p])==3334) hNOmega->Fill(NChargedTrig, 0);
        if (abs(tracks_pid[p])==3122) hNLambda->Fill(NChargedTrig, 0);
        if (abs(tracks_pid[p])==2212) hNProton->Fill(NChargedTrig, 0);
        if (abs(tracks_pid[p])==333) hNPhi->Fill(NChargedTrig, 0);
        if (abs(tracks_pid[p])==211) hNPion->Fill(NChargedTrig, 0);*/
        isPhiCounted.at(0) = kTRUE;
      }
      hNPairs->Fill(NChargedTrig, 0., part);
      if (abs(tracks_y[p]-tracks_y[tag])<0.1) continue;
      if (!isPhiCounted.at(1)) {
        hNPart->Fill(NChargedTrig, 1., part);
        isPhiCounted.at(1) = kTRUE;
      }
      hNPairs->Fill(NChargedTrig, 1., part);
    }
  }
  hNCharged->Fill(NChargedTrig, NCharged);     

  return kTRUE;
}

void Analysis_phi_O2::SlaveTerminate()
{
  // The SlaveTerminate() function is called after all entries or objects
  // have been processed. When running with PROOF SlaveTerminate() is called
  // on each slave server.

}

void Analysis_phi_O2::Terminate()
{
  // The Terminate() function is the last function to be called during
  // a query. It always runs on the client, it can be used to present
  // the results graphically or save the results to file.
                                
  string filename2;
  if (pythiaMonash) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Analysis_phi_O2/analysis_pythiaMonash.root";
  else if (pythiaRopes) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Analysis_phi_O2/analysis_pythiaRopes_y25.root";
  else if (epos4) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Analysis_phi_O2/analysis_epos4_y25.root";
  else {};
  TFile* file2 = new TFile(filename2.c_str(), "RECREATE");							
  CollectHisto(file2);
  file2->Close();
}
