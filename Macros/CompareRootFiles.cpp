// Object-by-object comparison of two ROOT files: the before/after check of a change that
// must not move any number. Written for the corrections refactoring (DESIGN_HISTORY.md,
// "The corrections block"), kept because every such change needs the same check.
//
// Exact equality on purpose, contents, errors and bin edges: a refactoring that changes a
// number in the last bit has changed something. Histograms (any dimension), THnSparse,
// TGraph, TObjString and TNamed titles are compared; canvases are skipped (they hold
// drawn copies of the rest), and so are "Provenance" directories (timestamp, commit).
//
// A standalone program and not a macro, so it does not depend on Cling:
//   clang++ -O1 -std=c++20 Macros/CompareRootFiles.cpp $(root-config --cflags --libs) -o compareRootFiles
//   ./compareRootFiles before.root after.root   # differences listed, exit code 1 if any
//
// Copy the output directory between the two runs: both write the same files (CLAUDE.md,
// *How to run it*).
#include "TCanvas.h"
#include "TDirectory.h"
#include "TFile.h"
#include "TGraph.h"
#include "TH1.h"
#include "THnSparse.h"
#include "TKey.h"
#include "TNamed.h"
#include "TObjString.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

static int nDiff = 0, nCompared = 0, nSkipped = 0;

static void report(const std::string& path, const std::string& what)
{
  if (nDiff < 100000)
    std::printf("DIFF %s: %s\n", path.c_str(), what.c_str());
  ++nDiff;
}

static bool sameAxis(const TAxis* a, const TAxis* b)
{
  if (a->GetNbins() != b->GetNbins())
    return false;
  for (int i = 1; i <= a->GetNbins() + 1; ++i)
    if (a->GetBinLowEdge(i) != b->GetBinLowEdge(i))
      return false;
  return true;
}

static void compareTH1(const std::string& path, TH1* a, TH1* b)
{
  if (a->GetDimension() != b->GetDimension() || !sameAxis(a->GetXaxis(), b->GetXaxis()) ||
      !sameAxis(a->GetYaxis(), b->GetYaxis()) || !sameAxis(a->GetZaxis(), b->GetZaxis())) {
    report(path, "axes differ");
    return;
  }
  const int n = a->GetNcells();
  int bad = 0;
  double worst = 0, worstErr = 0;
  for (int i = 0; i < n; ++i) {
    const double ca = a->GetBinContent(i), cb = b->GetBinContent(i);
    const double ea = a->GetBinError(i), eb = b->GetBinError(i);
    if (ca != cb || ea != eb) {
      ++bad;
      if (ca != cb)
        worst = std::max(worst, std::abs(ca - cb) / std::max(std::abs(ca), 1e-300));
      if (ea != eb)
        worstErr = std::max(worstErr, std::abs(ea - eb) / std::max(std::abs(ea), 1e-300));
    }
  }
  if (bad) {
    char buf[200];
    std::snprintf(buf, sizeof buf, " cells differ, worst relative difference: content %.3g, error %.3g", worst, worstErr);
    report(path, std::to_string(bad) + " of " + std::to_string(n) + buf);
  }
  if (a->GetEntries() != b->GetEntries())
    report(path, "entries differ");
}

static void compareSparse(const std::string& path, THnSparse* a, THnSparse* b)
{
  if (a->GetNdimensions() != b->GetNdimensions() || a->GetNbins() != b->GetNbins()) {
    report(path, "dimensions or number of filled bins differ");
    return;
  }
  std::vector<int> idx(a->GetNdimensions());
  int bad = 0;
  for (Long64_t i = 0; i < a->GetNbins(); ++i) {
    const double ca = a->GetBinContent(i, idx.data());
    const Long64_t j = b->GetBin(idx.data(), false);
    if (j < 0 || b->GetBinContent(j) != ca || b->GetBinError(j) != a->GetBinError(i))
      ++bad;
  }
  if (bad)
    report(path, std::to_string(bad) + " filled bins differ");
}

static void compareGraph(const std::string& path, TGraph* a, TGraph* b)
{
  if (a->GetN() != b->GetN()) {
    report(path, "number of points differs");
    return;
  }
  for (int i = 0; i < a->GetN(); ++i)
    if (a->GetX()[i] != b->GetX()[i] || a->GetY()[i] != b->GetY()[i] || a->GetErrorY(i) != b->GetErrorY(i)) {
      report(path, "point " + std::to_string(i) + " differs");
      return;
    }
}

static void walk(TDirectory* da, TDirectory* db, const std::string& prefix)
{
  for (auto* obj : *da->GetListOfKeys()) {
    auto* key = static_cast<TKey*>(obj);
    if (da->GetKey(key->GetName())->GetCycle() != key->GetCycle())
      continue; // only the highest cycle
    const std::string name = key->GetName();
    const std::string path = prefix + "/" + name;
    if (name == "Provenance")
      continue;

    TKey* kb = db->GetKey(name.c_str());
    if (!kb) {
      report(path, "missing in B");
      continue;
    }
    if (std::string(key->GetClassName()) != kb->GetClassName()) {
      report(path, std::string("class ") + key->GetClassName() + " vs " + kb->GetClassName());
      continue;
    }

    TObject* oa = key->ReadObj();
    TObject* ob = kb->ReadObj();
    if (auto* sa = dynamic_cast<TDirectory*>(oa)) {
      walk(sa, static_cast<TDirectory*>(ob), path);
      continue;
    }
    ++nCompared;
    if (auto* ha = dynamic_cast<TH1*>(oa))
      compareTH1(path, ha, static_cast<TH1*>(ob));
    else if (auto* sa = dynamic_cast<THnSparse*>(oa))
      compareSparse(path, sa, static_cast<THnSparse*>(ob));
    else if (auto* ga = dynamic_cast<TGraph*>(oa))
      compareGraph(path, ga, static_cast<TGraph*>(ob));
    else if (auto* osa = dynamic_cast<TObjString*>(oa)) {
      if (osa->GetString() != static_cast<TObjString*>(ob)->GetString())
        report(path, "string differs");
    } else if (dynamic_cast<TCanvas*>(oa)) {
      --nCompared;
      ++nSkipped; // drawn copies of what is compared above
    } else if (auto* na = dynamic_cast<TNamed*>(oa)) {
      if (std::string(na->GetTitle()) != static_cast<TNamed*>(ob)->GetTitle())
        report(path, "title differs");
    }
    delete oa;
    delete ob;
  }
  // keys only in B
  for (auto* obj : *db->GetListOfKeys()) {
    const std::string name = obj->GetName();
    if (name != "Provenance" && !da->GetKey(name.c_str()))
      report(prefix + "/" + name, "missing in A");
  }
}

int main(int argc, char** argv)
{
  if (argc != 3) {
    std::fprintf(stderr, "usage: rootcmp <fileA> <fileB>\n");
    return 2;
  }
  std::unique_ptr<TFile> a(TFile::Open(argv[1], "READ")), b(TFile::Open(argv[2], "READ"));
  if (!a || a->IsZombie() || !b || b->IsZombie()) {
    std::fprintf(stderr, "cannot open\n");
    return 2;
  }
  walk(a.get(), b.get(), "");
  std::printf("compared %d objects (%d canvases skipped), %d differences\n", nCompared, nSkipped, nDiff);
  return nDiff ? 1 : 0;
}
