//////////////////////////////////////////////////////////
// This class has been automatically generated on
// Mon Aug 14 18:01:54 2023 by ROOT version 6.26/10
// from TTree t/Pythia8 event tree
// found on file: pythia-00.root
//////////////////////////////////////////////////////////

#ifndef Analysis_Xcore_h
#define Analysis_Xcore_h

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


class Analysis_Xcore : public TSelector {
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
   TTreeReaderArray<Int_t> tracks_pity = {fReader, "tracks.pity"};
   TTreeReaderArray<Bool_t> tracks_charge = {fReader, "tracks.charge"};
   //TTreeReaderArray<Bool_t> tracks_had = {fReader, "tracks.had"};

   static const int nMultClasses = 11;
   static const int nTrigClasses = 30;

   TH1I* h_Evminbias[nMultClasses];
   TH1D* h_dndetaminbias[nMultClasses];

   TH1I* h_Evphi[nMultClasses];
   TH1D* h_dndetaphi[nMultClasses];

   TH1D* h_dndyK0s[nTrigClasses][nMultClasses];
   TH1D* h_dndyXi[nTrigClasses][nMultClasses];
   TH1D* h_dndyOmega[nTrigClasses][nMultClasses];
   TH1D* h_dndyLambda[nTrigClasses][nMultClasses];
   TH1D* h_dndyProton[nTrigClasses][nMultClasses];
   TH1D* h_dndyPhi[nTrigClasses][nMultClasses];
   TH1D* h_dndyPion[nTrigClasses][nMultClasses];

   Analysis_Xcore(TTree * /*tree*/ =0) { Reset(); }
   virtual ~Analysis_Xcore() { }
   virtual Int_t   Version() const { return 2; }
   virtual void    Reset();
   virtual void    Begin(TTree *tree);
   virtual void    SlaveBegin(TTree *tree);
   virtual void    Init(TTree *tree);
   virtual Bool_t  Notify();
   virtual void    BookHisto();
   virtual void    FillHisto(int i, int j, unsigned int p);
   virtual void    CollectHisto(TFile *file);
   virtual Bool_t  Process(Long64_t entry);
   virtual Int_t   GetEntry(Long64_t entry, Int_t getall = 0) { return fChain ? fChain->GetTree()->GetEntry(entry, getall) : 0; }
   virtual void    SetOption(const char *option) { fOption = option; }
   virtual void    SetObject(TObject *obj) { fObject = obj; }
   virtual void    SetInputList(TList *input) { fInput = input; }
   virtual TList  *GetOutputList() const { return fOutput; }
   virtual void    SlaveTerminate();
   virtual void    Terminate();

   ClassDef(Analysis_Xcore,0);

};

#endif

#ifdef Analysis_Xcore_cxx
void Analysis_Xcore::Reset()
{
   // Reset the data members to their initial value.

   for (int j = 0; j < nMultClasses; j++) {  
     h_Evminbias[j] = 0;
     h_dndetaminbias[j] = 0;
     
     h_Evphi[j] = 0;
     h_dndetaphi[j] = 0;	   
     
     for (int i = 0; i < nTrigClasses; i++) {
       h_dndyK0s[i][j] = 0;
       h_dndyXi[i][j] = 0;
       h_dndyOmega[i][j] = 0;
       h_dndyLambda[i][j] = 0;
       h_dndyProton[i][j] = 0;
       h_dndyPhi[i][j] = 0;
       h_dndyPion[i][j] = 0;
     }
   }  
}  
void Analysis_Xcore::Init(TTree *tree)
{
   // The Init() function is called when the selector needs to initialize
   // a new tree or chain. Typically here the reader is initialized.
   // It is normally not necessary to make changes to the generated
   // code, but the routine can be extended by the user if needed.
   // Init() will be called many times when running on PROOF
   // (once per file to be processed).

   fReader.SetTree(tree);
}

Bool_t Analysis_Xcore::Notify()
{
   // The Notify() function is called when a new file is opened. This
   // can be either for a new TTree in a TChain or when when a new TTree
   // is started when using PROOF. It is normally not necessary to make changes
   // to the generated code, but the routine can be extended by the
   // user if needed. The return value is currently not used.

   return kTRUE;
}

void Analysis_Xcore::BookHisto()
{  
   char hname[40];

   for (int j=0; j<nMultClasses; j++) {

     h_Evminbias[j] = new TH1I(Form("h_Evminbias%i",j+1), "", 2, 0.0, 2.0);
     fOutput->Add(h_Evminbias[j]);

     h_dndetaminbias[j] = new TH1D(Form("h_dndetaminbias%i",j+1), "; #eta; dN_{ch}/d#eta", 100, -0.5, 0.5);
     fOutput->Add(h_dndetaminbias[j]);

     h_Evphi[j] = new TH1I(Form("h_Evphi%i",j+1), "", 2, 0.0, 2.0);
     fOutput->Add(h_Evphi[j]);

     h_dndetaphi[j] = new TH1D(Form("h_dndetaphi%i",j+1), "; #eta; dN_{ch}/d#eta", 100, -0.5, 0.5);
     fOutput->Add(h_dndetaphi[j]);

     for (int i=0; i<nTrigClasses; i++) {	
       sprintf(hname,"h_dndyK0s%i_%i",i+1,j+1);
       h_dndyK0s[i][j] = new TH1D(hname, "; y; dN_{K_{S}^{0}}/dy", 100, -0.5, 0.5);
       fOutput->Add(h_dndyK0s[i][j]);

       sprintf(hname,"h_dndyXi%i_%i",i+1,j+1);
       h_dndyXi[i][j] = new TH1D(hname, "; y; dN_{#Xi}/dy", 100, -0.5, 0.5);
       fOutput->Add(h_dndyXi[i][j]);

       sprintf(hname,"h_dndyOmega%i_%i",i+1,j+1);
       h_dndyOmega[i][j] = new TH1D(hname, "; y; dN_{#Omega}/dy", 100, -0.5, 0.5);
       fOutput->Add(h_dndyOmega[i][j]);

       sprintf(hname,"h_dndyLambda%i_%i",i+1,j+1);
       h_dndyLambda[i][j] = new TH1D(hname, "; y; dN_{#Lambda}/dy", 100, -0.5, 0.5);
       fOutput->Add(h_dndyLambda[i][j]);

       sprintf(hname,"h_dndyProton%i_%i",i+1,j+1);
       h_dndyProton[i][j] = new TH1D(hname, "; y; dN_{p}/dy", 100, -0.5, 0.5);
       fOutput->Add(h_dndyProton[i][j]);

       sprintf(hname,"h_dndyPhi%i_%i",i+1,j+1);
       h_dndyPhi[i][j] = new TH1D(hname, "; y; dN_{#phi}/dy", 100, -0.5, 0.5);
       fOutput->Add(h_dndyPhi[i][j]);

       sprintf(hname,"h_dndyPion%i_%i",i+1,j+1);
       h_dndyPion[i][j] = new TH1D(hname, "; y; dN_{#pi}/dy", 100, -0.5, 0.5);
       fOutput->Add(h_dndyPion[i][j]);
     }										
   }       
}

void Analysis_Xcore::FillHisto(int i, int j, unsigned int p)
{
   if (abs(tracks_pid[p])==310) h_dndyK0s[i][j]->Fill(tracks_y[p]);
   if (abs(tracks_pid[p])==3312) h_dndyXi[i][j]->Fill(tracks_y[p]);
   if (abs(tracks_pid[p])==3334) h_dndyOmega[i][j]->Fill(tracks_y[p]);
   if (abs(tracks_pid[p])==3122) h_dndyLambda[i][j]->Fill(tracks_y[p]);
   if (abs(tracks_pid[p])==2212) h_dndyProton[i][j]->Fill(tracks_y[p]);
   if (abs(tracks_pid[p])==333) h_dndyPhi[i][j]->Fill(tracks_y[p]);
   if (abs(tracks_pid[p])==211) h_dndyPion[i][j]->Fill(tracks_y[p]);   
} 

void Analysis_Xcore::CollectHisto(TFile *file)
{
   char hname[40];

   file->cd();
	
   for (int j=0; j<nMultClasses; j++) {
     h_Evminbias[j] = dynamic_cast<TH1I*>(fOutput->FindObject(Form("h_Evminbias%i",j+1)));

     h_dndetaminbias[j] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndetaminbias%i",j+1)));
     h_dndetaminbias[j]->Scale(100./h_Evminbias[j]->GetBinContent(2));
     h_dndetaminbias[j]->Write();

     h_Evphi[j] = dynamic_cast<TH1I*>(fOutput->FindObject(Form("h_Evphi%i",j+1)));

     h_dndetaphi[j] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndetaphi%i",j+1)));
     h_dndetaphi[j]->Scale(100./h_Evphi[j]->GetBinContent(2));
     h_dndetaphi[j]->Write();

     for (int i=0; i<nTrigClasses; i++) {
       sprintf(hname,"h_dndyK0s%i_%i",i+1,j+1);	     
       h_dndyK0s[i][j] = dynamic_cast<TH1D*>(fOutput->FindObject(hname));
       if (i<=5) h_dndyK0s[i][j]->Scale(100./h_Evminbias[j]->GetBinContent(2));
       else h_dndyK0s[i][j]->Scale(100./h_Evphi[j]->GetBinContent(2));
       h_dndyK0s[i][j]->Write();       

       sprintf(hname,"h_dndyXi%i_%i",i+1,j+1);
       h_dndyXi[i][j] = dynamic_cast<TH1D*>(fOutput->FindObject(hname));
       if (i<=5) h_dndyXi[i][j]->Scale(100./h_Evminbias[j]->GetBinContent(2));
       else h_dndyXi[i][j]->Scale(100./h_Evphi[j]->GetBinContent(2));
       h_dndyXi[i][j]->Write();

       sprintf(hname,"h_dndyOmega%i_%i",i+1,j+1);
       h_dndyOmega[i][j] = dynamic_cast<TH1D*>(fOutput->FindObject(hname));
       if (i<=5) h_dndyOmega[i][j]->Scale(100./h_Evminbias[j]->GetBinContent(2));
       else h_dndyOmega[i][j]->Scale(100./h_Evphi[j]->GetBinContent(2));
       h_dndyOmega[i][j]->Write();

       sprintf(hname,"h_dndyLambda%i_%i",i+1,j+1);
       h_dndyLambda[i][j] = dynamic_cast<TH1D*>(fOutput->FindObject(hname));
       if (i<=5) h_dndyLambda[i][j]->Scale(100./h_Evminbias[j]->GetBinContent(2));
       else h_dndyLambda[i][j]->Scale(100./h_Evphi[j]->GetBinContent(2));
       h_dndyLambda[i][j]->Write();

       sprintf(hname,"h_dndyProton%i_%i",i+1,j+1);
       h_dndyProton[i][j] = dynamic_cast<TH1D*>(fOutput->FindObject(hname));
       if (i<=5) h_dndyProton[i][j]->Scale(100./h_Evminbias[j]->GetBinContent(2));
       else h_dndyProton[i][j]->Scale(100./h_Evphi[j]->GetBinContent(2));
       h_dndyProton[i][j]->Write();

       sprintf(hname,"h_dndyPhi%i_%i",i+1,j+1);
       h_dndyPhi[i][j] = dynamic_cast<TH1D*>(fOutput->FindObject(hname));
       if (i<=5) h_dndyPhi[i][j]->Scale(100./h_Evminbias[j]->GetBinContent(2));
       else h_dndyPhi[i][j]->Scale(100./h_Evphi[j]->GetBinContent(2));
       h_dndyPhi[i][j]->Write();

       sprintf(hname,"h_dndyPion%i_%i",i+1,j+1);
       h_dndyPion[i][j] = dynamic_cast<TH1D*>(fOutput->FindObject(hname));
       if (i<=5) h_dndyPion[i][j]->Scale(100./h_Evminbias[j]->GetBinContent(2));
       else h_dndyPion[i][j]->Scale(100./h_Evphi[j]->GetBinContent(2));
       h_dndyPion[i][j]->Write();
     }
   }
}

#endif // #ifdef Analysis_Xcore_cxx
