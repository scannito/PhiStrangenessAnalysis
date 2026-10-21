// The output of modelScan in three multiplicity classes: the pi spectrum with the Levy-Tsallis and
// blast-wave fits over both ranges and the best of the others, and below it data / fit, each bin against the
// average of the fit over it (as the "I" fit option compares them), computed by modelScan with the live functions.
//
// Usage: root -l -b -q 'drawModels.C("modelScan.root","pi_models.png")'
#include "TCanvas.h"
#include "TF1.h"
#include "TFile.h"
#include "TH1.h"
#include "TH1F.h"
#include "TLegend.h"
#include "TLine.h"
#include "TPad.h"
#include "TStyle.h"

#include <vector>

void drawModels(const char* file, const char* png)
{
  gStyle->SetOptStat(0);
  TFile f(file);
  struct Curve {
    const char* model;
    double upperEdge;
    int color;
    int style;
    const char* label;
  };
  const std::vector<Curve> curves = {
    {"LevyTsallis", 1.0, kRed + 1, 2, "Levy-Tsallis 0.2-1 (until 20 Oct)"},
    {"LevyTsallis", 3.0, kRed + 1, 1, "Levy-Tsallis 0.2-3"},
    {"BlastWave", 1.0, kBlue + 1, 2, "Blast-wave 0.2-1"},
    {"BlastWave", 3.0, kBlue + 1, 1, "Blast-wave 0.2-3 (since 20 Oct)"},
    {"BoseEinstein", 1.0, kGreen + 2, 2, "Bose-Einstein 0.2-1"},
  };
  // The highest, a middle and the lowest multiplicity; the labels are the titles modelScan gave the spectra.
  int nClasses = 0;
  while (f.Get(Form("data_%d", nClasses)))
    ++nClasses;
  const int shown[] = {0, nClasses / 2, nClasses - 1};

  TCanvas c("c", "", 1500, 850);
  c.Divide(3, 1);
  for (int k = 0; k < 3; ++k) {
    const int m = shown[k];
    c.cd(k + 1);
    auto* top = new TPad(Form("t%d", k), "", 0, 0.34, 1, 1);
    top->SetBottomMargin(0.02);
    top->SetLeftMargin(0.15);
    top->SetLogy();
    top->Draw();
    auto* bottom = new TPad(Form("b%d", k), "", 0, 0, 1, 0.34);
    bottom->SetTopMargin(0.02);
    bottom->SetBottomMargin(0.3);
    bottom->SetLeftMargin(0.15);
    bottom->Draw();

    auto* h = dynamic_cast<TH1*>(f.Get(Form("data_%d", m)));
    if (!h) {
      Error("drawModels", "no data_%d in %s", m, file);
      return;
    }
    top->cd();
    TH1F* frame = top->DrawFrame(0, 1e-3, 6, h->GetMaximum() * 5);
    frame->SetTitle(Form("#pi, %s %%, |#Deltay|<1;;1/N_{trig} d^{2}N/dp_{T}d#Deltay", h->GetTitle()));
    frame->GetXaxis()->SetLabelSize(0);
    h->SetMarkerStyle(20);
    h->SetMarkerColor(kBlack);
    h->SetLineColor(kBlack);
    h->Draw("E1 P SAME");
    auto* legend = new TLegend(0.28, 0.6, 0.98, 0.9);
    legend->SetBorderSize(0);
    legend->SetFillStyle(0);
    legend->SetTextSize(0.029);
    legend->AddEntry(h, "data", "lp");

    bottom->cd();
    TH1F* ratioFrame = bottom->DrawFrame(0, 0.4, 6, 1.6);
    ratioFrame->SetTitle(";p_{T} (GeV/c);data / fit");
    ratioFrame->GetXaxis()->SetTitleSize(0.1);
    ratioFrame->GetXaxis()->SetLabelSize(0.09);
    ratioFrame->GetYaxis()->SetTitleSize(0.09);
    ratioFrame->GetYaxis()->SetLabelSize(0.08);
    ratioFrame->GetYaxis()->SetTitleOffset(0.7);
    ratioFrame->GetYaxis()->SetNdivisions(505);

    for (const auto& curve : curves) {
      auto* fn = dynamic_cast<TF1*>(f.Get(Form("f_%s_%g_%d_main", curve.model, curve.upperEdge, m)));
      if (!fn) {
        Error("drawModels", "no curve %s %g in %s", curve.model, curve.upperEdge, file);
        return;
      }
      fn->SetLineColor(curve.color);
      fn->SetLineStyle(curve.style);
      fn->SetLineWidth(2);
      fn->SetNpx(100); // drawn from its 1000 saved points; with 1000 segments a dash pattern never shows
      top->cd();
      fn->Draw("SAME");
      legend->AddEntry(fn, Form("%s  #chi^{2}/ndf %.0f", curve.label, fn->GetChisquare() / fn->GetNDF()), "l");

      auto* ratio = dynamic_cast<TH1*>(f.Get(Form("ratio_%s_%g_%d", curve.model, curve.upperEdge, m)));
      if (!ratio) {
        Error("drawModels", "no ratio %s %g in %s", curve.model, curve.upperEdge, file);
        return;
      }
      ratio->SetMarkerColor(curve.color);
      ratio->SetLineColor(curve.color);
      ratio->SetMarkerStyle(curve.style == 1 ? 20 : 24);
      ratio->SetMarkerSize(0.8);
      bottom->cd();
      ratio->Draw("E1 P SAME");
    }
    top->cd();
    legend->Draw();
    bottom->cd();
    auto* one = new TLine(0, 1, 6, 1);
    one->SetLineStyle(3);
    one->Draw();
  }
  c.SaveAs(png);
}
