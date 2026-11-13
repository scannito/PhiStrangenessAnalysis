//////////////////////////////////////////////////////////
// This class has been automatically generated on
// Mon Aug 14 18:01:54 2023 by ROOT version 6.26/10
// from TTree t/Pythia8 event tree
// found on file: pythia-00.root
//////////////////////////////////////////////////////////

#ifndef Analysis_phi2_kappa_h
#define Analysis_phi2_kappa_h

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


class Analysis_phi2_kappa : public TSelector {
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
   TTreeReaderArray<Bool_t> tracks_had = {fReader, "tracks.had"};
   TTreeReaderArray<Double_t> tracks_kappa = {fReader, "tracks.kappa"};

   Bool_t pythia_new;

   static const int nMultClasses = 12;
   static const int nTrigClasses = 5;

   TH1I* h_nMBCh[nMultClasses+1];
   TH1I* h_nphiCh[nMultClasses+1];
   TH1I* h_PID[nTrigClasses];
   TH1D* h_kappa_hadron[nTrigClasses][nMultClasses+1];
   TH1D* h_kappa_K0s[nTrigClasses][nMultClasses+1];
   TH1D* h_kappa_Xi[nTrigClasses][nMultClasses+1];
   TH1D* h_kappa_Omega[nTrigClasses][nMultClasses+1];
   TH1D* h_kappa_Lambda[nTrigClasses][nMultClasses+1];
   TH1D* h_kappa_Proton[nTrigClasses][nMultClasses+1];
   TH1D* h_kappa_Phi[nTrigClasses][nMultClasses+1];
   TH1D* h_kappa_Pion[nTrigClasses][nMultClasses+1];

   Analysis_phi2_kappa(TTree * /*tree*/ =0) { Reset(); }
   virtual ~Analysis_phi2_kappa() { }
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

   ClassDef(Analysis_phi2_kappa,0);

};

#endif

#ifdef Analysis_phi2_kappa_cxx
void Analysis_phi2_kappa::Reset()
{
   // Reset the data members to their initial value.
   pythia_new = kFALSE;

   for (int j = 0; j < nMultClasses+1; j++) {
     h_nMBCh[j] = 0;
     h_nphiCh[j] = 0;
     for (int i = 0; i < nTrigClasses; i++) {
       if (j==0) h_PID[i] = 0;	     
       h_kappa_hadron[i][j] = 0;
       h_kappa_K0s[i][j] = 0;
       h_kappa_Xi[i][j] = 0;
       h_kappa_Omega[i][j] = 0;
       h_kappa_Lambda[i][j] = 0;
       h_kappa_Proton[i][j] = 0;
       h_kappa_Phi[i][j] = 0;
       h_kappa_Pion[i][j] = 0;
     }
   }
}
void Analysis_phi2_kappa::Init(TTree *tree)
{
   // The Init() function is called when the selector needs to initialize
   // a new tree or chain. Typically here the reader is initialized.
   // It is normally not necessary to make changes to the generated
   // code, but the routine can be extended by the user if needed.
   // Init() will be called many times when running on PROOF
   // (once per file to be processed).

   fReader.SetTree(tree);
}

Bool_t Analysis_phi2_kappa::Notify()
{
   // The Notify() function is called when a new file is opened. This
   // can be either for a new TTree in a TChain or when when a new TTree
   // is started when using PROOF. It is normally not necessary to make changes
   // to the generated code, but the routine can be extended by the
   // user if needed. The return value is currently not used.

   return kTRUE;
}

void Analysis_phi2_kappa::BookHisto()
{
   char hname[40];

   for (int j=0; j<nMultClasses+1; j++) {
     
     h_nMBCh[j] = new TH1I(Form("h_nMBCh%i",j), "; N_{ch}; dN_{ev}/dN_{ch}", 500, -0.5, 499.5);
     fOutput->Add(h_nMBCh[j]);

     h_nphiCh[j] = new TH1I(Form("h_nphiCh%i",j), "; N_{ch}; dN_{ev}/dN_{ch}", 500, -0.5, 499.5);
     fOutput->Add(h_nphiCh[j]);

     for (int i=0; i<nTrigClasses; i++) {
       if (j==0) {
	 sprintf(hname,"h_PID%i",i);      
         h_PID[i] = new TH1I(hname,";;",10000,-0.5,9999.5);
         fOutput->Add(h_PID[i]);
       }

       sprintf(hname,"h_kappa_hadron%i_%i",i,j);
       h_kappa_hadron[i][j] = new TH1D(hname, "; k_{eff}/k; dN_{h}/dk_{eff}/k", 100, 0.0, 20.0);
       fOutput->Add(h_kappa_hadron[i][j]);

       sprintf(hname,"h_kappa_K0s%i_%i",i,j);
       h_kappa_K0s[i][j] = new TH1D(hname, "; k_{eff}/k; dN_{K_{S}^{0}}/dk_{eff}/k", 100, 0.0, 20.0);
       fOutput->Add(h_kappa_K0s[i][j]);

       sprintf(hname,"h_kappa_Xi%i_%i",i,j);
       h_kappa_Xi[i][j] = new TH1D(hname, "; k_{eff}/k; dN_{#Xi}/dk_{eff}/k", 100, 0.0, 20.0);
       fOutput->Add(h_kappa_Xi[i][j]);

       sprintf(hname,"h_kappa_Omega%i_%i",i,j);
       h_kappa_Omega[i][j] = new TH1D(hname, "; k_{eff}/k; dN_{#Omega}/dk_{eff}/k", 100, 0.0, 20.0);
       fOutput->Add(h_kappa_Omega[i][j]);						

       sprintf(hname,"h_kappa_Lambda%i_%i",i,j);
       h_kappa_Lambda[i][j] = new TH1D(hname, "; k_{eff}/k; dN_{#Lambda}/dk_{eff}/k", 100, 0.0, 20.0);
       fOutput->Add(h_kappa_Lambda[i][j]);

       sprintf(hname,"h_kappa_Proton%i_%i",i,j);
       h_kappa_Proton[i][j] = new TH1D(hname, "; k_{eff}/k; dN_{p}/dk_{eff}/k", 100, 0.0, 20.0);
       fOutput->Add(h_kappa_Proton[i][j]);

       sprintf(hname,"h_kappa_Phi%i_%i",i,j);
       h_kappa_Phi[i][j] = new TH1D(hname, "; k_{eff}/k; dN_{#phi}/dk_{eff}/k", 100, 0.0, 20.0);
       fOutput->Add(h_kappa_Phi[i][j]);

       sprintf(hname,"h_kappa_Pion%i_%i",i,j);
       h_kappa_Pion[i][j] = new TH1D(hname, "; k_{eff}/k; dN_{#pi}/dk_{eff}/k", 100, 0.0, 20.0);
       fOutput->Add(h_kappa_Pion[i][j]);
     }
   }
}

void Analysis_phi2_kappa::CountPart(unsigned int p, int num1, int num2, int num3, int num4, int num5, int num6, int num7)
{													
   if (abs(tracks_pid[p])==310) num1++;
   if (abs(tracks_pid[p])==3312) num2++;
   if (abs(tracks_pid[p])==3334) num3++;
   if (abs(tracks_pid[p])==3122) num4++;
   if (abs(tracks_pid[p])==2212) num5++;
   if (abs(tracks_pid[p])==333) num6++;
   if (abs(tracks_pid[p])==211) num7++;
}

void Analysis_phi2_kappa::FillHisto(int i, int j, int num1, int num2, int num3, int num4, int num5, int num6, int num7)
{
   h_kappa_K0s[i][j]->Fill(num1);
   h_kappa_Xi[i][j]->Fill(num2);
   h_kappa_Omega[i][j]->Fill(num3);
   h_kappa_Lambda[i][j]->Fill(num4);
   h_kappa_Proton[i][j]->Fill(num5);
   h_kappa_Phi[i][j]->Fill(num6);
   h_kappa_Pion[i][j]->Fill(num7);
}	

void Analysis_phi2_kappa::CollectHisto(TFile *file)
{
   char hname[40];

   file->cd();

   for (int j=0; j<nMultClasses+1; j++) {

     h_nMBCh[j] = dynamic_cast<TH1I*>(fOutput->FindObject(Form("h_nMBCh%i",j)));
     h_nMBCh[j]->Write();

     h_nphiCh[j] = dynamic_cast<TH1I*>(fOutput->FindObject(Form("h_nphiCh%i",j)));
     h_nphiCh[j]->Write();

     for (int i=0; i<nTrigClasses; i++) {
       if (j==0) {
         sprintf(hname,"h_PID%i",i);      
         h_PID[i] = dynamic_cast<TH1I*>(fOutput->FindObject(hname));
         h_PID[i]->Write();
       }

       sprintf(hname,"h_kappa_hadron%i_%i",i,j);
       h_kappa_hadron[i][j] = dynamic_cast<TH1D*>(fOutput->FindObject(hname));
       h_kappa_hadron[i][j]->Write();

       sprintf(hname,"h_kappa_K0s%i_%i",i,j);
       h_kappa_K0s[i][j] = dynamic_cast<TH1D*>(fOutput->FindObject(hname));
       h_kappa_K0s[i][j]->Write();

       sprintf(hname,"h_kappa_Xi%i_%i",i,j);
       h_kappa_Xi[i][j] = dynamic_cast<TH1D*>(fOutput->FindObject(hname));
       h_kappa_Xi[i][j]->Write();

       sprintf(hname,"h_kappa_Omega%i_%i",i,j);
       h_kappa_Omega[i][j] = dynamic_cast<TH1D*>(fOutput->FindObject(hname));
       h_kappa_Omega[i][j]->Write();

       sprintf(hname,"h_kappa_Lambda%i_%i",i,j);
       h_kappa_Lambda[i][j] = dynamic_cast<TH1D*>(fOutput->FindObject(hname));
       h_kappa_Lambda[i][j]->Write();

       sprintf(hname,"h_kappa_Proton%i_%i",i,j);
       h_kappa_Proton[i][j] = dynamic_cast<TH1D*>(fOutput->FindObject(hname));
       h_kappa_Proton[i][j]->Write();

       sprintf(hname,"h_kappa_Phi%i_%i",i,j);
       h_kappa_Phi[i][j] = dynamic_cast<TH1D*>(fOutput->FindObject(hname));
       h_kappa_Phi[i][j]->Write();

       sprintf(hname,"h_kappa_Pion%i_%i",i,j);
       h_kappa_Pion[i][j] = dynamic_cast<TH1D*>(fOutput->FindObject(hname));
       h_kappa_Pion[i][j]->Write();
     }
   }
}

#endif // #ifdef Analysis_phi2_kappa_cxx
