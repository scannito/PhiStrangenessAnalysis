#include <vector>
#include <string>

#include <TROOT.h>
#include <TFile.h>
#include <TTree.h>
#include <TTreeReader.h>
#include <TTreeReaderArray.h>
#include <TMath.h>
#include <Riostream.h>

#include "EPOSEvent.h"


using namespace std;

  void EPOSInterface(int input) {
  vector<int> eposid, pdgid, mult, ifl1, ifl2, ifl3;
  vector<double> charge;
  int eposid_loc, pdgid_loc, qgsjetid_loc, gheishaid_loc, sibyllid_loc, ifl1_loc, ifl2_loc, ifl3_loc, counter_loc, mult_loc, deg_loc;
  string  name_loc;
  double mass_loc, charge_loc, width_loc;

  ifstream file1("part.dat", ios::in);
  if (!file1.is_open()) {
    cout << "CANNOT OPEN DATA FILE!" << endl;
  }
  while (file1.is_open()) {
    file1 >> eposid_loc >> pdgid_loc >> qgsjetid_loc >> gheishaid_loc >> sibyllid_loc >> name_loc >> ifl1_loc
          >> ifl2_loc >> ifl3_loc >> counter_loc >> mass_loc >> charge_loc >> width_loc >> mult_loc >> deg_loc;
    if (file1.eof()) break;
    eposid.push_back(eposid_loc);
    pdgid.push_back(pdgid_loc);
    ifl1.push_back(ifl1_loc);
    ifl2.push_back(ifl2_loc);
    ifl3.push_back(ifl3_loc);
    charge.push_back(charge_loc);
    mult.push_back(mult_loc);
  }
  file1.close();

  string str = to_string(input);
 
  string name_in = "/home/etlar/stefano/epos4.0.0/Data_EPOS4/z-pp136_1-" + str + ".root";
  TFile* file2 = TFile::Open(name_in.c_str());
  TTreeReader reader("teposevent0", file2);
  TTreeReaderArray<Int_t> id = {reader, "id"};
  TTreeReaderArray<Int_t> ist = {reader, "ist"};
  TTreeReaderArray<Int_t> ity = {reader, "ity"};
  TTreeReaderArray<Float_t> px = {reader, "px"};
  TTreeReaderArray<Float_t> py = {reader, "py"};
  TTreeReaderArray<Float_t> pz = {reader, "pz"};
  TTreeReaderArray<Float_t> e = {reader, "e"};

  string name_out = "/home/etlar/stefano/epos4.0.0/Data_EPOS4/EPOS4-" + str + ".root";
  TFile* file3 = TFile::Open(name_out.c_str(),"RECREATE");
  EPOSEvent* ee = new EPOSEvent();
  TTree* newtree = new TTree("t","EPOS4 event tree");
  newtree->Branch("events",&ee);

  while (reader.Next()) {
    vector<EPOSTrack> ets;
    for (unsigned int i=0; i<ist.GetSize(); i++) {
      if (ist[i] == 0) {
        EPOSTrack t;
        double p_eta, p_y, p_pT, p_p, p_E;
        bool p_ischarged, p_ishadron;

        p_pT = TMath::Sqrt(TMath::Power(px[i],2)+TMath::Power(py[i],2));
        t.pT = p_pT;
        p_p = TMath::Sqrt(TMath::Power(p_pT,2)+TMath::Power(pz[i],2));
        p_eta = 0.5*TMath::Log((p_p+pz[i])/(p_p-pz[i]));
        t.eta = p_eta;
        p_E = TMath::Sqrt(TMath::Power(e[i],2)+TMath::Power(p_p,2));
        p_y = 0.5*TMath::Log((p_E+pz[i])/(p_E-pz[i]));
        t.y = p_y;
	      t.pity = ity[i];

        for (unsigned int j=0; j<eposid.size(); j++) {
	        if (mult[j] == 1) {
	          if (id[i] == eposid[j]) {
              t.pid = pdgid[j];
              p_ischarged = (charge[j] != 0. ? true : false);
              t.charge = p_ischarged;
	            p_ishadron = (((ifl1[j]!=0 && ifl2[j]!=0) || (ifl1[j]!=0 && ifl3[j]!=0) || (ifl2[j]!=0 && ifl3[j]!=0)) ? true : false);
	            t.had = p_ishadron;
            }
          }	else if (mult[j] == 2) {		  
            if (id[i] == eposid[j]) {
              t.pid = pdgid[j];
              p_ischarged = (charge[j] != 0. ? true : false);
              t.charge = p_ischarged;
	            p_ishadron = (((ifl1[j]!=0 && ifl2[j]!=0) || (ifl1[j]!=0 && ifl3[j]!=0) || (ifl2[j]!=0 && ifl3[j]!=0)) ? true : false);
              t.had = p_ishadron;
            } else if (id[i] == -1*eposid[j]) {
              t.pid = -1*pdgid[j];
              p_ischarged = (charge[j] != 0. ? true : false);
              t.charge = p_ischarged;
	            p_ishadron = (((ifl1[j]!=0 && ifl2[j]!=0) || (ifl1[j]!=0 && ifl3[j]!=0) || (ifl2[j]!=0 && ifl3[j]!=0)) ? true : false);
              t.had = p_ishadron;
            }
	        }  
        }

        ets.push_back(t);
      }
    }

    bool fillTree = ee->init();
    ee->tracks = ets;
    if (fillTree) newtree->Fill();
  }

  newtree->Write();
  delete file3;
  delete ee;

  return;
}

