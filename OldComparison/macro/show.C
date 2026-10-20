// show.C - the table and the overlay of the framework's K0S/pi ratio against the old one (run.sh).
//
// The numerator file and object are the ones the comparison divided (run.sh reads them from
// globalConfigComparisonOld.json and passes them here), and the 2024/old column is checked bin by
// bin against them: a table that mixed two files, or a stale Old_Comparisons.root, stops here.
TH1* Get(TFile& f, const char* path)
{
  auto* h = dynamic_cast<TH1*>(f.Get(path));
  if (!h) {
    printf("[FATAL] show.C: no histogram '%s' in '%s'\n", path, f.GetName());
    gSystem->Exit(1);
  }
  return h;
}

void show(const char* newFile, const char* newObject, const char* oldFile, const char* cmpFile, const char* label, const char* png)
{
  TFile fn(newFile), fo(oldFile), fc(cmpFile);
  // Which production the numerator is: the comparison is only as current as that file. The path is
  // spelled here because this macro runs in Cling, where the framework's headers are not loaded.
  auto* produced = dynamic_cast<TNamed*>(fn.Get("MoreFullDifferential/Provenance/produced_at"));
  auto* commit = dynamic_cast<TNamed*>(fn.Get("MoreFullDifferential/Provenance/git_commit"));
  if (!produced) {
    printf("[FATAL] show.C: '%s' carries no provenance: it predates the records, and probably the current layout. "
           "Re-run the chain (README.md).\n", newFile);
    gSystem->Exit(1);
  }
  printf("2024 file produced %s, commit %s\n", produced->GetTitle(), commit ? commit->GetTitle() : "unknown");

  TH1* hn = Get(fn, newObject);
  TH1* hs = Get(fo, "Ratio_K0S_Pi_dy1_00_stat");
  TH1* hy = Get(fo, "Ratio_K0S_Pi_dy1_00_syst");
  TH1* r = Get(fc, Form("%s/%s", label, gSystem->BaseName(newObject)));

  printf("class        2024 extrap         old (stat)  (syst)        2024/old\n");
  for (int b = 1; b <= hn->GetNbinsX(); ++b) {
    // Relative 1e-6: the division is stored as a TH1F, in single precision.
    if (std::abs(hn->GetBinContent(b) / hs->GetBinContent(b) - r->GetBinContent(b)) > 1e-6 * std::abs(r->GetBinContent(b))) {
      printf("[FATAL] show.C: bin %d of the comparison is not 2024/old of these files: re-run run.sh.\n", b);
      gSystem->Exit(1);
    }
    printf("%5.0f-%-5.0f  %.4f +- %.4f   %.4f +- %.4f (%.4f)   %.3f +- %.3f\n", hn->GetXaxis()->GetBinLowEdge(b), hn->GetXaxis()->GetBinUpEdge(b),
           hn->GetBinContent(b), hn->GetBinError(b), hs->GetBinContent(b), hs->GetBinError(b), hy->GetBinError(b), r->GetBinContent(b), r->GetBinError(b));
  }

  TCanvas c("c", "", 800, 600);
  hy->SetFillColorAlpha(kGray + 1, 0.5);
  hy->SetLineColor(kGray + 1);
  hy->SetMarkerSize(0);
  hy->SetMinimum(0.08);
  hy->SetMaximum(0.16);
  hy->SetStats(0);
  hy->SetTitle(";Multiplicity percentile (%);2K^{0}_{S}/(#pi^{+}+#pi^{-}), |#Deltay|<1");
  hy->Draw("E2");
  hs->SetMarkerStyle(21);
  hs->SetMarkerColor(kGray + 2);
  hs->SetLineColor(kGray + 2);
  hs->Draw("E1 P SAME");
  hn->SetMarkerStyle(20);
  hn->SetMarkerColor(kRed + 1);
  hn->SetLineColor(kRed + 1);
  hn->Draw("E1 P SAME");
  TLegend l(0.45, 0.72, 0.88, 0.88);
  l.SetBorderSize(0);
  l.AddEntry(hn, "2024, framework (extrap., stat)", "lp");
  l.AddEntry(hs, "old macro (stat)", "lp");
  l.AddEntry(hy, "old macro (syst)", "f");
  l.Draw();
  c.SaveAs(png);
}
