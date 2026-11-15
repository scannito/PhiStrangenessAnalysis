#define Analysis_phi_xi_cxx
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
// root> T->Process("Analysis_phi_xi.C")
// root> T->Process("Analysis_phi_xi.C","some options")
// root> T->Process("Analysis_phi_xi.C+")
//


#include "Analysis_phi_xi.h"
#include <TStyle.h>
#include <TH1.h>
#include <TCanvas.h>
#include <Riostream.h>
#include <vector>

using namespace std;

TH1I* h_Ev[9];
TH1D* h_dndeta[9];

TH1D* h_dndyK0s1[9];
TH1D* h_dndyXi1[9];
TH1D* h_dndyOmega1[9];
TH1D* h_dndyLambda1[9];
TH1D* h_dndyProton1[9];
TH1D* h_dndyPion1[9];

TH1D* h_dndyK0s2[9];
TH1D* h_dndyXi2[9];
TH1D* h_dndyOmega2[9];
TH1D* h_dndyLambda2[9];
TH1D* h_dndyProton2[9];
TH1D* h_dndyPion2[9];

TH1D* h_dndyK0s3[9];
TH1D* h_dndyXi3[9];
TH1D* h_dndyOmega3[9];
TH1D* h_dndyLambda3[9];
TH1D* h_dndyProton3[9];
TH1D* h_dndyPion3[9];

TH1D* h_dndyK0s4[9];
TH1D* h_dndyXi4[9];
TH1D* h_dndyOmega4[9];
TH1D* h_dndyLambda4[9];
TH1D* h_dndyProton4[9];
TH1D* h_dndyPion4[9];

void Analysis_phi_xi::Begin(TTree * /*tree*/)
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

void Analysis_phi_xi::SlaveBegin(TTree *tree)
{
   // The SlaveBegin() function is called after the Begin() function.
   // When running with PROOF SlaveBegin() is called on each slave server.
   // The tree argument is deprecated (on PROOF 0 is passed).
  
   // Initialize the Tree branch adresses.
   Init(tree);    		

   TString option = GetOption();

   // Create dN_ch/deta histograms.
   for (int i = 0; i < 9; i++) {
     h_Ev[i] = new TH1I(Form("h_Ev%i",i+1), "", 2, -0.5, 1.5);	
     h_dndeta[i] = new TH1D(Form("h_dndeta%i",i+1), "; #eta; dN_{ch}/d#eta", 100, -0.5, 0.5);
     
     fOutput->Add(h_Ev[i]);
     fOutput->Add(h_dndeta[i]);

     h_dndyK0s1[i] = new TH1D(Form("h_dndyK0s1_%i",i+1), "; y; dN_{#K_{S}^{0}}/dy", 100, -0.5, 0.5);
     h_dndyXi1[i] = new TH1D(Form("h_dndyXi1_%i",i+1), "; y; dN_{#Xi}/dy", 100, -0.5, 0.5);
     h_dndyOmega1[i] = new TH1D(Form("h_dndyOmega1_%i",i+1), "; y; dN_{#Omega}/dy", 100, -0.5, 0.5);
     h_dndyLambda1[i] = new TH1D(Form("h_dndyLambda1_%i",i+1), "; y; dN_{#Lambda}/dy", 100, -0.5, 0.5);
     h_dndyProton1[i] = new TH1D(Form("h_dndyProton1_%i",i+1), "; y; dN_{p}/dy", 100, -0.5, 0.5);
     h_dndyPion1[i] = new TH1D(Form("h_dndyPion1_%i",i+1), "; y; dN_{#pi}/dy", 100, -0.5, 0.5);

     fOutput->Add(h_dndyK0s1[i]);
     fOutput->Add(h_dndyXi1[i]);
     fOutput->Add(h_dndyOmega1[i]);
     fOutput->Add(h_dndyLambda1[i]);
     fOutput->Add(h_dndyProton1[i]);
     fOutput->Add(h_dndyPion1[i]);

     h_dndyK0s2[i] = new TH1D(Form("h_dndyK0s2_%i",i+1), "; y; dN_{#K_{S}^{0}}/dy", 100, -0.5, 0.5);
     h_dndyXi2[i] = new TH1D(Form("h_dndyXi2_%i",i+1), "; y; dN_{#Xi}/dy", 100, -0.5, 0.5);
     h_dndyOmega2[i] = new TH1D(Form("h_dndyOmega2_%i",i+1), "; y; dN_{#Omega}/dy", 100, -0.5, 0.5);
     h_dndyLambda2[i] = new TH1D(Form("h_dndyLambda2_%i",i+1), "; y; dN_{#Lambda}/dy", 100, -0.5, 0.5);
     h_dndyProton2[i] = new TH1D(Form("h_dndyProton2_%i",i+1), "; y; dN_{p}/dy", 100, -0.5, 0.5);
     h_dndyPion2[i] = new TH1D(Form("h_dndyPion2_%i",i+1), "; y; dN_{#pi}/dy", 100, -0.5, 0.5);

     fOutput->Add(h_dndyK0s2[i]);
     fOutput->Add(h_dndyXi2[i]);
     fOutput->Add(h_dndyOmega2[i]);
     fOutput->Add(h_dndyLambda2[i]);
     fOutput->Add(h_dndyProton2[i]);
     fOutput->Add(h_dndyPion2[i]);

     h_dndyK0s3[i] = new TH1D(Form("h_dndyK0s3_%i",i+1), "; y; dN_{#K_{S}^{0}}/dy", 100, -0.5, 0.5);
     h_dndyXi3[i] = new TH1D(Form("h_dndyXi3_%i",i+1), "; y; dN_{#Xi}/dy", 100, -0.5, 0.5);
     h_dndyOmega3[i] = new TH1D(Form("h_dndyOmega3_%i",i+1), "; y; dN_{#Omega}/dy", 100, -0.5, 0.5);
     h_dndyLambda3[i] = new TH1D(Form("h_dndyLambda3_%i",i+1), "; y; dN_{#Lambda}/dy", 100, -0.5, 0.5);
     h_dndyProton3[i] = new TH1D(Form("h_dndyProton3_%i",i+1), "; y; dN_{p}/dy", 100, -0.5, 0.5);
     h_dndyPion3[i] = new TH1D(Form("h_dndyPion3_%i",i+1), "; y; dN_{#pi}/dy", 100, -0.5, 0.5);

     fOutput->Add(h_dndyK0s3[i]);
     fOutput->Add(h_dndyXi3[i]);
     fOutput->Add(h_dndyOmega3[i]);
     fOutput->Add(h_dndyLambda3[i]);
     fOutput->Add(h_dndyProton3[i]);
     fOutput->Add(h_dndyPion3[i]);

     h_dndyK0s4[i] = new TH1D(Form("h_dndyK0s4_%i",i+1), "; y; dN_{#K_{S}^{0}}/dy", 100, -0.5, 0.5);
     h_dndyXi4[i] = new TH1D(Form("h_dndyXi4_%i",i+1), "; y; dN_{#Xi}/dy", 100, -0.5, 0.5);
     h_dndyOmega4[i] = new TH1D(Form("h_dndyOmega4_%i",i+1), "; y; dN_{#Omega}/dy", 100, -0.5, 0.5);
     h_dndyLambda4[i] = new TH1D(Form("h_dndyLambda4_%i",i+1), "; y; dN_{#Lambda}/dy", 100, -0.5, 0.5);
     h_dndyProton4[i] = new TH1D(Form("h_dndyProton4_%i",i+1), "; y; dN_{p}/dy", 100, -0.5, 0.5);
     h_dndyPion4[i] = new TH1D(Form("h_dndyPion4_%i",i+1), "; y; dN_{#pi}/dy", 100, -0.5, 0.5);

     fOutput->Add(h_dndyK0s4[i]);
     fOutput->Add(h_dndyXi4[i]);
     fOutput->Add(h_dndyOmega4[i]);
     fOutput->Add(h_dndyLambda4[i]);
     fOutput->Add(h_dndyProton4[i]);
     fOutput->Add(h_dndyPion4[i]);
   }     
}

Bool_t Analysis_phi_xi::Process(Long64_t entry)
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
   int Ev[9] = {};

   // Check the trigger condition
   if (trig) {
     // Count the number of charged particles	   
     for (unsigned int p=0; p<tracks_charge.GetSize(); ++p) {
       if (tracks_charge[p] && ((tracks_eta[p]>-3.7 && tracks_eta[p]<-1.7) || (tracks_eta[p]>2.8 && tracks_eta[p]<5.1))) ++nCharged;
     }
 
     ///////////////////RUN1/////////////////
     //double xBin[10] = {-0.5, 13.5, 20.5, 27.5, 36.5, 47.5, 56.5, 65.5, 79.5, 179.5};  // Pythia with default parameters at 13.0 TeV
     //double xBin[10] = {-0.5, 15.5, 23.5, 29.5, 37.5, 47.5, 55.5, 64.5, 77.5, 175.5};  // Pythia with CR+Ropes parameters at 13.0 TeV 
     //double xBin[10] = {-0.5, 18.5, 27.5, 34.5, 41.5, 49.5, 55.5, 61.5, 73.5, 147.5};  // EPOS4 at 13.0 TeV
     ///////////////////RUN2/////////////////
     //double xBin[10] = {-0.5, 13.5, 21.5, 27.5, 36.5, 47.5, 56.5, 65.5, 79.5, 197.5};  // Pythia with default parameters at 13.0 TeV
     double xBin[10] = {-0.5, 15.5, 23.5, 29.5, 37.5, 47.5, 55.5, 64.5, 77.5, 185.5};  // Pythia with CR+Ropes parameters at 13.0 TeV
     //double xBin[10] = {-0.5, 17.5, 28.5, 35.5, 42.5, 50.5, 56.5, 62.5, 73.5, 168.5};  // EPOS4 at 13.0 TeV
     
     for (int i=0; i<9; i++) {
       // Check what the multiplicty class of the event is
       if (nCharged>=xBin[i] && nCharged<=xBin[i+1]) { 
         
	 vector<unsigned int> Phitag;
	 vector<unsigned int> Lambdatag, Xitag, Omegatag, Protontag;
	 vector<unsigned int> AntiLambdatag, AntiXitag, AntiOmegatag, AntiProtontag;
	 for (unsigned int p=0; p<tracks_charge.GetSize(); ++p) {
	   if (abs(tracks_y[p])<2.5 && tracks_pid[p] == 333) Phitag.push_back(p);
	   
	   if (abs(tracks_y[p])<2.5 && tracks_pid[p] == 3122) Lambdatag.push_back(p);
	   if (abs(tracks_y[p])<2.5 && tracks_pid[p] == 3312) Xitag.push_back(p);
	   if (abs(tracks_y[p])<2.5 && tracks_pid[p] == 3334) Omegatag.push_back(p);
           if (abs(tracks_y[p])<2.5 && tracks_pid[p] == 2212) Protontag.push_back(p);

	   if (abs(tracks_y[p])<2.5 && tracks_pid[p] == -3122) AntiLambdatag.push_back(p);
           if (abs(tracks_y[p])<2.5 && tracks_pid[p] == -3312) AntiXitag.push_back(p);
           if (abs(tracks_y[p])<2.5 && tracks_pid[p] == -3334) AntiOmegatag.push_back(p);
	   if (abs(tracks_y[p])<2.5 && tracks_pid[p] == -2212) AntiProtontag.push_back(p);
	 }

	 bool trigPhi_Xi = kFALSE;
         if (Phitag.size()>0) {
	   for (unsigned int phitag=0; phitag<Phitag.size(); phitag++) {
             if (trigPhi_Xi) break;
	     
	     for (unsigned int xitag=0; xitag<Xitag.size(); xitag++) {
	       if (trigPhi_Xi) break;
	       double deltay1 = tracks_y[xitag]-tracks_y[phitag];	     
	       if (deltay1>-1. && deltay1<0.) {
		 for (unsigned int antilambdatag=0; antilambdatag<AntiLambdatag.size(); antilambdatag++) {
		   if (trigPhi_Xi) break;
		   double deltay2 = tracks_y[antilambdatag]-tracks_y[phitag];
		   if (deltay2>0. && deltay2<1.) trigPhi_Xi = kTRUE;
		 }
	         for (unsigned int antixitag=0; antixitag<AntiXitag.size(); antixitag++) {
	           if (trigPhi_Xi) break;		 
                   double deltay2 = tracks_y[antixitag]-tracks_y[phitag];
                   if (deltay2>0. && deltay2<1.) trigPhi_Xi = kTRUE;
                 }
	         for (unsigned int antiomegatag=0; antiomegatag<AntiOmegatag.size(); antiomegatag++) {
                   if (trigPhi_Xi) break;
	           double deltay2 = tracks_y[antiomegatag]-tracks_y[phitag];
                   if (deltay2>0. && deltay2<1.) trigPhi_Xi = kTRUE;
                 }
	         for (unsigned int antiprotontag=0; antiprotontag<AntiProtontag.size(); antiprotontag++) {
                   if (trigPhi_Xi) break;
                   double deltay2 = tracks_y[antiprotontag]-tracks_y[phitag];
                   if (deltay2>0. && deltay2<1.) trigPhi_Xi = kTRUE;
                 }	 
               }
	       if (deltay1>0. && deltay1<1.) {
                 for (unsigned int antilambdatag=0; antilambdatag<AntiLambdatag.size(); antilambdatag++) {
                   if (trigPhi_Xi) break;
	           double deltay2 = tracks_y[antilambdatag]-tracks_y[phitag];
                   if (deltay2>-1. && deltay2<0.) trigPhi_Xi = kTRUE;
                 }
                 for (unsigned int antixitag=0; antixitag<AntiXitag.size(); antixitag++) {
                   if (trigPhi_Xi) break;
		   double deltay2 = tracks_y[antixitag]-tracks_y[phitag];
                   if (deltay2>-1. && deltay2<0.) trigPhi_Xi = kTRUE;
                 }
                 for (unsigned int antiomegatag=0; antiomegatag<AntiOmegatag.size(); antiomegatag++) {
                   if (trigPhi_Xi) break;
		   double deltay2 = tracks_y[antiomegatag]-tracks_y[phitag];
                   if (deltay2>-1. && deltay2<0.) trigPhi_Xi = kTRUE;
                 }
		 for (unsigned int antiprotontag=0; antiprotontag<AntiProtontag.size(); antiprotontag++) {
                   if (trigPhi_Xi) break;
                   double deltay2 = tracks_y[antiprotontag]-tracks_y[phitag];
                   if (deltay2>-1. && deltay2<0.) trigPhi_Xi = kTRUE;
                 }
               }
	     }

             for (unsigned int antixitag=0; antixitag<AntiXitag.size(); antixitag++) {
               if (trigPhi_Xi) break;
               double deltay1 = tracks_y[antixitag]-tracks_y[phitag];
               if (deltay1>-1. && deltay1<0.) {
                 for (unsigned int lambdatag=0; lambdatag<Lambdatag.size(); lambdatag++) {
                   if (trigPhi_Xi) break;
                   double deltay2 = tracks_y[lambdatag]-tracks_y[phitag];
                   if (deltay2>0. && deltay2<1.) trigPhi_Xi = kTRUE;
                 }
                 for (unsigned int xitag=0; xitag<Xitag.size(); xitag++) {
                   if (trigPhi_Xi) break;
                   double deltay2 = tracks_y[xitag]-tracks_y[phitag];
                   if (deltay2>0. && deltay2<1.) trigPhi_Xi = kTRUE;
                 }
                 for (unsigned int omegatag=0; omegatag<Omegatag.size(); omegatag++) {
                   if (trigPhi_Xi) break;
                   double deltay2 = tracks_y[omegatag]-tracks_y[phitag];
                   if (deltay2>0. && deltay2<1.) trigPhi_Xi = kTRUE;
                 }
		 for (unsigned int protontag=0; protontag<Protontag.size(); protontag++) {
                   if (trigPhi_Xi) break;
                   double deltay2 = tracks_y[protontag]-tracks_y[phitag];
                   if (deltay2>0. && deltay2<1.) trigPhi_Xi = kTRUE;
                 }
               }
               if (deltay1>0. && deltay1<1.) {
                 for (unsigned int lambdatag=0; lambdatag<Lambdatag.size(); lambdatag++) {
                   if (trigPhi_Xi) break;
                   double deltay2 = tracks_y[lambdatag]-tracks_y[phitag];
                   if (deltay2>-1. && deltay2<0.) trigPhi_Xi = kTRUE;
                 }
                 for (unsigned int xitag=0; xitag<Xitag.size(); xitag++) {
                   if (trigPhi_Xi) break;
                   double deltay2 = tracks_y[xitag]-tracks_y[phitag];
                   if (deltay2>-1. && deltay2<0.) trigPhi_Xi = kTRUE;
                 }
                 for (unsigned int omegatag=0; omegatag<Omegatag.size(); omegatag++) {
                   if (trigPhi_Xi) break;
                   double deltay2 = tracks_y[omegatag]-tracks_y[phitag];
                   if (deltay2>-1. && deltay2<0.) trigPhi_Xi = kTRUE;
                 }
		 for (unsigned int protontag=0; protontag<Protontag.size(); protontag++) {
                   if (trigPhi_Xi) break;
                   double deltay2 = tracks_y[protontag]-tracks_y[phitag];
                   if (deltay2>-1. && deltay2<0.) trigPhi_Xi = kTRUE;
                 }
               }
             }

	   }
         }

         if (trigPhi_Xi) {	 
	   Ev[i] = 1;
           h_Ev[i]->Fill(Ev[i]);
           for (unsigned int p=0; p<tracks_charge.GetSize(); ++p) {        
             if (tracks_charge[p] && abs(tracks_eta[p])<0.5) h_dndeta[i]->Fill(tracks_eta[p]);
       
             if (abs(tracks_y[p])<2.5) {	     
               unsigned int best_tag = Phitag[0]; 
               for (unsigned int tag=0; tag<Phitag.size()-1; tag++) {
                 best_tag = (abs(tracks_y[p]-tracks_y[tag+1])<abs(tracks_y[p]-tracks_y[tag]) ? tag+1 : tag);
               }

               if (abs(tracks_y[p]-tracks_y[best_tag])<0.1) {
                 if (abs(tracks_pid[p])==310) h_dndyK0s1[i]->Fill(tracks_y[p]);
                 if (abs(tracks_pid[p])==3312) h_dndyXi1[i]->Fill(tracks_y[p]);
                 if (abs(tracks_pid[p])==3334) h_dndyOmega1[i]->Fill(tracks_y[p]);
                 if (abs(tracks_pid[p])==3122) h_dndyLambda1[i]->Fill(tracks_y[p]);
                 if (abs(tracks_pid[p])==2212) h_dndyProton1[i]->Fill(tracks_y[p]);
                 if (abs(tracks_pid[p])==211) h_dndyPion1[i]->Fill(tracks_y[p]);
               }

	       if (abs(tracks_y[p]-tracks_y[best_tag])<0.5) {
                 if (abs(tracks_pid[p])==310) h_dndyK0s2[i]->Fill(tracks_y[p]);
                 if (abs(tracks_pid[p])==3312) h_dndyXi2[i]->Fill(tracks_y[p]);
                 if (abs(tracks_pid[p])==3334) h_dndyOmega2[i]->Fill(tracks_y[p]);
                 if (abs(tracks_pid[p])==3122) h_dndyLambda2[i]->Fill(tracks_y[p]);
                 if (abs(tracks_pid[p])==2212) h_dndyProton2[i]->Fill(tracks_y[p]);
                 if (abs(tracks_pid[p])==211) h_dndyPion2[i]->Fill(tracks_y[p]);
               }

	       if (abs(tracks_y[p]-tracks_y[best_tag])<1.0) {
                 if (abs(tracks_pid[p])==310) h_dndyK0s3[i]->Fill(tracks_y[p]);
                 if (abs(tracks_pid[p])==3312) h_dndyXi3[i]->Fill(tracks_y[p]);
                 if (abs(tracks_pid[p])==3334) h_dndyOmega3[i]->Fill(tracks_y[p]);
                 if (abs(tracks_pid[p])==3122) h_dndyLambda3[i]->Fill(tracks_y[p]);
                 if (abs(tracks_pid[p])==2212) h_dndyProton3[i]->Fill(tracks_y[p]);
                 if (abs(tracks_pid[p])==211) h_dndyPion3[i]->Fill(tracks_y[p]);
               }

	       if (abs(tracks_pid[p])==310) h_dndyK0s4[i]->Fill(tracks_y[p]);
               if (abs(tracks_pid[p])==3312) h_dndyXi4[i]->Fill(tracks_y[p]);
               if (abs(tracks_pid[p])==3334) h_dndyOmega4[i]->Fill(tracks_y[p]);
               if (abs(tracks_pid[p])==3122) h_dndyLambda4[i]->Fill(tracks_y[p]);
               if (abs(tracks_pid[p])==2212) h_dndyProton4[i]->Fill(tracks_y[p]);
               if (abs(tracks_pid[p])==211) h_dndyPion4[i]->Fill(tracks_y[p]);
             }
	   } 
	 }	 

	 break;
       }	 
     }
   }   

   return kTRUE;
}

void Analysis_phi_xi::SlaveTerminate()
{
   // The SlaveTerminate() function is called after all entries or objects
   // have been processed. When running with PROOF SlaveTerminate() is called
   // on each slave server.

}

void Analysis_phi_xi::Terminate()
{
   // The Terminate() function is the last function to be called during
   // a query. It always runs on the client, it can be used to present
   // the results graphically or save the results to file.

   string filename2;
   if (pythia_def) filename2 = "analysisphixi_pydef2.root";
   else if (pythia_new) filename2 = "analysisphixi_pynew2.root";
   else if (epos4) filename2 = "analysisphixi_epos2.root";
   TFile* file2 = new TFile(filename2.c_str(), "RECREATE");
   for (int i=0; i<9; ++i) {
     h_Ev[i] = dynamic_cast<TH1I*>(fOutput->FindObject(Form("h_Ev%i",i+1)));	   
     h_dndeta[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndeta%i",i+1)));

     h_dndeta[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndeta[i]->Write();

     h_dndyK0s1[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyK0s1_%i",i+1)));
     h_dndyXi1[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyXi1_%i",i+1)));
     h_dndyOmega1[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyOmega1_%i",i+1)));
     h_dndyLambda1[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyLambda1_%i",i+1)));
     h_dndyProton1[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyProton1_%i",i+1)));
     h_dndyPion1[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyPion1_%i",i+1)));

     h_dndyK0s1[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyK0s1[i]->Write();
     h_dndyXi1[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyXi1[i]->Write();
     h_dndyOmega1[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyOmega1[i]->Write();
     h_dndyLambda1[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyLambda1[i]->Write();
     h_dndyProton1[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyProton1[i]->Write();
     h_dndyPion1[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyPion1[i]->Write();

     h_dndyK0s2[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyK0s2_%i",i+1)));
     h_dndyXi2[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyXi2_%i",i+1)));
     h_dndyOmega2[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyOmega2_%i",i+1)));
     h_dndyLambda2[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyLambda2_%i",i+1)));
     h_dndyProton2[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyProton2_%i",i+1)));
     h_dndyPion2[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyPion2_%i",i+1)));

     h_dndyK0s2[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyK0s2[i]->Write();
     h_dndyXi2[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyXi2[i]->Write();
     h_dndyOmega2[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyOmega2[i]->Write();
     h_dndyLambda2[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyLambda2[i]->Write();
     h_dndyProton2[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyProton2[i]->Write();
     h_dndyPion2[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyPion2[i]->Write();

     h_dndyK0s3[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyK0s3_%i",i+1)));
     h_dndyXi3[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyXi3_%i",i+1)));
     h_dndyOmega3[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyOmega3_%i",i+1)));
     h_dndyLambda3[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyLambda3_%i",i+1)));
     h_dndyProton3[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyProton3_%i",i+1)));
     h_dndyPion3[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyPion3_%i",i+1)));

     h_dndyK0s3[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyK0s3[i]->Write();
     h_dndyXi3[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyXi3[i]->Write();
     h_dndyOmega3[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyOmega3[i]->Write();
     h_dndyLambda3[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyLambda3[i]->Write();
     h_dndyProton3[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyProton3[i]->Write();
     h_dndyPion3[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyPion3[i]->Write();

     h_dndyK0s4[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyK0s4_%i",i+1)));
     h_dndyXi4[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyXi4_%i",i+1)));
     h_dndyOmega4[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyOmega4_%i",i+1)));
     h_dndyLambda4[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyLambda4_%i",i+1)));
     h_dndyProton4[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyProton4_%i",i+1)));
     h_dndyPion4[i] = dynamic_cast<TH1D*>(fOutput->FindObject(Form("h_dndyPion4_%i",i+1)));

     h_dndyK0s4[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyK0s4[i]->Write();
     h_dndyXi4[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyXi4[i]->Write();
     h_dndyOmega4[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyOmega4[i]->Write();
     h_dndyLambda4[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyLambda4[i]->Write();
     h_dndyProton4[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyProton4[i]->Write();
     h_dndyPion4[i]->Scale(100./h_Ev[i]->GetBinContent(2));
     h_dndyPion4[i]->Write();
   }
   file2->Close();
}
