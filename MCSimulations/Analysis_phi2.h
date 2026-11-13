//////////////////////////////////////////////////////////
// This class has been automatically generated on
// Mon Aug 14 18:01:54 2023 by ROOT version 6.26/10
// from TTree t/Pythia8 event tree
// found on file: pythia-00.root
//////////////////////////////////////////////////////////

#ifndef Analysis_phi2_h
#define Analysis_phi2_h

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


class Analysis_phi2 : public TSelector {
public :
   TTreeReader     fReader;  //!the tree reader
   TTree          *fChain = 0;   //!pointer to the analyzed TTree or TChain

   // Readers to access the data (delete the ones you do not need).
   //TTreeReaderValue<Double_t> weight = {fReader, "weight"};
   //TTreeReaderArray<Double_t> tracks_phi = {fReader, "tracks.phi"};
   TTreeReaderArray<Double_t> tracks_eta = {fReader, "tracks.eta"};
   TTreeReaderArray<Double_t> tracks_y = {fReader, "tracks.y"};
   TTreeReaderArray<Double_t> tracks_pT = {fReader, "tracks.pT"};
   TTreeReaderArray<Int_t> tracks_pid = {fReader, "tracks.pid"};
   TTreeReaderArray<Bool_t> tracks_charge = {fReader, "tracks.charge"};
   //TTreeReaderArray<Bool_t> tracks_had = {fReader, "tracks.had"};

   Bool_t pythia_def, pythia_new, epos4;

   static const int nMultClasses = 12;
   static const int nTrigClasses = 4;

   TH1I* h_nCh[nMultClasses+1];
   TH1I* h_nK0s[nTrigClasses][nMultClasses+1];
   TH1I* h_nXi[nTrigClasses][nMultClasses+1];
   TH1I* h_nOmega[nTrigClasses][nMultClasses+1];
   TH1I* h_nLambda[nTrigClasses][nMultClasses+1];
   TH1I* h_nProton[nTrigClasses][nMultClasses+1];
   TH1I* h_nPhi[nTrigClasses][nMultClasses+1];
   TH1I* h_nPion[nTrigClasses][nMultClasses+1];

   Analysis_phi2(TTree * /*tree*/ =0) { Reset(); }
   virtual ~Analysis_phi2() { }
   virtual Int_t   Version() const { return 2; }
   virtual void    Reset();
   virtual void    Begin(TTree *tree);
   virtual void    SlaveBegin(TTree *tree);
   virtual void    Init(TTree *tree);
   virtual Bool_t  Notify();
   virtual void    BookHisto();										
   virtual void    CountPart(unsigned int p, int num1, int num2, int num3, int num4, int num5, int num6, int num7);
   virtual void    FillHisto(int i, int j, int num1, int num2, int num3, int num4, int num5, int num6, int num7);
   virtual void    CollectHisto(TFile *file);
   virtual Bool_t  Process(Long64_t entry);
   virtual Int_t   GetEntry(Long64_t entry, Int_t getall = 0) { return fChain ? fChain->GetTree()->GetEntry(entry, getall) : 0; }
   virtual void    SetOption(const char *option) { fOption = option; }
   virtual void    SetObject(TObject *obj) { fObject = obj; }
   virtual void    SetInputList(TList *input) { fInput = input; }
   virtual TList  *GetOutputList() const { return fOutput; }
   virtual void    SlaveTerminate();
   virtual void    Terminate();

   ClassDef(Analysis_phi2,0);

};

#endif

#ifdef Analysis_phi2_cxx
void Analysis_phi2::Reset()
{
   // Reset the data members to their initial value.
   pythia_def = kFALSE;
   pythia_new = kFALSE;
   epos4 = kFALSE;

   for (int j = 0; j < nMultClasses+1; j++) {
     h_nCh[j] = 0;
     for (int i = 0; i < nTrigClasses; i++) {
       h_nK0s[i][j] = 0;
       h_nXi[i][j] = 0;
       h_nOmega[i][j] = 0;
       h_nLambda[i][j] = 0;
       h_nProton[i][j] = 0;
       h_nPhi[i][j] = 0;
       h_nPion[i][j] = 0;
     }
   }
}
void Analysis_phi2::Init(TTree *tree)
{
   // The Init() function is called when the selector needs to initialize
   // a new tree or chain. Typically here the reader is initialized.
   // It is normally not necessary to make changes to the generated
   // code, but the routine can be extended by the user if needed.
   // Init() will be called many times when running on PROOF
   // (once per file to be processed).

   fReader.SetTree(tree);
}

Bool_t Analysis_phi2::Notify()
{
   // The Notify() function is called when a new file is opened. This
   // can be either for a new TTree in a TChain or when when a new TTree
   // is started when using PROOF. It is normally not necessary to make changes
   // to the generated code, but the routine can be extended by the
   // user if needed. The return value is currently not used.

   return kTRUE;
}

void Analysis_phi2::BookHisto()
{
   char hname[40];

   for (int j=0; j<nMultClasses+1; j++) {
     
     h_nCh[j] = new TH1I(Form("h_nCh%i",j), "; N_{ch}; dN_{ev}/dN_{ch}", 500, -0.5, 499.5);
     fOutput->Add(h_nCh[j]);

     for (int i=0; i<nTrigClasses; i++) {
       sprintf(hname,"h_nK0s%i_%i",i+1,j);
       h_nK0s[i][j] = new TH1I(hname, "; N_{K_{S}^{0}}; dN_{ev}/dN_{K_{S}^{0}}", 500, -0.5, 499.5);
       fOutput->Add(h_nK0s[i][j]);

       sprintf(hname,"h_nXi%i_%i",i+1,j);
       h_nXi[i][j] = new TH1I(hname, "; N_{#Xi}; dN_{ev}/dN_{#Xi}", 500, -0.5, 499.5);
       fOutput->Add(h_nXi[i][j]);

       sprintf(hname,"h_nOmega%i_%i",i+1,j);
       h_nOmega[i][j] = new TH1I(hname, "; N_{#Omega}; dN_{ev}/dN_{#Omega}", 500, -0.5, 499.5);
       fOutput->Add(h_nOmega[i][j]);						

       sprintf(hname,"h_nLambda%i_%i",i+1,j);
       h_nLambda[i][j] = new TH1I(hname, "; N_{#Lambda}; dN_{ev}/dN_{#Lambda}", 500, -0.5, 499.5);
       fOutput->Add(h_nLambda[i][j]);

       sprintf(hname,"h_nProton%i_%i",i+1,j);
       h_nProton[i][j] = new TH1I(hname, "; N_{p}; dN_{ev}/dN_{p}", 500, -0.5, 499.5);
       fOutput->Add(h_nProton[i][j]);

       sprintf(hname,"h_nPhi%i_%i",i+1,j);
       h_nPhi[i][j] = new TH1I(hname, "; N_{#phi}; dN_{ev}/dN_{#phi}", 500, -0.5, 499.5);
       fOutput->Add(h_nPhi[i][j]);

       sprintf(hname,"h_nPion%i_%i",i+1,j);
       h_nPion[i][j] = new TH1I(hname, "; N_{#pi}; dN_{ev}/dN_{#pi}", 500, -0.5, 499.5);
       fOutput->Add(h_nPion[i][j]);
     }
   }
}

void Analysis_phi2::CountPart(unsigned int p, int num1, int num2, int num3, int num4, int num5, int num6, int num7)
{													
   if (abs(tracks_pid[p])==310) num1++;
   if (abs(tracks_pid[p])==3312) num2++;
   if (abs(tracks_pid[p])==3334) num3++;
   if (abs(tracks_pid[p])==3122) num4++;
   if (abs(tracks_pid[p])==2212) num5++;
   if (abs(tracks_pid[p])==333) num6++;
   if (abs(tracks_pid[p])==211) num7++;
}

void Analysis_phi2::FillHisto(int i, int j, int num1, int num2, int num3, int num4, int num5, int num6, int num7)
{
   h_nK0s[i][j]->Fill(num1);
   h_nXi[i][j]->Fill(num2);
   h_nOmega[i][j]->Fill(num3);
   h_nLambda[i][j]->Fill(num4);
   h_nProton[i][j]->Fill(num5);
   h_nPhi[i][j]->Fill(num6);
   h_nPion[i][j]->Fill(num7);
}	

void Analysis_phi2::CollectHisto(TFile *file)
{
   char hname[40];

   file->cd();

   for (int j=0; j<nMultClasses+1; j++) {

     h_nCh[j] = dynamic_cast<TH1I*>(fOutput->FindObject(Form("h_nCh%i",j)));
     h_nCh[j]->Write();

     for (int i=0; i<nTrigClasses; i++) {
       sprintf(hname,"h_nK0s%i_%i",i+1,j);
       h_nK0s[i][j] = dynamic_cast<TH1I*>(fOutput->FindObject(hname));
       h_nK0s[i][j]->Write();

       sprintf(hname,"h_nXi%i_%i",i+1,j);
       h_nXi[i][j] = dynamic_cast<TH1I*>(fOutput->FindObject(hname));
       h_nXi[i][j]->Write();

       sprintf(hname,"h_nOmega%i_%i",i+1,j);
       h_nOmega[i][j] = dynamic_cast<TH1I*>(fOutput->FindObject(hname));
       h_nOmega[i][j]->Write();

       sprintf(hname,"h_nLambda%i_%i",i+1,j);
       h_nLambda[i][j] = dynamic_cast<TH1I*>(fOutput->FindObject(hname));
       h_nLambda[i][j]->Write();

       sprintf(hname,"h_nProton%i_%i",i+1,j);
       h_nProton[i][j] = dynamic_cast<TH1I*>(fOutput->FindObject(hname));
       h_nProton[i][j]->Write();

       sprintf(hname,"h_nPhi%i_%i",i+1,j);
       h_nPhi[i][j] = dynamic_cast<TH1I*>(fOutput->FindObject(hname));
       h_nPhi[i][j]->Write();

       sprintf(hname,"h_nPion%i_%i",i+1,j);
       h_nPion[i][j] = dynamic_cast<TH1I*>(fOutput->FindObject(hname));
       h_nPion[i][j]->Write();
     }
   }
}

#endif // #ifdef Analysis_phi2_cxx
