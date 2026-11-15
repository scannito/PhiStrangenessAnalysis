#include <Riostream.h>
#include <TH1.h>
#include <TGraphErrors.h>
#include <TMultiGraph.h>
#include <TFile.h>
#include <TCanvas.h>
#include <TLegend.h>
#include <TLatex.h>
#include <TMath.h>
#include "Plot.h"

void Plot_minbias() {
  
  TFile* file1 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/Minbias_moreclasses_results/analysis_minbias_pydef.root");
  TFile* file2 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/Minbias_moreclasses_results/analysis_minbias_pynew.root");
  TFile* file3 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/Minbias_moreclasses_results/analysis_minbias_epos.root");
  TFile* file4 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/HEPData/HEPData-ins1784041-v1-Table_17.root");
  TFile* file5 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/HEPData/HEPData-ins1784041-v1-Table_18.root");
  TFile* file6 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/HEPData/HEPData-ins1784041-v1-Table_19.root");
  TFile* file7 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/HEPData/HEPData-ins1762348-v1-Table_46.root");								
 
  TCanvas* c1 = new TCanvas("c1", "Strange hadrons yields", 0, 0, 900, 900);
  TCanvas* c2 = new TCanvas("c2", "Proton yields", 0, 0, 900, 900);
  TCanvas* c3 = new TCanvas("c3", "Phi yields", 0, 0, 900, 900);

  const int nMultClasses = 11;

  double x_def[nMultClasses], errx_def[nMultClasses];
  double ratioK0s_def[nMultClasses], errratioK0s_def[nMultClasses];
  double ratioXi_def[nMultClasses], errratioXi_def[nMultClasses];
  double ratioOmega_def[nMultClasses], errratioOmega_def[nMultClasses];
  double ratioLambda_def[nMultClasses], errratioLambda_def[nMultClasses];
  double ratioProton_def[nMultClasses], errratioProton_def[nMultClasses];
  double ratioPhi_def[nMultClasses], errratioPhi_def[nMultClasses];

  double x_new[nMultClasses], errx_new[nMultClasses];
  double ratioK0s_new[nMultClasses], errratioK0s_new[nMultClasses];
  double ratioXi_new[nMultClasses], errratioXi_new[nMultClasses];
  double ratioOmega_new[nMultClasses], errratioOmega_new[nMultClasses];
  double ratioLambda_new[nMultClasses], errratioLambda_new[nMultClasses];
  double ratioProton_new[nMultClasses], errratioProton_new[nMultClasses];
  double ratioPhi_new[nMultClasses], errratioPhi_new[nMultClasses];

  double x_epos[nMultClasses], errx_epos[nMultClasses];
  double ratioK0s_epos[nMultClasses], errratioK0s_epos[nMultClasses];
  double ratioXi_epos[nMultClasses], errratioXi_epos[nMultClasses];
  double ratioOmega_epos[nMultClasses], errratioOmega_epos[nMultClasses];
  double ratioLambda_epos[nMultClasses], errratioLambda_epos[nMultClasses];
  double ratioProton_epos[nMultClasses], errratioProton_epos[nMultClasses];
  double ratioPhi_epos[nMultClasses], errratioPhi_epos[nMultClasses];

  for (int i = 0; i < nMultClasses; i++) {
 
    x_def[i] = GetMeanContent("h_dndeta", file1, i+1);
    errx_def[i] = GetMeanContentError("h_dndeta", file1, i+1);    
    ratioK0s_def[i] = GetRatio("h_dndyK0s", "h_dndyPion", file1, i+1);
    errratioK0s_def[i] = GetRatioError("h_dndyK0s", "h_dndyPion", file1, i+1);
    ratioXi_def[i] = GetRatio("h_dndyXi", "h_dndyPion", file1, i+1);
    errratioXi_def[i] = GetRatioError("h_dndyXi", "h_dndyPion", file1, i+1);
    ratioOmega_def[i] = GetRatio("h_dndyOmega", "h_dndyPion", file1, i+1);
    errratioOmega_def[i] = GetRatioError("h_dndyOmega", "h_dndyPion", file1, i+1);
    ratioLambda_def[i] = GetRatio("h_dndyLambda", "h_dndyPion", file1, i+1);
    errratioLambda_def[i] = GetRatioError("h_dndyLambda", "h_dndyPion", file1, i+1);
    ratioProton_def[i] = GetRatio("h_dndyProton", "h_dndyPion", file1, i+1);
    errratioProton_def[i] = GetRatioError("h_dndyProton", "h_dndyPion", file1, i+1);
    ratioPhi_def[i] = GetRatio("h_dndyPhi", "h_dndyPion", file1, i+1);
    errratioPhi_def[i] = GetRatioError("h_dndyPhi", "h_dndyPion", file1, i+1);
												
    x_new[i] = GetMeanContent("h_dndeta", file2, i+1);
    errx_new[i] = GetMeanContentError("h_dndeta", file2, i+1);
    ratioK0s_new[i] = GetRatio("h_dndyK0s", "h_dndyPion", file2, i+1);
    errratioK0s_new[i] = GetRatioError("h_dndyK0s", "h_dndyPion", file2, i+1);
    ratioXi_new[i] = GetRatio("h_dndyXi", "h_dndyPion", file2, i+1);
    errratioXi_new[i] = GetRatioError("h_dndyXi", "h_dndyPion", file2, i+1);
    ratioOmega_new[i] = GetRatio("h_dndyOmega", "h_dndyPion", file2, i+1);
    errratioOmega_new[i] = GetRatioError("h_dndyOmega", "h_dndyPion", file2, i+1);
    ratioLambda_new[i] = GetRatio("h_dndyLambda", "h_dndyPion", file2, i+1);
    errratioLambda_new[i] = GetRatioError("h_dndyLambda", "h_dndyPion", file2, i+1);
    ratioProton_new[i] = GetRatio("h_dndyProton", "h_dndyPion", file2, i+1);
    errratioProton_new[i] = GetRatioError("h_dndyProton", "h_dndyPion", file2, i+1);
    ratioPhi_new[i] = GetRatio("h_dndyPhi", "h_dndyPion", file2, i+1);
    errratioPhi_new[i] = GetRatioError("h_dndyPhi", "h_dndyPion", file2, i+1);

    x_epos[i] = GetMeanContent("h_dndeta", file3, i+1);
    errx_epos[i] = GetMeanContentError("h_dndeta", file3, i+1);
    ratioK0s_epos[i] = GetRatio("h_dndyK0s", "h_dndyPion", file3, i+1);
    errratioK0s_epos[i] = GetRatioError("h_dndyK0s", "h_dndyPion", file3, i+1);
    ratioXi_epos[i] = GetRatio("h_dndyXi", "h_dndyPion", file3, i+1);
    errratioXi_epos[i] = GetRatioError("h_dndyXi", "h_dndyPion", file3, i+1);
    ratioOmega_epos[i] = GetRatio("h_dndyOmega", "h_dndyPion", file3, i+1);
    errratioOmega_epos[i] = GetRatioError("h_dndyOmega", "h_dndyPion", file3, i+1);
    ratioLambda_epos[i] = GetRatio("h_dndyLambda", "h_dndyPion", file3, i+1);
    errratioLambda_epos[i] = GetRatioError("h_dndyLambda", "h_dndyPion", file3, i+1);
    ratioProton_epos[i] = GetRatio("h_dndyProton", "h_dndyPion", file3, i+1);
    errratioProton_epos[i] = GetRatioError("h_dndyProton", "h_dndyPion", file3, i+1);
    ratioPhi_epos[i] = GetRatio("h_dndyPhi", "h_dndyPion", file3, i+1);
    errratioPhi_epos[i] = GetRatioError("h_dndyPhi", "h_dndyPion", file3, i+1);

  }	  

  TDirectoryFile* dir1 = (TDirectoryFile*)file4->Get("Table 17");
  TGraphErrors* Protonplot_data = (TGraphErrors*)dir1->Get("Graph1D_y2");

  TDirectoryFile* dir2 = (TDirectoryFile*)file5->Get("Table 18");
  TGraphErrors* K0splot_data = (TGraphErrors*)dir2->Get("Graph1D_y1");
  TGraphErrors* Lambdaplot_data = (TGraphErrors*)dir2->Get("Graph1D_y2");
  TGraphErrors* Xiplot_data = (TGraphErrors*)dir2->Get("Graph1D_y3");

  TDirectoryFile* dir3 = (TDirectoryFile*)file6->Get("Table 19");
  TGraphErrors* Omegaplot_data = (TGraphErrors*)dir3->Get("Graph1D_y1");

  TDirectoryFile* dir4 = (TDirectoryFile*)file7->Get("Table 46");
  TGraphErrors* Phiplot_data = (TGraphErrors*)dir4->Get("Graph1D_y1");

  file1->Close();
  file2->Close();
  file3->Close();
  file4->Close();
  file5->Close();
  file6->Close();
  file7->Close();

  TMultiGraph* mg1 = new TMultiGraph();
  mg1->GetXaxis()->SetLimits(2,100);
  mg1->SetMinimum(0.0007);
  mg1->SetMaximum(0.3);

  TMultiGraph* mg2 = new TMultiGraph();
  TMultiGraph* mg3 = new TMultiGraph();

  TGraphErrors* K0splot_def = new TGraphErrors(nMultClasses, x_def, ratioK0s_def, errx_def, errratioK0s_def);
  K0splot_def->Scale(2.);
  K0splot_def->SetMarkerColor(kBlack);
  K0splot_def->SetLineColor(kBlack);
  K0splot_def->SetMarkerStyle(34);
  K0splot_def->SetMarkerSize(2);
  mg1->Add(K0splot_def);

  TGraphErrors* Xiplot_def = new TGraphErrors(nMultClasses, x_def, ratioXi_def, errx_def, errratioXi_def);
  Xiplot_def->Scale(6.);
  Xiplot_def->SetMarkerColor(kSpring-1);
  Xiplot_def->SetLineColor(kSpring-1);
  Xiplot_def->SetMarkerStyle(34);
  Xiplot_def->SetMarkerSize(2);
  mg1->Add(Xiplot_def);

  TGraphErrors* Omegaplot_def = new TGraphErrors(nMultClasses, x_def, ratioOmega_def, errx_def, errratioOmega_def);
  Omegaplot_def->Scale(16.);
  Omegaplot_def->SetMarkerColor(kRed);
  Omegaplot_def->SetLineColor(kRed);
  Omegaplot_def->SetMarkerStyle(34);
  Omegaplot_def->SetMarkerSize(2);
  mg1->Add(Omegaplot_def);

  TGraphErrors* Lambdaplot_def = new TGraphErrors(nMultClasses, x_def, ratioLambda_def, errx_def, errratioLambda_def);
  Lambdaplot_def->Scale(2.);
  Lambdaplot_def->SetMarkerColor(kBlue);
  Lambdaplot_def->SetLineColor(kBlue);
  Lambdaplot_def->SetMarkerStyle(34);
  Lambdaplot_def->SetMarkerSize(2);
  mg1->Add(Lambdaplot_def);

  TGraphErrors* Protonplot_def = new TGraphErrors(nMultClasses, x_def, ratioProton_def, errx_def, errratioProton_def);
  Protonplot_def->SetMarkerColor(kViolet);
  Protonplot_def->SetLineColor(kViolet);
  Protonplot_def->SetMarkerStyle(34);
  Protonplot_def->SetMarkerSize(2);
  mg2->Add(Protonplot_def);

  TGraphErrors* Phiplot_def = new TGraphErrors(nMultClasses, x_def, ratioPhi_def, errx_def, errratioPhi_def);
  //Phiplot_def->Scale(2.);
  Phiplot_def->SetMarkerColor(kOrange+1);
  Phiplot_def->SetLineColor(kOrange+1);
  Phiplot_def->SetMarkerStyle(34);
  Phiplot_def->SetMarkerSize(2);
  mg3->Add(Phiplot_def);
				
  TGraphErrors* K0splot_new = new TGraphErrors(nMultClasses, x_new, ratioK0s_new, errx_new, errratioK0s_new);
  K0splot_new->Scale(2.);
  K0splot_new->SetMarkerColor(kBlack);
  K0splot_new->SetLineColor(kBlack);
  K0splot_new->SetMarkerStyle(33);
  K0splot_new->SetMarkerSize(2);
  mg1->Add(K0splot_new);

  TGraphErrors* Xiplot_new = new TGraphErrors(nMultClasses, x_new, ratioXi_new, errx_new, errratioXi_new);
  Xiplot_new->Scale(6.);
  Xiplot_new->SetMarkerColor(kSpring-1);
  Xiplot_new->SetLineColor(kSpring-1);
  Xiplot_new->SetMarkerStyle(33);
  Xiplot_new->SetMarkerSize(2);
  mg1->Add(Xiplot_new);

  TGraphErrors* Omegaplot_new = new TGraphErrors(nMultClasses, x_new, ratioOmega_new, errx_new, errratioOmega_new);
  Omegaplot_new->Scale(16.);
  Omegaplot_new->SetMarkerColor(kRed);
  Omegaplot_new->SetLineColor(kRed);
  Omegaplot_new->SetMarkerStyle(33);
  Omegaplot_new->SetMarkerSize(2);
  mg1->Add(Omegaplot_new);

  TGraphErrors* Lambdaplot_new = new TGraphErrors(nMultClasses, x_new, ratioLambda_new, errx_new, errratioLambda_new);
  Lambdaplot_new->Scale(2.);
  Lambdaplot_new->SetMarkerColor(kBlue);
  Lambdaplot_new->SetLineColor(kBlue);
  Lambdaplot_new->SetMarkerStyle(33);
  Lambdaplot_new->SetMarkerSize(2);
  mg1->Add(Lambdaplot_new);

  TGraphErrors* Protonplot_new = new TGraphErrors(nMultClasses, x_new, ratioProton_new, errx_new, errratioProton_new);
  Protonplot_new->SetMarkerColor(kViolet);
  Protonplot_new->SetLineColor(kViolet);
  Protonplot_new->SetMarkerStyle(33);
  Protonplot_new->SetMarkerSize(2);
  mg2->Add(Protonplot_new);

  TGraphErrors* Phiplot_new = new TGraphErrors(nMultClasses, x_new, ratioPhi_new, errx_new, errratioPhi_new);
  //Phiplot_new->Scale(2.);
  Phiplot_new->SetMarkerColor(kOrange+1);
  Phiplot_new->SetLineColor(kOrange+1);
  Phiplot_new->SetMarkerStyle(33);
  Phiplot_new->SetMarkerSize(2);
  mg3->Add(Phiplot_new);

  TGraphErrors* K0splot_epos = new TGraphErrors(nMultClasses, x_epos, ratioK0s_epos, errx_epos, errratioK0s_epos);
  K0splot_epos->Scale(2.);
  K0splot_epos->SetMarkerColor(kBlack);
  K0splot_epos->SetLineColor(kBlack);
  K0splot_epos->SetMarkerStyle(29);
  K0splot_epos->SetMarkerSize(2);
  mg1->Add(K0splot_epos);

  TGraphErrors* Xiplot_epos = new TGraphErrors(nMultClasses, x_epos, ratioXi_epos, errx_epos, errratioXi_epos);
  Xiplot_epos->Scale(6.);
  Xiplot_epos->SetMarkerColor(kSpring-1);
  Xiplot_epos->SetLineColor(kSpring-1);
  Xiplot_epos->SetMarkerStyle(29);
  Xiplot_epos->SetMarkerSize(2);
  mg1->Add(Xiplot_epos);

  TGraphErrors* Omegaplot_epos = new TGraphErrors(nMultClasses, x_epos, ratioOmega_epos, errx_epos, errratioOmega_epos);
  Omegaplot_epos->Scale(16.);
  Omegaplot_epos->SetMarkerColor(kRed);
  Omegaplot_epos->SetLineColor(kRed);
  Omegaplot_epos->SetMarkerStyle(29);
  Omegaplot_epos->SetMarkerSize(2);
  mg1->Add(Omegaplot_epos);

  TGraphErrors* Lambdaplot_epos = new TGraphErrors(nMultClasses, x_epos, ratioLambda_epos, errx_epos, errratioLambda_epos);
  Lambdaplot_epos->Scale(2.);
  Lambdaplot_epos->SetMarkerColor(kBlue);
  Lambdaplot_epos->SetLineColor(kBlue);
  Lambdaplot_epos->SetMarkerStyle(29);
  Lambdaplot_epos->SetMarkerSize(2);
  mg1->Add(Lambdaplot_epos);

  TGraphErrors* Protonplot_epos = new TGraphErrors(nMultClasses, x_epos, ratioProton_epos, errx_epos, errratioProton_epos);
  Protonplot_epos->SetMarkerColor(kViolet);
  Protonplot_epos->SetLineColor(kViolet);
  Protonplot_epos->SetMarkerStyle(29);
  Protonplot_epos->SetMarkerSize(2);
  mg2->Add(Protonplot_epos);

  TGraphErrors* Phiplot_epos = new TGraphErrors(nMultClasses, x_epos, ratioPhi_epos, errx_epos, errratioPhi_epos);
  //Phiplot_epos->Scale(2.);
  Phiplot_epos->SetMarkerColor(kOrange+1);
  Phiplot_epos->SetLineColor(kOrange+1);
  Phiplot_epos->SetMarkerStyle(29);
  Phiplot_epos->SetMarkerSize(2);
  mg3->Add(Phiplot_epos);

  K0splot_data->SetMarkerColor(kBlack);
  K0splot_data->SetLineColor(kBlack);
  K0splot_data->SetMarkerStyle(20);
  K0splot_data->SetMarkerSize(1.5);
  mg1->Add(K0splot_data);

  Xiplot_data->Scale(6.);
  Xiplot_data->SetMarkerColor(kSpring-1);
  Xiplot_data->SetLineColor(kSpring-1);
  Xiplot_data->SetMarkerStyle(20);
  Xiplot_data->SetMarkerSize(1.5);
  mg1->Add(Xiplot_data);

  Omegaplot_data->Scale(16.);
  Omegaplot_data->SetMarkerColor(kRed);
  Omegaplot_data->SetLineColor(kRed);
  Omegaplot_data->SetMarkerStyle(20);
  Omegaplot_data->SetMarkerSize(1.5);
  mg1->Add(Omegaplot_data);

  Lambdaplot_data->Scale(2.);
  Lambdaplot_data->SetMarkerColor(kBlue);
  Lambdaplot_data->SetLineColor(kBlue);
  Lambdaplot_data->SetMarkerStyle(20);
  Lambdaplot_data->SetMarkerSize(1.5);
  mg1->Add(Lambdaplot_data);

  Protonplot_data->SetMarkerColor(kViolet);
  Protonplot_data->SetLineColor(kViolet);
  Protonplot_data->SetMarkerStyle(20);
  Protonplot_data->SetMarkerSize(1.5);
  mg2->Add(Protonplot_data);

  Phiplot_data->SetMarkerColor(kOrange+1);
  Phiplot_data->SetLineColor(kOrange+1);
  Phiplot_data->SetMarkerStyle(20);
  Phiplot_data->SetMarkerSize(1.5);
  mg3->Add(Phiplot_data);

  c1->cd();
  c1->SetLogx(true);
  c1->SetLogy(true);

  mg1->GetXaxis()->SetMoreLogLabels(); 
  mg1->GetXaxis()->SetTitleSize(0.03);
  mg1->GetXaxis()->SetTitleOffset(1.3);
  mg1->GetXaxis()->SetLabelOffset(0.);
  mg1->GetXaxis()->SetLabelSize(0.03);
  mg1->GetXaxis()->SetTitle("#LTdN_{ch}/d#eta#GT_{|#eta|<0.5}");
  mg1->GetYaxis()->SetTitle("#LTN_{h}#GT/#LTN(#pi^{+}+#pi^{-})#GT");
  //mg1->GetYaxis()->SetTitle("Ratio of yields to (#pi^{+}+#pi^{#minus})");
  mg1->GetYaxis()->SetTitleOffset(1.2);
  mg1->GetYaxis()->SetLabelSize(0.025);
  mg1->Draw("AP");

  TLegend* L1 = new TLegend(0.6, 0.22, 0.7, 0.32);
  L1->SetTextSize(0.025);
  L1->AddEntry(K0splot_data, "ALICE pp, #sqrt{s} = 13.0 TeV", "p");  
  L1->AddEntry(K0splot_def, "PYTHIA8, Default", "p");
  L1->AddEntry(K0splot_new, "PYTHIA8, CR+Ropes", "p");
  L1->AddEntry(K0splot_epos, "EPOS4", "p");
  L1->SetLineWidth(0);
  L1->Draw("same");

  TLegend* L2 = new TLegend(0.68, 0.8, 0.8, 0.8);
  L2->SetTextSize(0.03);
  L2->AddEntry(K0splot_new, "2K^{0}_{S}", "l"); 
  L2->SetLineWidth(0);
  L2->Draw("same");

  TLegend* L3 = new TLegend(0.68, 0.73, 0.8, 0.73);
  L3->SetTextSize(0.03);
  L3->AddEntry(Lambdaplot_new, "#Lambda + #bar{#Lambda} (x2)", "l");
  L3->SetLineWidth(0);
  L3->Draw("same");

  TLegend* L4 = new TLegend(0.68, 0.6, 0.8, 0.6);
  L4->SetTextSize(0.03);
  L4->AddEntry(Xiplot_new, "#Xi^{-} + #bar{#Xi}^{+} (x6)", "l");
  L4->SetLineWidth(0);
  L4->Draw("same");

  TLegend* L5 = new TLegend(0.68, 0.43, 0.8, 0.43);
  L5->SetTextSize(0.03);
  L5->AddEntry(Omegaplot_new, "#Omega^{-} + #bar{#Omega}^{+} (x16)", "l");
  L5->SetLineWidth(0);
  L5->Draw("same");

  c2->cd();
  c2->SetLogx(true);

  mg2->GetXaxis()->SetMoreLogLabels(); 
  mg2->GetXaxis()->SetTitleSize(0.03);
  mg2->GetXaxis()->SetTitleOffset(1.3);
  mg2->GetXaxis()->SetLabelOffset(0.);
  mg2->GetXaxis()->SetLabelSize(0.03);
  mg2->GetXaxis()->SetTitle("#LTdN_{ch}/d#eta#GT_{|#eta|<0.5}");
  mg2->GetYaxis()->SetTitle("#LTN(p+#bar{p})#GT/#LTN(#pi^{+}+#pi^{-})#GT");
  //mg2->GetYaxis()->SetTitle("(p+#bar{p})/(#pi^{+}+#pi^{#minus})");
  mg2->GetYaxis()->SetTitleOffset(1.3);
  mg2->GetYaxis()->SetLabelSize(0.025);
  mg2->Draw("AP");

  TLegend* L6 = new TLegend(0.14, 0.67, 0.35, 0.77);
  L6->SetTextSize(0.025);
  L6->AddEntry(K0splot_data, "ALICE pp, #sqrt{s} = 13.0 TeV", "p");  
  L6->AddEntry(K0splot_def, "PYTHIA8, Default", "p");
  L6->AddEntry(K0splot_new, "PYTHIA8, CR+Ropes", "p");
  L6->AddEntry(K0splot_epos, "EPOS4", "p");
  L6->SetLineWidth(0);
  L6->Draw("same");

  /*TLegend* L7 = new TLegend(0.14, 0.8, 0.35, 0.8);
  L7->SetTextSize(0.03);
  L7->AddEntry(Protonplot_new, "p + #bar{p}", "l");  
  L7->SetLineWidth(0);
  L7->Draw("same");*/

  c3->cd();
  c3->SetLogx(true);

  mg3->GetXaxis()->SetMoreLogLabels();
  mg3->GetXaxis()->SetTitleSize(0.03);
  mg3->GetXaxis()->SetTitleOffset(1.3);
  mg3->GetXaxis()->SetLabelOffset(0.);
  mg3->GetXaxis()->SetLabelSize(0.03);
  mg3->GetXaxis()->SetTitle("#LTdN_{ch}/d#eta#GT_{|#eta|<0.5}");
  mg3->GetYaxis()->SetTitle("#LTN(#phi)#GT/#LTN(#pi^{+}+#pi^{-})#GT");
  mg3->GetYaxis()->SetTitleOffset(1.4);
  mg3->GetYaxis()->SetLabelSize(0.025);
  mg3->Draw("AP");

  TLegend* L8 = new TLegend(0.14, 0.67, 0.35, 0.77);
  L8->SetTextSize(0.025);
  L8->AddEntry(K0splot_data, "ALICE pp, #sqrt{s} = 13.0 TeV", "p");
  L8->AddEntry(K0splot_def, "PYTHIA8, Default", "p");
  L8->AddEntry(K0splot_new, "PYTHIA8, CR+Ropes", "p");
  L8->AddEntry(K0splot_epos, "EPOS4", "p");
  L8->SetLineWidth(0);
  L8->Draw("same");

  /*TLegend* L9 = new TLegend(0.14, 0.8, 0.35, 0.8);
  L9->SetTextSize(0.03);
  L9->AddEntry(Phiplot_new, "#phi", "l");
  L9->SetLineWidth(0);
  L9->Draw("same");*/

  c1->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Minbias_moreclasses_results/strange_enhancement.root");  
  c2->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Minbias_moreclasses_results/proton_enhancement.root");
  c3->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Minbias_moreclasses_results/phi_enhancement.root");
 											
  c1->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/strange_minbias.pdf");
  c2->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/proton_minbias.pdf");
  c3->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/phi_minbias.pdf");
} 
