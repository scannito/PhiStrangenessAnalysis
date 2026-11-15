#define Calibration_cxx
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
// root> T->Process("Calibration.C")
// root> T->Process("Calibration.C","some options")
// root> T->Process("Calibration.C+")
//


#include "Calibration.h"
#include <TStyle.h>
#include <TH1.h>
#include <TCanvas.h>
#include <Riostream.h>
#include <vector>

using namespace std;

TH1D* h_nCharged;

void Calibration::Begin(TTree * /*tree*/)
{
   // The Begin() function is called at the start of the query.
   // When running with PROOF Begin() is only called on the client.
   // The tree argument is deprecated (on PROOF 0 is passed).

   Reset();

   TString option = GetOption();
   Info("Begin", "starting calibration with process option: %s", option.Data());

   if (option.Contains("pythia_def")) pythia_def = kTRUE;
   if (option.Contains("pythia_new")) pythia_new = kTRUE;
   if (option.Contains("epos4")) epos4 = kTRUE;
}

void Calibration::SlaveBegin(TTree *tree)
{
   // The SlaveBegin() function is called after the Begin() function.
   // When running with PROOF SlaveBegin() is called on each slave server.
   // The tree argument is deprecated (on PROOF 0 is passed).
  
   // Initialize the Tree branch adresses.
   Init(tree);

   // Create multiplicity histogram. 
   h_nCharged = new TH1D("h_nCharged", "; N_{ch}; N_{ev}", 300, -0.5, 299.5);
   fOutput->Add(h_nCharged);
}

Bool_t Calibration::Process(Long64_t entry)
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

   // Check the trigger condition
   if (trig) {
     // Count the number of charged particles	   
     for (unsigned int p=0; p<tracks_charge.GetSize(); ++p) {
       if (tracks_charge[p] && ((tracks_eta[p]>-3.7 && tracks_eta[p]<-1.7) || (tracks_eta[p]>2.8 && tracks_eta[p]<5.1))) ++nCharged;
     }
     h_nCharged->Fill(nCharged);	       
   }

   return kTRUE;
}

void Calibration::SlaveTerminate()
{
   // The SlaveTerminate() function is called after all entries or objects
   // have been processed. When running with PROOF SlaveTerminate() is called
   // on each slave server.

}

void Calibration::Terminate()
{
   // The Terminate() function is the last function to be called during
   // a query. It always runs on the client, it can be used to present
   // the results graphically or save the results to file.

   h_nCharged = dynamic_cast<TH1D*>(fOutput->FindObject("h_nCharged"));
 
   // Create file with the multiplicity plot
   string filename1;
   /*if (pythia_def) filename1 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Calibration_moreclasses_results/calibration_pydef.root";
   else if (pythia_new) filename1 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Calibration_moreclasses_results/calibration_pynew.root";
   else if (epos4) filename1 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Calibration_moreclasses_results/calibration_epos.root";*/
   if (pythia_def) filename1 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Calibration_evenmoreclasses_results/calibration_pydef.root";
   else if (pythia_new) filename1 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Calibration_evenmoreclasses_results/calibration_pynew.root";
   else if (epos4) filename1 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Calibration_evenmoreclasses_results/calibration_epos.root";
   TFile* file1 = new TFile(filename1.c_str(), "RECREATE");										
   h_nCharged->Write();
   file1->Close();

   int nBin = h_nCharged->GetNbinsX();
   double sigma = h_nCharged->Integral();
   h_nCharged->Scale(1./sigma);

   /*vector<double> xBin;
   xBin.push_back(h_nCharged->GetBinCenter(1)-h_nCharged->GetBinWidth(1)/2);
   double perc[16] = {0.695, 0.665, 0.495, 0.465, 0.395, 0.365, 0.290, 0.270, 0.200, 0.180, 0.145, 0.135, 0.100, 0.090, 0.049, 0.045};
   for (int j=1; j<16; j+=2) {
     for (int i=1; i<=nBin; i++) {
       if (h_nCharged->Integral(i,nBin) >= perc[j] && h_nCharged->Integral(i,nBin) <= perc[j-1]) {
         xBin.push_back(h_nCharged->GetBinCenter(i)-h_nCharged->GetBinWidth(i)/2);
         break;
       }
     }
   }
   for (int i=1; i<=nBin; i++) {
     if (h_nCharged->GetBinContent(i) == 0.) {
       xBin.push_back(h_nCharged->GetBinCenter(i)-h_nCharged->GetBinWidth(i)/2);
       break;
     }
   }*/
   
   //vector<double> perc{1.0, 0.645, 0.460, 0.368, 0.276, 0.184, 0.138, 0.092, 0.046, 0.019, 0.0092, 0.0};
   vector<double> perc{1.0, 0.645, 0.460, 0.368, 0.276, 0.184, 0.138, 0.092, 0.046, 0.019, 0.0092, 0.0050, 0.0};
   vector<double> xBin;										
   for (unsigned int j=0; j<perc.size(); j++) {
     if (perc[j]==1.0) xBin.push_back(h_nCharged->GetBinCenter(1)-h_nCharged->GetBinWidth(1)/2);
     else if (perc[j]==0.0) {
       for (int i=1; i<=nBin; i++) {
         if (h_nCharged->GetBinContent(i) == 0.0) {
           xBin.push_back(h_nCharged->GetBinCenter(i)-h_nCharged->GetBinWidth(i)/2);
           break;
         }
       }
     }
     else {
       /*for (int i=1; i<=nBin-1; i++) {
	 if (abs(perc[j]-h_nCharged->Integral(i+1,nBin))>abs(perc[j]-h_nCharged->Integral(i,nBin))) { ;
           xBin.push_back(h_nCharged->GetBinCenter(i)-h_nCharged->GetBinWidth(i)/2);
           break;
         }
       }*/
       int bestBin = 1;       
       for (int i=1; i<=nBin; i++) bestBin = (abs(perc[j]-h_nCharged->Integral(i,nBin))<abs(perc[j]-h_nCharged->Integral(bestBin,nBin)) ? i : bestBin);
       xBin.push_back(h_nCharged->GetBinCenter(bestBin)-h_nCharged->GetBinWidth(bestBin)/2);       
     }  
   }	   

   string filename2;
   /*if (pythia_def) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Calibration_moreclasses_results/multclasses_pydef.txt";
   else if (pythia_new) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Calibration_moreclasses_results/multclasses_pynew.txt";
   else if (epos4) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Calibration_moreclasses_results/multclasses_epos.txt";*/
   if (pythia_def) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Calibration_evenmoreclasses_results/multclasses_pydef.txt";
   else if (pythia_new) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Calibration_evenmoreclasses_results/multclasses_pynew.txt";
   else if (epos4) filename2 = "/home/etlar/stefano/pythia8310/examples/rootfiles/Calibration_evenmoreclasses_results/multclasses_epos.txt";
   ofstream out(filename2, ios::out);												
   for (unsigned int i=0; i<xBin.size(); i++) {
     out << xBin[i] << endl;
   }							
   out.close();    
}
