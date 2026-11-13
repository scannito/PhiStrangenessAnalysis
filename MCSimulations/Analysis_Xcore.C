#define Analysis_Xcore_cxx
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
// root> T->Process("Analysis_Xcore.C")
// root> T->Process("Analysis_Xcore.C","some options")
// root> T->Process("Analysis_Xcore.C+")
//


#include "Analysis_Xcore.h"
#include <TStyle.h>
#include <TH1.h>
#include <TCanvas.h>
#include <Riostream.h>
#include <vector>

using namespace std;

void Analysis_Xcore::Begin(TTree * /*tree*/)
{
   // The Begin() function is called at the start of the query.
   // When running with PROOF Begin() is only called on the client.
   // The tree argument is deprecated (on PROOF 0 is passed).
 
   Reset();
}

void Analysis_Xcore::SlaveBegin(TTree *tree)
{
   // The SlaveBegin() function is called after the Begin() function.
   // When running with PROOF SlaveBegin() is called on each slave server.
   // The tree argument is deprecated (on PROOF 0 is passed).
  
   // Initialize the Tree branch adresses.
   Init(tree);    		

   // Create dN_ch/deta histograms.
   BookHisto();
}

Bool_t Analysis_Xcore::Process(Long64_t entry)
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
   int Evminbias[nMultClasses] = {}, Evphi[nMultClasses] = {}; 

   // Check the trigger condition
   if (trig) {
     // Count the number of charged particles	   
     for (unsigned int p=0; p<tracks_charge.GetSize(); ++p) {
       if (tracks_charge[p] && ((tracks_eta[p]>-3.7 && tracks_eta[p]<-1.7) || (tracks_eta[p]>2.8 && tracks_eta[p]<5.1))) ++nCharged;
     }
													
     double xBin[nMultClasses+1] = {-0.5, 19.5, 29.5, 36.5, 43.5, 51.5, 56.5, 63.5, 74.5, 86.5, 97.5, 180.5};  // EPOS4 at 13.0 TeV

     for (int j=0; j<nMultClasses; j++) {
       // Check what the multiplicty class of the event is
       if (nCharged>=xBin[j] && nCharged<=xBin[j+1]) {
         //Evminbias[j] = 1;
         h_Evminbias[j]->Fill(1);	 
         for (unsigned int p=0; p<tracks_charge.GetSize(); ++p) {
           if (tracks_charge[p] && abs(tracks_eta[p])<0.5) h_dndetaminbias[j]->Fill(tracks_eta[p]);
	   if (abs(tracks_y[p])<2.5) {
	     if (tracks_pity[p]==60) FillHisto(0,j,p);
	     if (tracks_pity[p]>=20 && tracks_pity[p]<=29) FillHisto(1,j,p);
	     if (tracks_pity[p]>=30 && tracks_pity[p]<=39) FillHisto(2,j,p);
	     if (tracks_pity[p]>=40 && tracks_pity[p]<=59) FillHisto(3,j,p);
	     if (tracks_pity[p]>=20 && tracks_pity[p]<=59) FillHisto(4,j,p);
             FillHisto(5,j,p);
	   }
	 }		     
         break;
       }	 
     }

     for (int j=0; j<nMultClasses; j++) {
       // Check what the multiplicty class of the event is
       if (nCharged>=xBin[j] && nCharged<=xBin[j+1]) {

         vector<unsigned int> Phitag;
         for (unsigned int p=0; p<tracks_charge.GetSize(); ++p) {
           if (abs(tracks_y[p])<2.5 && tracks_pid[p]==333) Phitag.push_back(p);
         }

         if (Phitag.size()>0) {
           //Evphi[j] = 1;
           h_Evphi[j]->Fill(1);
	   for (unsigned int p=0; p<tracks_charge.GetSize(); ++p) {
	     if (tracks_charge[p] && abs(tracks_eta[p])<0.5) h_dndetaphi[j]->Fill(tracks_eta[p]);
	     if (abs(tracks_y[p])<2.5) {
	       unsigned int best_tag = Phitag[0];
               for (unsigned int tag=0; tag<Phitag.size(); tag++) {
                 best_tag = (abs(tracks_y[p]-tracks_y[Phitag[tag]])<abs(tracks_y[p]-tracks_y[best_tag]) ? Phitag[tag] : best_tag);
	       }	 

	       if (tracks_pity[p]==60) {
  	         if (abs(tracks_y[p]-tracks_y[best_tag])<0.1) FillHisto(6,j,p);		       
	         if (abs(tracks_y[p]-tracks_y[best_tag])<0.5) FillHisto(12,j,p);      
                 if (abs(tracks_y[p]-tracks_y[best_tag])<1.0) FillHisto(18,j,p);      
	         FillHisto(24,j,p);
	       }

	       if (tracks_pity[p]>=20 && tracks_pity[p]<=29) {
                 if (abs(tracks_y[p]-tracks_y[best_tag])<0.1) FillHisto(7,j,p);
                 if (abs(tracks_y[p]-tracks_y[best_tag])<0.5) FillHisto(13,j,p);
                 if (abs(tracks_y[p]-tracks_y[best_tag])<1.0) FillHisto(19,j,p);
	         FillHisto(25,j,p);
               } 

	       if (tracks_pity[p]>=30 && tracks_pity[p]<=39) {
                 if (abs(tracks_y[p]-tracks_y[best_tag])<0.1) FillHisto(8,j,p);
                 if (abs(tracks_y[p]-tracks_y[best_tag])<0.5) FillHisto(14,j,p);
                 if (abs(tracks_y[p]-tracks_y[best_tag])<1.0) FillHisto(20,j,p);
	         if (p!=best_tag) FillHisto(26,j,p);
               } 

	       if (tracks_pity[p]>=40 && tracks_pity[p]<=59) {
                 if (abs(tracks_y[p]-tracks_y[best_tag])<0.1) FillHisto(9,j,p);
                 if (abs(tracks_y[p]-tracks_y[best_tag])<0.5) FillHisto(15,j,p);
                 if (abs(tracks_y[p]-tracks_y[best_tag])<1.0) FillHisto(21,j,p);
                 FillHisto(27,j,p);
               }

	       if (tracks_pity[p]>=20 && tracks_pity[p]<=59) {
                 if (abs(tracks_y[p]-tracks_y[best_tag])<0.1) FillHisto(10,j,p);
                 if (abs(tracks_y[p]-tracks_y[best_tag])<0.5) FillHisto(16,j,p);
                 if (abs(tracks_y[p]-tracks_y[best_tag])<1.0) FillHisto(22,j,p);
                 FillHisto(28,j,p);
               }
             
               if (abs(tracks_y[p]-tracks_y[best_tag])<0.1) FillHisto(11,j,p);      
               if (abs(tracks_y[p]-tracks_y[best_tag])<0.5) FillHisto(17,j,p);      
               if (abs(tracks_y[p]-tracks_y[best_tag])<1.0) FillHisto(23,j,p);
	       FillHisto(29,j,p);
	       
             }     
	   }
	 }
         break;
       }
     }

   }   

   return kTRUE;
}

void Analysis_Xcore::SlaveTerminate()
{
   // The SlaveTerminate() function is called after all entries or objects
   // have been processed. When running with PROOF SlaveTerminate() is called
   // on each slave server.

}

void Analysis_Xcore::Terminate()
{
   // The Terminate() function is the last function to be called during
   // a query. It always runs on the client, it can be used to present
   // the results graphically or save the results to file.
															
   string filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Xcore_moreclasses_results/analysis_Xcoredetails_epos.root";	
   TFile* file2 = new TFile(filename2.c_str(), "RECREATE");									
												
   CollectHisto(file2);

   file2->Close();
}

