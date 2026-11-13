//////////////////////////////////////////////////////////
// This class has been automatically generated on
// Mon Aug 14 18:01:54 2023 by ROOT version 6.26/10
// from TTree t/Pythia8 event tree
// found on file: pythia-00.root
//////////////////////////////////////////////////////////

#ifndef Calibration_Run3_h
#define Calibration_Run3_h

#include <TROOT.h>
#include <TH1.h>
#include <TChain.h>
#include <TFile.h>
#include <TSelector.h>
#include <TTreeReader.h>
#include <TTreeReaderValue.h>
#include <TTreeReaderArray.h>
#include <vector>

// Headers needed by this particular selector


class Calibration_Run3 : public TSelector {
public :
   TTreeReader     fReader;  //!the tree reader
   TTree          *fChain = 0;   //!pointer to the analyzed TTree or TChain

   // Readers to access the data (delete the ones you do not need).
   //TTreeReaderValue<Double_t> weight = {fReader, "weight"};
   //TTreeReaderArray<Double_t> tracks_phi = {fReader, "tracks.phi"};
   //TTreeReaderArray<Bool_t> tracks_had = {fReader, "tracks.had"};

   TTreeReaderArray<Double_t> tracks_eta = {fReader, "tracks.eta"};
   TTreeReaderArray<Double_t> tracks_y = {fReader, "tracks.y"};
   TTreeReaderArray<Double_t> tracks_pT = {fReader, "tracks.pT"};
   TTreeReaderArray<Int_t> tracks_pid = {fReader, "tracks.pid"};
   TTreeReaderArray<Bool_t> tracks_charge = {fReader, "tracks.charge"};
   

   Bool_t pythiaMonash, pythiaRopes, epos4;

   vector<Double_t> perc{1.0, 0.70, 0.50, 0.40, 0.30, 0.20, 0.15, 0.10, 0.05, 0.01, 0.0};
   vector<Double_t> xBin;

   TH1D* hEvSelection;
   TH1D* hNCharged;

   Calibration_Run3(TTree * /*tree*/ =0) { }
   virtual ~Calibration_Run3() { }
   virtual Int_t   Version() const { return 2; }
   virtual void    Reset();
   virtual void    Begin(TTree *tree);
   virtual void    SlaveBegin(TTree *tree);
   virtual void    Init(TTree *tree);
   virtual Bool_t  Notify();
   virtual Bool_t  Process(Long64_t entry);
   virtual Int_t   GetEntry(Long64_t entry, Int_t getall = 0) { return fChain ? fChain->GetTree()->GetEntry(entry, getall) : 0; }
   virtual void    SetOption(const char *option) { fOption = option; }
   virtual void    SetObject(TObject *obj) { fObject = obj; }
   virtual void    SetInputList(TList *input) { fInput = input; }
   virtual TList  *GetOutputList() const { return fOutput; }
   virtual void    SlaveTerminate();
   virtual void    Terminate();

   void BookHisto();
   
   Bool_t EventIsINELgt0();
   //Bool_t EventHasPhi();
   Bool_t AcceptEvent();

   ClassDef(Calibration_Run3,0);

};

#endif

#ifdef Calibration_Run3_cxx
void Calibration_Run3::Reset()
{
   // Reset the data members to their initial value.
   pythiaMonash = kFALSE;
   pythiaRopes = kFALSE;
   epos4 = kFALSE;

   hEvSelection = nullptr;
   hNCharged = nullptr;
}
void Calibration_Run3::Init(TTree *tree)
{
   // The Init() function is called when the selector needs to initialize
   // a new tree or chain. Typically here the reader is initialized.
   // It is normally not necessary to make changes to the generated
   // code, but the routine can be extended by the user if needed.
   // Init() will be called many times when running on PROOF
   // (once per file to be processed).

   fReader.SetTree(tree);
}

Bool_t Calibration_Run3::Notify()
{
   // The Notify() function is called when a new file is opened. This
   // can be either for a new TTree in a TChain or when when a new TTree
   // is started when using PROOF. It is normally not necessary to make changes
   // to the generated code, but the routine can be extended by the
   // user if needed. The return value is currently not used.

   return kTRUE;
}

void Calibration_Run3::BookHisto()
{
   //hEvSelection = new TH1D("hEvSelection", "; ; #it{N}_{ev}", 3, 0., 3.);
   hEvSelection = new TH1D("hEvSelection", "; ; #it{N}_{ev}", 2, 0., 2.);
   hEvSelection->GetXaxis()->SetBinLabel(1, "All events");
   hEvSelection->GetXaxis()->SetBinLabel(2, "INEL>0 events");
   //hEvSelection->GetXaxis()->SetBinLabel(3, "Events with a #phi");
   fOutput->Add(hEvSelection);

   hNCharged = new TH1D("hNCharged", "; #it{N}_{ch}; d#it{N}_{ev}/d#it{N}_{ch}", 500, 0., 500.);
   fOutput->Add(hNCharged);
}

Bool_t Calibration_Run3::EventIsINELgt0()
{
   for (size_t p = 0; p < tracks_eta.GetSize(); ++p) {
      if (tracks_charge[p] && abs(tracks_eta[p]) < 1.) return kTRUE;
   }
   return kFALSE;
}

/*Bool_t Calibration_O2::EventHasPhi()
{
   for (size_t p = 0; p < tracks_pid.GetSize(); ++p) {
      if (abs(tracks_y[p])<0.5 && tracks_pid[p] == 333) return kTRUE;
   }
   return kFALSE;
}*/

Bool_t Calibration_Run3::AcceptEvent()
{
   hEvSelection->Fill(0);
   if (!EventIsINELgt0()) return kFALSE;
   hEvSelection->Fill(1);
   //if (!EventHasPhi()) return kFALSE;
   //hEvSelection->Fill(2);
   return kTRUE;
}


#endif // #ifdef Calibration_Run3_cxx
