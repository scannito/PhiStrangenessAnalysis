#ifndef Plot_h
#define Plot_h

#include <TH1.h>
#include <TFile.h>
#include <TMath.h>

double GetMeanContent(string hstring, TFile* file, int input) {
							
  TH1D* hname;
  string str = to_string(input);
  string locstring = hstring+str;
  hname = (TH1D*)file->Get(locstring.c_str());
  hname->SetDirectory(0);

  double mean = {};
  int nbin = hname->GetNbinsX();
  for (int bin = 1; bin <= nbin; ++bin) mean += hname->GetBinContent(bin);
  mean /= nbin;

  delete hname;
  return mean;

}

double GetMeanContentError(string hstring, TFile* file, int input) {

  TH1D* hname;
  string str = to_string(input);
  string locstring = hstring+str;
  hname = (TH1D*)file->Get(locstring.c_str());
  hname->SetDirectory(0);

  double errmean = {};
  double mean = GetMeanContent(hstring, file, input);
  int nbin = hname->GetNbinsX();
  for (int bin = 1; bin <= nbin; ++bin) errmean += TMath::Power(hname->GetBinContent(bin)-mean,2);
  errmean = TMath::Sqrt(errmean/(nbin*(nbin-1)));

  delete hname;
  return errmean;

}

double GetRatio(string hstring1, string hstring2, TFile* file, int input) {

  double mean1 = GetMeanContent(hstring1, file, input);
  double mean2 = GetMeanContent(hstring2, file, input);
  double ratio = mean1/mean2;

  return ratio;

}

double GetRatioError(string hstring1, string hstring2, TFile* file, int input) {

  double mean1 = GetMeanContent(hstring1, file, input);
  double errmean1 = GetMeanContentError(hstring1, file, input);

  double mean2 = GetMeanContent(hstring2, file, input);
  double errmean2 = GetMeanContentError(hstring2, file, input);

  double ratio = mean1/mean2;
  double errratio = ratio*TMath::Sqrt(TMath::Power(errmean1/mean1,2)+TMath::Power(errmean2/mean2,2));

  return errratio;

}

double GetMeanContentPlus(string hstring, TFile* file, int input1, int input2) {

  TH1D* hname;
  string str1 = to_string(input1);
  string str2 = to_string(input2);
  string locstring = hstring+str1+"_"+str2; ;
  hname = (TH1D*)file->Get(locstring.c_str());
  hname->SetDirectory(0);

  double mean = {};
  int nbin = hname->GetNbinsX();
  for (int bin = 1; bin <= nbin; ++bin) mean += hname->GetBinContent(bin);
  mean /= nbin;

  delete hname;
  return mean;

}

double GetMeanContentErrorPlus(string hstring, TFile* file, int input1, int input2) {

  TH1D* hname;
  string str1 = to_string(input1);
  string str2 = to_string(input2);
  string locstring = hstring+str1+"_"+str2; ;
  hname = (TH1D*)file->Get(locstring.c_str());
  hname->SetDirectory(0);

  double errmean = {};
  double mean = GetMeanContentPlus(hstring, file, input1, input2);
  int nbin = hname->GetNbinsX();
  for (int bin = 1; bin <= nbin; ++bin) errmean += TMath::Power(hname->GetBinContent(bin)-mean,2);
  errmean = TMath::Sqrt(errmean/(nbin*(nbin-1)));

  delete hname;
  return errmean;

}

double GetRatioPlus(string hstring1, string hstring2, TFile* file, int input1, int input2) {

  double mean1 = GetMeanContentPlus(hstring1, file, input1, input2);
  double mean2 = GetMeanContentPlus(hstring2, file, input1, input2);
  double ratio = mean1/mean2;

  return ratio;

}

double GetRatioErrorPlus(string hstring1, string hstring2, TFile* file, int input1, int input2) {

  double mean1 = GetMeanContentPlus(hstring1, file, input1, input2);
  double errmean1 = GetMeanContentErrorPlus(hstring1, file, input1, input2);

  double mean2 = GetMeanContentPlus(hstring2, file, input1, input2);
  double errmean2 = GetMeanContentErrorPlus(hstring2, file, input1, input2);
					
  double ratio = mean1/mean2;
  double errratio = ratio*TMath::Sqrt(TMath::Power(errmean1/mean1,2)+TMath::Power(errmean2/mean2,2));

  return errratio;

}

double GetMeanContentHisto(string hstring, TFile* file, int input) {

  TH1D* hname;
  string str = to_string(input);
  string locstring = hstring+str;
  hname = (TH1D*)file->Get(locstring.c_str());
  hname->SetDirectory(0);

  double mean = hname->GetMean();

  delete hname;
  return mean;

}

double GetMeanContentErrorHisto(string hstring, TFile* file, int input) {

  TH1D* hname;
  string str = to_string(input);
  string locstring = hstring+str;
  hname = (TH1D*)file->Get(locstring.c_str());
  hname->SetDirectory(0);

  double errmean = hname->GetMeanError();

  delete hname;
  return errmean;

}

double GetRatioHisto(string hstring1, string hstring2, TFile* file, int input) {

  double mean1 = GetMeanContentHisto(hstring1, file, input);
  double mean2 = GetMeanContentHisto(hstring2, file, input);
  double ratio = mean1/mean2;

  return ratio;

}

double GetRatioErrorHisto(string hstring1, string hstring2, TFile* file, int input) {

  double mean1 = GetMeanContentHisto(hstring1, file, input);
  double errmean1 = GetMeanContentErrorHisto(hstring1, file, input);

  double mean2 = GetMeanContentHisto(hstring2, file, input);
  double errmean2 = GetMeanContentErrorHisto(hstring2, file, input);

  double ratio = mean1/mean2;
  double errratio = ratio*TMath::Sqrt(TMath::Power(errmean1/mean1,2)+TMath::Power(errmean2/mean2,2));

  return errratio;

}

double GetMeanContentPlusHisto(string hstring, TFile* file, int input1, int input2) {

  TH1D* hname;
  string str1 = to_string(input1);
  string str2 = to_string(input2);
  string locstring = hstring+str1+"_"+str2; ;
  hname = (TH1D*)file->Get(locstring.c_str());
  hname->SetDirectory(0);

  double mean = hname->GetMean();

  delete hname;
  return mean;

}

double GetMeanContentErrorPlusHisto(string hstring, TFile* file, int input1, int input2) {

  TH1D* hname;
  string str1 = to_string(input1);
  string str2 = to_string(input2);
  string locstring = hstring+str1+"_"+str2; ;
  hname = (TH1D*)file->Get(locstring.c_str());
  hname->SetDirectory(0);

  double errmean = hname->GetMeanError();

  delete hname;
  return errmean;

}

double GetRatioPlusHisto(string hstring1, string hstring2, TFile* file, int input1, int input2) {

  double mean1 = GetMeanContentPlusHisto(hstring1, file, input1, input2);
  double mean2 = GetMeanContentPlusHisto(hstring2, file, input1, input2);
  double ratio = mean1/mean2;

  return ratio;

}

double GetRatioErrorPlusHisto(string hstring1, string hstring2, TFile* file, int input1, int input2) {

  double mean1 = GetMeanContentPlusHisto(hstring1, file, input1, input2);
  double errmean1 = GetMeanContentErrorPlusHisto(hstring1, file, input1, input2);

  double mean2 = GetMeanContentPlusHisto(hstring2, file, input1, input2);
  double errmean2 = GetMeanContentErrorPlusHisto(hstring2, file, input1, input2);

  double ratio = mean1/mean2;
  double errratio = ratio*TMath::Sqrt(TMath::Power(errmean1/mean1,2)+TMath::Power(errmean2/mean2,2));

  return errratio;

}

double GetIntegralPlusHisto(string hstring, TFile* file, int input1, int input2) {
  
  TH1D* hname;
  string str1 = to_string(input1);
  string str2 = to_string(input2);
  string locstring = hstring+str1+"_"+str2; ;
  hname = (TH1D*)file->Get(locstring.c_str());
  hname->SetDirectory(0);

  double integral = {};
  int nbin = hname->GetNbinsX();
  for (int bin = 1; bin <= nbin; ++bin) integral += hname->GetBinContent(bin) * hname->GetBinCenter(bin);

  delete hname;
  return integral;
}

double GetEntriesPlusHisto(string hstring, TFile* file, int input1, int input2) {

  TH1D* hname;
  string str1 = to_string(input1);
  string str2 = to_string(input2);
  string locstring = hstring+str1+"_"+str2; ;
  hname = (TH1D*)file->Get(locstring.c_str());
  hname->SetDirectory(0);

  double entries = hname->GetEntries();

  delete hname;
  return entries;
}

double GetNormalisedIntegralPlusHisto(string hstring1, TFile* file, int input1, int input2, int input3, int input4) {

  double integral = GetIntegralPlusHisto(hstring1, file, input1, input2);
  double normalisation = GetEntriesPlusHisto(hstring1, file, input3, input4);
  double normintegral = integral/normalisation;

  return normintegral;

}

double GetNormalisedIntegralErrorPlusHisto(string hstring1, TFile* file, int input1, int input2, int input3, int input4) {
													
  double integral = GetIntegralPlusHisto(hstring1, file, input1, input2);
  double errintegral = TMath::Sqrt(integral);

  double normalisation = GetEntriesPlusHisto(hstring1, file, input3, input4);
  double errnormalisation = TMath::Sqrt(normalisation);

  double normintegral = integral/normalisation;
  double errnormintegral = normintegral*TMath::Sqrt(TMath::Power(errintegral/integral,2)+TMath::Power(errnormalisation/normalisation,2));

  return errnormintegral;

}

double GetIntegralRatioPlusHisto(string hstring1, string hstring2, TFile* file, int input1, int input2) {

  double integral1 = GetIntegralPlusHisto(hstring1, file, input1, input2);
  double integral2 = GetIntegralPlusHisto(hstring2, file, input1, input2);
  double ratio = integral1/integral2;

  return ratio;

}

double GetIntegralRatioErrorPlusHisto(string hstring1, string hstring2, TFile* file, int input1, int input2) {

  double integral1 = GetIntegralPlusHisto(hstring1, file, input1, input2);
  double errintegral1 = TMath::Sqrt(integral1);

  double integral2 = GetIntegralPlusHisto(hstring2, file, input1, input2);
  double errintegral2 = TMath::Sqrt(integral2);

  double ratio = integral1/integral2;
  double errratio = ratio*TMath::Sqrt(TMath::Power(errintegral1/integral1,2)+TMath::Power(errintegral2/integral2,2));

  return errratio;

}

double GetNormalisedIntegralRatioPlusHisto(string hstring1, string hstring2, TFile* file, int input1, int input2, int input3, int input4) {

  double normintegral1 = GetNormalisedIntegralPlusHisto(hstring1, file, input1, input2, input3, input4);
  double normintegral2 = GetNormalisedIntegralPlusHisto(hstring2, file, input1, input2, input3, input4);
  double ratio = normintegral1/normintegral2;

  return ratio;

}

double GetNormalisedIntegralRatioErrorPlusHisto(string hstring1, string hstring2, TFile* file, int input1, int input2, int input3, int input4) {

  double normintegral1 = GetNormalisedIntegralPlusHisto(hstring1, file, input1, input2, input3, input4);
  double errnormintegral1 = GetNormalisedIntegralErrorPlusHisto(hstring1, file, input1, input2, input3, input4);

  double normintegral2 = GetNormalisedIntegralPlusHisto(hstring2, file, input1, input2, input3, input4);
  double errnormintegral2 = GetNormalisedIntegralErrorPlusHisto(hstring2, file, input1, input2, input3, input4);

  double ratio = normintegral1/normintegral2;
  double errratio = ratio*TMath::Sqrt(TMath::Power(errnormintegral1/normintegral1,2)+TMath::Power(errnormintegral2/normintegral2,2));

  return errratio;

}

double GetDoubleRatioError(double ratio1, double errratio1, double ratio2, double errratio2) {

  double doubratio = ratio1/ratio2;
  double errdoubratio = doubratio*TMath::Sqrt(TMath::Power(errratio1/ratio1,2) + TMath::Power(errratio2/ratio2,2));

  return errdoubratio;

}
                                                      
void PlotFeatures(TGraphErrors* graph, Color_t color, Style_t style, Size_t size) {
 												
  graph->SetMarkerColor(color);
  graph->SetLineColor(color);
  graph->SetMarkerStyle(style);
  graph->SetMarkerSize(size);
  //mg->Add(graph);  

}

void PlotFeaturesPlus(TGraphErrors* graph, Style_t markstyle, Color_t markcolor, Size_t marksize, Style_t linestyle, Color_t linecolor, Width_t linewidth, Style_t fillstyle, Color_t fillcolor, Float_t alpha, TMultiGraph* mg) {
															
  graph->SetMarkerStyle(markstyle);
  graph->SetMarkerColor(markcolor);
  graph->SetMarkerSize(marksize);
  graph->SetLineStyle(linestyle);
  graph->SetLineColor(linecolor);
  graph->SetLineWidth(linewidth);
  graph->SetFillStyle(fillstyle);
  graph->SetFillColorAlpha(fillcolor,alpha);
  mg->Add(graph);

}

#endif
