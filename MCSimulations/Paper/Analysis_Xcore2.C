#define Analysis_Xcore2_cxx
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
// root> T->Process("Analysis_Xcore2.C")
// root> T->Process("Analysis_Xcore2.C","some options")
// root> T->Process("Analysis_Xcore2.C+")
//


#include "Analysis_Xcore2.h"
#include <TStyle.h>
#include <TH1.h>
#include <TCanvas.h>
#include <Riostream.h>
#include <vector>

using namespace std;

const int nMultClasses = 12;
const int nTrigClasses = 10;

void Analysis_Xcore2::Begin(TTree * /*tree*/)
{
   // The Begin() function is called at the start of the query.
   // When running with PROOF Begin() is only called on the client.
   // The tree argument is deprecated (on PROOF 0 is passed).
 
   Reset();
}

void Analysis_Xcore2::SlaveBegin(TTree *tree)
{
   // The SlaveBegin() function is called after the Begin() function.
   // When running with PROOF SlaveBegin() is called on each slave server.
   // The tree argument is deprecated (on PROOF 0 is passed).
  
   // Initialize the Tree branch adresses.
   Init(tree);    		

   // Create dN_ch/deta histograms.
   BookHisto();
}

Bool_t Analysis_Xcore2::Process(Long64_t entry)
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
   int nChminbias[nMultClasses] = {}, nChphi[nMultClasses] = {};
   int nhadron[nTrigClasses][nMultClasses] = {}, nK0s[nTrigClasses][nMultClasses] = {};
   int nXi[nTrigClasses][nMultClasses] = {}, nOmega[nTrigClasses][nMultClasses] = {}, nLambda[nTrigClasses][nMultClasses] = {};
   int nProton[nTrigClasses][nMultClasses] = {}, nPhi[nTrigClasses][nMultClasses] = {}, nPion[nTrigClasses][nMultClasses] = {}; 

   // Check the trigger condition
   if (trig) {
     // Count the number of charged particles	   
     for (unsigned int p=0; p<tracks_charge.GetSize(); ++p) {
       if (tracks_charge[p] && ((tracks_eta[p]>-3.7 && tracks_eta[p]<-1.7) || (tracks_eta[p]>2.8 && tracks_eta[p]<5.1))) ++nCharged;
     }
													
     double xBin[nMultClasses+1] = {-0.5, 19.5, 29.5, 36.5, 43.5, 51.5, 56.5, 63.5, 74.5, 86.5, 97.5, 105.5, 190.5};  // EPOS4 at 13.0 TeV
													
     for (int j=0; j<nMultClasses; j++) {
       // Check what the multiplicty class of the event is
       if (nCharged>=xBin[j] && nCharged<=xBin[j+1]) {
         for (unsigned int p=0; p<tracks_charge.GetSize(); ++p) {
           if (tracks_charge[p] && abs(tracks_eta[p])<0.5) nChminbias[j]++;
	   if (abs(tracks_y[p])<2.5) {
	     if (tracks_pity[p]==60) {
	       if (tracks_had[p]) nhadron[0][j]++;	     
	       if (abs(tracks_pid[p])==310) nK0s[0][j]++;
               if (abs(tracks_pid[p])==3312) nXi[0][j]++;
               if (abs(tracks_pid[p])==3334) nOmega[0][j]++;
               if (abs(tracks_pid[p])==3122) nLambda[0][j]++;
               if (abs(tracks_pid[p])==2212) nProton[0][j]++;
               if (abs(tracks_pid[p])==333) nPhi[0][j]++;
               if (abs(tracks_pid[p])==211) nPion[0][j]++;
             }
	     if (tracks_had[p]) nhadron[1][j]++;
	     if (abs(tracks_pid[p])==310) nK0s[1][j]++;
             if (abs(tracks_pid[p])==3312) nXi[1][j]++;
             if (abs(tracks_pid[p])==3334) nOmega[1][j]++;
             if (abs(tracks_pid[p])==3122) nLambda[1][j]++;
             if (abs(tracks_pid[p])==2212) nProton[1][j]++;
             if (abs(tracks_pid[p])==333) nPhi[1][j]++;
             if (abs(tracks_pid[p])==211) nPion[1][j]++;
	   }
	 }
         h_nChminbias[j]->Fill(nChminbias[j]);
         for (int i = 0; i < 2; i++) {
	   h_nhadron[i][j]->Fill(nhadron[i][j]);	 
           h_nK0s[i][j]->Fill(nK0s[i][j]);
           h_nXi[i][j]->Fill(nXi[i][j]);
           h_nOmega[i][j]->Fill(nOmega[i][j]);
           h_nLambda[i][j]->Fill(nLambda[i][j]);
           h_nProton[i][j]->Fill(nProton[i][j]);
           h_nPhi[i][j]->Fill(nPhi[i][j]);
           h_nPion[i][j]->Fill(nPion[i][j]);
         }	 

         vector<unsigned int> Phitag;
         for (unsigned int p=0; p<tracks_charge.GetSize(); ++p) {
           if (abs(tracks_y[p])<2.5 && tracks_pid[p]==333) Phitag.push_back(p);
         }
         if (Phitag.size()>0) {
	   for (unsigned int p=0; p<tracks_charge.GetSize(); ++p) {
	     if (tracks_charge[p] && abs(tracks_eta[p])<0.5) nChphi[j]++;
	     if (abs(tracks_y[p])<2.5) {
	       unsigned int best_tag = Phitag[0];
               for (unsigned int tag=0; tag<Phitag.size(); tag++) {
                 best_tag = (abs(tracks_y[p]-tracks_y[Phitag[tag]])<abs(tracks_y[p]-tracks_y[best_tag]) ? Phitag[tag] : best_tag);
	       }	 
	       if (tracks_pity[p]==60) {
  	         if (abs(tracks_y[p]-tracks_y[best_tag])<0.1) {
       		   if (tracks_had[p]) nhadron[2][j]++;			 
                   if (abs(tracks_pid[p])==310) nK0s[2][j]++;
                   if (abs(tracks_pid[p])==3312) nXi[2][j]++;
                   if (abs(tracks_pid[p])==3334) nOmega[2][j]++;
                   if (abs(tracks_pid[p])==3122) nLambda[2][j]++;
                   if (abs(tracks_pid[p])==2212) nProton[2][j]++;
                   if (abs(tracks_pid[p])==333) nPhi[2][j]++;
                   if (abs(tracks_pid[p])==211) nPion[2][j]++;
		 }
		 if (abs(tracks_y[p]-tracks_y[best_tag])<0.5) {
	           if (tracks_had[p]) nhadron[4][j]++;		 
		   if (abs(tracks_pid[p])==310) nK0s[4][j]++;
                   if (abs(tracks_pid[p])==3312) nXi[4][j]++;
                   if (abs(tracks_pid[p])==3334) nOmega[4][j]++;
                   if (abs(tracks_pid[p])==3122) nLambda[4][j]++;
                   if (abs(tracks_pid[p])==2212) nProton[4][j]++;
                   if (abs(tracks_pid[p])==333) nPhi[4][j]++;
                   if (abs(tracks_pid[p])==211) nPion[4][j]++;
	         }	 
                 if (abs(tracks_y[p]-tracks_y[best_tag])<1.0) {
		   if (tracks_had[p]) nhadron[6][j]++;	 
		   if (abs(tracks_pid[p])==310) nK0s[6][j]++;
                   if (abs(tracks_pid[p])==3312) nXi[6][j]++;
                   if (abs(tracks_pid[p])==3334) nOmega[6][j]++;
                   if (abs(tracks_pid[p])==3122) nLambda[6][j]++;
                   if (abs(tracks_pid[p])==2212) nProton[6][j]++;
                   if (abs(tracks_pid[p])==333) nPhi[6][j]++;
                   if (abs(tracks_pid[p])==211) nPion[6][j]++;
	         }	 
		 if (tracks_had[p]) nhadron[8][j]++;
		 if (abs(tracks_pid[p])==310) nK0s[8][j]++;
                 if (abs(tracks_pid[p])==3312) nXi[8][j]++;
                 if (abs(tracks_pid[p])==3334) nOmega[8][j]++;
                 if (abs(tracks_pid[p])==3122) nLambda[8][j]++;
                 if (abs(tracks_pid[p])==2212) nProton[8][j]++;
                 if (abs(tracks_pid[p])==333) nPhi[8][j]++;
                 if (abs(tracks_pid[p])==211) nPion[8][j]++;		
	       }
               if (abs(tracks_y[p]-tracks_y[best_tag])<0.1) {
		 if (tracks_had[p]) nhadron[3][j]++;      
		 if (abs(tracks_pid[p])==310) nK0s[3][j]++;
                 if (abs(tracks_pid[p])==3312) nXi[3][j]++;
                 if (abs(tracks_pid[p])==3334) nOmega[3][j]++;
                 if (abs(tracks_pid[p])==3122) nLambda[3][j]++;
                 if (abs(tracks_pid[p])==2212) nProton[3][j]++;
                 if (abs(tracks_pid[p])==333) nPhi[3][j]++;
                 if (abs(tracks_pid[p])==211) nPion[3][j]++;
	       }	 
               if (abs(tracks_y[p]-tracks_y[best_tag])<0.5) {
		 if (tracks_had[p]) nhadron[5][j]++;      
		 if (abs(tracks_pid[p])==310) nK0s[5][j]++;
                 if (abs(tracks_pid[p])==3312) nXi[5][j]++;
                 if (abs(tracks_pid[p])==3334) nOmega[5][j]++;
                 if (abs(tracks_pid[p])==3122) nLambda[5][j]++;
                 if (abs(tracks_pid[p])==2212) nProton[5][j]++;
                 if (abs(tracks_pid[p])==333) nPhi[5][j]++;
                 if (abs(tracks_pid[p])==211) nPion[5][j]++;
	       }	 
               if (abs(tracks_y[p]-tracks_y[best_tag])<1.0) {
		 if (tracks_had[p]) nhadron[7][j]++;      
                 if (abs(tracks_pid[p])==310) nK0s[7][j]++;
                 if (abs(tracks_pid[p])==3312) nXi[7][j]++;
                 if (abs(tracks_pid[p])==3334) nOmega[7][j]++;
                 if (abs(tracks_pid[p])==3122) nLambda[7][j]++;
                 if (abs(tracks_pid[p])==2212) nProton[7][j]++;
                 if (abs(tracks_pid[p])==333) nPhi[7][j]++;
                 if (abs(tracks_pid[p])==211) nPion[7][j]++;
	       }	 
	       if (tracks_had[p]) nhadron[9][j]++;
	       if (abs(tracks_pid[p])==310) nK0s[9][j]++;
               if (abs(tracks_pid[p])==3312) nXi[9][j]++;
               if (abs(tracks_pid[p])==3334) nOmega[9][j]++;
               if (abs(tracks_pid[p])==3122) nLambda[9][j]++;
               if (abs(tracks_pid[p])==2212) nProton[9][j]++;
               if (abs(tracks_pid[p])==333) nPhi[9][j]++;
               if (abs(tracks_pid[p])==211) nPion[9][j]++;
             }     
	   }
	   h_nChphi[j]->Fill(nChphi[j]);
           for (int i = 2; i < nTrigClasses; i++) {
	     h_nhadron[i][j]->Fill(nhadron[i][j]);	   
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
   }   

   return kTRUE;
}

void Analysis_Xcore2::SlaveTerminate()
{
   // The SlaveTerminate() function is called after all entries or objects
   // have been processed. When running with PROOF SlaveTerminate() is called
   // on each slave server.

}

void Analysis_Xcore2::Terminate()
{
   // The Terminate() function is the last function to be called during
   // a query. It always runs on the client, it can be used to present
   // the results graphically or save the results to file.
																						
   string filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Xcore_evenmoreclasses_results2/analysis_Xcore_had_epos.root";	
   TFile* file2 = new TFile(filename2.c_str(), "RECREATE");									
   CollectHisto(file2);
   file2->Close();
}

