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

void Plot_minbias_doubratio(int refindex) {
  
  TFile* file1 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/Minbias_moreclasses_results/analysis_minbias_pydef.root");
  TFile* file2 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/Minbias_moreclasses_results/analysis_minbias_pynew.root");
  TFile* file3 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/Minbias_moreclasses_results/analysis_minbias_epos.root");
 
  TCanvas* c4 = new TCanvas("c4", "K0s doubratio", 0, 0, 900, 900);
  TCanvas* c5 = new TCanvas("c5", "Xi doubratio", 0, 0, 900, 900);
  TCanvas* c6 = new TCanvas("c6", "Omega doubratio", 0, 0, 900, 900);
  TCanvas* c7 = new TCanvas("c7", "Lambda doubratio", 0, 0, 900, 900);
  TCanvas* c8 = new TCanvas("c8", "Proton doubratio", 0, 0, 900, 900);
  TCanvas* c9 = new TCanvas("c9", "Phi doubratio", 0, 0, 900, 900);

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
								
  double doubratioK0s_new[nMultClasses], errdoubratioK0s_new[nMultClasses];
  double doubratioXi_new[nMultClasses], errdoubratioXi_new[nMultClasses];
  double doubratioOmega_new[nMultClasses], errdoubratioOmega_new[nMultClasses];
  double doubratioLambda_new[nMultClasses], errdoubratioLambda_new[nMultClasses];
  double doubratioProton_new[nMultClasses], errdoubratioProton_new[nMultClasses];
  double doubratioPhi_new[nMultClasses], errdoubratioPhi_new[nMultClasses];

  double x_epos[nMultClasses], errx_epos[nMultClasses];

  double ratioK0s_epos[nMultClasses], errratioK0s_epos[nMultClasses];
  double ratioXi_epos[nMultClasses], errratioXi_epos[nMultClasses];
  double ratioOmega_epos[nMultClasses], errratioOmega_epos[nMultClasses];
  double ratioLambda_epos[nMultClasses], errratioLambda_epos[nMultClasses];
  double ratioProton_epos[nMultClasses], errratioProton_epos[nMultClasses];
  double ratioPhi_epos[nMultClasses], errratioPhi_epos[nMultClasses];

  double doubratioK0s_epos[nMultClasses], errdoubratioK0s_epos[nMultClasses];
  double doubratioXi_epos[nMultClasses], errdoubratioXi_epos[nMultClasses];
  double doubratioOmega_epos[nMultClasses], errdoubratioOmega_epos[nMultClasses];
  double doubratioLambda_epos[nMultClasses], errdoubratioLambda_epos[nMultClasses];
  double doubratioProton_epos[nMultClasses], errdoubratioProton_epos[nMultClasses];
  double doubratioPhi_epos[nMultClasses], errdoubratioPhi_epos[nMultClasses];

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

  for (int i = 0; i < nMultClasses; i++) {
    doubratioK0s_new[i] = ratioK0s_new[i]/ratioK0s_new[refindex];
    errdoubratioK0s_new[i] = GetDoubleRatioError(ratioK0s_new[i], errratioK0s_new[i], ratioK0s_new[refindex], errratioK0s_new[refindex]);
    doubratioXi_new[i] = ratioXi_new[i]/ratioXi_new[refindex];
    errdoubratioXi_new[i] = GetDoubleRatioError(ratioXi_new[i], errratioXi_new[i], ratioXi_new[refindex], errratioXi_new[refindex]);
    doubratioOmega_new[i] = ratioOmega_new[i]/ratioOmega_new[refindex];
    errdoubratioOmega_new[i] = GetDoubleRatioError(ratioOmega_new[i], errratioOmega_new[i], ratioOmega_new[refindex], errratioOmega_new[refindex]);
    doubratioLambda_new[i] = ratioLambda_new[i]/ratioLambda_new[refindex];
    errdoubratioLambda_new[i] = GetDoubleRatioError(ratioLambda_new[i], errratioLambda_new[i], ratioLambda_new[refindex], errratioLambda_new[refindex]);
    doubratioProton_new[i] = ratioProton_new[i]/ratioProton_new[refindex];
    errdoubratioProton_new[i] = GetDoubleRatioError(ratioProton_new[i], errratioProton_new[i], ratioProton_new[refindex], errratioProton_new[refindex]);
    doubratioPhi_new[i] = ratioPhi_new[i]/ratioPhi_new[refindex];
    errdoubratioPhi_new[i] = GetDoubleRatioError(ratioPhi_new[i], errratioPhi_new[i], ratioPhi_new[refindex], errratioPhi_new[refindex]);

    doubratioK0s_epos[i] = ratioK0s_epos[i]/ratioK0s_epos[refindex];
    errdoubratioK0s_epos[i] = GetDoubleRatioError(ratioK0s_epos[i], errratioK0s_epos[i], ratioK0s_epos[refindex], errratioK0s_epos[refindex]);
    doubratioXi_epos[i] = ratioXi_epos[i]/ratioXi_epos[refindex];
    errdoubratioXi_epos[i] = GetDoubleRatioError(ratioXi_epos[i], errratioXi_epos[i], ratioXi_epos[refindex], errratioXi_epos[refindex]);
    doubratioOmega_epos[i] = ratioOmega_epos[i]/ratioOmega_epos[refindex];
    errdoubratioOmega_epos[i] = GetDoubleRatioError(ratioOmega_epos[i], errratioOmega_epos[i], ratioOmega_epos[refindex], errratioOmega_epos[refindex]);
    doubratioLambda_epos[i] = ratioLambda_epos[i]/ratioLambda_epos[refindex];
    errdoubratioLambda_epos[i] = GetDoubleRatioError(ratioLambda_epos[i], errratioLambda_epos[i], ratioLambda_epos[refindex], errratioLambda_epos[refindex]);
    doubratioProton_epos[i] = ratioProton_epos[i]/ratioProton_epos[refindex];
    errdoubratioProton_epos[i] = GetDoubleRatioError(ratioProton_epos[i], errratioProton_epos[i], ratioProton_epos[refindex], errratioProton_epos[refindex]);
    doubratioPhi_epos[i] = ratioPhi_epos[i]/ratioPhi_epos[refindex];
    errdoubratioPhi_epos[i] = GetDoubleRatioError(ratioPhi_epos[i], errratioPhi_epos[i], ratioPhi_epos[refindex], errratioPhi_epos[refindex]);
  }

  file1->Close();
  file2->Close();
  file3->Close();

  TMultiGraph* mg4 = new TMultiGraph();
  TMultiGraph* mg5 = new TMultiGraph();
  TMultiGraph* mg6 = new TMultiGraph();
  TMultiGraph* mg7 = new TMultiGraph();
  TMultiGraph* mg8 = new TMultiGraph();
  TMultiGraph* mg9 = new TMultiGraph();

  TGraphErrors* K0splot2_new = new TGraphErrors(nMultClasses, x_new, doubratioK0s_new, errx_new, errdoubratioK0s_new);
  PlotFeatures(K0splot2_new, kBlack, 33, 2, mg4);

  TGraphErrors* Xiplot2_new = new TGraphErrors(nMultClasses, x_new, doubratioXi_new, errx_new, errdoubratioXi_new);
  PlotFeatures(Xiplot2_new, kSpring-1, 33, 2, mg5);

  TGraphErrors* Omegaplot2_new = new TGraphErrors(nMultClasses, x_new, doubratioOmega_new, errx_new, errdoubratioOmega_new);
  PlotFeatures(Omegaplot2_new, kRed, 33, 2, mg6);

  TGraphErrors* Lambdaplot2_new = new TGraphErrors(nMultClasses, x_new, doubratioLambda_new, errx_new, errdoubratioLambda_new);
  PlotFeatures(Lambdaplot2_new, kBlue, 33, 2, mg7);

  TGraphErrors* Protonplot2_new = new TGraphErrors(nMultClasses, x_new, doubratioProton_new, errx_new, errdoubratioProton_new);
  PlotFeatures(Protonplot2_new, kViolet, 33, 2, mg8);

  TGraphErrors* Phiplot2_new = new TGraphErrors(nMultClasses, x_new, doubratioPhi_new, errx_new, errdoubratioPhi_new);
  PlotFeatures(Phiplot2_new, kOrange+1, 33, 2, mg9);
 
  TGraphErrors* K0splot2_epos = new TGraphErrors(nMultClasses, x_epos, doubratioK0s_epos, errx_epos, errdoubratioK0s_epos);
  PlotFeatures(K0splot2_epos, kBlack, 29, 2, mg4);

  TGraphErrors* Xiplot2_epos = new TGraphErrors(nMultClasses, x_epos, doubratioXi_epos, errx_epos, errdoubratioXi_epos);
  PlotFeatures(Xiplot2_epos, kSpring-1, 29, 2, mg5);

  TGraphErrors* Omegaplot2_epos = new TGraphErrors(nMultClasses, x_epos, doubratioOmega_epos, errx_epos, errdoubratioOmega_epos);
  PlotFeatures(Omegaplot2_epos, kRed, 29, 2, mg6);

  TGraphErrors* Lambdaplot2_epos = new TGraphErrors(nMultClasses, x_epos, doubratioLambda_epos, errx_epos, errdoubratioLambda_epos);
  PlotFeatures(Lambdaplot2_epos, kBlue, 29, 2, mg7);

  TGraphErrors* Protonplot2_epos = new TGraphErrors(nMultClasses, x_epos, doubratioProton_epos, errx_epos, errdoubratioProton_epos);
  PlotFeatures(Protonplot2_epos, kViolet, 29, 2, mg8);

  TGraphErrors* Phiplot2_epos = new TGraphErrors(nMultClasses, x_epos, doubratioPhi_epos, errx_epos, errdoubratioPhi_epos);
  PlotFeatures(Phiplot2_epos, kOrange+1, 29, 2, mg9);    
 
  c4->cd();
  c4->SetLogx(true);

  mg4->GetXaxis()->SetMoreLogLabels();
  mg4->GetXaxis()->SetTitleSize(0.03);
  mg4->GetXaxis()->SetTitleOffset(1.3);
  mg4->GetXaxis()->SetLabelOffset(0.);
  mg4->GetXaxis()->SetTitle("<dN_{ch}/d#eta>_{|#eta|<0.5}");
  mg4->GetYaxis()->SetTitle("Ratio of yields to (#pi^{+} + #pi^{-})");
  mg4->GetYaxis()->SetTitleOffset(1.4);
  mg4->Draw("AP");

  c5->cd();
  c5->SetLogx(true);

  mg5->GetXaxis()->SetMoreLogLabels();
  mg5->GetXaxis()->SetTitleSize(0.03);
  mg5->GetXaxis()->SetTitleOffset(1.3);
  mg5->GetXaxis()->SetLabelOffset(0.);
  mg5->GetXaxis()->SetTitle("<dN_{ch}/d#eta>_{|#eta|<0.5}");
  mg5->GetYaxis()->SetTitle("Ratio of yield to (#pi^{+} + #pi^{-})");
  mg5->GetYaxis()->SetTitleOffset(1.4);
  mg5->GetYaxis()->SetLabelSize(0.03);
  mg5->Draw("AP");

  c6->cd();
  c6->SetLogx(true);

  mg6->GetXaxis()->SetMoreLogLabels();
  mg6->GetXaxis()->SetTitleSize(0.03);
  mg6->GetXaxis()->SetTitleOffset(1.3);
  mg6->GetXaxis()->SetLabelOffset(0.);
  mg6->GetXaxis()->SetTitle("<dN_{ch}/d#eta>_{|#eta|<0.5}");
  mg6->GetYaxis()->SetTitle("Ratio of yields to (#pi^{+} + #pi^{-})");
  mg6->GetYaxis()->SetTitleOffset(1.4);
  mg6->Draw("AP");

  c7->cd();
  c7->SetLogx(true);

  mg7->GetXaxis()->SetMoreLogLabels();
  mg7->GetXaxis()->SetTitleSize(0.03);
  mg7->GetXaxis()->SetTitleOffset(1.3);
  mg7->GetXaxis()->SetLabelOffset(0.);
  mg7->GetXaxis()->SetTitle("<dN_{ch}/d#eta>_{|#eta|<0.5}");
  mg7->GetYaxis()->SetTitle("Ratio of yield to (#pi^{+} + #pi^{-})");
  mg7->GetYaxis()->SetTitleOffset(1.4);
  mg7->GetYaxis()->SetLabelSize(0.03);
  mg7->Draw("AP");

  c8->cd();
  c8->SetLogx(true);

  mg8->GetXaxis()->SetMoreLogLabels();
  mg8->GetXaxis()->SetTitleSize(0.03);
  mg8->GetXaxis()->SetTitleOffset(1.3);
  mg8->GetXaxis()->SetLabelOffset(0.);
  mg8->GetXaxis()->SetTitle("<dN_{ch}/d#eta>_{|#eta|<0.5}");
  mg8->GetYaxis()->SetTitle("Ratio of yields to (#pi^{+} + #pi^{-})");
  mg8->GetYaxis()->SetTitleOffset(1.4);
  mg8->Draw("AP");

  c9->cd();
  c9->SetLogx(true);

  mg9->GetXaxis()->SetMoreLogLabels();
  mg9->GetXaxis()->SetTitleSize(0.03);
  mg9->GetXaxis()->SetTitleOffset(1.3);
  mg9->GetXaxis()->SetLabelOffset(0.);
  mg9->GetXaxis()->SetTitle("<dN_{ch}/d#eta>_{|#eta|<0.5}");
  mg9->GetYaxis()->SetTitle("Ratio of yield to (#pi^{+} + #pi^{-})");
  mg9->GetYaxis()->SetTitleOffset(1.4);
  mg9->GetYaxis()->SetLabelSize(0.03);
  mg9->Draw("AP");

  c4->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Minbias_moreclasses_results/k0s_doubratio.root");
  c5->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Minbias_moreclasses_results/xi_doubratio.root");
  c6->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Minbias_moreclasses_results/omega_doubratio.root");
  c7->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Minbias_moreclasses_results/lambda_doubratio.root");
  c8->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Minbias_moreclasses_results/proton_doubratio.root");
  c9->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Minbias_moreclasses_results/phi_doubratio.root");
} 
