#pragma once

#include "BinningUtils.h"
#include "HistogramUtils.h"
#include "Logger.h"
#include "OutputNames.h"
#include "RootIOHelpers.h"

#include "RunEnvironment.h"

#include "TCanvas.h"
#include "TFile.h"
#include "TH1.h"

#include <cmath>
#include <filesystem>
#include <format>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// The series a plot is made of, found by what they are and never by a name built by hand: the deterministic half of
// the plots (DESIGN_NOTES section 1, "Plots: deterministic components, composed"). What a series is, where it is read,
// what may be compared with what and how two series are combined are decided here; a composition only chooses series
// and how they look.
//
// A name is built forward, with the OutputNames builders and the multiplicity interval found in the file's own
// binning_mult stamp; a missing object stops the run with what was asked and what the file holds (NotInFiles), never a
// series skipped in silence. Two series are compared by the stamps of the directories they were read from
// (RootIO::CompareBinningStamps, the rule ComparisonTask uses) and by their X axes: dividing or summing needs them
// equal; drawing them together does not, and a disagreement is warned and listed.
namespace PlotSeries
{

// What was asked is not in the files: a species, a |dy| limit, an interval, a scheme, a section, an object. Its own type
// so that a front end can tell it from a malformed request (phistrangeplot exits with 3 on it).
struct NotInFiles : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// A multiplicity interval, by its edges as the binning_mult stamp has them.
struct Interval {
  double low{0.0};
  double high{0.0};
};

struct Series {
  std::shared_ptr<TH1> hist;   // detached; non-finite bins already set empty (NonFiniteBins counts them)
  std::shared_ptr<TFile> file; // kept open: the stamps are read from it when the series is compared
  std::string fileName;
  std::string path;            // of the object in the file, "" for a series computed here
  std::string label;           // the legend's text: the composition's, or AutoLabels'
  TDirectory* schemeDir{nullptr};  // whose mult and ptPhi stamps describe the series
  std::vector<TDirectory*> speciesDirs; // whose ptAssoc stamps do (one for a species, two for a species ratio)
  int nonFiniteBins{0};
  std::vector<std::string> notes; // what Describe reports besides the origin: operations, warnings
  // What the series is, by meaning (file, kind, species, dy, mult, binning, ...): what AutoLabels puts in the legend.
  std::map<std::string, std::string> facts;
};

namespace Detail
{
// One open file per name, shared by every series read from it.
inline std::shared_ptr<TFile> Open(const std::string& fileName)
{
  static std::map<std::string, std::weak_ptr<TFile>> open;
  if (auto file = open[fileName].lock())
    return file;
  std::shared_ptr<TFile> file(RootIO::OpenOrThrow(fileName, "READ", "PlotSeries").release());
  open[fileName] = file;
  return file;
}

// The only directory of 'base' that satisfies 'accept', when 'name' is empty; 'name' itself otherwise.
template <class Accept>
TDirectory* PickDir(TDirectory* base, const std::string& name, const std::string& what, const std::string& fileName, Accept accept)
{
  if (!name.empty()) {
    TDirectory* dir = base ? base->GetDirectory(name.c_str()) : nullptr;
    if (!dir)
      throw NotInFiles(std::format("[FATAL] PlotSeries: '{}' has no {} '{}'.", fileName, what, name));
    return dir;
  }
  std::vector<TDirectory*> found;
  if (base && base->GetListOfKeys())
    for (TObject* key : *base->GetListOfKeys())
      if (TDirectory* dir = base->GetDirectory(key->GetName()); dir && accept(dir))
        found.push_back(dir);
  if (found.size() != 1)
    throw NotInFiles(std::format("[FATAL] PlotSeries: '{}' holds {} {}s; name the one to read.", fileName, found.size(), what));
  return found.front();
}

inline TDirectory* Scheme(TFile* file, const std::string& scheme, const std::string& fileName)
{
  return PickDir(file, scheme, "binning scheme", fileName,
                 [](TDirectory* dir) { return dir->Get(RootIO::kStampMult.c_str()) != nullptr; });
}

inline TDirectory* Section(TDirectory* schemeDir, const std::string& section, const std::string& fileName)
{
  return PickDir(schemeDir, section, "section", fileName, [](TDirectory* dir) {
    const std::string name = dir->GetName();
    return name == OutputNames::kExtract1D || name == OutputNames::kExtract2D;
  });
}

// The index of 'interval' in the multiplicity binning stamped in 'schemeDir'.
inline int MultIndex(TDirectory* schemeDir, Interval interval, const std::string& fileName)
{
  const std::vector<double> edges = RootIO::ReadBinningStamp(schemeDir, RootIO::kStampMult, fileName);
  if (edges.empty())
    throw NotInFiles(std::format("[FATAL] PlotSeries: '{}' carries no '{}' stamp, so no multiplicity interval can be "
                                 "looked up in it: re-run the task that wrote it.",
                                 fileName, RootIO::kStampMult));
  for (int multBin : BinningUtils::BinIndices(edges))
    if (edges[multBin] == interval.low && edges[multBin + 1] == interval.high)
      return multBin;
  std::string available;
  for (int multBin : BinningUtils::BinIndices(edges))
    available += std::format("{}{}", available.empty() ? "" : ", ", BinningUtils::BinLabel(edges, multBin));
  throw NotInFiles(std::format("[FATAL] PlotSeries: {}-{} is not a multiplicity interval of '{}'. Available: {}.",
                               interval.low, interval.high, fileName, available));
}

// 'dy' among the |dy| limits the section was produced with, when they were recorded (OutputNames::kDeltaYLimits); a
// file written before they were is left to the missing object.
inline void RequireDeltaY(TDirectory* sectionDir, double dy, const std::string& fileName)
{
  const std::optional<std::vector<double>> limits = RootIO::ReadValueList(sectionDir, OutputNames::kDeltaYLimits, fileName);
  if (!limits || std::find(limits->begin(), limits->end(), dy) != limits->end())
    return;
  std::string available;
  for (double limit : *limits)
    available += std::format("{}{}", available.empty() ? "" : ", ", BinningUtils::FormatDeltaY(limit));
  throw NotInFiles(std::format("[FATAL] PlotSeries: |dy| < {} is not a limit of section '{}' of '{}'. Available: {}.",
                               BinningUtils::FormatDeltaY(dy), sectionDir->GetName(), fileName, available));
}

// The object as a detached copy, its non-finite bins emptied (content and error 0: not drawn with the default
// options) and counted, so that a failed fit never shows as a value of 0.
inline Series Read(const std::shared_ptr<TFile>& file, const std::string& fileName, const std::vector<std::string>& dirs,
                   const std::string& name, TDirectory* schemeDir, std::vector<TDirectory*> speciesDirs)
{
  const std::string path = RootIO::ObjectPath(dirs, name);
  auto* raw = dynamic_cast<TH1*>(file->Get(path.c_str()));
  if (!raw)
    throw NotInFiles(std::format("[FATAL] PlotSeries: no histogram '{}' in '{}'.", path, fileName));
  std::shared_ptr<TH1> hist(static_cast<TH1*>(raw->Clone()));
  hist->SetDirectory(0);
  delete raw;

  int nonFinite = 0;
  for (int bin = 0; bin <= hist->GetNcells(); ++bin)
    if (!std::isfinite(hist->GetBinContent(bin)) || !std::isfinite(hist->GetBinError(bin))) {
      hist->SetBinContent(bin, 0.);
      hist->SetBinError(bin, 0.);
      ++nonFinite;
    }
  if (nonFinite)
    Logger::Warn("PlotSeries: '{}' in '{}' has {} non-finite bin(s), left empty in the plot.", path, fileName, nonFinite);
  return Series{.hist = std::move(hist), .file = file, .fileName = fileName, .path = path, .schemeDir = schemeDir,
                .speciesDirs = std::move(speciesDirs), .nonFiniteBins = nonFinite};
}
} // namespace Detail

// Where in a PhiAssocSpectra file: the binning scheme and the section (Extract1D, Extract2D); empty means the only one.
struct Where {
  std::string scheme;
  std::string section;
};

// -----------------------------------------------------------------------------
// The series of a PhiAssocSpectra file
// -----------------------------------------------------------------------------
inline Series Spectrum(const std::string& fileName, const std::string& species, double dy, Interval mult, bool extended = false,
                       const Where& where = {})
{
  auto file = Detail::Open(fileName);
  TDirectory* schemeDir = Detail::Scheme(file.get(), where.scheme, fileName);
  TDirectory* sectionDir = Detail::Section(schemeDir, where.section, fileName);
  Detail::RequireDeltaY(sectionDir, dy, fileName);
  TDirectory* speciesDir = Detail::PickDir(sectionDir, species, "species", fileName, OutputNames::IsSpeciesDir);
  const std::vector<double> multBinning = RootIO::ReadBinningStamp(schemeDir, RootIO::kStampMult, fileName);
  const int multBin = Detail::MultIndex(schemeDir, mult, fileName);
  const std::string name = OutputNames::Spectrum(species, dy, multBinning, multBin);
  Series s = Detail::Read(file, fileName, {schemeDir->GetName(), sectionDir->GetName(), species, OutputNames::kSpectraDir},
                          extended ? OutputNames::SpectrumExtended(name) : name, schemeDir, {speciesDir});
  s.facts = {{"file", fileName}, {"kind", extended ? "spectrum, extended to pT = 0" : "spectrum"}, {"species", species},
             {"dy", std::format("|#Delta y| < {}", BinningUtils::FormatDeltaY(dy))}, {"mult", std::format("{}%", BinningUtils::BinLabel(multBinning, multBin))}};
  return s;
}

// The yield of a species against multiplicity: measured, or extrapolated to pT = 0.
inline Series Trend(const std::string& fileName, const std::string& species, double dy, bool extrapolated = false,
                    const Where& where = {})
{
  auto file = Detail::Open(fileName);
  TDirectory* schemeDir = Detail::Scheme(file.get(), where.scheme, fileName);
  TDirectory* sectionDir = Detail::Section(schemeDir, where.section, fileName);
  Detail::RequireDeltaY(sectionDir, dy, fileName);
  TDirectory* speciesDir = Detail::PickDir(sectionDir, species, "species", fileName, OutputNames::IsSpeciesDir);
  Series s = Detail::Read(file, fileName, {schemeDir->GetName(), sectionDir->GetName(), species, OutputNames::kTrendsDir},
                          extrapolated ? OutputNames::TrendExtrapolated(species, dy) : OutputNames::Trend(species, dy), schemeDir,
                          {speciesDir});
  s.facts = {{"file", fileName}, {"kind", extrapolated ? "yield, extrapolated" : "yield, measured"}, {"species", species},
             {"dy", std::format("|#Delta y| < {}", BinningUtils::FormatDeltaY(dy))}};
  return s;
}

inline Series TrendExtrapOverMeas(const std::string& fileName, const std::string& species, double dy, const Where& where = {})
{
  auto file = Detail::Open(fileName);
  TDirectory* schemeDir = Detail::Scheme(file.get(), where.scheme, fileName);
  TDirectory* sectionDir = Detail::Section(schemeDir, where.section, fileName);
  Detail::RequireDeltaY(sectionDir, dy, fileName);
  TDirectory* speciesDir = Detail::PickDir(sectionDir, species, "species", fileName, OutputNames::IsSpeciesDir);
  Series s = Detail::Read(file, fileName, {schemeDir->GetName(), sectionDir->GetName(), species, OutputNames::kTrendsDir},
                          OutputNames::TrendExtrapOverMeas(species, dy), schemeDir, {speciesDir});
  s.facts = {{"file", fileName}, {"kind", "extrapolated over measured yield"}, {"species", species}, {"dy", std::format("|#Delta y| < {}", BinningUtils::FormatDeltaY(dy))}};
  return s;
}

// The ratio of two species' yields, as the correlation task wrote it (yield_ratios).
inline Series SpeciesRatio(const std::string& fileName, const std::string& num, const std::string& den, double dy,
                           bool extrapolated = false, const Where& where = {})
{
  auto file = Detail::Open(fileName);
  TDirectory* schemeDir = Detail::Scheme(file.get(), where.scheme, fileName);
  TDirectory* sectionDir = Detail::Section(schemeDir, where.section, fileName);
  Detail::RequireDeltaY(sectionDir, dy, fileName);
  std::vector<TDirectory*> speciesDirs{Detail::PickDir(sectionDir, num, "species", fileName, OutputNames::IsSpeciesDir),
                                       Detail::PickDir(sectionDir, den, "species", fileName, OutputNames::IsSpeciesDir)};
  Series s = Detail::Read(file, fileName, {schemeDir->GetName(), sectionDir->GetName(), OutputNames::kRatiosDir},
                          extrapolated ? OutputNames::ParticleRatioExtrapolated(num, den, dy) : OutputNames::ParticleRatioMeasured(num, den, dy),
                          schemeDir, std::move(speciesDirs));
  s.facts = {{"file", fileName}, {"kind", extrapolated ? "species ratio, extrapolated" : "species ratio, measured"},
             {"species", std::format("{}/{}", num, den)}, {"dy", std::format("|#Delta y| < {}", BinningUtils::FormatDeltaY(dy))}};
  return s;
}

// -----------------------------------------------------------------------------
// The series of a Purity file and of a Corrections file
// -----------------------------------------------------------------------------
inline Series Purity(const std::string& fileName, const std::string& purityKey, Interval mult,
                     OutputNames::SpectrumBinning binning = OutputNames::SpectrumBinning::Analysis, const std::string& scheme = "")
{
  auto file = Detail::Open(fileName);
  TDirectory* schemeDir = Detail::Scheme(file.get(), scheme, fileName);
  const std::vector<double> multBinning = RootIO::ReadBinningStamp(schemeDir, RootIO::kStampMult, fileName);
  const int multBin = Detail::MultIndex(schemeDir, mult, fileName);
  Series s = Detail::Read(file, fileName, OutputNames::SummaryPath(schemeDir->GetName(), binning),
                          OutputNames::Purity(purityKey, OutputNames::MultTag(multBinning, multBin), binning), schemeDir, {});
  s.facts = {{"file", fileName}, {"kind", "purity"}, {"species", purityKey}, {"mult", std::format("{}%", BinningUtils::BinLabel(multBinning, multBin))},
             {"binning", binning == OutputNames::SpectrumBinning::Source ? "source binning" : "analysis binning"}};
  return s;
}

// A correction map of one class, or integrated over multiplicity (no interval).
inline Series CorrectionMap(const std::string& fileName, const std::string& particle, OutputNames::CorrectionMap map,
                            std::optional<Interval> mult,
                            OutputNames::SpectrumBinning binning = OutputNames::SpectrumBinning::Analysis, const std::string& scheme = "")
{
  auto file = Detail::Open(fileName);
  TDirectory* schemeDir = Detail::Scheme(file.get(), scheme, fileName);
  std::string multTag = OutputNames::kMultIntegrated;
  std::string multLabel = "multiplicity-integrated";
  if (mult) {
    const std::vector<double> multBinning = RootIO::ReadBinningStamp(schemeDir, RootIO::kStampMult, fileName);
    const int multBin = Detail::MultIndex(schemeDir, *mult, fileName);
    multTag = OutputNames::MultTag(multBinning, multBin);
    multLabel = std::format("{}%", BinningUtils::BinLabel(multBinning, multBin));
  }
  Series s = Detail::Read(file, fileName, OutputNames::CorrectionMapPath(schemeDir->GetName(), map, binning),
                          OutputNames::CorrectionMapName(particle, map, multTag, binning), schemeDir, {});
  constexpr std::string_view kKinds[] = {"efficiency", "signal loss", "signal loss over event loss"};
  s.facts = {{"file", fileName}, {"kind", std::string(kKinds[static_cast<int>(map)])}, {"species", particle}, {"mult", multLabel},
             {"binning", binning == OutputNames::SpectrumBinning::Source ? "source binning" : "analysis binning"}};
  return s;
}

// An object of a file the framework did not write (a preliminary, a reference): by its path, and with no stamp, so
// that only its X axis can be checked against another series.
inline Series Object(const std::string& fileName, const std::string& path)
{
  auto file = Detail::Open(fileName);
  const std::size_t slash = path.find_last_of('/');
  std::vector<std::string> dirs;
  if (slash != std::string::npos) {
    std::string head = path.substr(0, slash);
    for (std::size_t from = 0, to; from <= head.size(); from = to + 1) {
      to = head.find('/', from);
      if (to == std::string::npos)
        to = head.size();
      dirs.push_back(head.substr(from, to - from));
    }
  }
  Series s = Detail::Read(file, fileName, dirs, slash == std::string::npos ? path : path.substr(slash + 1), nullptr, {});
  s.facts = {{"file", fileName}, {"kind", path}};
  return s;
}

// -----------------------------------------------------------------------------
// Comparing and combining
// -----------------------------------------------------------------------------
// What a comparison of two series found, for Describe: the stamps that differ, and those that could not be checked.
struct Comparison {
  std::vector<std::string> differs;
  std::vector<std::string> notChecked;
};

// The stamps of the directories the two series were read from (mult and ptPhi of the scheme, ptAssoc of each species
// they share), by the rule of ComparisonTask, and their X axes. 'strict' (dividing, summing): a difference stops the
// run. Otherwise (drawing together): it is warned and returned.
inline Comparison Compare(const Series& a, const Series& b, bool strict)
{
  Comparison result;
  auto check = [&](TDirectory* da, TDirectory* db, const std::string& stamp, const std::string& where) {
    if (!da || !db) {
      result.notChecked.push_back(std::format("{} ({})", stamp, where));
      return;
    }
    const RootIO::StampComparison c = RootIO::CompareBinningStamps(da, db, stamp, std::format("'{}'", a.fileName), std::format("'{}'", b.fileName));
    if (c.outcome == RootIO::StampComparison::Outcome::NotChecked)
      result.notChecked.push_back(std::format("{} ({})", stamp, where));
    else if (c.outcome == RootIO::StampComparison::Outcome::Differs)
      result.differs.push_back(std::format("{} ({}):\n{}", stamp, where, c.detail));
  };
  check(a.schemeDir, b.schemeDir, RootIO::kStampMult, "scheme");
  check(a.schemeDir, b.schemeDir, RootIO::kStampPtPhi, "scheme");
  for (TDirectory* sa : a.speciesDirs)
    for (TDirectory* sb : b.speciesDirs)
      if (std::string(sa->GetName()) == sb->GetName())
        check(sa, sb, RootIO::kStampPtAssoc, sa->GetName());

  const std::string axes = BinningUtils::Compare(BinningUtils::AxisEdges(a.hist->GetXaxis()), BinningUtils::AxisEdges(b.hist->GetXaxis()),
                                                 a.fileName, b.fileName);
  if (!axes.empty())
    result.differs.push_back(std::format("X axis:\n{}", axes));

  for (const std::string& what : result.notChecked)
    Logger::Warn("PlotSeries: '{}' and '{}': {} not stamped in both, not checked.", a.path, b.path, what);
  if (!result.differs.empty()) {
    std::string all;
    for (const std::string& d : result.differs)
      all += d + "\n";
    if (strict)
      throw std::runtime_error(std::format("[FATAL] PlotSeries: '{}' of '{}' and '{}' of '{}' cannot be combined, their binnings "
                                           "differ:\n{}",
                                           a.path, a.fileName, b.path, b.fileName, all));
    Logger::Warn("PlotSeries: '{}' of '{}' and '{}' of '{}' are drawn together with different binnings:\n{}", a.path,
                 a.fileName, b.path, b.fileName, all);
  }
  return result;
}

// num / den, with the option of TH1::Divide stated by the caller, never guessed: "" for independent samples (two
// productions, two years), "B" when each numerator is a subset of the denominator (a closure).
inline Series Ratio(const Series& num, const Series& den, const std::string& divideOption, const std::string& name = "")
{
  Compare(num, den, true);
  Series r = num;
  r.hist = std::shared_ptr<TH1>(HistogramUtils::MakeRatioHist(num.hist.get(), den.hist.get(), name.empty() ? std::format("{}_over_{}", num.hist->GetName(), den.hist->GetName()) : name,
                                                               num.hist->GetTitle(), 1.0, 1.0, divideOption.c_str()).release());
  r.path.clear();
  r.notes.push_back(std::format("ratio of '{}' ({}) to '{}' ({}), Divide option \"{}\"", num.path, num.fileName, den.path,
                                den.fileName, divideOption));
  return r;
}

inline Series Sum(const Series& a, const Series& b, const std::string& name = "")
{
  Compare(a, b, true);
  Series s = a;
  s.hist = std::shared_ptr<TH1>(static_cast<TH1*>(a.hist->Clone(name.empty() ? std::format("{}_plus_{}", a.hist->GetName(), b.hist->GetName()).c_str() : name.c_str())));
  s.hist->SetDirectory(0);
  s.hist->Add(b.hist.get());
  s.path.clear();
  s.notes.push_back(std::format("sum of '{}' ({}) and '{}' ({})", a.path, a.fileName, b.path, b.fileName));
  return s;
}

inline Series Scale(const Series& a, double factor)
{
  Series s = a;
  s.hist = std::shared_ptr<TH1>(static_cast<TH1*>(a.hist->Clone(std::format("{}_x{}", a.hist->GetName(), factor).c_str())));
  s.hist->SetDirectory(0);
  s.hist->Scale(factor);
  s.notes.push_back(std::format("scaled by {}", factor));
  return s;
}

// What was resolved, said before drawing: each series, where it comes from and what was done to it, and what the
// series drawn together disagree on or could not be checked for.
inline std::string Describe(const std::vector<Series>& series)
{
  std::string out;
  for (std::size_t i = 0; i < series.size(); ++i) {
    const Series& s = series[i];
    out += std::format("series {}: '{}'\n  {} in {}\n", i, s.label, s.path.empty() ? "(computed)" : s.path, s.fileName);
    for (const std::string& note : s.notes)
      out += std::format("  {}\n", note);
    if (s.nonFiniteBins)
      out += std::format("  {} non-finite bin(s) left empty\n", s.nonFiniteBins);
  }
  for (std::size_t i = 1; i < series.size(); ++i) {
    const Comparison c = Compare(series.front(), series[i], false);
    for (const std::string& d : c.differs)
      out += std::format("DIFFERS, series 0 and {}: {}\n", i, d);
    for (const std::string& n : c.notChecked)
      out += std::format("NOT checked, series 0 and {}: {}\n", i, n);
  }
  return out;
}

// -----------------------------------------------------------------------------
// The legend and the output
// -----------------------------------------------------------------------------
struct LegendText {
  std::string header;              // what every series shares
  std::vector<std::string> labels; // per series, what tells it apart (or the label the composition gave it)
};

// The facts that differ between the series label them; the ones they share go in the header. File names are cut to
// the part that tells them apart: the words (between '/', '_' and '.') they all start or end with are removed.
inline LegendText AutoLabels(const std::vector<Series>& series)
{
  auto words = [](const std::string& name) {
    std::vector<std::string> out{""};
    for (char ch : name) {
      if (ch == '/' || ch == '_' || ch == '.')
        out.emplace_back();
      else
        out.back() += ch;
    }
    return out;
  };
  std::vector<std::vector<std::string>> fileWords;
  for (const Series& s : series)
    fileWords.push_back(words(s.facts.count("file") ? s.facts.at("file") : s.fileName));
  std::size_t prefix = 0, suffix = 0;
  if (fileWords.size() > 1) {
    const std::size_t shortest = std::min_element(fileWords.begin(), fileWords.end(), [](auto& a, auto& b) { return a.size() < b.size(); })->size();
    auto sameAt = [&](auto at) { return std::all_of(fileWords.begin(), fileWords.end(), [&](auto& w) { return at(w) == at(fileWords[0]); }); };
    while (prefix < shortest && sameAt([&](auto& w) { return w[prefix]; }))
      ++prefix;
    while (suffix < shortest - prefix && sameAt([&](auto& w) { return w[w.size() - 1 - suffix]; }))
      ++suffix;
  }
  std::vector<std::string> files;
  for (const auto& w : fileWords) {
    std::string kept;
    for (std::size_t i = prefix; i + suffix < w.size(); ++i)
      kept += std::format("{}{}", kept.empty() ? "" : "_", w[i]);
    files.push_back(kept);
  }
  std::map<std::string, std::set<std::string>> values;
  for (const Series& s : series)
    for (const auto& [key, value] : s.facts)
      values[key].insert(value);

  // The order a reader expects: what it is, of what, where, then the file.
  std::vector<std::string> order{"kind", "species", "dy", "mult", "binning"};
  for (const auto& [key, set] : values)
    if (std::find(order.begin(), order.end(), key) == order.end() && key != "file")
      order.push_back(key);
  order.push_back("file");

  LegendText text;
  for (const std::string& key : order)
    if (values.count(key) && values[key].size() == 1 && series.size() > 1 && key != "file")
      text.header += std::format("{}{}", text.header.empty() ? "" : ", ", *values[key].begin());
  for (std::size_t i = 0; i < series.size(); ++i) {
    if (!series[i].label.empty()) {
      text.labels.push_back(series[i].label);
      continue;
    }
    std::string label;
    for (const std::string& key : order) {
      const auto fact = series[i].facts.find(key);
      if (fact == series[i].facts.end() || (values[key].size() < 2 && series.size() > 1))
        continue;
      label += std::format("{}{}", label.empty() ? "" : ", ", key == "file" ? files[i] : fact->second);
    }
    text.labels.push_back(label);
  }
  return text;
}

// The plot kept: 'outBase' with each extension of 'formats', and always 'outBase'.root with the canvas, a copy of each
// series as it was drawn (Objects/series<i>), and the provenance: the recipe (the composition or the specification),
// when, the code's commit and fingerprint, each input's own provenance. A .root that is not a plot written here is
// never overwritten: an input above all.
inline void Save(TCanvas* canvas, const std::string& outBase, const std::vector<Series>& series, const std::string& recipe,
                 const std::vector<std::string>& formats = {"pdf", "png"})
{
  const std::string rootName = outBase + ".root";
  if (std::filesystem::exists(rootName)) {
    std::unique_ptr<TFile> existing(TFile::Open(rootName.c_str(), "READ"));
    if (!existing || !RootIO::FindPath(existing.get(), {"Plot", RootIO::Detail::kProvenanceDir}))
      throw std::runtime_error(std::format("[FATAL] PlotSeries::Save: '{}' exists and is not a plot written here; it is "
                                           "not overwritten. Choose another output.",
                                           rootName));
  }
  if (const std::filesystem::path dir = std::filesystem::path(outBase).parent_path(); !dir.empty())
    RootIO::CreateDirectoriesOrThrow(dir, "PlotSeries::Save");
  for (const std::string& format : formats)
    canvas->SaveAs(std::format("{}.{}", outBase, format).c_str());

  std::unique_ptr<TFile> out = RootIO::OpenOrThrow(rootName, "RECREATE", "PlotSeries::Save");
  out->WriteTObject(canvas, canvas->GetName(), "Overwrite");
  for (std::size_t i = 0; i < series.size(); ++i) {
    TDirectory* dir = RootIO::MakePath(out.get(), {"Objects", std::format("series{}", i)});
    dir->WriteTObject(series[i].hist.get(), series[i].hist->GetName(), "Overwrite");
  }
  std::map<std::string, std::string> facts = RunEnvironment::Facts();
  facts["produced_at"] = RunEnvironment::TimestampNow();
  facts["recipe"] = recipe;
  RootIO::WriteProvenance(out.get(), "Plot", facts);
  for (std::size_t i = 0; i < series.size(); ++i) {
    std::map<std::string, std::string> input{{"file", series[i].fileName}, {"path", series[i].path}};
    if (series[i].schemeDir)
      for (const auto& [key, value] : RootIO::ReadProvenance(series[i].file.get(), series[i].schemeDir->GetName()))
        if (key == "produced_at" || key == "git_commit" || key == "sources_fingerprint")
          input[key] = value;
    RootIO::WriteProvenance(out.get(), "Plot", input, std::format("series{}", i));
  }
  out->Close();
}

// -----------------------------------------------------------------------------
// What a file holds
// -----------------------------------------------------------------------------
struct Catalog {
  struct Section {
    std::string name;
    std::vector<std::string> species;
    std::optional<std::vector<double>> deltaYLimits; // none: written before they were recorded
  };
  struct Scheme {
    std::string name;
    std::vector<double> multBinning, ptPhiBinning;
    std::vector<Section> sections;
    std::map<std::string, std::string> provenance; // produced_at, git_commit, ...
  };
  std::string fileName;
  std::vector<Scheme> schemes;
};

// Read forward from the directories and the stamps, never from object names.
inline Catalog ReadCatalog(const std::string& fileName)
{
  auto file = Detail::Open(fileName);
  Catalog catalog{.fileName = fileName};
  for (TObject* key : *file->GetListOfKeys()) {
    TDirectory* schemeDir = file->GetDirectory(key->GetName());
    if (!schemeDir || !schemeDir->Get(RootIO::kStampMult.c_str()))
      continue;
    Catalog::Scheme scheme{.name = schemeDir->GetName(),
                           .multBinning = RootIO::ReadBinningStamp(schemeDir, RootIO::kStampMult, fileName),
                           .ptPhiBinning = RootIO::ReadBinningStamp(schemeDir, RootIO::kStampPtPhi, fileName),
                           .provenance = RootIO::ReadProvenance(file.get(), schemeDir->GetName())};
    for (const std::string& sectionName : {OutputNames::kExtract1D, OutputNames::kExtract2D}) {
      TDirectory* sectionDir = schemeDir->GetDirectory(sectionName.c_str());
      if (!sectionDir)
        continue;
      Catalog::Section section{.name = sectionName, .deltaYLimits = RootIO::ReadValueList(sectionDir, OutputNames::kDeltaYLimits, fileName)};
      for (TObject* k : *sectionDir->GetListOfKeys())
        if (OutputNames::IsSpeciesDir(sectionDir->GetDirectory(k->GetName())))
          section.species.emplace_back(k->GetName());
      scheme.sections.push_back(std::move(section));
    }
    catalog.schemes.push_back(std::move(scheme));
  }
  return catalog;
}

} // namespace PlotSeries
