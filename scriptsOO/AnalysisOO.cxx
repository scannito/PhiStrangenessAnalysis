#include "utils.h"

const int kNSample_ = 40;
const int kNSample =40;

//#define FILL_MC

//double v2[]{0.0265861, 0.0594878, 0.0688282, 0.090802, 0.124496, 0.149796, 0.208711, 0.225195};
//double v3[]{-0.00211993, 0.002142, -0.00721255, -0.00267548, -0.0102533, -0.0383771, 0.00220425, 0.138793};
//double v3_err[]{0.00194359, 0.00513119, 0.0119493, 0.0181151, 0.0323995, 0.0462108, 0.0713528, 0.293651};
//double v2_err[]{0.000382161, 0.000655158, 0.0010819, 0.001471, 0.0020354, 0.00220158, 0.00211266, 0.00436864};

//double v2[]{0.0187656, 0.0528284, 0.0637202, 0.0875567, 0.120773, 0.147558, 0.207523, 0.224011};
//double v3[]{0.00136464, 0.00464468, -0.00641032, -0.00248064, -0.0159467, -0.0438954, 0.00341182, 0.114916};
//double v2_err[]{0.000395706, 0.00103544, 0.00104943, 0.00169508, 0.00205471, 0.00266375, 0.00216507, 0.00386486};
//double v3_err[]{0.00195307, 0.00776566, 0.010108, 0.0194212, 0.0342716, 0.0568273, 0.0669355, 0.222861};
double dummy_cumul[]{0., 0., 0., 0., 0., 0., 0., 0.};

double v2[]{0.0214358, 0.0214146, 0.0276255, 0.0371122, 0.049086, 0.0652712, 0.0898144, 0.121519};
double v3[]{-0.00178241, -0.00193297, -0.00125711, 0.000852246, 0.000461152, -0.00229157, 0.00531891, 0.024676};
double v2_err[]{0.000158883, 0.000226836, 0.000370396, 0.00052188, 0.000642474, 0.00110484, 0.00132943, 0.00262934};
double v3_err[]{0.00110212, 0.00127017, 0.00185343, 0.00232215, 0.00350357, 0.00672436, 0.00946282, 0.0248665};

void cumulant_ratio(double &mean, double &rms, const double *denom, const double *num, const double nSkip = 0., const int class_m = 0, const double fac = 0, const double *cumul = dummy_cumul, const double fac2 = 0, const double *cumul2 = dummy_cumul){
  mean = 0.0;
  double mean_p = 0.0;
  double mean_m = 0.0;
  double mean_p3 = 0.0;
  double mean_m3 = 0.0;
  rms = 0.0;
  for(int sample = 0; sample < kNSample_; sample++)
  {
    if (std::abs(denom[sample]) > 1.e-9) {
      mean = mean + (num[sample] / denom[sample]) + fac * v2[class_m] * cumul[sample] + fac2 * v3[class_m] * std::pow(cumul2[sample], 2.);
      mean_p = mean_p + (num[sample] / denom[sample]) + fac * (v2[class_m] + v2_err[class_m]) * cumul[sample] + fac2 * (v3[class_m]) * std::pow(cumul2[sample], 2.);
      mean_m = mean_m + (num[sample] / denom[sample]) + fac * (v2[class_m] - v2_err[class_m]) * cumul[sample] + fac2 * (v3[class_m]) * std::pow(cumul2[sample], 2.);
      mean_p3 = mean_p3 + (num[sample] / denom[sample]) + fac * (v2[class_m]) * cumul[sample] + fac2 * (v3[class_m] + v3_err[class_m]) * std::pow(cumul2[sample], 2.);
      mean_m3 = mean_m3 + (num[sample] / denom[sample]) + fac * (v2[class_m]) * cumul[sample] + fac2 * (v3[class_m] - v3_err[class_m]) * std::pow(cumul2[sample], 2.);
    }
  }
  mean = mean / ( kNSample - nSkip);
  mean_p = mean_p / ( kNSample - nSkip);
  mean_m = mean_m / ( kNSample - nSkip);
  mean_p3 = mean_p3 / ( kNSample - nSkip);
  mean_m3 = mean_m3 / ( kNSample - nSkip);
  for(int sample = 0; sample < kNSample_; sample++)
  {
    if (std::abs(denom[sample]) > 1.e-9) {
      rms = rms + powI(mean - (num[sample] / denom[sample] + fac * v2[class_m] * cumul[sample] + fac2 * v3[class_m] * std::pow(cumul2[sample], 2.)), 2);
    }
  }

  rms = std::sqrt(std::pow(mean_p - mean_m, 2) + std::pow(mean_p3 - mean_m3, 2) + rms / (kNSample - nSkip) / (kNSample - nSkip - 1));
}

void cumulant(double &mean, double &rms, const double *denom, const double *num, const double nSkip = 0.){
  mean = 0.0;
  rms = 0.0;
  for(int sample = 0; sample < kNSample_; sample++)
  {
    if (std::abs(denom[sample]) > 1.e-9) {
      mean = mean + (num[sample]);
    }
  }
  mean = mean / ( kNSample - nSkip);
  for(int sample = 0; sample < kNSample_; sample++)
  {
    if (std::abs(denom[sample]) > 1.e-9) {
      rms = rms + powI(mean - (num[sample]), 2);
    }
  }
}



void AnalysisOO(const char* period = "PP_noWD", const char* obs = "k6k2", int const n = 15)
{
  int ncut = 2 * n + 1;

  TFile f(Form("out_sys_%s_finalBinning_%s_%d.root", period, obs, n), "recreate");
  std::string obs_str{obs};

  for(int iVar = 364; iVar < 365; ++iVar)
  {
    double iB0 = 0.;
    double iB1 = 0.;
    double nSkip = 0;

    double k1_gen[10][kNSample_];
    double k2sk_gen[10][kNSample_];
    double k2_gen[10][kNSample_];
    double k3_gen[10][kNSample_];
    double k4_gen[10][kNSample_];
    double k5_gen[10][kNSample_];
    double k6_gen[10][kNSample_];
    double k1_gen_n[10][kNSample_];
    double k2_gen_n[10][kNSample_];
    double k3_gen_n[10][kNSample_];
    double k4_gen_n[10][kNSample_];
    double k5_gen_n[10][kNSample_];
    double k6_gen_n[10][kNSample_];

    for(int sample = 0; sample < kNSample_; sample++)
    {
      TFile *fin = new TFile(Form("out/output_sys_%d_%d.root", /*kResDir,*/ sample, ncut));

      if (!fin || sample == 46 || sample == 81) {nSkip++; fin->Close(); delete fin; std::cout << sample << std::endl; continue;}

      TProfile *N1p = (TProfile*)fin->Get(Form("var_%d/N1p", iVar));
      TProfile *N1 = (TProfile*)fin->Get(Form("var_%d/N1", iVar));
      TProfile *N2 = (TProfile*)fin->Get(Form("var_%d/N2", iVar));
      TProfile *N3 = (TProfile*)fin->Get(Form("var_%d/N3", iVar));
      TProfile *N4 = (TProfile*)fin->Get(Form("var_%d/N4", iVar));
      TProfile *N5 = (TProfile*)fin->Get(Form("var_%d/N5", iVar));
      TProfile *N6 = (TProfile*)fin->Get(Form("var_%d/N6", iVar));
      TProfile *N1_n = (TProfile*)fin->Get(Form("var_%d/N1_n", iVar));
      TProfile *N2_n = (TProfile*)fin->Get(Form("var_%d/N2_n", iVar));
      TProfile *N3_n = (TProfile*)fin->Get(Form("var_%d/N3_n", iVar));
      TProfile *N4_n = (TProfile*)fin->Get(Form("var_%d/N4_n", iVar));
      TProfile *N5_n = (TProfile*)fin->Get(Form("var_%d/N5_n", iVar));
      TProfile *N6_n = (TProfile*)fin->Get(Form("var_%d/N6_n", iVar));

      if (!N1){
        std::cout << sample << std::endl;
        std::cout << "skip..." << std::endl;
        nSkip++; fin->Close(); delete fin; continue;
      }

      for(int i = 1; i <= kNCentBinsSmall; i++)
      {
        k1_gen_n[i - 1][sample] = N1_n->GetBinContent(i);
        k2_gen_n[i - 1][sample] = N2_n->GetBinContent(i) - powI(N1_n->GetBinContent(i), 2);
        k3_gen_n[i - 1][sample] = N3_n->GetBinContent(i) - (3. * N2_n->GetBinContent(i) * N1_n->GetBinContent(i)) + (2. * powI(N1_n->GetBinContent(i), 3));

        k1_gen[i - 1][sample] = N1->GetBinContent(i);
        k2sk_gen[i - 1][sample] = N1p->GetBinContent(i);
        k2_gen[i - 1][sample] = N2->GetBinContent(i) - powI(N1->GetBinContent(i), 2);
        k3_gen[i - 1][sample] = N3->GetBinContent(i) - (3. * N2->GetBinContent(i) * N1->GetBinContent(i)) + (2. * powI(N1->GetBinContent(i), 3));
        k4_gen[i - 1][sample] = N4->GetBinContent(i) - (4. * N3->GetBinContent(i) * N1->GetBinContent(i)) - (3. * powI(N2->GetBinContent(i), 2)) + (12. * N2->GetBinContent(i) * powI(N1->GetBinContent(i), 2)) - (6. * powI(N1->GetBinContent(i), 4));
        k5_gen[i - 1][sample] = N5->GetBinContent(i) - (5. * N4->GetBinContent(i) * N1->GetBinContent(i)) - (10. * N3->GetBinContent(i) * N2->GetBinContent(i)) + (20. * N3->GetBinContent(i) * powI(N1->GetBinContent(i), 2)) + (30. * powI(N2->GetBinContent(i), 2) * N1->GetBinContent(i)) - (60. * N2->GetBinContent(i) * powI(N1->GetBinContent(i), 3)) + (24. * powI(N1->GetBinContent(i), 5));
        k6_gen[i - 1][sample] = N6->GetBinContent(i) - (6. * N5->GetBinContent(i) * N1->GetBinContent(i)) - (15. * N4->GetBinContent(i) * N2->GetBinContent(i)) + (30. * N4->GetBinContent(i) * powI(N1->GetBinContent(i), 2)) - (10. * powI(N3->GetBinContent(i), 2)) + (120. * N3->GetBinContent(i) * N2->GetBinContent(i) * N1->GetBinContent(i))
                                - (120. * N3->GetBinContent(i) * powI(N1->GetBinContent(i), 3)) + (30. * powI(N2->GetBinContent(i), 3)) - (270. * powI(N2->GetBinContent(i), 2) * powI(N1->GetBinContent(i), 2)) + (360. * N2->GetBinContent(i) * powI(N1->GetBinContent(i), 4)) - (120. * powI(N1->GetBinContent(i), 6));
      }

      fin->Close();
      delete fin;
    }

    TGraphErrors g;
    g.SetName(Form("g_%d", iVar));
    g.SetLineWidth(2);
    g.SetLineColor(kRed);
    g.SetMarkerColor(kRed);

    double mult[]{31.25, 18.68, 12.90, 10.03, 7.95, 6.32, 4.49, 2.54};
    //double mult[]{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,32.5,37.5};
//    double mult[]{5, 15, 25, 35, 45, 55, 65, 75};

    int nPoints = kMultV0M ? kNCentBins : kNTrklBins;
    for(int i = 1; i <= nPoints; i++)
    {
      double mean = 0.0;
      double rms = 0.0;
      //std::cout << nSkip << std::endl;
      //cumulant_ratio(mean, rms, k2sk[i - 1], k2[i - 1], nSkip);
      if (obs_str == "k2k2sk") cumulant_ratio(mean, rms, k2sk_gen[i - 1], k2_gen[i - 1], nSkip);
      else if (obs_str == "k4k2") cumulant_ratio(mean, rms, k2_gen[i - 1], k4_gen[i - 1], nSkip, i - 1, 0., k2_gen[i - 1]);
      else if (obs_str == "k6k2") cumulant_ratio(mean, rms, k2_gen[i - 1], k6_gen[i - 1], nSkip, i - 1, 0., k4_gen[i - 1], 0., k2_gen[i - 1]);
//      else if (obs_str == "k6k2") cumulant_ratio(mean, rms, k2_gen[i - 1], k6_gen[i - 1], nSkip, i - 1, 15., k4_gen[i - 1], 3., k2_gen[i - 1]);
      else if (obs_str == "k2sk") cumulant(mean, rms, k2_gen[i - 1], k2sk_gen[i - 1], nSkip);
      else if (obs_str == "k1") cumulant(mean, rms, k2_gen[i - 1], k1_gen[i - 1], nSkip);
      else if (obs_str == "k2") cumulant(mean, rms, k2_gen[i - 1], k2_gen[i - 1], nSkip);
      else if (obs_str == "k3") cumulant(mean, rms, k2_gen[i - 1], k3_gen[i - 1], nSkip);
      else if (obs_str == "k4") cumulant(mean, rms, k2_gen[i - 1], k4_gen[i - 1], nSkip);
      else if (obs_str == "k5") cumulant(mean, rms, k2_gen[i - 1], k5_gen[i - 1], nSkip);
      else if (obs_str == "k6") cumulant(mean, rms, k2_gen[i - 1], k6_gen[i - 1], nSkip);
      else if (obs_str == "k1_n") cumulant(mean, rms, k2_gen[i - 1], k1_gen_n[i - 1], nSkip);
      else if (obs_str == "k2_n") cumulant(mean, rms, k2_gen[i - 1], k2_gen_n[i - 1], nSkip);
      else if (obs_str == "k3_n") cumulant(mean, rms, k2_gen[i - 1], k3_gen_n[i - 1], nSkip);
      else if (obs_str == "k2k1_n") cumulant_ratio(mean, rms, k1_gen_n[i - 1], k2_gen_n[i - 1], nSkip);
      else if (obs_str == "k3k1_n") cumulant_ratio(mean, rms, k1_gen_n[i - 1], k3_gen_n[i - 1], nSkip);

      std::cout << mean << " +/- " << rms << std::endl;
      g.SetPoint(i - 1, mult[i - 1], mean);
      g.SetPointError(i - 1, 0, rms);

    }

    TCanvas c(Form("c_%d", iVar), Form("c_%d", iVar));
    c.cd();
    f.cd();
    g.Write();
  }

  f.Close();
}
