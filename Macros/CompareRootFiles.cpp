// Object-by-object comparison of two ROOT files: the before/after check of a change that
// must not move any number. Written for the corrections refactoring (DESIGN_HISTORY.md,
// "The corrections block"), kept because every such change needs the same check.
//
// Exact equality on purpose, contents, errors and bin edges: a refactoring that changes a
// number in the last bit has changed something. Histograms (any dimension), THnSparse,
// TGraph, TTree (entries, branches, values), TObjString and the titles of plain TNamed are
// compared; canvases are skipped (they hold drawn copies of the rest), and so is every
// directory whose name starts with "Provenance" (timestamp, commit; ComparisonTask copies its
// inputs' into Provenance_numerator and Provenance_denominator).
//
// What it cannot compare it says, and counts as a failure: an object of another class, a
// subclass of TNamed other than those above (a TTree is a TNamed, and so is a RooFitResult:
// comparing their titles would pass any content), a TTree branch of a type it does not read.
// A comparison that skipped something in silence would be the "wrong numbers, no error" of
// CLAUDE.md in the tool meant to catch it.
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
#include "TLeaf.h"
#include "TObjString.h"
#include "TTree.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

static int nDiff = 0, nCompared = 0, nSkipped = 0, nNotCompared = 0;

static void report(const std::string& path, const std::string& what)
{
  if (nDiff < 100000)
    std::printf("DIFF %s: %s\n", path.c_str(), what.c_str());
  ++nDiff;
}

// Equal, with two NaN equal: a cell that cannot be computed is NaN on both sides of a refactoring that did not
// touch it (the purity of an empty pi_tof bin, for one), and NaN != NaN would report it as changed every time.
static bool same(double a, double b) { return a == b || (std::isnan(a) && std::isnan(b)); }

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
    if (!same(ca, cb) || !same(ea, eb)) {
      ++bad;
      if (!same(ca, cb))
        worst = std::max(worst, std::abs(ca - cb) / std::max(std::abs(ca), 1e-300));
      if (!same(ea, eb))
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
    if (j < 0 || !same(b->GetBinContent(j), ca) || !same(b->GetBinError(j), a->GetBinError(i)))
      ++bad;
  }
  if (bad)
    report(path, std::to_string(bad) + " filled bins differ");
}

static void notCompared(const std::string& path, const std::string& what)
{
  std::printf("NOT COMPARED %s: %s\n", path.c_str(), what.c_str());
  ++nNotCompared;
}

static bool isProvenance(const std::string& name) { return name.rfind("Provenance", 0) == 0; }

// One branch, entry by entry: the values a branch of type T holds, read into the same type on both sides.
template <typename T>
static void compareObjectBranch(const std::string& path, TTree* a, TTree* b, const char* name)
{
  T* va = nullptr;
  T* vb = nullptr;
  a->SetBranchAddress(name, &va);
  b->SetBranchAddress(name, &vb);
  for (Long64_t i = 0; i < a->GetEntries(); ++i) {
    a->GetBranch(name)->GetEntry(i);
    b->GetBranch(name)->GetEntry(i);
    if (*va != *vb) {
      report(path + "/" + name, "entry " + std::to_string(i) + " differs");
      break;
    }
  }
  a->ResetBranchAddresses();
  b->ResetBranchAddresses();
}

// The leaves of one entry of a branch of plain numbers: same number, same types, same values.
static bool sameLeaves(TObjArray* leavesA, TObjArray* leavesB)
{
  if (leavesA->GetEntries() != leavesB->GetEntries())
    return false;
  for (int l = 0; l < leavesA->GetEntries(); ++l) {
    auto* la = static_cast<TLeaf*>(leavesA->At(l));
    auto* lb = static_cast<TLeaf*>(leavesB->At(l));
    if (la->GetLen() != lb->GetLen() || std::string(la->GetTypeName()) != lb->GetTypeName())
      return false;
    for (int k = 0; k < la->GetLen(); ++k)
      if (!same(la->GetValue(k), lb->GetValue(k)))
        return false;
  }
  return true;
}

// Entries, the set of branches with their types, and every value. Branches of plain numbers are read through their
// leaves; strings and vectors of strings through their own type; any other type is said, not passed.
static void compareTree(const std::string& path, TTree* a, TTree* b)
{
  if (a->GetEntries() != b->GetEntries()) {
    report(path, "entries differ");
    return;
  }
  for (auto* obj : *a->GetListOfBranches()) {
    auto* ba = static_cast<TBranch*>(obj);
    const char* name = ba->GetName();
    TBranch* bb = b->GetBranch(name);
    if (!bb) {
      report(path + "/" + name, "branch missing in B");
      continue;
    }
    const std::string type = ba->GetClassName();
    if (type != bb->GetClassName()) {
      report(path + "/" + name, "branch type " + type + " vs " + bb->GetClassName());
      continue;
    }
    if (type == "string") {
      compareObjectBranch<std::string>(path, a, b, name);
    } else if (type == "vector<string>") {
      compareObjectBranch<std::vector<std::string>>(path, a, b, name);
    } else if (type.empty()) {
      auto* leavesA = ba->GetListOfLeaves();
      auto* leavesB = bb->GetListOfLeaves();
      for (Long64_t i = 0; i < a->GetEntries(); ++i) {
        ba->GetEntry(i);
        bb->GetEntry(i);
        if (!sameLeaves(leavesA, leavesB)) {
          report(path + "/" + name, "entry " + std::to_string(i) + " differs");
          break;
        }
      }
    } else {
      notCompared(path + "/" + name, "a branch of type " + type);
    }
  }
  for (auto* obj : *b->GetListOfBranches())
    if (!a->GetBranch(obj->GetName()))
      report(path + "/" + obj->GetName(), "branch missing in A");
}

static void compareGraph(const std::string& path, TGraph* a, TGraph* b)
{
  if (a->GetN() != b->GetN()) {
    report(path, "number of points differs");
    return;
  }
  for (int i = 0; i < a->GetN(); ++i)
    if (!same(a->GetX()[i], b->GetX()[i]) || !same(a->GetY()[i], b->GetY()[i]) || !same(a->GetErrorY(i), b->GetErrorY(i))) {
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
    if (isProvenance(name))
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
    else if (auto* ta = dynamic_cast<TTree*>(oa))
      compareTree(path, ta, static_cast<TTree*>(ob));
    else if (auto* osa = dynamic_cast<TObjString*>(oa)) {
      if (osa->GetString() != static_cast<TObjString*>(ob)->GetString())
        report(path, "string differs");
    } else if (dynamic_cast<TCanvas*>(oa)) {
      --nCompared;
      ++nSkipped; // drawn copies of what is compared above
    } else if (oa->IsA() == TNamed::Class()) {
      if (std::string(oa->GetTitle()) != ob->GetTitle())
        report(path, "title differs");
    } else {
      --nCompared;
      notCompared(path, std::string("an object of class ") + oa->ClassName());
    }
    delete oa;
    delete ob;
  }
  // keys only in B
  for (auto* obj : *db->GetListOfKeys()) {
    const std::string name = obj->GetName();
    if (!isProvenance(name) && !da->GetKey(name.c_str()))
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
  // "N NOT COMPARED" only when there are some, so that a check written for the old line still reads it.
  const std::string notComparedNote = nNotCompared ? ", " + std::to_string(nNotCompared) + " NOT COMPARED" : "";
  std::printf("compared %d objects (%d canvases skipped%s), %d differences\n", nCompared, nSkipped, notComparedNote.c_str(), nDiff);
  return nDiff || nNotCompared ? 1 : 0;
}
