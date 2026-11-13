#define Analysis_phi2_kappa_cxx
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
// root> T->Process("Analysis_phi2_kappa.C")
// root> T->Process("Analysis_phi2_kappa.C","some options")
// root> T->Process("Analysis_phi2_kappa.C+")
//


#include "Analysis_phi2_kappa.h"
#include <TStyle.h>
#include <TH1.h>
#include <TCanvas.h>
#include <Riostream.h>
#include <vector>

using namespace std;

const int nMultClasses = 12;
const int nTrigClasses = 5;

void Analysis_phi2_kappa::Begin(TTree * /*tree*/)
{
   // The Begin() function is called at the start of the query.
   // When running with PROOF Begin() is only called on the client.
   // The tree argument is deprecated (on PROOF 0 is passed).
 
   Reset();

}

void Analysis_phi2_kappa::SlaveBegin(TTree *tree)
{
   // The SlaveBegin() function is called after the Begin() function.
   // When running with PROOF SlaveBegin() is called on each slave server.
   // The tree argument is deprecated (on PROOF 0 is passed).
  
   // Initialize the Tree branch adresses.
   Init(tree);    		

   BookHisto();
}

Bool_t Analysis_phi2_kappa::Process(Long64_t entry)
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
   int nMBCh[nMultClasses+1] = {}, nphiCh[nMultClasses+1] = {};
   double kappa_K0s[nTrigClasses][nMultClasses+1] = {}, kappa_Xi[nTrigClasses][nMultClasses+1] = {};
   double kappa_Omega[nTrigClasses][nMultClasses+1] = {}, kappa_Lambda[nTrigClasses][nMultClasses+1] = {};
   double kappa_Proton[nTrigClasses][nMultClasses+1] = {}, kappa_Phi[nTrigClasses][nMultClasses+1] = {}, kappa_Pion[nTrigClasses][nMultClasses+1] = {};
  
   if (trig) {
     // Count the number of charged particles and phis 
     vector<unsigned int> Phitag;
     for (unsigned int p=0; p<tracks_charge.GetSize(); ++p) {
       if (tracks_charge[p] && ((tracks_eta[p]>-3.7 && tracks_eta[p]<-1.7) || (tracks_eta[p]>2.8 && tracks_eta[p]<5.1))) ++nCharged;
       if (abs(tracks_y[p])<2.5 && tracks_pid[p] == 333) Phitag.push_back(p);
     }	

     ///////////////////12MULTIPLICITYCLASSES//////////////////
     double xBin[nMultClasses+1] = {-0.5, 16.5, 24.5, 30.5, 38.5, 49.5, 56.5, 65.5, 78.5, 92.5, 101.5, 108.5, 185.5};  // Pythia CR+Ropes at 13.0 TeV 
     
     // Charged particles, strange hadrons and pions over the entire multiplicity distribution, without classes MB
     for (unsigned int p=0; p<tracks_charge.GetSize(); ++p) {									
       if (tracks_charge[p] && abs(tracks_eta[p])<0.5) nMBCh[0]++;
       if (abs(tracks_y[p])<2.5) {
	 if (tracks_had[p]) h_PID[0]->Fill(abs(tracks_pid[p]));   
         if (tracks_had[p] && abs(tracks_pid[p])!=333) h_kappa_hadron[0][0]->Fill(tracks_kappa[p]);	 
	 //if (abs(tracks_pid[p])==310 || abs(tracks_pid[p])==3312 || abs(tracks_pid[p])==3334 || abs(tracks_pid[p])==3122) h_kappa_hadron[0][0]->Fill(tracks_kappa[p]);
         if (abs(tracks_pid[p])==310) h_kappa_K0s[0][0]->Fill(tracks_kappa[p]);
         if (abs(tracks_pid[p])==3312) h_kappa_Xi[0][0]->Fill(tracks_kappa[p]);
         if (abs(tracks_pid[p])==3334) h_kappa_Omega[0][0]->Fill(tracks_kappa[p]);
         if (abs(tracks_pid[p])==3122) h_kappa_Lambda[0][0]->Fill(tracks_kappa[p]);
         if (abs(tracks_pid[p])==2212) h_kappa_Proton[0][0]->Fill(tracks_kappa[p]);
         if (abs(tracks_pid[p])==333) h_kappa_Phi[0][0]->Fill(tracks_kappa[p]);
         if (abs(tracks_pid[p])==211) h_kappa_Pion[0][0]->Fill(tracks_kappa[p]);
       }
     }
     h_nMBCh[0]->Fill(nMBCh[0]);

     // Charged particles, strange hadrons and pions per class MB
     for (int j=1; j<nMultClasses+1; j++) {
       // Check what the multiplicty class of the event is
       if (nCharged>=xBin[j-1] && nCharged<=xBin[j]) {
         for (unsigned int p=0; p<tracks_charge.GetSize(); ++p) {
           if (tracks_charge[p] && abs(tracks_eta[p])<0.5) nMBCh[j]++;
           if (abs(tracks_y[p])<2.5) {
	     if (tracks_had[p] && abs(tracks_pid[p])!=333) h_kappa_hadron[0][j]->Fill(tracks_kappa[p]);	   
	     //if (abs(tracks_pid[p])==310 || abs(tracks_pid[p])==3312 || abs(tracks_pid[p])==3334 || abs(tracks_pid[p])==3122) h_kappa_hadron[0][j]->Fill(tracks_kappa[p]);
             if (abs(tracks_pid[p])==310) h_kappa_K0s[0][j]->Fill(tracks_kappa[p]);
             if (abs(tracks_pid[p])==3312) h_kappa_Xi[0][j]->Fill(tracks_kappa[p]);
             if (abs(tracks_pid[p])==3334) h_kappa_Omega[0][j]->Fill(tracks_kappa[p]);
             if (abs(tracks_pid[p])==3122) h_kappa_Lambda[0][j]->Fill(tracks_kappa[p]);
             if (abs(tracks_pid[p])==2212) h_kappa_Proton[0][j]->Fill(tracks_kappa[p]);
             if (abs(tracks_pid[p])==333) h_kappa_Phi[0][j]->Fill(tracks_kappa[p]);
             if (abs(tracks_pid[p])==211) h_kappa_Pion[0][j]->Fill(tracks_kappa[p]);
           }
         }
         h_nMBCh[j]->Fill(nMBCh[j]);
         break;
       }
     }


     if (Phitag.size()>0) {
       // Charged particles, strange hadrons and pions over the entire multiplicity distribution, without classes with phi trigger	     
       for (unsigned int p=0; p<tracks_charge.GetSize(); ++p) {						
         if (tracks_charge[p] && abs(tracks_eta[p])<0.5) nphiCh[0]++;
         if (abs(tracks_y[p])<2.5) {
           unsigned int best_tag = Phitag[0];
           for (unsigned int tag=0; tag<Phitag.size(); tag++) {
             best_tag = (abs(tracks_y[p]-tracks_y[Phitag[tag]])<abs(tracks_y[p]-tracks_y[best_tag]) ? Phitag[tag] : best_tag);
           }
           if (abs(tracks_y[p]-tracks_y[best_tag])<0.1) {
	     if (tracks_had[p]) h_PID[1]->Fill(abs(tracks_pid[p]));	  
	     if (tracks_had[p] && abs(tracks_pid[p])!=333) h_kappa_hadron[1][0]->Fill(tracks_kappa[p]);
	     //if (abs(tracks_pid[p])==310 || abs(tracks_pid[p])==3312 || abs(tracks_pid[p])==3334 || abs(tracks_pid[p])==3122) h_kappa_hadron[1][0]->Fill(tracks_kappa[p]); 
             if (abs(tracks_pid[p])==311) h_kappa_K0s[1][0]->Fill(tracks_kappa[p]);
             if (abs(tracks_pid[p])==3312) h_kappa_Xi[1][0]->Fill(tracks_kappa[p]);
             if (abs(tracks_pid[p])==3334) h_kappa_Omega[1][0]->Fill(tracks_kappa[p]);
             if (abs(tracks_pid[p])==3122) h_kappa_Lambda[1][0]->Fill(tracks_kappa[p]);
             if (abs(tracks_pid[p])==2212) h_kappa_Proton[1][0]->Fill(tracks_kappa[p]);
             if (abs(tracks_pid[p])==333) h_kappa_Phi[1][0]->Fill(tracks_kappa[p]);
             if (abs(tracks_pid[p])==211) h_kappa_Pion[1][0]->Fill(tracks_kappa[p]);
           }
           if (abs(tracks_y[p]-tracks_y[best_tag])<0.5) {
             if (tracks_had[p]) h_PID[2]->Fill(abs(tracks_pid[p]));	
             if (tracks_had[p] && abs(tracks_pid[p])!=333) h_kappa_hadron[2][0]->Fill(tracks_kappa[p]);	     
	     //if (abs(tracks_pid[p])==310 || abs(tracks_pid[p])==3312 || abs(tracks_pid[p])==3334 || abs(tracks_pid[p])==3122) h_kappa_hadron[2][0]->Fill(tracks_kappa[p]);
             if (abs(tracks_pid[p])==310) h_kappa_K0s[2][0]->Fill(tracks_kappa[p]);
             if (abs(tracks_pid[p])==3312) h_kappa_Xi[2][0]->Fill(tracks_kappa[p]);
             if (abs(tracks_pid[p])==3334) h_kappa_Omega[2][0]->Fill(tracks_kappa[p]);
             if (abs(tracks_pid[p])==3122) h_kappa_Lambda[2][0]->Fill(tracks_kappa[p]);
             if (abs(tracks_pid[p])==2212) h_kappa_Proton[2][0]->Fill(tracks_kappa[p]);
             if (abs(tracks_pid[p])==333) h_kappa_Phi[2][0]->Fill(tracks_kappa[p]);
             if (abs(tracks_pid[p])==211) h_kappa_Pion[2][0]->Fill(tracks_kappa[p]);
           } 									
           if (abs(tracks_y[p]-tracks_y[best_tag])<1.0) {
             if (tracks_had[p]) h_PID[3]->Fill(abs(tracks_pid[p]));	   
	     if (tracks_had[p] && abs(tracks_pid[p])!=333) h_kappa_hadron[3][0]->Fill(tracks_kappa[p]);
	     //if (abs(tracks_pid[p])==310 || abs(tracks_pid[p])==3312 || abs(tracks_pid[p])==3334 || abs(tracks_pid[p])==3122) h_kappa_hadron[3][0]->Fill(tracks_kappa[p]);
             if (abs(tracks_pid[p])==310) h_kappa_K0s[3][0]->Fill(tracks_kappa[p]);
             if (abs(tracks_pid[p])==3312) h_kappa_Xi[3][0]->Fill(tracks_kappa[p]);
             if (abs(tracks_pid[p])==3334) h_kappa_Omega[3][0]->Fill(tracks_kappa[p]);
             if (abs(tracks_pid[p])==3122) h_kappa_Lambda[3][0]->Fill(tracks_kappa[p]);
             if (abs(tracks_pid[p])==2212) h_kappa_Proton[3][0]->Fill(tracks_kappa[p]);
             if (abs(tracks_pid[p])==333) h_kappa_Phi[3][0]->Fill(tracks_kappa[p]);
             if (abs(tracks_pid[p])==211) h_kappa_Pion[3][0]->Fill(tracks_kappa[p]);
           } 									
	   if (tracks_had[p]) h_PID[4]->Fill(abs(tracks_pid[p]));
	   if (tracks_had[p] && abs(tracks_pid[p])!=333) h_kappa_hadron[4][0]->Fill(tracks_kappa[p]);
	   //if (abs(tracks_pid[p])==310 || abs(tracks_pid[p])==3312 || abs(tracks_pid[p])==3334 || abs(tracks_pid[p])==3122) h_kappa_hadron[4][0]->Fill(tracks_kappa[p]);
           if (abs(tracks_pid[p])==310) h_kappa_K0s[4][0]->Fill(tracks_kappa[p]);
           if (abs(tracks_pid[p])==3312) h_kappa_Xi[4][0]->Fill(tracks_kappa[p]);
           if (abs(tracks_pid[p])==3334) h_kappa_Omega[4][0]->Fill(tracks_kappa[p]);
           if (abs(tracks_pid[p])==3122) h_kappa_Lambda[4][0]->Fill(tracks_kappa[p]);
           if (abs(tracks_pid[p])==2212) h_kappa_Proton[4][0]->Fill(tracks_kappa[p]);
           if (abs(tracks_pid[p])==333) h_kappa_Phi[4][0]->Fill(tracks_kappa[p]);
           if (abs(tracks_pid[p])==211) h_kappa_Pion[4][0]->Fill(tracks_kappa[p]);
         }
       } 
       h_nphiCh[0]->Fill(nphiCh[0]);

       // Charged particles, strange hadrons and pions per class with phi trigger 
       for (int j=1; j<nMultClasses+1; j++) {
         // Check what the multiplicty class of the event is
         if (nCharged>=xBin[j-1] && nCharged<=xBin[j]) {
           for (unsigned int p=0; p<tracks_charge.GetSize(); ++p) {
             if (tracks_charge[p] && abs(tracks_eta[p])<0.5) nphiCh[j]++;
             if (abs(tracks_y[p])<2.5) {
               unsigned int best_tag = Phitag[0];
               for (unsigned int tag=0; tag<Phitag.size(); tag++) {
                 best_tag = (abs(tracks_y[p]-tracks_y[Phitag[tag]])<abs(tracks_y[p]-tracks_y[best_tag]) ? Phitag[tag] : best_tag);
               }
               if (abs(tracks_y[p]-tracks_y[best_tag])<0.1) {
		 if (tracks_had[p] && abs(tracks_pid[p])!=333) h_kappa_hadron[1][j]->Fill(tracks_kappa[p]);      
                 //if (abs(tracks_pid[p])==310 || abs(tracks_pid[p])==3312 || abs(tracks_pid[p])==3334 || abs(tracks_pid[p])==3122) h_kappa_hadron[1][j]->Fill(tracks_kappa[p]);
		 if (abs(tracks_pid[p])==310) h_kappa_K0s[1][j]->Fill(tracks_kappa[p]);
                 if (abs(tracks_pid[p])==3312) h_kappa_Xi[1][j]->Fill(tracks_kappa[p]);
                 if (abs(tracks_pid[p])==3334) h_kappa_Omega[1][j]->Fill(tracks_kappa[p]);
                 if (abs(tracks_pid[p])==3122) h_kappa_Lambda[1][j]->Fill(tracks_kappa[p]);
                 if (abs(tracks_pid[p])==2212) h_kappa_Proton[1][j]->Fill(tracks_kappa[p]);
                 if (abs(tracks_pid[p])==333) h_kappa_Phi[1][j]->Fill(tracks_kappa[p]);
                 if (abs(tracks_pid[p])==211) h_kappa_Pion[1][j]->Fill(tracks_kappa[p]);
               }
               if (abs(tracks_y[p]-tracks_y[best_tag])<0.5) {
		 if (tracks_had[p] && abs(tracks_pid[p])!=333) h_kappa_hadron[2][j]->Fill(tracks_kappa[p]);      
		 //if (abs(tracks_pid[p])==310 || abs(tracks_pid[p])==3312 || abs(tracks_pid[p])==3334 || abs(tracks_pid[p])==3122) h_kappa_hadron[2][j]->Fill(tracks_kappa[p]);
                 if (abs(tracks_pid[p])==310) h_kappa_K0s[2][j]->Fill(tracks_kappa[p]);
                 if (abs(tracks_pid[p])==3312) h_kappa_Xi[2][j]->Fill(tracks_kappa[p]);
                 if (abs(tracks_pid[p])==3334) h_kappa_Omega[2][j]->Fill(tracks_kappa[p]);
                 if (abs(tracks_pid[p])==3122) h_kappa_Lambda[2][j]->Fill(tracks_kappa[p]);
                 if (abs(tracks_pid[p])==2212) h_kappa_Proton[2][j]->Fill(tracks_kappa[p]);
                 if (abs(tracks_pid[p])==333) h_kappa_Phi[2][j]->Fill(tracks_kappa[p]);
                 if (abs(tracks_pid[p])==211) h_kappa_Pion[2][j]->Fill(tracks_kappa[p]);
               }
               if (abs(tracks_y[p]-tracks_y[best_tag])<1.0) {
		 if (tracks_had[p]) h_kappa_hadron[3][j]->Fill(tracks_kappa[p]);      
		 //if (abs(tracks_pid[p])==310 || abs(tracks_pid[p])==3312 || abs(tracks_pid[p])==3334 || abs(tracks_pid[p])==3122) h_kappa_hadron[3][j]->Fill(tracks_kappa[p]);
                 if (abs(tracks_pid[p])==310) h_kappa_K0s[3][j]->Fill(tracks_kappa[p]);
                 if (abs(tracks_pid[p])==3312) h_kappa_Xi[3][j]->Fill(tracks_kappa[p]);
                 if (abs(tracks_pid[p])==3334) h_kappa_Omega[3][j]->Fill(tracks_kappa[p]);
                 if (abs(tracks_pid[p])==3122) h_kappa_Lambda[3][j]->Fill(tracks_kappa[p]);
                 if (abs(tracks_pid[p])==2212) h_kappa_Proton[3][j]->Fill(tracks_kappa[p]);
                 if (abs(tracks_pid[p])==333) h_kappa_Phi[3][j]->Fill(tracks_kappa[p]);
                 if (abs(tracks_pid[p])==211) h_kappa_Pion[3][j]->Fill(tracks_kappa[p]);
               }
	       if (tracks_had[p] && abs(tracks_pid[p])!=333) h_kappa_hadron[4][j]->Fill(tracks_kappa[p]);
	       //if (abs(tracks_pid[p])==310 || abs(tracks_pid[p])==3312 || abs(tracks_pid[p])==3334 || abs(tracks_pid[p])==3122) h_kappa_hadron[4][j]->Fill(tracks_kappa[p]);
               if (abs(tracks_pid[p])==310) h_kappa_K0s[4][j]->Fill(tracks_kappa[p]);
               if (abs(tracks_pid[p])==3312) h_kappa_Xi[4][j]->Fill(tracks_kappa[p]);
               if (abs(tracks_pid[p])==3334) h_kappa_Omega[4][j]->Fill(tracks_kappa[p]);
               if (abs(tracks_pid[p])==3122) h_kappa_Lambda[4][j]->Fill(tracks_kappa[p]);
               if (abs(tracks_pid[p])==2212) h_kappa_Proton[4][j]->Fill(tracks_kappa[p]);
               if (abs(tracks_pid[p])==333) h_kappa_Phi[4][j]->Fill(tracks_kappa[p]);
               if (abs(tracks_pid[p])==211) h_kappa_Pion[4][j]->Fill(tracks_kappa[p]);
             }
           }
           h_nphiCh[j]->Fill(nphiCh[j]);
           break;
         }
       }	   
     }     
   }

   return kTRUE;
}

void Analysis_phi2_kappa::SlaveTerminate()
{
   // The SlaveTerminate() function is called after all entries or objects
   // have been processed. When running with PROOF SlaveTerminate() is called
   // on each slave server.

}

void Analysis_phi2_kappa::Terminate()
{
   // The Terminate() function is the last function to be called during
   // a query. It always runs on the client, it can be used to present
   // the results graphically or save the results to file.
																	
   string filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_kappa_results/analysis_phi_kappa2.root";
   TFile* file2 = new TFile(filename2.c_str(), "RECREATE");							
   CollectHisto(file2);
   file2->Close();
}
