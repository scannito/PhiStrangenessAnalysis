// =============================================================================
// CompareAxes.C  --  standalone axis diagnostic for the phi-strangeness inputs
//
// Answers two questions that no other tool in the framework answers directly:
//   1. what binning does each input file ACTUALLY contain?
//   2. do the files that must be combined (data vs MC, SE vs ME) agree?
//
// Depends on nothing but ROOT: it must be usable on files that the framework
// itself would refuse, and it must not share the assumptions it is checking.
//
//   root -l 'CompareAxes.C("scan",    "../JSONConfigs/globalConfigMCpp.json")'
//   root -l 'CompareAxes.C("compare", "../JSONConfigs/globalConfigMCpp.json")'
//   root -l 'CompareAxes.C("scan",    "dir:../DataFile/pp/DeltaY/MC")'
//   root -l 'CompareAxes.C("compare", "a.root,b.root")'
//
// The JSON form simply harvests every "*.root" string in the file, whatever the
// key names or the inheritance structure: no schema is assumed, so it keeps
// working when the configuration layout changes.
// =============================================================================

#include "TAxis.h"
#include "TClass.h"
#include "TFile.h"
#include "TH1.h"
#include "THnBase.h"
#include "TKey.h"
#include "TList.h"
#include "TSystemDirectory.h"
#include "TSystemFile.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

// Directory inside the O2 output that holds the task containers. Everything
// below it is discovered, nothing is assumed about names or axis order.
static const std::string kBasePath = "phi-strange-correlation/phiStrangenessCorrelation/";

// =============================================================================
// INPUT DISCOVERY
// =============================================================================

namespace
{

std::vector<std::string> Split(const std::string& s, char sep)
{
  std::vector<std::string> out;
  size_t start = 0;
  while (start <= s.size()) {
    const size_t pos = s.find(sep, start);
    const std::string token = s.substr(start, pos == std::string::npos ? std::string::npos : pos - start);
    if (!token.empty())
      out.push_back(token);
    if (pos == std::string::npos)
      break;
    start = pos + 1;
  }
  return out;
}

bool EndsWith(const std::string& s, const std::string& suffix)
{
  return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Harvests every quoted "....root" string from a text file. Deliberately not a
// JSON parser: no key name, no nesting and no 'inherits' resolution is assumed,
// so this keeps working with any configuration layout.
std::vector<std::string> RootFilesMentionedIn(const std::string& configPath)
{
  std::vector<std::string> found;
  std::set<std::string> seen;

  std::ifstream in(configPath);
  if (!in) {
    std::cerr << "[ERROR] cannot open configuration file: " << configPath << std::endl;
    return found;
  }

  std::string line;
  while (std::getline(in, line)) {
    size_t pos = 0;
    while ((pos = line.find('"', pos)) != std::string::npos) {
      const size_t end = line.find('"', pos + 1);
      if (end == std::string::npos)
        break;
      const std::string token = line.substr(pos + 1, end - pos - 1);
      if (EndsWith(token, ".root") && seen.insert(token).second)
        found.push_back(token);
      pos = end + 1;
    }
  }
  return found;
}

std::vector<std::string> RootFilesInDirectory(const std::string& dirPath)
{
  std::vector<std::string> found;

  TSystemDirectory dir(dirPath.c_str(), dirPath.c_str());
  std::unique_ptr<TList> files(dir.GetListOfFiles());
  if (!files) {
    std::cerr << "[ERROR] cannot list directory: " << dirPath << std::endl;
    return found;
  }

  TIter next(files.get());
  while (auto* entry = static_cast<TSystemFile*>(next())) {
    const std::string name = entry->GetName();
    if (!entry->IsDirectory() && EndsWith(name, ".root"))
      found.push_back(dirPath + "/" + name);
  }
  std::sort(found.begin(), found.end());
  return found;
}

// "cfg.json" | "dir:some/path" | "a.root,b.root"
std::vector<std::string> ResolveInputs(const std::string& source)
{
  if (source.empty()) {
    std::cerr << "[ERROR] no input given. Pass a JSON config, 'dir:<path>' or a comma-separated list." << std::endl;
    return {};
  }
  if (source.rfind("dir:", 0) == 0)
    return RootFilesInDirectory(source.substr(4));
  if (EndsWith(source, ".json"))
    return RootFilesMentionedIn(source);
  return Split(source, ',');
}

// =============================================================================
// AXIS READING
// =============================================================================

struct AxisInfo {
  std::string title;
  std::vector<double> edges;
};

// GetBinLowEdge/GetBinUpEdge on purpose: GetXbins() is empty for fixed-width
// axes, which would look like "no binning at all".
bool ReadAxis(TObject* obj, int axisIndex, AxisInfo& info)
{
  const TAxis* axis = nullptr;

  if (auto* hn = dynamic_cast<THnBase*>(obj)) {
    if (axisIndex < 0 || axisIndex >= hn->GetNdimensions())
      return false;
    axis = hn->GetAxis(axisIndex);
  } else if (auto* h = dynamic_cast<TH1*>(obj)) {
    if (axisIndex >= h->GetDimension())
      return false;
    axis = (axisIndex == 0) ? h->GetXaxis() : (axisIndex == 1) ? h->GetYaxis() : h->GetZaxis();
  } else {
    return false;
  }

  const int n = axis->GetNbins();
  info.title = axis->GetTitle();
  info.edges.clear();
  info.edges.reserve(n + 1);
  for (int i = 1; i <= n; ++i)
    info.edges.push_back(axis->GetBinLowEdge(i));
  info.edges.push_back(axis->GetBinUpEdge(n));
  return true;
}

int AxisCount(TObject* obj)
{
  if (auto* hn = dynamic_cast<THnBase*>(obj))
    return hn->GetNdimensions();
  if (auto* h = dynamic_cast<TH1*>(obj))
    return h->GetDimension();
  return 0;
}

std::string FormatEdges(const std::vector<double>& edges, size_t maxShown = 40)
{
  std::string s = "[";
  const size_t n = std::min(edges.size(), maxShown);
  for (size_t i = 0; i < n; ++i) {
    if (i)
      s += ", ";
    char buf[32];
    snprintf(buf, sizeof(buf), "%g", edges[i]);
    s += buf;
  }
  if (edges.size() > maxShown)
    s += ", ... (+" + std::to_string(edges.size() - maxShown) + ")";
  s += "]";
  return s;
}

bool IsUniform(const std::vector<double>& edges)
{
  if (edges.size() < 3)
    return true;
  const double w = edges[1] - edges[0];
  for (size_t i = 1; i + 1 < edges.size(); ++i)
    if (std::abs((edges[i + 1] - edges[i]) - w) > 1e-9)
      return false;
  return true;
}

// Empty string when the two binnings agree; otherwise every difference found,
// not just the first: a stale production usually differs in several places.
std::string CompareEdges(const std::vector<double>& a, const std::vector<double>& b, double epsilon = 1e-9)
{
  std::string report;

  if (a.size() != b.size()) {
    report += "        bin count: " + std::to_string(a.empty() ? 0 : a.size() - 1) + " vs " +
              std::to_string(b.empty() ? 0 : b.size() - 1) + "\n";
  }

  const size_t n = std::min(a.size(), b.size());
  for (size_t i = 0; i < n; ++i) {
    if (std::abs(a[i] - b[i]) > epsilon) {
      char buf[128];
      snprintf(buf, sizeof(buf), "        edge %zu: %g vs %g\n", i, a[i], b[i]);
      report += buf;
    }
  }
  return report;
}

// =============================================================================
// FILE INVENTORY
// =============================================================================

// object path (relative to kBasePath) -> axes
using FileInventory = std::map<std::string, std::vector<AxisInfo>>;

void WalkDirectory(TDirectory* dir, const std::string& prefix, FileInventory& inventory)
{
  TIter next(dir->GetListOfKeys());
  while (auto* key = static_cast<TKey*>(next())) {
    TClass* cl = TClass::GetClass(key->GetClassName());
    if (!cl)
      continue;

    const std::string name = prefix + key->GetName();

    if (cl->InheritsFrom(TDirectory::Class())) {
      if (auto* sub = dir->GetDirectory(key->GetName()))
        WalkDirectory(sub, name + "/", inventory);
      continue;
    }

    if (!cl->InheritsFrom(TH1::Class()) && !cl->InheritsFrom(THnBase::Class()))
      continue;

    TObject* obj = key->ReadObj();
    if (!obj)
      continue;

    std::vector<AxisInfo> axes;
    const int n = AxisCount(obj);
    for (int a = 0; a < n; ++a) {
      AxisInfo info;
      if (ReadAxis(obj, a, info))
        axes.push_back(std::move(info));
    }
    inventory[name] = std::move(axes);

    delete obj;
  }
}

bool BuildInventory(const std::string& path, FileInventory& inventory)
{
  std::unique_ptr<TFile> f(TFile::Open(path.c_str(), "READ"));
  if (!f || f->IsZombie()) {
    std::cerr << "  [ERROR] cannot open " << path << std::endl;
    return false;
  }

  TDirectory* base = f->GetDirectory(kBasePath.c_str());
  if (!base) {
    std::cerr << "  [ERROR] base path not found in " << path << ": " << kBasePath << std::endl;
    return false;
  }

  WalkDirectory(base, "", inventory);
  return true;
}

std::string ShortLabel(const std::string& path)
{
  const size_t slash = path.find_last_of('/');
  return (slash == std::string::npos) ? path : path.substr(slash + 1);
}

} // namespace

// =============================================================================
// MODES
// =============================================================================

void Scan(const std::vector<std::string>& inputs)
{
  for (const auto& path : inputs) {
    std::cout << "\n==================================================================" << std::endl;
    std::cout << path << std::endl;
    std::cout << "==================================================================" << std::endl;

    FileInventory inventory;
    if (!BuildInventory(path, inventory))
      continue;

    for (const auto& [object, axes] : inventory) {
      std::cout << "  " << object << std::endl;
      for (size_t a = 0; a < axes.size(); ++a) {
        char head[192];
        snprintf(head, sizeof(head), "      axis %zu  %-30s %4zu bins  [%g, %g]",
                 a, axes[a].title.empty() ? "(no title)" : axes[a].title.c_str(),
                 axes[a].edges.size() - 1, axes[a].edges.front(), axes[a].edges.back());
        std::cout << head << std::endl;

        // Uniform axes are fully described by bins/min/max. The full edge list
        // is printed only for variable axes, where a mismatch can hide.
        if (!IsUniform(axes[a].edges))
          std::cout << "              " << FormatEdges(axes[a].edges) << std::endl;
      }
    }
    std::cout << "  (" << inventory.size() << " containers)" << std::endl;
  }
}

void CompareGroups(const std::vector<std::string>& inputs)
{
  if (inputs.size() < 2) {
    std::cerr << "[ERROR] comparison needs at least two files" << std::endl;
    return;
  }

  std::vector<std::string> labels;
  std::vector<FileInventory> inventories;

  for (const auto& path : inputs) {
    FileInventory inventory;
    if (!BuildInventory(path, inventory))
      continue;
    labels.push_back(ShortLabel(path));
    inventories.push_back(std::move(inventory));
  }

  if (inventories.size() < 2) {
    std::cerr << "[ERROR] fewer than two files could be read" << std::endl;
    return;
  }

  // Every container present in more than one file is compared axis by axis,
  // against the first file that contains it. Matching by object path means no
  // guessing about which axis is which: same name, same expected binning.
  std::set<std::string> objects;
  for (const auto& inv : inventories)
    for (const auto& [object, axes] : inv)
      objects.insert(object);

  int nCompared = 0;
  int nMismatched = 0;
  std::set<std::string> onlyInSome;

  for (const auto& object : objects) {
    std::vector<size_t> present;
    for (size_t i = 0; i < inventories.size(); ++i)
      if (inventories[i].count(object))
        present.push_back(i);

    if (present.size() < inventories.size())
      onlyInSome.insert(object);
    if (present.size() < 2)
      continue;

    const auto& reference = inventories[present[0]].at(object);
    std::string report;

    for (size_t k = 1; k < present.size(); ++k) {
      const auto& other = inventories[present[k]].at(object);

      if (other.size() != reference.size()) {
        report += "      " + labels[present[k]] + ": different number of axes (" +
                  std::to_string(other.size()) + " vs " + std::to_string(reference.size()) + ")\n";
        continue;
      }

      for (size_t a = 0; a < reference.size(); ++a) {
        const std::string diff = CompareEdges(reference[a].edges, other[a].edges);
        if (!diff.empty()) {
          report += "      axis " + std::to_string(a) + "  (" +
                    (reference[a].title.empty() ? "no title" : reference[a].title) + ")   " +
                    labels[present[0]] + " vs " + labels[present[k]] + "\n" + diff;
        }
      }
    }

    ++nCompared;
    if (!report.empty()) {
      ++nMismatched;
      std::cout << "\n[MISMATCH] " << object << std::endl;
      std::cout << report;
    }
  }

  std::cout << "\n==================================================================" << std::endl;
  std::cout << "Containers compared across files: " << nCompared << std::endl;
  std::cout << "With mismatching axes:            " << nMismatched << std::endl;

  if (!onlyInSome.empty()) {
    std::cout << "\nPresent in some files only (" << onlyInSome.size() << ", not compared):" << std::endl;
    int shown = 0;
    for (const auto& object : onlyInSome) {
      if (shown++ == 10) {
        std::cout << "  ..." << std::endl;
        break;
      }
      std::cout << "  " << object << std::endl;
    }
  }

  if (nMismatched > 0) {
    std::cout << "\nA rebin can only merge bins, so it rescues a mismatch only when one"
              << "\nbinning is a strict subset of the other. An edge falling inside a source"
              << "\nbin (6.5 inside [6.4, 7.0]) cannot be reconciled offline: one of the two"
              << "\nproductions has to be re-run." << std::endl;
  }
  std::cout << "==================================================================" << std::endl;
}

// =============================================================================
// ENTRY POINT
// =============================================================================

void CompareAxes(const char* mode = "scan", const char* source = "")
{
  const std::vector<std::string> inputs = ResolveInputs(source);
  if (inputs.empty())
    return;

  std::cout << "Inputs (" << inputs.size() << "):" << std::endl;
  for (const auto& p : inputs)
    std::cout << "  " << p << std::endl;

  const std::string m = mode;
  if (m == "scan") {
    Scan(inputs);
  } else if (m == "compare") {
    CompareGroups(inputs);
  } else {
    std::cerr << "Usage: CompareAxes(\"scan\"|\"compare\", \"<config.json> | dir:<path> | a.root,b.root\")" << std::endl;
  }
}
