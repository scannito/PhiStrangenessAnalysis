// show.C - the table and the overlay of the framework's 2024 K0S/pi ratios (offline efficiency, online, online without
// phi) against the 2024 preliminary remade from the old macro, with a ratio pad (run.sh).
//
// 'pairs' is "label|numerator file|numerator object;..." as globalConfigComparisonOld.json declares them (run.sh reads
// them from there), and every new/preliminary of the table is checked bin by bin against those files: a table that
// mixed two files, or a stale Old_Comparisons.root, stops here.

TH1* Get(TFile& f, const char* path)
{
  auto* h = dynamic_cast<TH1*>(f.Get(path));
  if (!h) {
    printf("[FATAL] show.C: no histogram '%s' in '%s'\n", path, f.GetName());
    gSystem->Exit(1);
  }
  return h;
}

void show(const char* pairs, const char* oldFile, const char* cmpFile, const char* png)
{
  gStyle->SetOptStat(0);
  // the legend and the style of each pair, by its label in the configuration
  struct Style { const char* legend; const char* column; int color, marker; double size; };
  const std::map<std::string, Style> styles = {{"offline_over_preliminary", {"2024, new: offline eff. (stat)", "offline", kRed + 1, 20, 1.6}},
                                               {"online_over_preliminary", {"2024, new: online eff. (stat)", "online", kBlue + 1, 25, 1.4}},
                                               {"onlinenophi_over_preliminary", {"2024, new: online eff., no #phi (stat)", "online, no phi", kGreen + 2, 27, 1.8}}};
  TFile fo(oldFile), fc(cmpFile);
  TH1* hs = Get(fo, "Ratio_K0S_Pi_dy1_00_stat");
  TH1* hy = Get(fo, "Ratio_K0S_Pi_dy1_00_syst");

  struct Pair { Style style; TH1* value; TH1* ratio; };
  std::vector<Pair> list;
  std::vector<std::unique_ptr<TFile>> files;
  std::string purity; // "true" or "false", the same in every file
  for (TObject* token : *TString(pairs).Tokenize(";")) {
    TObjArray* parts = ((TObjString*)token)->GetString().Tokenize("|");
    const std::string label = ((TObjString*)parts->At(0))->GetString().Data();
    const TString file = ((TObjString*)parts->At(1))->GetString(), object = ((TObjString*)parts->At(2))->GetString();
    if (!styles.count(label)) {
      printf("[FATAL] show.C: no style for the pair '%s'\n", label.c_str());
      gSystem->Exit(1);
    }
    TFile& f = *files.emplace_back(std::make_unique<TFile>(file));
    // which production, and whether the purity was applied: the comparison is only as current as these files, and a
    // plot that mixed the two settings would read the purity's 4 % as a difference between the modes. The paths are
    // spelled here because this macro runs in Cling, where the framework's headers are not loaded.
    auto* produced = dynamic_cast<TNamed*>(f.Get("MoreFullDifferential/Provenance/produced_at"));
    auto* commit = dynamic_cast<TNamed*>(f.Get("MoreFullDifferential/Provenance/git_commit"));
    auto* block = dynamic_cast<TNamed*>(f.Get("MoreFullDifferential/Provenance/config_block"));
    const TString config = block ? block->GetTitle() : "";
    const std::string applied = config.Contains("\"apply_purity\":true") ? "true" : config.Contains("\"apply_purity\":false") ? "false" : "";
    if (!produced || applied.empty()) {
      printf("[FATAL] show.C: '%s' carries no provenance, or no apply_purity in it: it predates the records. Re-run the chain "
             "(README.md).\n", file.Data());
      gSystem->Exit(1);
    }
    if (!purity.empty() && applied != purity) {
      printf("[FATAL] show.C: '%s' has apply_purity %s, the files before it %s: re-run the three tasks with one setting.\n",
             file.Data(), applied.c_str(), purity.c_str());
      gSystem->Exit(1);
    }
    purity = applied;
    printf("%s: produced %s, commit %s, apply_purity %s\n", gSystem->BaseName(file), produced->GetTitle(), commit ? commit->GetTitle() : "unknown", applied.c_str());

    TH1* hn = Get(f, object);
    TH1* r = Get(fc, Form("%s/%s", label.c_str(), gSystem->BaseName(object)));
    for (int b = 1; b <= hn->GetNbinsX(); ++b) {
      // relative 1e-6: the division is stored as a TH1F, in single precision
      if (std::abs(hn->GetBinContent(b) / hs->GetBinContent(b) - r->GetBinContent(b)) > 1e-6 * std::abs(r->GetBinContent(b))) {
        printf("[FATAL] show.C: bin %d of '%s' is not new/preliminary of these files: re-run run.sh.\n", b, label.c_str());
        gSystem->Exit(1);
      }
    }
    const Style& s = styles.at(label);
    for (TH1* h : {hn, r}) { h->SetMarkerStyle(s.marker); h->SetMarkerColor(s.color); h->SetLineColor(s.color); h->SetMarkerSize(s.size); }
    list.push_back({s, hn, r});
  }

  printf("class       preliminary (stat) [syst]");
  for (const auto& p : list) printf(" | %-16s", p.style.column);
  printf("   (new/preliminary)\n");
  for (int b = 1; b <= hs->GetNbinsX(); ++b) {
    printf("%5.0f-%-5.0f %.4f (%.4f) [%.4f]", hs->GetXaxis()->GetBinLowEdge(b), hs->GetXaxis()->GetBinUpEdge(b), hs->GetBinContent(b),
           hs->GetBinError(b), hy->GetBinError(b));
    for (const auto& p : list) printf(" | %.3f +- %.3f   ", p.ratio->GetBinContent(b), p.ratio->GetBinError(b));
    printf("\n");
  }

  // the y ranges from what is drawn, so that no point of a future run falls off a pad
  double lo = 1e9, hi = -1e9, rlo = 1e9, rhi = -1e9;
  for (int b = 1; b <= hs->GetNbinsX(); ++b) {
    lo = std::min(lo, hy->GetBinContent(b) - hy->GetBinError(b));
    hi = std::max(hi, hy->GetBinContent(b) + hy->GetBinError(b));
    rlo = std::min(rlo, 1 - hy->GetBinError(b) / hy->GetBinContent(b));
    rhi = std::max(rhi, 1 + hy->GetBinError(b) / hy->GetBinContent(b));
    for (const auto& p : list) {
      lo = std::min(lo, p.value->GetBinContent(b) - p.value->GetBinError(b));
      rlo = std::min(rlo, p.ratio->GetBinContent(b) - p.ratio->GetBinError(b));
      rhi = std::max(rhi, p.ratio->GetBinContent(b) + p.ratio->GetBinError(b));
    }
  }

  TCanvas c("c", "", 900, 850);
  TPad top("top", "", 0, 0.32, 1, 1), bot("bot", "", 0, 0, 1, 0.32);
  top.SetBottomMargin(0); top.SetLeftMargin(0.12); top.SetRightMargin(0.04);
  bot.SetTopMargin(0); bot.SetBottomMargin(0.32); bot.SetLeftMargin(0.12); bot.SetRightMargin(0.04); bot.SetGridy();
  top.Draw(); bot.Draw();

  top.cd();
  hy->SetFillColorAlpha(kGray + 1, 0.3); hy->SetLineColor(kGray + 1); hy->SetMarkerSize(0);
  // the space above the data is for the legend: about as tall again as the data's span
  hy->SetMinimum(lo - 0.1 * (hi - lo)); hy->SetMaximum(hi + 1.1 * (hi - lo));
  hy->SetTitle(";;2K^{0}_{S}/(#pi^{+}+#pi^{-}), |#Deltay|<1");
  hy->GetXaxis()->SetLabelSize(0); hy->GetYaxis()->SetTitleSize(0.05); hy->GetYaxis()->SetLabelSize(0.045); hy->GetYaxis()->SetTitleOffset(1.1);
  hy->Draw("E2");
  hs->SetMarkerStyle(21); hs->SetMarkerColor(kGray + 2); hs->SetLineColor(kGray + 2); hs->Draw("E1 SAME");
  TLegend l(0.37, 0.575, 0.91, 0.805); l.SetBorderSize(0); l.SetFillStyle(0); l.SetTextSize(0.04);
  TLatex text; text.SetNDC(); text.SetTextFont(42); text.SetTextSize(0.04); text.SetTextAlign(12);
  text.DrawLatex(0.389, 0.835, Form("pp, #sqrt{#it{s}} = 13.6 TeV, purity %s", purity == "true" ? "applied" : "not applied"));
  for (const auto& p : list) {
    p.value->Draw("E1 SAME");
    l.AddEntry(p.value, p.style.legend, "lp");
  }
  l.AddEntry(hs, "2024, preliminary (stat)", "lp");
  l.AddEntry(hy, "2024, preliminary (syst)", "f");
  l.Draw();

  bot.cd();
  TH1* band = (TH1*)hy->Clone("band");
  for (int b = 1; b <= band->GetNbinsX(); ++b) {
    band->SetBinError(b, hy->GetBinError(b) / hy->GetBinContent(b));
    band->SetBinContent(b, 1.0);
  }
  const double margin = 0.1 * (rhi - rlo);
  band->SetMinimum(rlo - margin); band->SetMaximum(rhi + margin);
  band->SetTitle(";Multiplicity percentile (%);new / preliminary");
  band->GetXaxis()->SetLabelSize(0.1); band->GetXaxis()->SetTitleSize(0.11); band->GetXaxis()->SetTitleOffset(1.1);
  band->GetYaxis()->SetLabelSize(0.09); band->GetYaxis()->SetTitleSize(0.085); band->GetYaxis()->SetTitleOffset(0.62); band->GetYaxis()->SetNdivisions(505);
  band->Draw("E2");
  TLine one(band->GetXaxis()->GetXmin(), 1, band->GetXaxis()->GetXmax(), 1); one.SetLineStyle(2); one.Draw();
  for (const auto& p : list) p.ratio->Draw("E1 SAME");
  c.SaveAs(png);
}
