//////////////////////////////////////////////////////////
// This class has been automatically generated on
// Mon Aug 14 18:01:54 2023 by ROOT version 6.26/10
// from TTree t/Pythia8 event tree
// found on file: pythia-00.root
//////////////////////////////////////////////////////////

#ifndef Analysis_phi_Run3_h
#define Analysis_phi_Run3_h

#include <TROOT.h>
#include <TChain.h>
#include <TFile.h>
#include <TSelector.h>
#include <TTreeReader.h>
#include <TTreeReaderValue.h>
#include <TTreeReaderArray.h>
#include <TH1.h>
#include <vector>

// Headers needed by this particular selector


class Analysis_phi_Run3 : public TSelector {
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

   vector<Double_t> xBin;

   static const int NMultClasses = 10;
   static const int NDeltaYClasses = 4;

   TH1D* hNCh[NMultClasses+1];
   TH1D* hNK0s[NMultClasses+1];
   TH1D* hNPion[NMultClasses+1];

   /*TH1I* hNXi[NMultClasses+1];
   TH1I* hNOmega[NMultClasses+1];
   TH1I* hNLambda[NMultClasses+1];
   TH1I* hNProton[NMultClasses+1];
   TH1I* hNPhi[NMultClasses+1];*/

   Analysis_phi_Run3(TTree * /*tree*/ =0) { Reset(); }
   virtual ~Analysis_phi_Run3() { }
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
   void CollectHisto(TFile *file);

   Bool_t EventIsINELgt0();
   //Bool_t EventHasPhi();
   Bool_t AcceptEvent();

   ClassDef(Analysis_phi_Run3,0);

};

#endif

#ifdef Analysis_phi_Run3_cxx
void Analysis_phi_Run3::Reset()
{
   // Reset the data members to their initial value.
   pythiaMonash = kFALSE;
   pythiaRopes = kFALSE;
   epos4 = kFALSE;

   for (int j = 0; j < NMultClasses+1; j++) {
      hNCh[j] = nullptr;

      hNK0s[j] = nullptr;
      hNPion[j] = nullptr;

      /*for (int i = 0; i < nTrigClasses; i++) {
         hNXi[i][j] = 0;
         hNOmega[i][j] = 0;
         hNLambda[i][j] = 0;
         hNProton[i][j] = 0;
         hNPhi[i][j] = 0;
      }*/
   }
}
void Analysis_phi_Run3::Init(TTree *tree)
{
   // The Init() function is called when the selector needs to initialize
   // a new tree or chain. Typically here the reader is initialized.
   // It is normally not necessary to make changes to the generated
   // code, but the routine can be extended by the user if needed.
   // Init() will be called many times when running on PROOF
   // (once per file to be processed).

   fReader.SetTree(tree);
}

Bool_t Analysis_phi_Run3::Notify()
{
   // The Notify() function is called when a new file is opened. This
   // can be either for a new TTree in a TChain or when when a new TTree
   // is started when using PROOF. It is normally not necessary to make changes
   // to the generated code, but the routine can be extended by the
   // user if needed. The return value is currently not used.

   return kTRUE;
}

void Analysis_phi_Run3::BookHisto()
{
   char hname[40];

   for (int j=0; j<NMultClasses+1; j++) {
     
     hNCh[j] = new TH1D(Form("hNCh%i",j), "; N_{ch}; dN_{ev}/dN_{ch}", 500, -0.5, 499.5);
     fOutput->Add(hNCh[j]);

      sprintf(hname,"hNK0s%i",j);
      hNK0s[j] = new TH1D(hname, "; N_{K_{S}^{0}}; dN_{ev}/dN_{K_{S}^{0}}", 500, -0.5, 499.5);
      fOutput->Add(hNK0s[j]);

      sprintf(hname,"hNPion%i",j);
      hNPion[j] = new TH1D(hname, "; N_{#pi}; dN_{ev}/dN_{#pi}", 500, -0.5, 499.5);
      fOutput->Add(hNPion[j]);

      /*for (int i=0; i<nTrigClasses; i++) {
         sprintf(hname,"hNXi%i_%i",i+1,j);
         hNXi[i][j] = new TH1I(hname, "; N_{#Xi}; dN_{ev}/dN_{#Xi}", 500, -0.5, 499.5);
         fOutput->Add(hNXi[i][j]);

         sprintf(hname,"hNOmega%i_%i",i+1,j);
         hNOmega[i][j] = new TH1I(hname, "; N_{#Omega}; dN_{ev}/dN_{#Omega}", 500, -0.5, 499.5);
         fOutput->Add(hNOmega[i][j]);						

         sprintf(hname,"hNLambda%i_%i",i+1,j);
         hNLambda[i][j] = new TH1I(hname, "; N_{#Lambda}; dN_{ev}/dN_{#Lambda}", 500, -0.5, 499.5);
         fOutput->Add(hNLambda[i][j]);

         sprintf(hname,"hNProton%i_%i",i+1,j);
         hNProton[i][j] = new TH1I(hname, "; N_{p}; dN_{ev}/dN_{p}", 500, -0.5, 499.5);
         fOutput->Add(hNProton[i][j]);

         sprintf(hname,"hNPhi%i_%i",i+1,j);
         hNPhi[i][j] = new TH1I(hname, "; N_{#phi}; dN_{ev}/dN_{#phi}", 500, -0.5, 499.5);
         fOutput->Add(hNPhi[i][j]);
      }*/
   }
}

void Analysis_phi_Run3::CollectHisto(TFile *file)
{
   char hname[40];

   file->cd();

   for (int j=0; j<NMultClasses+1; j++) {

      hNCh[j] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("hNCh%i",j)));
      hNCh[j]->Write();

      sprintf(hname,"hNK0s%i",j);
      hNK0s[j] = dynamic_cast<TH1D*>(fOutput->FindObject(hname));
      hNK0s[j]->Write();

      sprintf(hname,"hNPion%i",j);
      hNPion[j] = dynamic_cast<TH1D*>(fOutput->FindObject(hname));
      hNPion[j]->Write();

      /*for (int i=0; i<nTrigClasses; i++) {

         sprintf(hname,"hNXi%i_%i",i+1,j);
         hNXi[i][j] = dynamic_cast<TH1I*>(fOutput->FindObject(hname));
         hNXi[i][j]->Write();

         sprintf(hname,"hNOmega%i_%i",i+1,j);
         hNOmega[i][j] = dynamic_cast<TH1I*>(fOutput->FindObject(hname));
         hNOmega[i][j]->Write();

         sprintf(hname,"hNLambda%i_%i",i+1,j);
         hNLambda[i][j] = dynamic_cast<TH1I*>(fOutput->FindObject(hname));
         hNLambda[i][j]->Write();

         sprintf(hname,"hNProton%i_%i",i+1,j);
         hNProton[i][j] = dynamic_cast<TH1I*>(fOutput->FindObject(hname));
         hNProton[i][j]->Write();

         sprintf(hname,"hNPhi%i_%i",i+1,j);
         hNPhi[i][j] = dynamic_cast<TH1I*>(fOutput->FindObject(hname));
         hNPhi[i][j]->Write();
      }*/
   }
}

Bool_t Analysis_phi_Run3::EventIsINELgt0()
{
   for (size_t p = 0; p < tracks_eta.GetSize(); ++p) {
      if (tracks_charge[p] && abs(tracks_eta[p]) < 1.) {
         return kTRUE;
      }
   }
   return kFALSE;
}

/*Bool_t Analysis_phi_Run3::EventHasPhi()
{
   for (size_t p = 0; p < tracks_pid.GetSize(); ++p) {
      if (abs(tracks_y[p])<0.5 && tracks_pid[p] == 333) {
         return kTRUE;
      }
   }
   return kFALSE;
}*/

Bool_t Analysis_phi_Run3::AcceptEvent()
{
   if (!EventIsINELgt0() /*|| !EventHasPhi()*/) return kFALSE;
   return kTRUE;
}

#endif // #ifdef Analysis_phi_Run3_cxx
