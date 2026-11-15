#define Analysis_minbias_cxx
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
// root> T->Process("Analysis_minbias.C")
// root> T->Process("Analysis_minbias.C","some options")
// root> T->Process("Analysis_minbias.C+")
//


#include "Analysis_minbias.h"
#include <TStyle.h>
#include <TH1.h>
#include <TCanvas.h>
#include <Riostream.h>
#include <vector>

using namespace std;

const int nMultClasses = 12;

TH1I* h_Ev[nMultClasses];
TH1D* h_dndeta[nMultClasses];
TH1D* h_dndyK0s[nMultClasses];
TH1D* h_dndyXi[nMultClasses];
TH1D* h_dndyOmega[nMultClasses];
TH1D* h_dndyLambda[nMultClasses];
TH1D* h_dndyProton[nMultClasses];
TH1D* h_dndyPhi[nMultClasses];
TH1D* h_dndyPion[nMultClasses];

void Analysis_minbias::Begin(TTree * /*tree*/)
{
   // The Begin() function is called at the start of the query.
   // When running with PROOF Begin() is only called on the client.
   // The tree argument is deprecated (on PROOF 0 is passed).
 
   Reset();

   TString option = GetOption();
   Info("Begin", "starting analysis with process option: %s", option.Data());

   if (option.Contains("pythia_def")) pythia_def = kTRUE;
   if (option.Contains("pythia_new")) pythia_new = kTRUE;
   if (option.Contains("epos4")) epos4 = kTRUE;
}

void Analysis_minbias::SlaveBegin(TTree *tree)
{
   // The SlaveBegin() function is called after the Begin() function.
   // When running with PROOF SlaveBegin() is called on each slave server.
   // The tree argument is deprecated (on PROOF 0 is passed).
  
   // Initialize the Tree branch adresses.
   Init(tree);    		

   TString option = GetOption();

   // Create dN_ch/deta histograms.
   for (int i = 0; i < nMultClasses; i++) {
     h_Ev[i] = new TH1I(Form("h_Ev%i",i+1), "", 2, -0.5, 1.5);
     h_dndeta[i] = new TH1D(Form("h_dndeta%i",i+1), "; #eta; dN_{ch}/d#eta", 100, -0.5, 0.5);
     h_dndyK0s[i] = new TH1D(Form("h_dndyK0s%i",i+1), "; y; dN_{K_{S}^{0}}/dy", 100, -0.5, 0.5);
     h_dndyXi[i] = new TH1D(Form("h_dndyXi%i",i+1), "; y; dN_{#Xi}/dy", 100, -0.5, 0.5);
     h_dndyOmega[i] = new TH1D(Form("h_dndyOmega%i",i+1), "; y; dN_{#Omega}/dy", 100, -0.5, 0.5);
     h_dndyLambda[i] = new TH1D(Form("h_dndyLambda%i",i+1), "; y; dN_{#Lambda}/dy", 100, -0.5, 0.5);
     h_dndyProton[i] = new TH1D(Form("h_dndyProton%i",i+1), "; y; dN_{p}/dy", 100, -0.5, 0.5);
     h_dndyPhi[i] = new TH1D(Form("h_dndyPhi%i",i+1), "; y; dN_{#phi}/dy", 100, -0.5, 0.5);
     h_dndyPion[i] = new TH1D(Form("h_dndyPion%i",i+1), "; y; dN_{#pi}/dy", 100, -0.5, 0.5);
												
     fOutput->Add(h_Ev[i]);
     fOutput->Add(h_dndeta[i]);
     fOutput->Add(h_dndyK0s[i]);
     fOutput->Add(h_dndyXi[i]);
     fOutput->Add(h_dndyOmega[i]);
     fOutput->Add(h_dndyLambda[i]);
     fOutput->Add(h_dndyProton[i]);
     fOutput->Add(h_dndyPhi[i]);
     fOutput->Add(h_dndyPion[i]);
   }     
}

Bool_t Analysis_minbias::Process(Long64_t entry)
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

   bool trig = kFALSE;
   for (unsigned int p=0; p<tracks_eta.GetSize(); ++p) {
     if (tracks_charge[p] && abs(tracks_eta[p])<1.) {
       trig = kTRUE;
       break;
     }	     
   }

   double nCharged = 0;
   int Ev[nMultClasses] = {};

   // Check the trigger condition
   if (trig) {
     // Count the number of charged particles	   
     for (unsigned int p=0; p<tracks_charge.GetSize(); ++p) {
       if (tracks_charge[p] && ((tracks_eta[p]>-3.7 && tracks_eta[p]<-1.7) || (tracks_eta[p]>2.8 && tracks_eta[p]<5.1))) ++nCharged;
     }

     ////////////////9MULTIPLICITYCLASSES(LOWSTATISTICS)///////////////
     //double xBin[10] = {-0.5, 13.5, 20.5, 27.5, 36.5, 47.5, 56.5, 65.5, 79.5, 179.5};  // Pythia with default parameters for run2 at 13.0 TeV
     //double xBin[10] = {-0.5, 15.5, 23.5, 29.5, 37.5, 47.5, 55.5, 64.5, 77.5, 175.5};  // Pythia with CR+Ropes parameters for run2 at 13.0 TeV 
     //double xBin[10] = {-0.5, 17.5, 28.5, 35.5, 42.5, 50.5, 56.5, 62.5, 73.5, 168.5};  // EPOS4 at 13.0 TeV
     //double xBin[10] = {-0.5, 15.5, 24.5, 31.5, 39.5, 47.5, 54.5, 61.5, 73.5, 162.5};  // EPOS4 with PFE at 13.0 TeV
     ////////////////11MULTIPLICITYCLASSES(HIGHSTATISTICS)///////////////
     //double xBin[nMultClasses+1] = {-0.5, 14.5, 22.5, 29.5, 37.5, 49.5, 57.5, 66.5, 80.5, 95.5, 105.5, 197.5};  // Pythia with default parameters at 13.0 TeV with more classes
     //double xBin[nMultClasses+1] = {-0.5, 16.5, 24.5, 30.5, 38.5, 49.5, 56.5, 65.5, 78.5, 92.5, 101.5, 185.5};  // Pythia with CR+Ropes parameters at 13.0 TeV with more classes
     //double xBin[nMultClasses+1] = {-0.5, 19.5, 29.5, 36.5, 43.5, 51.5, 57.5, 63.5, 74.5, 86.5, 97.5, 186.5};  // EPOS4 at 13.0 TeV with more classes
     ////////////////12MULTIPLICITYCLASSES(HIGHSTATISTICS)///////////////
     //double xBin[nMultClasses+1] = {-0.5, 14.5, 22.5, 29.5, 37.5, 49.5, 57.5, 66.5, 80.5, 95.5, 105.5, 112.5, 197.5};  // Pythia default at 13.0 TeV 
     //double xBin[nMultClasses+1] = {-0.5, 16.5, 24.5, 30.5, 38.5, 49.5, 56.5, 65.5, 78.5, 92.5, 101.5, 108.5, 185.5};  // Pythia CR+Ropes at 13.0 TeV
     double xBin[nMultClasses+1] = {-0.5, 19.5, 29.5, 36.5, 43.5, 51.5, 57.5, 63.5, 74.5, 86.5, 97.5, 105.5, 190.5};  // EPOS4 at 13.0 TeV 

     for (int i=0; i<nMultClasses; i++) {
       // Check what the multiplicty class of the event is
       if (nCharged>=xBin[i] && nCharged<=xBin[i+1]) {
         Ev[i] = 1;
         h_Ev[i]->Fill(Ev[i]);	 
         for (unsigned int p=0; p<tracks_charge.GetSize(); ++p) {
           if (tracks_charge[p] && abs(tracks_eta[p])<0.5) h_dndeta[i]->Fill(tracks_eta[p]);
	   if (abs(tracks_y[p])<0.5) {
	     if (abs(tracks_pid[p])==310) h_dndyK0s[i]->Fill(tracks_y[p]);
             if (abs(tracks_pid[p])==3312) h_dndyXi[i]->Fill(tracks_y[p]);
             if (abs(tracks_pid[p])==3334) h_dndyOmega[i]->Fill(tracks_y[p]);
             if (abs(tracks_pid[p])==3122) h_dndyLambda[i]->Fill(tracks_y[p]);
             if (abs(tracks_pid[p])==2212) h_dndyProton[i]->Fill(tracks_y[p]);
             if (abs(tracks_pid[p])==333) h_dndyPhi[i]->Fill(tracks_y[p]);
             if (abs(tracks_pid[p])==211) h_dndyPion[i]->Fill(tracks_y[p]);	   
           }		     
         }
         break;	   
       }	 
     }
   }   

   return kTRUE;
}

void Analysis_minbias::SlaveTerminate()
{
   // The SlaveTerminate() function is called after all entries or objects
   // have been processed. When running with PROOF SlaveTerminate() is called
   // on each slave server.

}

void Analysis_minbias::Terminate()
{
   // The Terminate() function is the last function to be called during
   // a query. It always runs on the client, it can be used to present
   // the results graphically or save the results to file.

   string filename2;
   /*if (pythia_def) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Minbias_moreclasses_results/analysis_minbias_pydef.root";
   else if (pythia_new) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Minbias_moreclasses_results/analysis_minbias_pynew.root";
   else if (epos4) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Minbias_moreclasses_results/analysis_minbias_epos.root";*/
   if (pythia_def) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Minbias_evenmoreclasses_results/analysis_minbias_pydef.root";
   else if (pythia_new) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Minbias_evenmoreclasses_results/analysis_minbias_pynew.root";
   else if (epos4) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Minbias_evenmoreclasses_results/analysis_minbias_epos.root";
   TFile* file2 = new TFile(filename2.c_str(), "RECREATE");
   //TDirectory* dir = file2->mkdir("dir");
   //dir->cd();
   for (int i=0; i<nMultClasses; ++i) {
     h_Ev[i] = dynamic_cast<TH1I*>(fOutput->FindObject(Form("h_Ev%i",i+1)));	   
     h_dndeta[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndeta%i",i+1)));
     h_dndyK0s[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyK0s%i",i+1)));
     h_dndyXi[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyXi%i",i+1)));
     h_dndyOmega[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyOmega%i",i+1)));
     h_dndyLambda[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyLambda%i",i+1)));
     h_dndyProton[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyProton%i",i+1)));
     h_dndyPhi[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyPhi%i",i+1)));
     h_dndyPion[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyPion%i",i+1)));

     h_dndeta[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyK0s[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyXi[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyOmega[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyLambda[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyProton[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyPhi[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyPion[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     
     h_dndeta[i]->Write();
     h_dndyK0s[i]->Write();
     h_dndyXi[i]->Write();
     h_dndyOmega[i]->Write();
     h_dndyLambda[i]->Write();
     h_dndyProton[i]->Write();
     h_dndyPhi[i]->Write();
     h_dndyPion[i]->Write();
   }
   file2->Close();
}
