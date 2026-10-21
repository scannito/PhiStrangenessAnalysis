// The output of twoFits in three multiplicity classes: the pi spectrum with the Levy-Tsallis over 0.2-1.0
// (the default until 20 October 2026), the blast-wave over 0.2-3.0 (the default since) and the two fits
// (Levy-Tsallis 0.2-1.0, and a Levy-Tsallis over 1.0-3.0 dashed for the high side); below, data / curve, each
// bin against the average of the curve that serves it, computed by twoFits with the live functions. The cases are
// asked for by their key in twoFits.cpp; labels and classes are read from the file, and each curve was written over
// the side it serves.
//
// Usage: root -l -b -q 'drawTwoFits.C("twoFits.root","pi_two_fits.png")'
#include "TCanvas.h"
#include "TF1.h"
#include "TFile.h"
#include "TH1.h"
#include "TLegend.h"
#include "TLine.h"
#include "TPad.h"
#include "TStyle.h"

void drawTwoFits(const char* file, const char* png)
{
  gStyle->SetOptStat(0);
  TFile f(file);
  struct Shown {
    const char* key;
    int color;
  };
  const Shown cases[] = {{"levy", kRed + 1}, {"blastwave", kBlue + 1}, {"levy+levy1.0", kGreen + 2}};
  int nClasses = 0;
  while (f.Get(Form("data_%d", nClasses)))
    ++nClasses;
  const int shown[] = {0, nClasses / 2, nClasses - 1}; // the highest, a middle and the lowest multiplicity

  TCanvas c("c", "", 1500, 800);
  for (int k = 0; k < 3; ++k) {
    const int m = shown[k];
    auto* h = dynamic_cast<TH1*>(f.Get(Form("data_%d", m)));
    if (!h) {
      Error("drawTwoFits", "no data_%d in %s", m, file);
      return;
    }
    c.cd();
    auto* top = new TPad(Form("t%d", k), "", k / 3., 0.35, (k + 1) / 3., 1.0);
    top->SetBottomMargin(0.02);
    top->SetLogy();
    top->Draw();
    top->cd();
    auto* frame = top->DrawFrame(0.0, h->GetMinimum(0) * 0.02, 6.0, h->GetMaximum() * 3);
    frame->SetTitle(Form("#pi, FT0M %s %%, |#Deltay|<1;;1/N_{trig} d^{2}N/dp_{T}d#Deltay", h->GetTitle()));
    frame->GetXaxis()->SetLabelSize(0);
    h->SetMarkerStyle(20);
    h->SetMarkerColor(kBlack);
    h->SetLineColor(kBlack);
    h->Draw("same pe");
    auto* legend = new TLegend(0.3, 0.68, 0.89, 0.89);
    legend->SetBorderSize(0);
    legend->SetFillStyle(0);
    legend->AddEntry(h, "data", "pe");

    c.cd();
    auto* bottom = new TPad(Form("b%d", k), "", k / 3., 0.0, (k + 1) / 3., 0.35);
    bottom->SetTopMargin(0.02);
    bottom->SetBottomMargin(0.25);
    bottom->Draw();
    bottom->cd();
    auto* ratioFrame = bottom->DrawFrame(0.0, 0.6, 6.0, 1.4);
    ratioFrame->SetTitle(";p_{T} (GeV/c);data / curve");
    ratioFrame->GetXaxis()->SetTitleSize(0.09);
    ratioFrame->GetXaxis()->SetLabelSize(0.08);
    ratioFrame->GetYaxis()->SetLabelSize(0.08);
    ratioFrame->GetYaxis()->SetTitleSize(0.08);
    ratioFrame->GetYaxis()->SetTitleOffset(0.6);
    auto* one = new TLine(0, 1, 6, 1);
    one->SetLineStyle(3);
    one->Draw();

    for (const auto& cs : cases) {
      auto* main = dynamic_cast<TF1*>(f.Get(Form("f_%s_%d_main", cs.key, m)));
      auto* high = dynamic_cast<TF1*>(f.Get(Form("f_%s_%d_high", cs.key, m))); // null for a single fit
      if (!main) {
        Error("drawTwoFits", "no case %s in %s", cs.key, file);
        return;
      }
      top->cd();
      for (TF1* fn : {main, high}) {
        if (!fn)
          continue;
        fn->SetLineColor(cs.color);
        fn->SetLineWidth(2);
        fn->SetLineStyle(fn == high ? 2 : 1);
        fn->SetNpx(100); // drawn from its 1000 saved points; with 1000 segments a dash pattern never shows
        fn->Draw("same");
      }
      legend->AddEntry(main, Form("%s%s", main->GetTitle(), high ? " (dashed)" : ""), "l");

      auto* ratio = dynamic_cast<TH1*>(f.Get(Form("ratio_%s_%d", cs.key, m)));
      if (!ratio) {
        Error("drawTwoFits", "no ratio for %s in %s", cs.key, file);
        return;
      }
      ratio->SetMarkerColor(cs.color);
      ratio->SetLineColor(cs.color);
      ratio->SetMarkerStyle(20);
      ratio->SetMarkerSize(0.7);
      bottom->cd();
      ratio->Draw("same pe");
    }
    top->cd();
    legend->Draw();
  }
  c.SaveAs(png);
}
