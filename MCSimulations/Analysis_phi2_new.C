#define Analysis_phi2_new_cxx
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
// root> T->Process("Analysis_phi2_new.C")
// root> T->Process("Analysis_phi2_new.C","some options")
// root> T->Process("Analysis_phi2_new.C+")
//


#include "Analysis_phi2_new.h"
#include <TStyle.h>
#include <TH1.h>
#include <TCanvas.h>
#include <Riostream.h>
#include <vector>

using namespace std;

const int nMultClasses = 12;
//const int nTrigClasses = 4;
const int nTrigClasses = 3;

void Analysis_phi2_new::Begin(TTree * /*tree*/)
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

void Analysis_phi2_new::SlaveBegin(TTree *tree)
{
   // The SlaveBegin() function is called after the Begin() function.
   // When running with PROOF SlaveBegin() is called on each slave server.
   // The tree argument is deprecated (on PROOF 0 is passed).
  
   // Initialize the Tree branch adresses.
   Init(tree);    		

   TString option = GetOption();

   BookHisto();
}

Bool_t Analysis_phi2_new::Process(Long64_t entry)
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
   int nCh[nMultClasses+1] = {};
   int nK0s[nTrigClasses][nMultClasses+1] = {}, nXi[nTrigClasses][nMultClasses+1] = {}, nOmega[nTrigClasses][nMultClasses+1] = {}, nLambda[nTrigClasses][nMultClasses+1] = {};
   int nProton[nTrigClasses][nMultClasses+1] = {}, nPhi[nTrigClasses][nMultClasses+1] = {}, nPion[nTrigClasses][nMultClasses+1] = {};
   bool cK0s[nTrigClasses][nMultClasses+1], cXi[nTrigClasses][nMultClasses+1], cOmega[nTrigClasses][nMultClasses+1], cLambda[nTrigClasses][nMultClasses+1];
   bool cProton[nTrigClasses][nMultClasses+1], cPhi[nTrigClasses][nMultClasses+1], cPion[nTrigClasses][nMultClasses+1];
 																						
   // Check the trigger condition
   if (trig) {
     // Count the number of charged particles and phis 
     vector<unsigned int> Phitag;
     for (unsigned int p=0; p<tracks_charge.GetSize(); ++p) {
       if (tracks_charge[p] && ((tracks_eta[p]>-3.7 && tracks_eta[p]<-1.7) || (tracks_eta[p]>2.8 && tracks_eta[p]<5.1))) ++nCharged;
       //if (abs(tracks_y[p])<2.5 && tracks_pid[p] == 333) Phitag.push_back(p);
       if (abs(tracks_y[p])<0.8 && tracks_pid[p] == 333) Phitag.push_back(p);
     }	

     ///////////////////12MULTIPLICITYCLASSES//////////////////
     double xBin[nMultClasses+1] = {-0.5, 16.5, 24.5, 30.5, 38.5, 49.5, 56.5, 65.5, 78.5, 92.5, 101.5, 108.5, 185.5};  // Pythia CR+Ropes at 13.0 TeV 
     //double xBin[nMultClasses+1] = {-0.5, 19.5, 29.5, 36.5, 43.5, 51.5, 56.5, 63.5, 74.5, 86.5, 97.5, 105.5, 190.5};  // EPOS4 at 13.0 TeV

     if (Phitag.size()>0) {

       // Charged particles, strange hadrons and pions over the entire multiplicity distribution, without classes	     
       for (unsigned int p=0; p<tracks_charge.GetSize(); ++p) {
         if (tracks_charge[p] && abs(tracks_eta[p])<0.5) nCh[0]++;
         //if (abs(tracks_y[p])<2.5) {
          if (abs(tracks_y[p])<0.8) {
           for (int i = 0; i < nTrigClasses; i++) {
             if (abs(tracks_pid[p])==310) cK0s[i][0] = kFALSE;
             if (abs(tracks_pid[p])==3312) cXi[i][0] = kFALSE;
             if (abs(tracks_pid[p])==3334) cOmega[i][0] = kFALSE;
             if (abs(tracks_pid[p])==3122) cLambda[i][0] = kFALSE;
             if (abs(tracks_pid[p])==2212) cProton[i][0] = kFALSE;
             if (abs(tracks_pid[p])==333) cPhi[i][0] = kFALSE;
             if (abs(tracks_pid[p])==211) cPion[i][0] = kFALSE;          
           }
           for (unsigned int tag=0; tag<Phitag.size(); tag++) {
             //if (abs(tracks_y[p]-tracks_y[Phitag[tag]])<0.1) {
             if (abs(tracks_y[p]-tracks_y[Phitag[tag]])<0.2) {
               if (abs(tracks_pid[p])==310 && !cK0s[0][0]) {nK0s[0][0]++; cK0s[0][0] = kTRUE;}
               if (abs(tracks_pid[p])==3312 && !cXi[0][0]) {nXi[0][0]++; cXi[0][0] = kTRUE;}
               if (abs(tracks_pid[p])==3334 && !cOmega[0][0]) {nOmega[0][0]++; cOmega[0][0] = kTRUE;}
               if (abs(tracks_pid[p])==3122 && !cLambda[0][0]) {nLambda[0][0]++; cLambda[0][0] = kTRUE;}
               if (abs(tracks_pid[p])==2212 && !cProton[0][0]) {nProton[0][0]++; cProton[0][0] = kTRUE;}
               if (abs(tracks_pid[p])==333 && !cPhi[0][0]) {nPhi[0][0]++; cPhi[0][0] = kTRUE;}
               if (abs(tracks_pid[p])==211 && !cPion[0][0]) {nPion[0][0]++; cPion[0][0] = kTRUE;}
             }
             if (abs(tracks_y[p]-tracks_y[Phitag[tag]])<0.5) {
               if (abs(tracks_pid[p])==310 && !cK0s[1][0]) {nK0s[1][0]++; cK0s[1][0] = kTRUE;}
               if (abs(tracks_pid[p])==3312 && !cXi[1][0]) {nXi[1][0]++; cXi[1][0] = kTRUE;}
               if (abs(tracks_pid[p])==3334 && !cOmega[1][0]) {nOmega[1][0]++; cOmega[1][0] = kTRUE;}
               if (abs(tracks_pid[p])==3122 && !cLambda[1][0]) {nLambda[1][0]++; cLambda[1][0] = kTRUE;}
               if (abs(tracks_pid[p])==2212 && !cProton[1][0]) {nProton[1][0]++; cProton[1][0] = kTRUE;}
               if (abs(tracks_pid[p])==333 && !cPhi[1][0]) {nPhi[1][0]++; cPhi[1][0] = kTRUE;}
               if (abs(tracks_pid[p])==211 && !cPion[1][0]) {nPion[1][0]++; cPion[1][0] = kTRUE;}
             }
             /*if (abs(tracks_y[p]-tracks_y[Phitag[tag]])<1.0) {
               if (abs(tracks_pid[p])==310 && !cK0s[2][0]) {nK0s[2][0]++; cK0s[2][0] = kTRUE;}
               if (abs(tracks_pid[p])==3312 && !cXi[2][0]) {nXi[2][0]++; cXi[2][0] = kTRUE;}
               if (abs(tracks_pid[p])==3334 && !cOmega[2][0]) {nOmega[2][0]++; cOmega[2][0] = kTRUE;}
               if (abs(tracks_pid[p])==3122 && !cLambda[2][0]) {nLambda[2][0]++; cLambda[2][0] = kTRUE;} 
               if (abs(tracks_pid[p])==2212 && !cProton[2][0]) {nProton[2][0]++; cProton[2][0] = kTRUE;}
               if (abs(tracks_pid[p])==333 && !cPhi[2][0]) {nPhi[2][0]++; cPhi[2][0] = kTRUE;}
               if (abs(tracks_pid[p])==211 && !cPion[2][0]) {nPion[2][0]++; cPion[2][0] = kTRUE;}
             }
             if (abs(tracks_pid[p])==310 && !cK0s[3][0]) {nK0s[3][0]++; cK0s[3][0] = kTRUE;}
             if (abs(tracks_pid[p])==3312 && !cXi[3][0]) {nXi[3][0]++; cXi[3][0] = kTRUE;}
             if (abs(tracks_pid[p])==3334 && !cOmega[3][0]) {nOmega[3][0]++; cOmega[3][0] = kTRUE;}
             if (abs(tracks_pid[p])==3122 && !cLambda[3][0]) {nLambda[3][0]++; cLambda[3][0] = kTRUE;}
             if (abs(tracks_pid[p])==2212 && !cProton[3][0]) {nProton[3][0]++; cProton[3][0] = kTRUE;}
             if (abs(tracks_pid[p])==333 && !cPhi[3][0]) {nPhi[3][0]++; cPhi[3][0] = kTRUE;}
             if (abs(tracks_pid[p])==211 && !cPion[3][0]) {nPion[3][0]++; cPion[3][0] = kTRUE;}*/
             if (abs(tracks_pid[p])==310 && !cK0s[2][0]) {nK0s[2][0]++; cK0s[2][0] = kTRUE;}
             if (abs(tracks_pid[p])==3312 && !cXi[2][0]) {nXi[2][0]++; cXi[2][0] = kTRUE;}
             if (abs(tracks_pid[p])==3334 && !cOmega[2][0]) {nOmega[2][0]++; cOmega[2][0] = kTRUE;}
             if (abs(tracks_pid[p])==3122 && !cLambda[2][0]) {nLambda[2][0]++; cLambda[2][0] = kTRUE;}
             if (abs(tracks_pid[p])==2212 && !cProton[2][0]) {nProton[2][0]++; cProton[2][0] = kTRUE;}
             if (abs(tracks_pid[p])==333 && !cPhi[2][0]) {nPhi[2][0]++; cPhi[2][0] = kTRUE;}
             if (abs(tracks_pid[p])==211 && !cPion[2][0]) {nPion[2][0]++; cPion[2][0] = kTRUE;}
           }      
         }
       } 
       h_nCh[0]->Fill(nCh[0]);
       for (int i = 0; i < nTrigClasses; i++) {
         h_nK0s[i][0]->Fill(nK0s[i][0]);
         h_nXi[i][0]->Fill(nXi[i][0]);
         h_nOmega[i][0]->Fill(nOmega[i][0]);
         h_nLambda[i][0]->Fill(nLambda[i][0]);
         h_nProton[i][0]->Fill(nProton[i][0]);
         h_nPhi[i][0]->Fill(nPhi[i][0]);
         h_nPion[i][0]->Fill(nPion[i][0]);
       }

       // Charged particles, strange hadrons and pions per class 
       for (int j=1; j<nMultClasses+1; j++) {
         // Check what the multiplicty class of the event is
         if (nCharged>=xBin[j-1] && nCharged<=xBin[j]) {
           for (unsigned int p=0; p<tracks_charge.GetSize(); ++p) {
             if (tracks_charge[p] && abs(tracks_eta[p])<0.5) nCh[j]++;
             //if (abs(tracks_y[p])<2.5) {
             if (abs(tracks_y[p])<0.8) {
               for (int i = 0; i < nTrigClasses; i++) {
                 if (abs(tracks_pid[p])==310) cK0s[i][j] = kFALSE;
                 if (abs(tracks_pid[p])==3312) cXi[i][j] = kFALSE;
                 if (abs(tracks_pid[p])==3334) cOmega[i][j] = kFALSE;
                 if (abs(tracks_pid[p])==3122) cLambda[i][j] = kFALSE;
                 if (abs(tracks_pid[p])==2212) cProton[i][j] = kFALSE;
                 if (abs(tracks_pid[p])==333) cPhi[i][j] = kFALSE;
                 if (abs(tracks_pid[p])==211) cPion[i][j] = kFALSE;          
               }
               for (unsigned int tag=0; tag<Phitag.size(); tag++) {
                 //if (abs(tracks_y[p]-tracks_y[Phitag[tag]])<0.1) {
                 if (abs(tracks_y[p]-tracks_y[Phitag[tag]])<0.2) {
                   if (abs(tracks_pid[p])==310 && !cK0s[0][j]) {nK0s[0][j]++; cK0s[0][j] = kTRUE;}
                   if (abs(tracks_pid[p])==3312 && !cXi[0][j]) {nXi[0][j]++; cXi[0][j] = kTRUE;} 
                   if (abs(tracks_pid[p])==3334 && !cOmega[0][j]) {nOmega[0][j]++; cOmega[0][j] = kTRUE;}
                   if (abs(tracks_pid[p])==3122 && !cLambda[0][j]) {nLambda[0][j]++; cLambda[0][j] = kTRUE;}
                   if (abs(tracks_pid[p])==2212 && !cProton[0][j]) {nProton[0][j]++; cProton[0][j] = kTRUE;}
                   if (abs(tracks_pid[p])==333 && !cPhi[0][j]) {nPhi[0][j]++; cPhi[0][j] = kTRUE;}
                   if (abs(tracks_pid[p])==211 && !cPion[0][j]) {nPion[0][j]++; cPion[0][j] = kTRUE;}
                 }
                 if (abs(tracks_y[p]-tracks_y[Phitag[tag]])<0.5) {
                   if (abs(tracks_pid[p])==310 && !cK0s[1][j]) {nK0s[1][j]++; cK0s[1][j] = kTRUE;}
                   if (abs(tracks_pid[p])==3312 && !cXi[1][j]) {nXi[1][j]++; cXi[1][j] = kTRUE;}
                   if (abs(tracks_pid[p])==3334 && !cOmega[1][j]) {nOmega[1][j]++; cOmega[1][j] = kTRUE;}
                   if (abs(tracks_pid[p])==3122 && !cLambda[1][j]) {nLambda[1][j]++; cLambda[1][j] = kTRUE;}
                   if (abs(tracks_pid[p])==2212 && !cProton[1][j]) {nProton[1][j]++; cProton[1][j] = kTRUE;}
                   if (abs(tracks_pid[p])==333 && !cPhi[1][j]) {nPhi[1][j]++; cPhi[1][j] = kTRUE;}
                   if (abs(tracks_pid[p])==211 && !cPion[1][j]) {nPion[1][j]++; cPion[1][j] = kTRUE;}
                 }
                 /*if (abs(tracks_y[p]-tracks_y[Phitag[tag]])<1.0) {
                   if (abs(tracks_pid[p])==310 && !cK0s[2][j]) {nK0s[2][j]++; cK0s[2][j] = kTRUE;}
                   if (abs(tracks_pid[p])==3312 && !cXi[2][j]) {nXi[2][j]++; cXi[2][j] = kTRUE;}
                   if (abs(tracks_pid[p])==3334 && !cOmega[2][j]) {nOmega[2][j]++; cOmega[2][j] = kTRUE;}
                   if (abs(tracks_pid[p])==3122 && !cLambda[2][j]) {nLambda[2][j]++; cLambda[2][j] = kTRUE;}
                   if (abs(tracks_pid[p])==2212 && !cProton[2][j]) {nProton[2][j]++; cProton[2][j] = kTRUE;}
                   if (abs(tracks_pid[p])==333 && !cPhi[2][j]) {nPhi[2][j]++; cPhi[2][j] = kTRUE;}
                   if (abs(tracks_pid[p])==211 && !cPion[2][j]) {nPion[2][j]++; cPion[2][j] = kTRUE;}
                 }
                 if (abs(tracks_pid[p])==310 && !cK0s[3][j]) {nK0s[3][j]++; cK0s[3][j] = kTRUE;}
                 if (abs(tracks_pid[p])==3312 && !cXi[3][j]) {nXi[3][j]++; cXi[3][j] = kTRUE;}
                 if (abs(tracks_pid[p])==3334 && !cOmega[3][j]) {nOmega[3][j]++; cOmega[3][j] = kTRUE;}
                 if (abs(tracks_pid[p])==3122 && !cLambda[3][j]) {nLambda[3][j]++; cLambda[3][j] = kTRUE;}
                 if (abs(tracks_pid[p])==2212 && !cProton[3][j]) {nProton[3][j]++; cProton[3][j] = kTRUE;}
                 if (abs(tracks_pid[p])==333 && !cPhi[3][j]) {nPhi[3][j]++; cPhi[3][j] = kTRUE;}
                 if (abs(tracks_pid[p])==211 && !cPion[3][j]) {nPion[3][j]++; cPion[3][j] = kTRUE;}*/
                 if (abs(tracks_pid[p])==310 && !cK0s[2][j]) {nK0s[2][j]++; cK0s[2][j] = kTRUE;}
                 if (abs(tracks_pid[p])==3312 && !cXi[2][j]) {nXi[2][j]++; cXi[2][j] = kTRUE;}
                 if (abs(tracks_pid[p])==3334 && !cOmega[2][j]) {nOmega[2][j]++; cOmega[2][j] = kTRUE;}
                 if (abs(tracks_pid[p])==3122 && !cLambda[2][j]) {nLambda[2][j]++; cLambda[2][j] = kTRUE;}
                 if (abs(tracks_pid[p])==2212 && !cProton[2][j]) {nProton[2][j]++; cProton[2][j] = kTRUE;}
                 if (abs(tracks_pid[p])==333 && !cPhi[2][j]) {nPhi[2][j]++; cPhi[2][j] = kTRUE;}
                 if (abs(tracks_pid[p])==211 && !cPion[2][j]) {nPion[2][j]++; cPion[2][j] = kTRUE;}
               }
             }
           }
           h_nCh[j]->Fill(nCh[j]);
           for (int i = 0; i < nTrigClasses; i++) {
             h_nK0s[i][j]->Fill(nK0s[i][j]);
             h_nXi[i][j]->Fill(nXi[i][j]);
             h_nOmega[i][j]->Fill(nOmega[i][j]);
             h_nLambda[i][j]->Fill(nLambda[i][j]);
             h_nProton[i][j]->Fill(nProton[i][j]);
             h_nPhi[i][j]->Fill(nPhi[i][j]);
             h_nPion[i][j]->Fill(nPion[i][j]);
           }
           break;
         }
       }	   
     }     
   }

   return kTRUE;
}

void Analysis_phi2_new::SlaveTerminate()
{
   // The SlaveTerminate() function is called after all entries or objects
   // have been processed. When running with PROOF SlaveTerminate() is called
   // on each slave server.

}

void Analysis_phi2_new::Terminate()
{
   // The Terminate() function is the last function to be called during
   // a query. It always runs on the client, it can be used to present
   // the results graphically or save the results to file.
																	
   string filename2;
   if (pythia_def) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_newmethod_results/analysis_phi_pydef2.root";
   else if (pythia_new) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_newmethod_results/analysis_phi_pynew2.root";
   else if (epos4) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_newmethod_results/analysis_phi_epos2.root";
   TFile* file2 = new TFile(filename2.c_str(), "RECREATE");							
   CollectHisto(file2);
   file2->Close();
}
