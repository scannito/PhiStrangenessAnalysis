#define Analysis_phi2_cxx
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
// root> T->Process("Analysis_phi2.C")
// root> T->Process("Analysis_phi2.C","some options")
// root> T->Process("Analysis_phi2.C+")
//


#include "Analysis_phi2.h"
#include <TStyle.h>
#include <TH1.h>
#include <TCanvas.h>
#include <Riostream.h>
#include <vector>

using namespace std;

const int nMultClasses = 12;
const int nTrigClasses = 4;

void Analysis_phi2::Begin(TTree * /*tree*/)
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

void Analysis_phi2::SlaveBegin(TTree *tree)
{
   // The SlaveBegin() function is called after the Begin() function.
   // When running with PROOF SlaveBegin() is called on each slave server.
   // The tree argument is deprecated (on PROOF 0 is passed).
  
   // Initialize the Tree branch adresses.
   Init(tree);    		

   TString option = GetOption();

   BookHisto();
}

Bool_t Analysis_phi2::Process(Long64_t entry)
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
 																						
   // Check the trigger condition
   /*if (trig) {
     // Count the number of charged particles	   
     for (unsigned int p=0; p<tracks_charge.GetSize(); ++p) {
       if (tracks_charge[p] && ((tracks_eta[p]>-3.7 && tracks_eta[p]<-1.7) || (tracks_eta[p]>2.8 && tracks_eta[p]<5.1))) ++nCharged;
     }
 
     ///////////////////9MULTIPLICITYCLASSES/////////////////
     //double xBin[10] = {-0.5, 13.5, 21.5, 27.5, 36.5, 47.5, 56.5, 65.5, 79.5, 197.5};  // Pythia with default parameters at 13.0 TeV
     //double xBin[10] = {-0.5, 15.5, 23.5, 29.5, 37.5, 47.5, 55.5, 64.5, 77.5, 185.5};  // Pythia with CR+Ropes parameters at 13.0 TeV
     //double xBin[10] = {-0.5, 17.5, 27.5, 34.5, 42.5, 50.5, 57.5, 63.5, 74.5, 158.5};  // EPOS4 with 1freeze-out per hydro simulation at 13.0 TeV
     ///////////////////11MULTIPLICITYCLASSES//////////////////
     //double xBin[nMultClasses+1] = {-0.5, 14.5, 22.5, 29.5, 37.5, 49.5, 57.5, 66.5, 80.5, 95.5, 105.5, 197.5};  // Pythia with default parameters at 13.0 TeV with more classes
     //double xBin[nMultClasses+1] = {-0.5, 16.5, 24.5, 30.5, 38.5, 49.5, 56.5, 65.5, 78.5, 92.5, 101.5, 185.5};  // Pythia with CR+Ropes parameters at 13.0 TeV with more classes
     //double xBin[nMultClasses+1] = {-0.5, 19.5, 29.5, 36.5, 43.5, 51.5, 56.5, 63.5, 74.5, 86.5, 97.5, 190.5};  // EPOS4 at 13.0 TeV with more classes
     ///////////////////12MULTIPLICITYCLASSES//////////////////
     double xBin[nMultClasses+1] = {-0.5, 16.5, 24.5, 30.5, 38.5, 49.5, 56.5, 65.5, 78.5, 92.5, 101.5, 108.5, 185.5};  // Pythia CR+Ropes at 13.0 TeV with even more classes
     //double xBin[nMultClasses+1] = {-0.5, 19.5, 29.5, 36.5, 43.5, 51.5, 56.5, 63.5, 74.5, 86.5, 97.5, 105.5, 190.5};  // EPOS4 at 13.0 TeV with even more classes
  
     for (int j=0; j<nMultClasses; j++) {
       // Check what the multiplicty class of the event is
       if (nCharged>=xBin[j] && nCharged<=xBin[j+1]) { 
         
         vector<unsigned int> Phitag;
         for (unsigned int p=0; p<tracks_charge.GetSize(); ++p) {
           if (abs(tracks_y[p])<2.5 && tracks_pid[p] == 333) Phitag.push_back(p);
         }

	 if (Phitag.size()>0) {
	   for (unsigned int p=0; p<tracks_charge.GetSize(); ++p) {	   
             if (tracks_charge[p] && abs(tracks_eta[p])<0.5) nCh[j]++; 
             if (abs(tracks_y[p])<2.5) {
	       unsigned int best_tag = Phitag[0]; 
	       for (unsigned int tag=0; tag<Phitag.size(); tag++) {
	         best_tag = (abs(tracks_y[p]-tracks_y[Phitag[tag]])<abs(tracks_y[p]-tracks_y[best_tag]) ? Phitag[tag] : best_tag);
	       }
	       if (abs(tracks_y[p]-tracks_y[best_tag])<0.1) {
		 if (abs(tracks_pid[p])==310) nK0s[0][j]++;
                 if (abs(tracks_pid[p])==3312) nXi[0][j]++;
                 if (abs(tracks_pid[p])==3334) nOmega[0][j]++;
                 if (abs(tracks_pid[p])==3122) nLambda[0][j]++;
                 if (abs(tracks_pid[p])==2212) nProton[0][j]++;
                 if (abs(tracks_pid[p])==333) nPhi[0][j]++;
                 if (abs(tracks_pid[p])==211) nPion[0][j]++;
	       }	 
               if (abs(tracks_y[p]-tracks_y[best_tag])<0.5) {
		 if (abs(tracks_pid[p])==310) nK0s[1][j]++;
                 if (abs(tracks_pid[p])==3312) nXi[1][j]++;
                 if (abs(tracks_pid[p])==3334) nOmega[1][j]++;
                 if (abs(tracks_pid[p])==3122) nLambda[1][j]++;
                 if (abs(tracks_pid[p])==2212) nProton[1][j]++;
                 if (abs(tracks_pid[p])==333) nPhi[1][j]++;
                 if (abs(tracks_pid[p])==211) nPion[1][j]++;     
	       }
               if (abs(tracks_y[p]-tracks_y[best_tag])<1.0) {
                 if (abs(tracks_pid[p])==310) nK0s[2][j]++;
                 if (abs(tracks_pid[p])==3312) nXi[2][j]++;
                 if (abs(tracks_pid[p])==3334) nOmega[2][j]++;
                 if (abs(tracks_pid[p])==3122) nLambda[2][j]++;
                 if (abs(tracks_pid[p])==2212) nProton[2][j]++;
                 if (abs(tracks_pid[p])==333) nPhi[2][j]++;
                 if (abs(tracks_pid[p])==211) nPion[2][j]++;		       
	       }
               if (abs(tracks_pid[p])==310) nK0s[3][j]++;
               if (abs(tracks_pid[p])==3312) nXi[3][j]++;
               if (abs(tracks_pid[p])==3334) nOmega[3][j]++;
               if (abs(tracks_pid[p])==3122) nLambda[3][j]++;
               if (abs(tracks_pid[p])==2212) nProton[3][j]++;
               if (abs(tracks_pid[p])==333) nPhi[3][j]++;
               if (abs(tracks_pid[p])==211) nPion[3][j]++;
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
         }

	 break;
       }	 
     }
   }*/

   if (trig) {
     // Count the number of charged particles and phis 
     vector<unsigned int> Phitag;
     for (unsigned int p=0; p<tracks_charge.GetSize(); ++p) {
       if (tracks_charge[p] && ((tracks_eta[p]>-3.7 && tracks_eta[p]<-1.7) || (tracks_eta[p]>2.8 && tracks_eta[p]<5.1))) ++nCharged;
       if (abs(tracks_y[p])<2.5 && tracks_pid[p] == 333) Phitag.push_back(p);
     }	

     ///////////////////12MULTIPLICITYCLASSES//////////////////
     //double xBin[nMultClasses+1] = {-0.5, 16.5, 24.5, 30.5, 38.5, 49.5, 56.5, 65.5, 78.5, 92.5, 101.5, 108.5, 185.5};  // Pythia CR+Ropes at 13.0 TeV 
     double xBin[nMultClasses+1] = {-0.5, 19.5, 29.5, 36.5, 43.5, 51.5, 56.5, 63.5, 74.5, 86.5, 97.5, 105.5, 190.5};  // EPOS4 at 13.0 TeV
																					
     if (Phitag.size()>0) {

       // Charged particles, strange hadrons and pions over the entire multiplicity distribution, without classes	     
       for (unsigned int p=0; p<tracks_charge.GetSize(); ++p) {
         if (tracks_charge[p] && abs(tracks_eta[p])<0.5) nCh[0]++;
         if (abs(tracks_y[p])<2.5) {
           unsigned int best_tag = Phitag[0];
           for (unsigned int tag=0; tag<Phitag.size(); tag++) {
             best_tag = (abs(tracks_y[p]-tracks_y[Phitag[tag]])<abs(tracks_y[p]-tracks_y[best_tag]) ? Phitag[tag] : best_tag);
           }
           //if (abs(tracks_y[p]-tracks_y[best_tag])<0.1) {
           if (abs(tracks_y[p]-tracks_y[best_tag])<0.01) {		   
             if (abs(tracks_pid[p])==310) nK0s[0][0]++;
             if (abs(tracks_pid[p])==3312) nXi[0][0]++;
             if (abs(tracks_pid[p])==3334) nOmega[0][0]++;
             if (abs(tracks_pid[p])==3122) nLambda[0][0]++;
             if (abs(tracks_pid[p])==2212) nProton[0][0]++;
             if (abs(tracks_pid[p])==333) nPhi[0][0]++;
             if (abs(tracks_pid[p])==211) nPion[0][0]++;
           }
           //if (abs(tracks_y[p]-tracks_y[best_tag])<0.5) {
           if (abs(tracks_y[p]-tracks_y[best_tag])<0.03) {		   
             if (abs(tracks_pid[p])==310) nK0s[1][0]++;
             if (abs(tracks_pid[p])==3312) nXi[1][0]++;
             if (abs(tracks_pid[p])==3334) nOmega[1][0]++;
             if (abs(tracks_pid[p])==3122) nLambda[1][0]++;
             if (abs(tracks_pid[p])==2212) nProton[1][0]++;
             if (abs(tracks_pid[p])==333) nPhi[1][0]++;
             if (abs(tracks_pid[p])==211) nPion[1][0]++;
           }
           //if (abs(tracks_y[p]-tracks_y[best_tag])<1.0) {
	         if (abs(tracks_y[p]-tracks_y[best_tag])<0.8) {
             if (abs(tracks_pid[p])==310) nK0s[2][0]++;
             if (abs(tracks_pid[p])==3312) nXi[2][0]++;
             if (abs(tracks_pid[p])==3334) nOmega[2][0]++;
             if (abs(tracks_pid[p])==3122) nLambda[2][0]++;
             if (abs(tracks_pid[p])==2212) nProton[2][0]++;
             if (abs(tracks_pid[p])==333) nPhi[2][0]++;
             if (abs(tracks_pid[p])==211) nPion[2][0]++;
           }
           if (abs(tracks_pid[p])==310) nK0s[3][0]++;
           if (abs(tracks_pid[p])==3312) nXi[3][0]++;
           if (abs(tracks_pid[p])==3334) nOmega[3][0]++;
           if (abs(tracks_pid[p])==3122) nLambda[3][0]++;
           if (abs(tracks_pid[p])==2212) nProton[3][0]++;
           if (abs(tracks_pid[p])==333) nPhi[3][0]++;
           if (abs(tracks_pid[p])==211) nPion[3][0]++;
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
             if (abs(tracks_y[p])<2.5) {
               unsigned int best_tag = Phitag[0];
               for (unsigned int tag=0; tag<Phitag.size(); tag++) {
                 best_tag = (abs(tracks_y[p]-tracks_y[Phitag[tag]])<abs(tracks_y[p]-tracks_y[best_tag]) ? Phitag[tag] : best_tag);
               }
               //if (abs(tracks_y[p]-tracks_y[best_tag])<0.1) {
	       if (abs(tracks_y[p]-tracks_y[best_tag])<0.01) {
                 if (abs(tracks_pid[p])==310) nK0s[0][j]++;
                 if (abs(tracks_pid[p])==3312) nXi[0][j]++;
                 if (abs(tracks_pid[p])==3334) nOmega[0][j]++;
                 if (abs(tracks_pid[p])==3122) nLambda[0][j]++;
                 if (abs(tracks_pid[p])==2212) nProton[0][j]++;
                 if (abs(tracks_pid[p])==333) nPhi[0][j]++;
                 if (abs(tracks_pid[p])==211) nPion[0][j]++;
               }
               //if (abs(tracks_y[p]-tracks_y[best_tag])<0.5) {
	       if (abs(tracks_y[p]-tracks_y[best_tag])<0.03) {	       
                 if (abs(tracks_pid[p])==310) nK0s[1][j]++;
                 if (abs(tracks_pid[p])==3312) nXi[1][j]++;
                 if (abs(tracks_pid[p])==3334) nOmega[1][j]++;
                 if (abs(tracks_pid[p])==3122) nLambda[1][j]++;
                 if (abs(tracks_pid[p])==2212) nProton[1][j]++;
                 if (abs(tracks_pid[p])==333) nPhi[1][j]++;
                 if (abs(tracks_pid[p])==211) nPion[1][j]++;
               }
               //if (abs(tracks_y[p]-tracks_y[best_tag])<1.0) {
	       if (abs(tracks_y[p]-tracks_y[best_tag])<0.8) {	       
                 if (abs(tracks_pid[p])==310) nK0s[2][j]++;
                 if (abs(tracks_pid[p])==3312) nXi[2][j]++;
                 if (abs(tracks_pid[p])==3334) nOmega[2][j]++;
                 if (abs(tracks_pid[p])==3122) nLambda[2][j]++;
                 if (abs(tracks_pid[p])==2212) nProton[2][j]++;
                 if (abs(tracks_pid[p])==333) nPhi[2][j]++;
                 if (abs(tracks_pid[p])==211) nPion[2][j]++;
               }
               if (abs(tracks_pid[p])==310) nK0s[3][j]++;
               if (abs(tracks_pid[p])==3312) nXi[3][j]++;
               if (abs(tracks_pid[p])==3334) nOmega[3][j]++;
               if (abs(tracks_pid[p])==3122) nLambda[3][j]++;
               if (abs(tracks_pid[p])==2212) nProton[3][j]++;
               if (abs(tracks_pid[p])==333) nPhi[3][j]++;
               if (abs(tracks_pid[p])==211) nPion[3][j]++;
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

void Analysis_phi2::SlaveTerminate()
{
   // The SlaveTerminate() function is called after all entries or objects
   // have been processed. When running with PROOF SlaveTerminate() is called
   // on each slave server.

}

void Analysis_phi2::Terminate()
{
   // The Terminate() function is the last function to be called during
   // a query. It always runs on the client, it can be used to present
   // the results graphically or save the results to file.
																	
   string filename2;
   //if (pythia_def) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_moreclasses_results2/analysis_phi_pydef.root";
   //else if (pythia_new) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_moreclasses_results2/analysis_phi_pynew.root";
   //else if (epos4) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_moreclasses_results2/analysis_phi_epos.root";
   if (pythia_def) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_evenmoreclasses_results2/analysis_phi_pydef.root";
   else if (pythia_new) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_evenmoreclasses_results2/analysis_phi_pynew_omegatest_rap.root";
   else if (epos4) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_evenmoreclasses_results2/analysis_phi_epos_omegatest_rap.root";
   TFile* file2 = new TFile(filename2.c_str(), "RECREATE");							
   CollectHisto(file2);
   file2->Close();
}
