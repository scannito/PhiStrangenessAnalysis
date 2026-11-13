//////////////////////////////////////////////////////////
// This class has been automatically generated on
// Mon Aug 14 18:01:54 2023 by ROOT version 6.26/10
// from TTree t/Pythia8 event tree
// found on file: pythia-00.root
//////////////////////////////////////////////////////////

#ifndef Analysis_phi_O2_h
#define Analysis_phi_O2_h

#include <vector>
#include <map>

#include <TROOT.h>
#include <TChain.h>
#include <TFile.h>
#include <TSelector.h>
#include <TTreeReader.h>
#include <TTreeReaderValue.h>
#include <TTreeReaderArray.h>
#include <TH2.h>
#include <TH3.h>

class ParticleType {
public:
   enum Type {
      K0S,
      Lambda,
      Xi,
      Omega,
      Phi,
      Proton,
      Pion,
      Unknown
    };

   static const char* ToString(Type type) {
      switch (type) {
         case K0S:      return "K0S";
         case Lambda:   return "Lambda";
         case Xi:       return "Xi";
         case Omega:    return "Omega";
         case Phi:      return "Phi";
         case Proton:   return "Proton";
         case Pion:     return "Pion";
         default:       return "Unknown";
      }
   }

   static Type FromPDG(int pdgCode) {
      static const std::map<int, Type> pdgToType = {
         {310, K0S},
         {3122, Lambda},
         {3312, Xi},
         {3334, Omega},
         {333, Phi},
         {2212, Proton},
         {211, Pion}     
      };

      int absCode = std::abs(pdgCode); 
      auto it = pdgToType.find(absCode);
      return (it != pdgToType.end()) ? it->second : Unknown;
   }
};

class Analysis_phi_O2 : public TSelector {
public :
   TTreeReader     fReader;      //!the tree reader
   TTree          *fChain = 0;   //!pointer to the analyzed TTree or TChain

   // Readers to access the data (delete the ones you do not need).
   TTreeReaderArray<Double_t> tracks_eta = {fReader, "tracks.eta"};
   TTreeReaderArray<Double_t> tracks_y = {fReader, "tracks.y"};
   TTreeReaderArray<Double_t> tracks_pT = {fReader, "tracks.pT"};
   TTreeReaderArray<Int_t> tracks_pid = {fReader, "tracks.pid"};
   TTreeReaderArray<Bool_t> tracks_charge = {fReader, "tracks.charge"};

   static constexpr size_t NDeltaYClasses = 2;

   Bool_t pythiaMonash, pythiaRopes, epos4;

   vector<Double_t> xBin;
   vector<Double_t> yBin = {0., 1., 2.};
   vector<Double_t> zBin = {0., 1., 2., 3., 4., 5., 6., 7.};

   TH2D* hNCharged;
   TH2D* hNTriggerPart;
   TH3D* hNPart;
   TH3D* hNPairs;

   /*TH2D* hNK0S;
   TH2D* hNXi;
   TH2D* hNOmega;
   TH2D* hNLambda;
   TH2D* hNProton;
   TH2D* hNPhi;
   TH2D* hNPion;*/

   Analysis_phi_O2(TTree * /*tree*/ =0) { Reset(); }
   virtual ~Analysis_phi_O2() { }
   virtual Int_t   Version() const { return 2; }
   virtual void    Reset();
   virtual void    Begin(TTree *tree);
   virtual void    SlaveBegin(TTree *tree);
   virtual void    Init(TTree *tree);
   virtual Bool_t  Notify();
   virtual Bool_t  Process(Long64_t entry);
   virtual Int_t   GetEntry(Long64_t entry, Int_t getall = 0) { return fChain ? fChain->GetTree()->GetEntry(entry, getall) : 0; }
   virtual void    SetOption(const char *option) { fOption = option; }
   virtual void    SetObject(TObject *obj) { fObject = obj; }
   virtual void    SetInputList(TList *input) { fInput = input; }
   virtual TList  *GetOutputList() const { return fOutput; }
   virtual void    SlaveTerminate();
   virtual void    Terminate();

   void BookHisto();										
   void CollectHisto(TFile *file);

   Bool_t EventIsINELgt0();
   Bool_t EventHasPhi();
   Bool_t AcceptEvent();

   ClassDef(Analysis_phi_O2,0);

};

#endif

#ifdef Analysis_phi_O2_cxx
void Analysis_phi_O2::Reset()
{
   // Reset the data members to their initial value.
   pythiaMonash = kFALSE;
   pythiaRopes = kFALSE;
   epos4 = kFALSE;

   hNCharged = nullptr;
   hNTriggerPart = nullptr;
   hNPart = nullptr;
   hNPairs = nullptr;

   /*hNK0S = nullptr;
   hNXi = nullptr;
   hNOmega = nullptr;
   hNLambda = nullptr;
   hNProton = nullptr;
   hNPhi = nullptr;
   hNPion = nullptr;*/
}

void Analysis_phi_O2::Init(TTree *tree)
{
   // The Init() function is called when the selector needs to initialize
   // a new tree or chain. Typically here the reader is initialized.
   // It is normally not necessary to make changes to the generated
   // code, but the routine can be extended by the user if needed.
   // Init() will be called many times when running on PROOF
   // (once per file to be processed).

   fReader.SetTree(tree);
}

Bool_t Analysis_phi_O2::Notify()
{
   // The Notify() function is called when a new file is opened. This
   // can be either for a new TTree in a TChain or when when a new TTree
   // is started when using PROOF. It is normally not necessary to make changes
   // to the generated code, but the routine can be extended by the
   // user if needed. The return value is currently not used.

   return kTRUE;
}

/*void Analysis_phi_O2::BookHisto()
{
   hNCharged = new TH2D("hNCharged", "; #it{N}_{ch}; d#it{N}_{ev}/d#it{N}_{ch}", NMultClasses, xBin, 500, 0., 500.);
   fOutput->Add(hNCharged);

   hNK0S = new TH2D("hNK0S", "; N_{K_{S}^{0}}; dN_{ev}/dN_{K_{S}^{0}}", NMultClasses, xBin, NDeltaYClasses, 0, NDeltaYClasses);
   fOutput->Add(hNK0S);

   hNXi = new TH2D("hNXi", "; N_{#Xi}; dN_{ev}/dN_{#Xi}", NMultClasses, xBin, NDeltaYClasses, 0, NDeltaYClasses);
   fOutput->Add(hNXi);

   hNOmega = new TH2D("hNOmega", "; N_{#Omega}; dN_{ev}/dN_{#Omega}", NMultClasses, xBin, NDeltaYClasses, 0, NDeltaYClasses);
   fOutput->Add(hNOmega);

   hNLambda = new TH2D("hNLambda", "; N_{#Lambda}; dN_{ev}/dN_{#Lambda}", NMultClasses, xBin, NDeltaYClasses, 0, NDeltaYClasses);
   fOutput->Add(hNLambda);

   hNProton = new TH2D("hNProton", "; N_{p}; dN_{ev}/dN_{p}", NMultClasses, xBin, NDeltaYClasses, 0, NDeltaYClasses);
   fOutput->Add(hNProton);

   hNPhi = new TH2D("hNPhi", "; N_{#phi}; dN_{ev}/dN_{#phi}", NMultClasses, xBin, NDeltaYClasses, 0, NDeltaYClasses);
   fOutput->Add(hNPhi);

   hNPion = new TH2D("hNPion", "; N_{#pi}; dN_{ev}/dN_{#pi}", NMultClasses, xBin, NDeltaYClasses, 0, NDeltaYClasses);
   fOutput->Add(hNPion);
}*/

void Analysis_phi_O2::BookHisto() {
   hNCharged = new TH2D("hNCharged", "; #it{N}_{ch}; #it{N}_{ch}; d#it{N}_{ev}/d#it{N}_{ch}", xBin.size()-1, xBin.data(), 500, 0., 500.);
   fOutput->Add(hNCharged);

   hNTriggerPart = new TH2D("hNTriggerPart", "; #it{N}_{ch}; #it{N}_{#phi}; d#it{N}_{ev}/d#it{N}_{#phi}", xBin.size()-1, xBin.data(), 100, 0., 100.);
   fOutput->Add(hNTriggerPart);

   hNPart = new TH3D("hNPart", "; #it{N}_{ch}; #Delta#it{y} class; Hadron; d#it{N}_{ev}/d#it{N}_{had}", xBin.size()-1, xBin.data(), yBin.size()-1, yBin.data(), zBin.size()-1, zBin.data());
   hNPart->GetYaxis()->SetBinLabel(1, "Inclusive");
   hNPart->GetYaxis()->SetBinLabel(2, "#Delta#it{y} < 0.1");
   hNPart->GetZaxis()->SetBinLabel(1, ParticleType::ToString(ParticleType::K0S));
   hNPart->GetZaxis()->SetBinLabel(2, ParticleType::ToString(ParticleType::Lambda));
   hNPart->GetZaxis()->SetBinLabel(3, ParticleType::ToString(ParticleType::Xi));
   hNPart->GetZaxis()->SetBinLabel(4, ParticleType::ToString(ParticleType::Omega));
   hNPart->GetZaxis()->SetBinLabel(5, ParticleType::ToString(ParticleType::Phi));
   hNPart->GetZaxis()->SetBinLabel(6, ParticleType::ToString(ParticleType::Proton));
   hNPart->GetZaxis()->SetBinLabel(7, ParticleType::ToString(ParticleType::Pion));
   fOutput->Add(hNPart);

   hNPairs = new TH3D("hNPairs", "; #it{N}_{ch}; #Delta#it{y} class; Hadron; d#it{N}_{ev}/d#it{N}_{had}", xBin.size()-1, xBin.data(), yBin.size()-1, yBin.data(), zBin.size()-1, zBin.data());
   hNPairs->GetYaxis()->SetBinLabel(1, "Inclusive");
   hNPairs->GetYaxis()->SetBinLabel(2, "#Delta#it{y} < 0.1");
   hNPairs->GetZaxis()->SetBinLabel(1, ParticleType::ToString(ParticleType::K0S));
   hNPairs->GetZaxis()->SetBinLabel(2, ParticleType::ToString(ParticleType::Lambda));
   hNPairs->GetZaxis()->SetBinLabel(3, ParticleType::ToString(ParticleType::Xi));
   hNPairs->GetZaxis()->SetBinLabel(4, ParticleType::ToString(ParticleType::Omega));
   hNPairs->GetZaxis()->SetBinLabel(5, ParticleType::ToString(ParticleType::Phi));
   hNPairs->GetZaxis()->SetBinLabel(6, ParticleType::ToString(ParticleType::Proton));
   hNPairs->GetZaxis()->SetBinLabel(7, ParticleType::ToString(ParticleType::Pion));
   fOutput->Add(hNPairs);
}

/*void Analysis_phi_O2::CollectHisto(TFile *file)
{
   file->cd();

   hNCharged = dynamic_cast<TH2D*>(fOutput->FindObject("hNCharged"));
   hNCharged->Write();

   hNK0S = dynamic_cast<TH2D*>(fOutput->FindObject("hNK0S"));
   hNK0S->Write();

   hNXi = dynamic_cast<TH2D*>(fOutput->FindObject("hNXi"));
   hNXi->Write();

   hNOmega = dynamic_cast<TH2D*>(fOutput->FindObject("hNOmega"));
   hNOmega->Write();

   hNLambda = dynamic_cast<TH2D*>(fOutput->FindObject("hNLambda"));
   hNLambda->Write();

   hNProton = dynamic_cast<TH2D*>(fOutput->FindObject("hNProton"));
   hNProton->Write();

   hNPhi = dynamic_cast<TH2D*>(fOutput->FindObject("hNPhi"));
   hNPhi->Write();

   hNPion = dynamic_cast<TH2D*>(fOutput->FindObject("hNPion"));
   hNPion->Write();
}*/

void Analysis_phi_O2::CollectHisto(TFile *file)
{
   file->cd();

   hNCharged = dynamic_cast<TH2D*>(fOutput->FindObject("hNCharged"));
   hNCharged->Write();

   hNTriggerPart = dynamic_cast<TH2D*>(fOutput->FindObject("hNTriggerPart"));
   hNTriggerPart->Write();

   hNPart = dynamic_cast<TH3D*>(fOutput->FindObject("hNPart"));
   hNPart->Write();

   hNPairs = dynamic_cast<TH3D*>(fOutput->FindObject("hNPairs"));
   hNPairs->Write();
}

Bool_t Analysis_phi_O2::EventIsINELgt0()
{
   for (size_t p = 0; p < tracks_eta.GetSize(); ++p) {
      if (tracks_charge[p] && abs(tracks_eta[p]) < 1.) {
         return kTRUE;
      }
   }
   return kFALSE;
}

Bool_t Analysis_phi_O2::EventHasPhi()
{
   for (size_t p = 0; p < tracks_pid.GetSize(); ++p) {
      if (abs(tracks_y[p])<0.5 && tracks_pid[p] == 333) {
         return kTRUE;
      }
   }
   return kFALSE;
}

Bool_t Analysis_phi_O2::AcceptEvent()
{
   if (!EventIsINELgt0() || !EventHasPhi()) return kFALSE;
   return kTRUE;
}

#endif // #ifdef Analysis_phi_O2_cxx
