#define Analysis_phi_Run3_cxx
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
// root> T->Process("Analysis_phi_Run3.C")
// root> T->Process("Analysis_phi_Run3.C","some options")
// root> T->Process("Analysis_phi_Run3.C+")
//


#include "Analysis_phi_Run3.h"
#include <TStyle.h>
#include <TH1.h>
#include <TCanvas.h>
#include <Riostream.h>
#include <vector>

using namespace std;

void Analysis_phi_Run3::Begin(TTree * /*tree*/)
{
  // The Begin() function is called at the start of the query.
  // When running with PROOF Begin() is only called on the client.
  // The tree argument is deprecated (on PROOF 0 is passed).

  Reset();

  TString option = GetOption();
  Info("Begin", "starting analysis with process option: %s", option.Data());

  if (option.Contains("pythiaMonash")) pythiaMonash = kTRUE;
  if (option.Contains("pythiaRopes")) {
    pythiaRopes = kTRUE;
    xBin = {0, 9, 14, 17, 22, 29, 33, 39, 47, 62, 124}; // Pythia CR+Ropes at 13.6 TeV
  }
  if (option.Contains("epos4")) {
    epos4 = kTRUE;
    xBin = {0, 11, 17, 21, 25, 31, 34, 38, 45, 59, 114}; // EPOS4 at 13.6 TeV
  }
}

void Analysis_phi_Run3::SlaveBegin(TTree *tree)
{
  // The SlaveBegin() function is called after the Begin() function.
  // When running with PROOF SlaveBegin() is called on each slave server.
  // The tree argument is deprecated (on PROOF 0 is passed).

  // Initialize the Tree branch adresses.
  Init(tree);    		

  TString option = GetOption();

  BookHisto();
}

Bool_t Analysis_phi_Run3::Process(Long64_t entry)
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

  double NCharged = 0;
  int nCh[NMultClasses+1] = {};
  int nK0s[NMultClasses+1] = {}, nXi[NMultClasses+1] = {}, nOmega[NMultClasses+1] = {}, nLambda[NMultClasses+1] = {};
  int nProton[NMultClasses+1] = {}, nPhi[NMultClasses+1] = {}, nPion[NMultClasses+1] = {};

  // Count the number of charged particles and phis 
  vector<size_t> Phitag;
  for (size_t p=0; p<tracks_charge.GetSize(); ++p) {
    if (tracks_charge[p] && ((tracks_eta[p]>-3.3 && tracks_eta[p]<-2.1) || (tracks_eta[p]>3.5 && tracks_eta[p]<4.9))) ++NCharged;
    if (abs(tracks_y[p])<0.5 && tracks_pid[p] == 333 && tracks_pT[p]>0.4 && tracks_pT[p]<10) Phitag.push_back(p);
  }
                                      
  if (Phitag.size()>0) {
    // Charged particles, strange hadrons and pions over the entire multiplicity distribution, without classes	     
    for (size_t p=0; p<tracks_charge.GetSize(); ++p) {
      if (tracks_charge[p] && abs(tracks_eta[p])<0.5) nCh[0]++;
      if (abs(tracks_y[p])<0.5) {
        if (abs(tracks_pid[p])==310 && /*tracks_pT[p]>0.1 &&*/ tracks_pT[p]<6) nK0s[0]++;
        if (abs(tracks_pid[p])==211 && /*tracks_pT[p]>0.2 &&*/ tracks_pT[p]<3) nPion[0]++;
        /*if (abs(tracks_pid[p])==3312) nXi[3][0]++;
        if (abs(tracks_pid[p])==3334) nOmega[3][0]++;
        if (abs(tracks_pid[p])==3122) nLambda[3][0]++;
        if (abs(tracks_pid[p])==2212) nProton[3][0]++;
        if (abs(tracks_pid[p])==333) nPhi[3][0]++;*/
      }
    } 
    hNCh[0]->Fill(nCh[0]);
    hNK0s[0]->Fill(nK0s[0]);
    hNPion[0]->Fill(nPion[0]);
    /*for (int i = 0; i < nTrigClasses; i++) {
      hNXi[i][0]->Fill(nXi[i][0]);
      hNOmega[i][0]->Fill(nOmega[i][0]);
      hNLambda[i][0]->Fill(nLambda[i][0]);
      hNProton[i][0]->Fill(nProton[i][0]);
      hNPhi[i][0]->Fill(nPhi[i][0]);
    }*/

    // Charged particles, strange hadrons and pions per class 
    for (int j=1; j<NMultClasses+1; j++) {
      // Check what the multiplicty class of the event is
      if (NCharged>=xBin[j-1] && NCharged<=xBin[j]) {
        for (size_t p=0; p<tracks_charge.GetSize(); ++p) {
          if (tracks_charge[p] && abs(tracks_eta[p])<0.5) nCh[j]++;
          if (abs(tracks_y[p])<0.5) {
            if (abs(tracks_pid[p])==310 && /*tracks_pT[p]>0.1 &&*/ tracks_pT[p]<6) nK0s[j]++;
            if (abs(tracks_pid[p])==211 && /*tracks_pT[p]>0.2 &&*/ tracks_pT[p]<3) nPion[j]++;
            /*if (abs(tracks_pid[p])==3312) nXi[3][j]++;
            if (abs(tracks_pid[p])==3334) nOmega[3][j]++;
            if (abs(tracks_pid[p])==3122) nLambda[3][j]++;
            if (abs(tracks_pid[p])==2212) nProton[3][j]++;
            if (abs(tracks_pid[p])==333) nPhi[3][j]++;*/
          }
        }
        hNCh[j]->Fill(nCh[j]);
        hNK0s[j]->Fill(nK0s[j]);
        hNPion[j]->Fill(nPion[j]);
        /*for (int i = 0; i < nTrigClasses; i++) {
          hNXi[i][j]->Fill(nXi[i][j]);
          hNOmega[i][j]->Fill(nOmega[i][j]);
          hNLambda[i][j]->Fill(nLambda[i][j]);
          hNProton[i][j]->Fill(nProton[i][j]);
          hNPhi[i][j]->Fill(nPhi[i][j]);
        }*/
        break;
      }
    }	   
  }     

  return kTRUE;
}

void Analysis_phi_Run3::SlaveTerminate()
{
  // The SlaveTerminate() function is called after all entries or objects
  // have been processed. When running with PROOF SlaveTerminate() is called
  // on each slave server.

}

void Analysis_phi_Run3::Terminate()
{
  // The Terminate() function is the last function to be called during
  // a query. It always runs on the client, it can be used to present
  // the results graphically or save the results to file.
                                
  string filename2;
  if (pythiaMonash) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Analysis_phi_Run3/analysis_phi_pythiaMonash.root";
  else if (pythiaRopes) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Analysis_phi_Run3/analysis_phi_pythiaRopes_cutpT.root";
  else if (epos4) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Analysis_phi_Run3/analysis_phi_epos4.root";

  TFile* file2 = new TFile(filename2.c_str(), "RECREATE");							
  CollectHisto(file2);
  file2->Close();
}
