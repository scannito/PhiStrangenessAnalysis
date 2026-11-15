#include "utils.h"

#include <TStopwatch.h>
#include <TFile.h>
#include <TH1D.h>
#include <TNtupleD.h>
#include <Riostream.h>
#include <TProfile.h>

//int n = 1; // 1.2
//int n = 3; // 1.0
//int n = 5; // 0.8
//int n = 7; // 0.6
//int n = 9; // 0.4
//int n = 11; // 0.2

void Tuple2Hist(const int smpl = 0, const int iVarMin = 364, const int iVarMax = 365, const int n = 1)
{
  std::cout << "n = " << n << std::endl;

  TStopwatch w;
  w.Start();

  int sample = smpl;
  int skippedVar = 0;

  for (int iVar = iVarMin; iVar < iVarMax; ++iVar){
    bool inVars = false;
    for (int iV{0}; iV < kNVar; ++iV) {
      if (iVar == kVar[iV]) {
        inVars = true;
      }
    }
    if (!inVars) continue;
    // const char* dir = kTriggerSel == 0x1 ? "MB" : "HM";
    TFile *fin = TFile::Open(Form("%s/out__file__tot_s_%d.root", "/data/mciacco/out", sample));
    if (!fin || fin->TestBit(TFile::kZombie)){
      std::cout << "no input, skip" << std::endl;
      skippedVar += 1;

      delete fin;
      continue;
    }
    TFile fout(Form("%s/output_sys_%d_%d.root", kResDir, sample, n), "recreate");

    TNtupleD *tuple_moment_gen;
    tuple_moment_gen = (TNtupleD*)fin->Get("evts");
    tuple_moment_gen->SetCacheSize(0);

    int evt[10] = {0};
    double centrality;

    double *arg_gen;
    double total_event_gen = 0.;
    TProfile *N1p = nullptr;
    TProfile *N1 = nullptr;
    TProfile *N2 = nullptr;
    TProfile *N3 = nullptr;
    TProfile *N4 = nullptr;
    TProfile *N5 = nullptr;
    TProfile *N6 = nullptr;
    TProfile *N1_n = nullptr;
    TProfile *N2_n = nullptr;
    TProfile *N3_n = nullptr;
    TProfile *N4_n = nullptr;
    TProfile *N5_n = nullptr;
    TProfile *N6_n = nullptr;

    total_event_gen = tuple_moment_gen->GetEntriesFast();
    // non-central moments of generated distribution
    N1p = new TProfile("N1p", "N1p", kNCentBinsSmall, kCentBinsSmall);
    N1 = new TProfile("N1", "N1", kNCentBinsSmall, kCentBinsSmall);
    N2 = new TProfile("N2", "N2", kNCentBinsSmall, kCentBinsSmall);
    N3 = new TProfile("N3", "N3", kNCentBinsSmall, kCentBinsSmall);
    N4 = new TProfile("N4", "N4", kNCentBinsSmall, kCentBinsSmall);
    N5 = new TProfile("N5", "N5", kNCentBinsSmall, kCentBinsSmall);
    N6 = new TProfile("N6", "N6", kNCentBinsSmall, kCentBinsSmall);
    N1_n = new TProfile("N1_n", "N1_n", kNCentBinsSmall, kCentBinsSmall);
    N2_n = new TProfile("N2_n", "N2_n", kNCentBinsSmall, kCentBinsSmall);
    N3_n = new TProfile("N3_n", "N3_n", kNCentBinsSmall, kCentBinsSmall);
    N4_n = new TProfile("N4_n", "N4_n", kNCentBinsSmall, kCentBinsSmall);
    N5_n = new TProfile("N5_n", "N5_n", kNCentBinsSmall, kCentBinsSmall);
    N6_n = new TProfile("N6_n", "N6_n", kNCentBinsSmall, kCentBinsSmall);


    int readError = 0;

    for (int j_gen = 0; j_gen < total_event_gen; j_gen++)
    {
  //    if (j_gen % 10000 == 0) std::cout << j_gen << std::endl;

      if (tuple_moment_gen->GetEntry(j_gen) < 0) {readError = -999; break;}
      arg_gen = tuple_moment_gen->GetArgs();
      centrality = kMultV0M ? arg_gen[0] : arg_gen[3];
      // if(centrality < 0.1) std::cout << centrality << std::endl;

      double qP1_p = arg_gen[n];
      double qP1_n = arg_gen[n + 1];

      // full formula (gen)
      N1p->Fill(centrality, qP1_p + qP1_n);
      N1->Fill(centrality, qP1_p - qP1_n);
      N2->Fill(centrality, std::pow(qP1_p - qP1_n, 2));
      N3->Fill(centrality, std::pow(qP1_p - qP1_n, 3));
      N4->Fill(centrality, std::pow(qP1_p - qP1_n, 4));
      N5->Fill(centrality, std::pow(qP1_p - qP1_n, 5));
      N6->Fill(centrality, std::pow(qP1_p - qP1_n, 6));
      N1_n->Fill(centrality, qP1_n);
      N2_n->Fill(centrality, std::pow(qP1_n, 2));
      N3_n->Fill(centrality, std::pow(qP1_n, 3));
      N4_n->Fill(centrality, std::pow(qP1_n, 4));
      N5_n->Fill(centrality, std::pow(qP1_n, 5));
      N6_n->Fill(centrality, std::pow(qP1_n, 6));

    }
    if (readError < 0) {
      std::cout << "no input, skip" << std::endl;
      skippedVar += 1;

      delete fin;
      continue;
    }

    fout.mkdir(Form("var_%d", iVar));
    fout.cd(Form("var_%d", iVar));

    N1p->Write();
    N1->Write();
    N2->Write();
    N3->Write();
    N4->Write();
    N5->Write();
    N6->Write();
    N1_n->Write();
    N2_n->Write();
    N3_n->Write();
    N4_n->Write();
    N5_n->Write();
    N6_n->Write();


    fout.Close();
    fin->Close();
    delete fin;
  }

  std::cout << "skipped = " << skippedVar << std::endl;
  w.Stop();
  w.Print();

}
