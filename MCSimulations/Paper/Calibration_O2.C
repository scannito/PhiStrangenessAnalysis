#define Calibration_O2_cxx
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
// root> T->Process("Calibration_O2.C")
// root> T->Process("Calibration_O2.C","some options")
// root> T->Process("Calibration_O2.C+")
//

#include <vector>
#include <Riostream.h>

#include <TStyle.h>
#include <TH1.h>
#include <TCanvas.h>

#include "Calibration_O2.h"


using namespace std;

void Calibration_O2::Begin(TTree * /*tree*/)
{
  // The Begin() function is called at the start of the query.
  // When running with PROOF Begin() is only called on the client.
  // The tree argument is deprecated (on PROOF 0 is passed).

  TString option = GetOption();
  Info("Begin", "Starting calibration with process option: %s", option.Data());

  if (option.Contains("pythiaMonash")) pythiaMonash = kTRUE;
  else if (option.Contains("pythiaRopes")) pythiaRopes = kTRUE;
  else if (option.Contains("epos4")) epos4 = kTRUE;
  else {};
}

void Calibration_O2::SlaveBegin(TTree *tree)
{
  // The SlaveBegin() function is called after the Begin() function.
  // When running with PROOF SlaveBegin() is called on each slave server.
  // The tree argument is deprecated (on PROOF 0 is passed).

  // Initialize the Tree branch adresses.
  Init(tree);

  // Create histograms
  BookHisto();
}

Bool_t Calibration_O2::Process(Long64_t entry)
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

  // Check the trigger condition
  if(!AcceptEvent()) return kTRUE;
    
  Double_t NCharged = 0.;

  // Count the number of charged particles	   
  for (size_t p=0; p<tracks_charge.GetSize(); ++p) {
    if (tracks_charge[p] && ((tracks_eta[p]>-3.7 && tracks_eta[p]<-1.7) || (tracks_eta[p]>2.8 && tracks_eta[p]<5.1))) ++NCharged;
  }
  hNCharged->Fill(NCharged);	       

  return kTRUE;
}

void Calibration_O2::SlaveTerminate()
{
  // The SlaveTerminate() function is called after all entries or objects
  // have been processed. When running with PROOF SlaveTerminate() is called
  // on each slave server.

}

void Calibration_O2::Terminate()
{
  // The Terminate() function is the last function to be called during
  // a query. It always runs on the client, it can be used to present
  // the results graphically or save the results to file.

  string filename1;
  if (pythiaMonash) filename1 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Calibration_O2/cal_pythiaMonash.root";
  else if (pythiaRopes) filename1 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Calibration_O2/cal_pythiaRopes.root";
  else if (epos4) filename1 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Calibration_O2/cal_epos4.root";
  else {};

  TFile* file1 = new TFile(filename1.c_str(), "RECREATE");										
  hEvSelection = dynamic_cast<TH1D*>(fOutput->FindObject("hEvSelection"));
  hEvSelection->Write();
  hNCharged = dynamic_cast<TH1D*>(fOutput->FindObject("hNCharged"));
  hNCharged->Write();
  file1->Close();

  Int_t nBin = hNCharged->GetNbinsX();
  Double_t sigma = hNCharged->Integral();
  hNCharged->Scale(1./sigma);

  for (size_t j=0; j<perc.size(); j++) {
    if (perc[j]==1.0) xBin.push_back(hNCharged->GetBinLowEdge(1));
    else if (perc[j]==0.0) {
      for (int i=1; i<=nBin; i++) {
        if (hNCharged->GetBinContent(i) == 0.0) {
          xBin.push_back(hNCharged->GetBinLowEdge(i));
          break;
        }
      }
    }
    else {
      Int_t bestBin = 1;       
      for (Int_t i=1; i<=nBin; i++) bestBin = (abs(perc[j]-hNCharged->Integral(i,nBin)) < abs(perc[j]-hNCharged->Integral(bestBin,nBin)) ? i : bestBin);
      xBin.push_back(hNCharged->GetBinLowEdge(bestBin));       
    }  
  }	   

  string filename2;
  if (pythiaMonash) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Calibration_O2/multclasses_pythiaMonash.txt";
  else if (pythiaRopes) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Calibration_O2/multclasses_pythiaRopes.txt";
  else if (epos4) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Calibration_O2/multclasses_epos4.txt";
  else {};
  ofstream out(filename2, ios::out);												
  for (size_t i=0; i<xBin.size(); i++) {
    out << xBin[i] << endl;
  }							
  out.close();    
}
