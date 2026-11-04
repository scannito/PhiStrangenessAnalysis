#pragma once

#include "BinningUtils.h"
#include "CanvasManager.h"
#include "HistogramUtils.h"
#include "Logger.h"
#include "OutputNames.h"
#include "RootIOHelpers.h"

#include "TCanvas.h"
#include "TFile.h"
#include "TF1.h"
#include "TH1.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <format>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// The series a plot is made of, found by what they are and never by a name built by hand: the deterministic half of
// the plots (DESIGN_NOTES section 1, "Plots: deterministic components, composed"). What a series is, where it is read,
// what may be compared with what, how two series are combined and how they are put on a pad are decided here; a
// composition only chooses series and how they look.
//
// A name is built forward, with the OutputNames builders and the multiplicity interval found in the file's own
// binning_mult stamp; a missing object stops the run with what was asked and what the file holds (NotInFiles), never a
// series skipped in silence. Two series are compared by the stamps of the directories they were read from
// (RootIO::CompareBinningStamps, the rule ComparisonTask uses) and by their X axes: dividing or summing needs them equal
// (RequireCombinable); drawing them together does not, and Draw says what differs. The data are kept as read, a
// non-finite bin included, so that it propagates through a sum or a ratio; only the copy that is drawn leaves it empty.
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

// What a series is, in words, for the legend (AutoLabels). The file is the origins'.
struct Facts {
  std::optional<std::string> kind, species, dy, mult, binning;
};

// Where a series was read: what its provenance records and what its stamps are compared on.
struct Origin {
  std::shared_ptr<TFile> file; // kept open: the stamps and the provenance are read from it
  std::string fileName;
  std::string path;                     // of the object in the file
  TDirectory* schemeDir{nullptr};       // whose mult and ptPhi stamps describe it; null for a foreign file
  std::vector<TDirectory*> speciesDirs; // whose ptAssoc stamps do (one for a species, two for a species ratio)
};

// How a series is drawn, when the composition says so; otherwise Draw takes the palette by position. Presentation of
// the composition's, like the label, never written into the data.
struct Look {
  std::optional<int> color, marker;
  std::optional<double> markerSize;
};

struct Series {
  std::shared_ptr<TH1> hist;   // detached, as read: a non-finite bin stays non-finite
  std::vector<Origin> origins; // one when read, both operands' after Ratio or Sum
  std::string label;           // the legend's text when the composition gives one; AutoLabels' otherwise
  int nonFiniteBins{0};
  std::vector<std::string> notes; // what Describe reports besides the origins: the operations
  Facts facts;
  Look look;
};

// Which yield of a species against multiplicity.
enum class TrendKind { Measured,
                       Extrapolated,
                       ExtrapOverMeas };

// Where in a PhiAssocSpectra file: the binning scheme and the section (Extract1D, Extract2D); empty means the only one.
struct Where {
  std::string scheme;
  std::string section;
};

namespace Detail
{
inline std::map<std::string, std::weak_ptr<TFile>>& OpenFiles()
{
  static std::map<std::string, std::weak_ptr<TFile>> open;
  return open;
}

// One open file per name, shared by every series read from it.
inline std::shared_ptr<TFile> Open(const std::string& fileName)
{
  if (auto file = OpenFiles()[fileName].lock())
    return file;
  std::shared_ptr<TFile> file(RootIO::OpenOrThrow(fileName, "READ", "PlotSeries").release());
  OpenFiles()[fileName] = file;
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
  return PickDir(file, scheme, "binning scheme", fileName, RootIO::IsSchemeDir);
}

inline TDirectory* Species(TDirectory* sectionDir, const std::string& species, const std::string& fileName)
{
  return PickDir(sectionDir, species, "species", fileName, OutputNames::IsSpeciesDir);
}

// The multiplicity binning of a scheme, from its stamp: no interval can be looked up in a file without it.
inline std::vector<double> MultBinning(TDirectory* schemeDir, const std::string& fileName)
{
  std::vector<double> edges = RootIO::ReadBinningStamp(schemeDir, RootIO::kStampMult, fileName);
  if (edges.empty())
    throw NotInFiles(std::format("[FATAL] PlotSeries: '{}' carries no '{}' stamp, so no multiplicity interval can be "
                                 "looked up in it: re-run the task that wrote it.",
                                 fileName, RootIO::kStampMult));
  return edges;
}

// The index of 'interval' in 'edges', by the framework's one test of "is an edge" (BinningUtils::FindEdgeIndex).
inline int MultIndex(std::span<const double> edges, Interval interval, const std::string& fileName)
{
  const std::optional<int> low = BinningUtils::FindEdgeIndex(edges, interval.low);
  if (low && BinningUtils::FindEdgeIndex(edges, interval.high) == *low + 1)
    return *low;
  throw NotInFiles(std::format("[FATAL] PlotSeries: {}-{} is not a multiplicity interval of '{}'. Available: {}.",
                               interval.low, interval.high, fileName,
                               Logger::Join(BinningUtils::BinIndices(edges) | std::views::transform([&](int multBin) {
                                              return BinningUtils::BinLabel(edges, multBin);
                                            }))));
}

// A PhiAssocSpectra file opened at a scheme and a section that hold the |dy| limit 'dy', when the limits were recorded
// (OutputNames::kDeltaYLimits); a file written before they were is left to the missing object.
struct Located {
  std::shared_ptr<TFile> file;
  TDirectory* schemeDir{nullptr};
  TDirectory* sectionDir{nullptr};
};
inline Located LocateSection(const std::string& fileName, const Where& where)
{
  Located at{.file = Open(fileName)};
  at.schemeDir = Scheme(at.file.get(), where.scheme, fileName);
  at.sectionDir = PickDir(at.schemeDir, where.section, "section", fileName, OutputNames::IsSectionDir);
  return at;
}
inline Located Locate(const std::string& fileName, const Where& where, double dy)
{
  const Located at = LocateSection(fileName, where);
  const std::optional<std::vector<double>> limits = RootIO::ReadValueList(at.sectionDir, OutputNames::kDeltaYLimits, fileName);
  if (limits && std::ranges::none_of(*limits, [&](double limit) { return std::abs(limit - dy) <= BinningUtils::kEdgeTolerance; }))
    throw NotInFiles(std::format("[FATAL] PlotSeries: |dy| < {} is not a limit of section '{}' of '{}'. Available: {}.",
                                 BinningUtils::FormatDeltaY(dy), at.sectionDir->GetName(), fileName,
                                 Logger::Join(*limits | std::views::transform(BinningUtils::FormatDeltaY))));
  return at;
}

// The histogram at 'path', detached and as it is; its non-finite visible bins counted and warned about. Absent is
// NotInFiles; present but not a histogram (a TGraph) is said as such.
// The object under 'path', of class T. Absent is NotInFiles (the request names what the file does not have); another
// class is an error of the file, said apart.
template <class T>
inline std::unique_ptr<T> Get(TFile* file, const std::string& fileName, const std::string& path)
{
  std::unique_ptr<TObject> obj(file->Get(path.c_str()));
  if (!obj)
    throw NotInFiles(std::format("[FATAL] PlotSeries: no object '{}' in '{}'.", path, fileName));
  auto* typed = dynamic_cast<T*>(obj.get());
  if (!typed)
    throw std::runtime_error(std::format("[FATAL] PlotSeries: '{}' in '{}' is a {}, not a {}.", path, fileName, obj->ClassName(), T::Class_Name()));
  obj.release();
  return std::unique_ptr<T>(typed);
}

inline Series Read(const std::shared_ptr<TFile>& file, const std::string& fileName, const std::string& path,
                   TDirectory* schemeDir, std::vector<TDirectory*> speciesDirs)
{
  std::unique_ptr<TH1> raw = Get<TH1>(file.get(), fileName, path);
  raw->SetDirectory(0);
  std::shared_ptr<TH1> hist(raw.release());

  int nonFinite = 0;
  for (int bin = 1; bin <= hist->GetNbinsX(); ++bin)
    if (!std::isfinite(hist->GetBinContent(bin)) || !std::isfinite(hist->GetBinError(bin)))
      ++nonFinite;
  if (nonFinite)
    Logger::Warn("PlotSeries: '{}' in '{}' has {} non-finite bin(s): left empty where drawn, carried through any sum or ratio.",
                 path, fileName, nonFinite);
  return Series{.hist = std::move(hist),
                .origins = {Origin{.file = file, .fileName = fileName, .path = path, .schemeDir = schemeDir, .speciesDirs = std::move(speciesDirs)}},
                .nonFiniteBins = nonFinite};
}

inline std::string DyFact(double dy) { return std::format("|#Delta y| < {}", BinningUtils::FormatDeltaY(dy)); }
inline std::string MultFact(std::span<const double> edges, int multBin) { return std::format("{}%", BinningUtils::BinLabel(edges, multBin)); }
inline std::string BinningFact(OutputNames::SpectrumBinning binning)
{
  return binning == OutputNames::SpectrumBinning::Source ? "source binning" : "analysis binning";
}

inline Series SpectrumAt(const std::string& fileName, const std::string& species, double dy, Interval mult, bool extended,
                         const Where& where)
{
  const Located at = Locate(fileName, where, dy);
  TDirectory* speciesDir = Species(at.sectionDir, species, fileName);
  const std::vector<double> edges = MultBinning(at.schemeDir, fileName);
  const int multBin = MultIndex(edges, mult, fileName);
  const std::string name = OutputNames::Spectrum(species, dy, edges, multBin);
  Series s = Read(at.file, fileName,
                  RootIO::ObjectPath(OutputNames::SpectraPath(at.schemeDir->GetName(), at.sectionDir->GetName(), species),
                                     extended ? OutputNames::SpectrumExtended(name) : name),
                  at.schemeDir, {speciesDir});
  s.facts = {.kind = extended ? "spectrum, extended to pT = 0" : "spectrum", .species = species, .dy = DyFact(dy),
             .mult = MultFact(edges, multBin)};
  return s;
}
} // namespace Detail

// -----------------------------------------------------------------------------
// The series of a PhiAssocSpectra file
// -----------------------------------------------------------------------------
inline Series Spectrum(const std::string& fileName, const std::string& species, double dy, Interval mult, const Where& where = {})
{
  return Detail::SpectrumAt(fileName, species, dy, mult, false, where);
}

// The same spectrum with the extrapolated bin down to pT = 0 in front, its fitted curve attached.
inline Series SpectrumExtended(const std::string& fileName, const std::string& species, double dy, Interval mult, const Where& where = {})
{
  return Detail::SpectrumAt(fileName, species, dy, mult, true, where);
}

// The yield of a species against multiplicity: measured, extrapolated to pT = 0, or the one over the other.
inline Series Trend(const std::string& fileName, const std::string& species, double dy, TrendKind kind = TrendKind::Measured,
                    const Where& where = {})
{
  const Detail::Located at = Detail::Locate(fileName, where, dy);
  TDirectory* speciesDir = Detail::Species(at.sectionDir, species, fileName);
  const std::string name = kind == TrendKind::Measured       ? OutputNames::Trend(species, dy)
                           : kind == TrendKind::Extrapolated ? OutputNames::TrendExtrapolated(species, dy)
                                                             : OutputNames::TrendExtrapOverMeas(species, dy);
  Series s = Detail::Read(at.file, fileName,
                          RootIO::ObjectPath(OutputNames::TrendsPath(at.schemeDir->GetName(), at.sectionDir->GetName(), species), name),
                          at.schemeDir, {speciesDir});
  s.facts = {.kind = kind == TrendKind::Measured       ? "yield, measured"
                     : kind == TrendKind::Extrapolated ? "yield, extrapolated"
                                                       : "extrapolated over measured yield",
             .species = species,
             .dy = Detail::DyFact(dy)};
  return s;
}

// The ratio of two species' yields, measured or extrapolated, as the correlation task wrote it (yield_ratios).
inline Series SpeciesRatio(const std::string& fileName, const std::string& num, const std::string& den, double dy,
                           TrendKind kind = TrendKind::Measured, const Where& where = {})
{
  if (kind == TrendKind::ExtrapOverMeas)
    throw std::runtime_error("[FATAL] PlotSeries::SpeciesRatio: a species ratio is measured or extrapolated.");
  const Detail::Located at = Detail::Locate(fileName, where, dy);
  const bool extrapolated = kind == TrendKind::Extrapolated;
  Series s = Detail::Read(at.file, fileName,
                          RootIO::ObjectPath(OutputNames::RatiosPath(at.schemeDir->GetName(), at.sectionDir->GetName()),
                                             extrapolated ? OutputNames::ParticleRatioExtrapolated(num, den, dy)
                                                          : OutputNames::ParticleRatioMeasured(num, den, dy)),
                          at.schemeDir, {Detail::Species(at.sectionDir, num, fileName), Detail::Species(at.sectionDir, den, fileName)});
  s.facts = {.kind = extrapolated ? "species ratio, extrapolated" : "species ratio, measured", .species = std::format("{}/{}", num, den),
             .dy = Detail::DyFact(dy)};
  return s;
}

// -----------------------------------------------------------------------------
// The series of a Purity file and of a Corrections file (a scheme, no section)
// -----------------------------------------------------------------------------
inline Series Purity(const std::string& fileName, const std::string& purityKey, Interval mult,
                     OutputNames::SpectrumBinning binning = OutputNames::SpectrumBinning::Analysis, const std::string& scheme = "")
{
  auto file = Detail::Open(fileName);
  TDirectory* schemeDir = Detail::Scheme(file.get(), scheme, fileName);
  const std::vector<double> edges = Detail::MultBinning(schemeDir, fileName);
  const int multBin = Detail::MultIndex(edges, mult, fileName);
  Series s = Detail::Read(file, fileName,
                          RootIO::ObjectPath(OutputNames::SummaryPath(schemeDir->GetName(), binning),
                                             OutputNames::Purity(purityKey, OutputNames::MultTag(edges, multBin), binning)),
                          schemeDir, {});
  s.facts = {.kind = "purity", .species = purityKey, .mult = Detail::MultFact(edges, multBin), .binning = Detail::BinningFact(binning)};
  return s;
}

// The |dy| limits of a section of a PhiAssocSpectra file, as the correlation task recorded them. A file written before
// they were recorded has none to give: re-run the task.
inline std::vector<double> DeltaYLimits(const std::string& fileName, const Where& where = {})
{
  const Detail::Located at = Detail::LocateSection(fileName, where);
  std::optional<std::vector<double>> limits = RootIO::ReadValueList(at.sectionDir, OutputNames::kDeltaYLimits, fileName);
  if (!limits)
    throw NotInFiles(std::format("[FATAL] PlotSeries: section '{}' of '{}' does not record its |dy| limits ('{}'): it was written "
                                 "before they were; re-run the correlation task.",
                                 at.sectionDir->GetName(), fileName, OutputNames::kDeltaYLimits));
  return *limits;
}

// The scheme and the section a request names, with the empty ones resolved to the only one the file has.
inline Where Resolve(const std::string& fileName, const Where& where = {})
{
  const Detail::Located at = Detail::LocateSection(fileName, where);
  return {at.schemeDir->GetName(), at.sectionDir->GetName()};
}
inline std::string SchemeName(const std::string& fileName, const std::string& scheme = "")
{
  return Detail::Scheme(Detail::Open(fileName).get(), scheme, fileName)->GetName();
}

namespace Detail
{
// A table of what a file holds (OutputNames::kCorrectionParticles...): absent, the file was written before it.
inline std::vector<std::vector<std::string>> Table(TDirectory* dir, const std::string& name, const std::vector<std::string>& columns,
                                                   const std::string& fileName, const std::string& task)
{
  std::optional<std::vector<std::vector<std::string>>> rows = RootIO::ReadStringTable(dir, name, columns, "PlotSeries");
  if (!rows)
    throw NotInFiles(std::format("[FATAL] PlotSeries: '{}' does not record '{}' in '{}': it was written before the record existed; "
                                 "re-run the {}.",
                                 fileName, name, dir->GetName(), task));
  return *rows;
}

inline std::vector<std::string> Column(const std::vector<std::vector<std::string>>& rows)
{
  std::vector<std::string> values;
  for (const std::vector<std::string>& row : rows)
    values.push_back(row.front());
  return values;
}
} // namespace Detail

// The particles a corrections file holds maps of, as MCTask recorded them.
inline std::vector<std::string> CorrectionParticles(const std::string& fileName, const std::string& scheme = "")
{
  auto file = Detail::Open(fileName);
  return Detail::Column(Detail::Table(Detail::Scheme(file.get(), scheme, fileName), OutputNames::kCorrectionParticles,
                                      OutputNames::kParticleColumns, fileName, "MC task"));
}

// The purity keys a purity file holds spectra of, as PurityTask recorded them.
inline std::vector<std::string> PurityKeys(const std::string& fileName, const std::string& scheme = "")
{
  auto file = Detail::Open(fileName);
  return Detail::Column(Detail::Table(Detail::Scheme(file.get(), scheme, fileName), OutputNames::kPurityKeys,
                                      OutputNames::kPurityKeyColumns, fileName, "purity task"));
}

// The species ratios a section holds, numerator and denominator, as the correlation task recorded them.
inline std::vector<std::pair<std::string, std::string>> SpeciesRatios(const std::string& fileName, const Where& where = {})
{
  const Detail::Located at = Detail::LocateSection(fileName, where);
  std::vector<std::pair<std::string, std::string>> ratios;
  for (const std::vector<std::string>& row :
       Detail::Table(at.sectionDir, OutputNames::kSpeciesRatios, OutputNames::kSpeciesRatioColumns, fileName, "correlation task"))
    ratios.emplace_back(row[0], row[1]);
  return ratios;
}

// The species of a section of a PhiAssocSpectra file: its directories that hold spectra or trends.
inline std::vector<std::string> SpeciesIn(const std::string& fileName, const Where& where = {})
{
  const Detail::Located at = Detail::LocateSection(fileName, where);
  std::vector<std::string> species;
  for (TObject* key : *at.sectionDir->GetListOfKeys())
    if (OutputNames::IsSpeciesDir(at.sectionDir->GetDirectory(key->GetName())))
      species.emplace_back(key->GetName());
  return species;
}

// A correction map of one class, or integrated over multiplicity (no interval).
inline Series CorrectionMap(const std::string& fileName, const std::string& particle, OutputNames::CorrectionMap map,
                            std::optional<Interval> mult,
                            OutputNames::SpectrumBinning binning = OutputNames::SpectrumBinning::Analysis, const std::string& scheme = "")
{
  auto file = Detail::Open(fileName);
  TDirectory* schemeDir = Detail::Scheme(file.get(), scheme, fileName);
  std::string multTag = OutputNames::kMultIntegrated;
  std::string multFact = "multiplicity-integrated";
  if (mult) {
    const std::vector<double> edges = Detail::MultBinning(schemeDir, fileName);
    const int multBin = Detail::MultIndex(edges, *mult, fileName);
    multTag = OutputNames::MultTag(edges, multBin);
    multFact = Detail::MultFact(edges, multBin);
  }
  Series s = Detail::Read(file, fileName,
                          RootIO::ObjectPath(OutputNames::CorrectionMapPath(schemeDir->GetName(), map, binning),
                                             OutputNames::CorrectionMapName(particle, map, multTag, binning)),
                          schemeDir, {});
  s.facts = {.kind = std::string(OutputNames::CorrectionMapTitle(map)), .species = particle, .mult = multFact,
             .binning = Detail::BinningFact(binning)};
  return s;
}

// An object of a file the framework did not write (a preliminary, a reference): by its path, and with no stamp, so
// that only its X axis can be checked against another series.
inline Series Object(const std::string& fileName, const std::string& path)
{
  Series s = Detail::Read(Detail::Open(fileName), fileName, path, nullptr, {});
  s.facts = {.kind = path};
  return s;
}

// -----------------------------------------------------------------------------
// Comparing and combining
// -----------------------------------------------------------------------------
// What two series disagree on: the stamps that differ and those that could not be checked, and their X axes.
struct Comparison {
  std::vector<std::string> differs;
  std::vector<std::string> notChecked;
};

// The stamps of the directories the two series were read from (mult and ptPhi of the scheme, ptAssoc of each species
// they share), by the rule of ComparisonTask, and their X axes. Only says: RequireCombinable stops, Draw warns,
// Describe reports. A combined series is compared through its first origin, whose stamps its operands shared.
inline Comparison Compare(const Series& a, const Series& b)
{
  Comparison result;
  const Origin& oa = a.origins.front();
  const Origin& ob = b.origins.front();
  auto check = [&](TDirectory* da, TDirectory* db, const std::string& stamp, const std::string& where) {
    if (!da || !db) {
      result.notChecked.push_back(std::format("{} ({}): a series of a foreign file has no stamp", stamp, where));
      return;
    }
    // One directory of one open file holds one stamp, or none: the same binning either way, nothing to compare.
    if (da == db)
      return;
    const RootIO::StampComparison c =
      RootIO::CompareBinningStamps(da, db, stamp, std::format("'{}'", oa.fileName), std::format("'{}'", ob.fileName));
    if (c.outcome == RootIO::StampComparison::Outcome::NotChecked)
      result.notChecked.push_back(std::format("{} ({}): {}", stamp, where, c.detail));
    else if (c.outcome == RootIO::StampComparison::Outcome::Differs)
      result.differs.push_back(std::format("{} ({}): {}", stamp, where, c.detail));
  };
  check(oa.schemeDir, ob.schemeDir, RootIO::kStampMult, "scheme");
  check(oa.schemeDir, ob.schemeDir, RootIO::kStampPtPhi, "scheme");
  for (TDirectory* sa : oa.speciesDirs)
    for (TDirectory* sb : ob.speciesDirs)
      if (std::string(sa->GetName()) == sb->GetName())
        check(sa, sb, RootIO::kStampPtAssoc, sa->GetName());

  const std::string axes = BinningUtils::Compare(BinningUtils::AxisEdges(a.hist->GetXaxis()), BinningUtils::AxisEdges(b.hist->GetXaxis()),
                                                 oa.fileName, ob.fileName);
  if (!axes.empty())
    result.differs.push_back(std::format("X axis:\n{}", axes));
  return result;
}

// Dividing or summing two series needs them to describe the same bins: a difference stops the run, a stamp that could
// not be checked is warned.
inline void RequireCombinable(const Series& a, const Series& b)
{
  const Comparison c = Compare(a, b);
  for (const std::string& n : c.notChecked)
    Logger::Warn("PlotSeries: '{}' and '{}', not checked: {}", a.origins.front().path, b.origins.front().path, n);
  if (!c.differs.empty())
    throw std::runtime_error(std::format("[FATAL] PlotSeries: '{}' of '{}' and '{}' of '{}' cannot be combined, their binnings "
                                         "differ:\n{}",
                                         a.origins.front().path, a.origins.front().fileName, b.origins.front().path,
                                         b.origins.front().fileName, Logger::Join(c.differs, "\n")));
}

namespace Detail
{
// A series computed from two: both origins, the facts both share (the kind given), no label, the operation noted.
inline Series Combined(const Series& a, const Series& b, std::unique_ptr<TH1> hist, const std::string& kind, const std::string& note)
{
  Series s{.hist = std::shared_ptr<TH1>(hist.release()), .origins = a.origins, .nonFiniteBins = 0, .notes = a.notes};
  s.hist->SetDirectory(0);
  s.origins.insert(s.origins.end(), b.origins.begin(), b.origins.end());
  s.notes.insert(s.notes.end(), b.notes.begin(), b.notes.end());
  s.notes.push_back(note);
  auto common = [](const std::optional<std::string>& x, const std::optional<std::string>& y) { return x == y ? x : std::nullopt; };
  s.facts = {.kind = kind, .species = common(a.facts.species, b.facts.species), .dy = common(a.facts.dy, b.facts.dy),
             .mult = common(a.facts.mult, b.facts.mult), .binning = common(a.facts.binning, b.facts.binning)};
  for (int bin = 1; bin <= s.hist->GetNbinsX(); ++bin)
    if (!std::isfinite(s.hist->GetBinContent(bin)) || !std::isfinite(s.hist->GetBinError(bin)))
      ++s.nonFiniteBins;
  return s;
}
} // namespace Detail

// num / den, with the option of TH1::Divide stated by the caller, never guessed: "" for independent samples (two
// productions, two years), "B" when each numerator is a subset of the denominator (a closure).
inline Series Ratio(const Series& num, const Series& den, const std::string& divideOption, const std::string& name = "")
{
  RequireCombinable(num, den);
  return Detail::Combined(num, den,
                          HistogramUtils::MakeRatioHist(num.hist.get(), den.hist.get(),
                                                        name.empty() ? std::format("{}_over_{}", num.hist->GetName(), den.hist->GetName()) : name,
                                                        num.hist->GetTitle(), 1.0, 1.0, divideOption.c_str()),
                          "ratio",
                          std::format("ratio of '{}' ({}) to '{}' ({}), Divide option \"{}\"", num.origins.front().path,
                                      num.origins.front().fileName, den.origins.front().path, den.origins.front().fileName,
                                      divideOption));
}

inline Series Sum(const Series& a, const Series& b, const std::string& name = "")
{
  RequireCombinable(a, b);
  std::unique_ptr<TH1> sum(static_cast<TH1*>(a.hist->Clone(name.empty() ? std::format("{}_plus_{}", a.hist->GetName(), b.hist->GetName()).c_str() : name.c_str())));
  sum->SetDirectory(0);
  sum->Add(b.hist.get());
  return Detail::Combined(a, b, std::move(sum), "sum",
                          std::format("sum of '{}' ({}) and '{}' ({})", a.origins.front().path, a.origins.front().fileName,
                                      b.origins.front().path, b.origins.front().fileName));
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

// -----------------------------------------------------------------------------
// Putting series on a pad, describing them
// -----------------------------------------------------------------------------
// Each series on the current pad, the first one without SAME, a styled copy with its non-finite bins left empty (the
// series keeps them); what the series drawn together disagree on is warned here, once. Returns the copies, for a
// legend.
inline std::vector<TH1*> Draw(const std::vector<Series>& series, const std::string& option = "E1")
{
  for (std::size_t i = 1; i < series.size(); ++i) {
    const Comparison c = Compare(series.front(), series[i]);
    for (const std::string& d : c.differs)
      Logger::Warn("PlotSeries: drawn together with a different binning, series 0 and {}: {}", i, d);
    for (const std::string& n : c.notChecked)
      Logger::Warn("PlotSeries: drawn together, not checked, series 0 and {}: {}", i, n);
  }
  // An emptied bin is left out only by the options that skip empty bins. Measured on a purity with one NaN bin (3-4 Nov
  // 2026): E, E1 and P leave it out; so does no option at all on a histogram with errors (Sumw2), which ROOT then draws
  // as E; E0 draws a marker at 0, HIST a drop to 0. Other options are refused rather than guessed, for a series that has
  // such a bin.
  std::string bare = option;
  std::erase(bare, ' ');
  std::ranges::transform(bare, bare.begin(), [](unsigned char ch) { return std::toupper(ch); });
  for (const Series& s : series) {
    const bool skipsEmpty = (bare.empty() && s.hist->GetSumw2N() > 0) || bare == "E" || bare == "E1" || bare == "P";
    if (s.nonFiniteBins && !skipsEmpty)
      throw std::runtime_error(std::format("[FATAL] PlotSeries::Draw: '{}' has {} non-finite bin(s), which option '{}' would draw at 0; "
                                           "draw it with E, E1 or P.", s.origins.front().path, s.nonFiniteBins, option));
  }
  std::vector<TH1*> drawn;
  for (std::size_t i = 0; i < series.size(); ++i) {
    std::unique_ptr<TH1> copy(static_cast<TH1*>(series[i].hist->Clone()));
    copy->SetDirectory(0);
    for (int bin = 1; bin <= copy->GetNbinsX(); ++bin)
      if (!std::isfinite(copy->GetBinContent(bin)) || !std::isfinite(copy->GetBinError(bin))) {
        copy->SetBinContent(bin, 0.);
        copy->SetBinError(bin, 0.);
      }
    const std::string opt = i == 0 ? option : std::format("{} SAME", option);
    const Look& look = series[i].look;
    TH1* d = CanvasManager::DrawStyledCopy(copy.get(), opt.c_str(), look.color.value_or(CanvasManager::ClassColor(static_cast<int>(i))),
                                           look.marker.value_or(20));
    if (look.markerSize)
      d->SetMarkerSize(*look.markerSize);
    // No statistics box: a plot never wants one, and ROOT's default would draw it when the file is opened elsewhere.
    d->SetStats(false);
    // A curve attached to the series (the fit of an extended spectrum) is drawn with it, in its colour, so that it reads
    // against its own points; its line style is the series' own (which fit it is).
    for (TObject* function : *d->GetListOfFunctions())
      if (auto* curve = dynamic_cast<TF1*>(function)) {
        curve->SetLineColor(d->GetMarkerColor());
        curve->SetLineWidth(2);
      }
    drawn.push_back(d);
  }
  return drawn;
}

// What was resolved, said before drawing: each series, where it comes from and what was done to it, and what the
// series drawn together disagree on (DIFFERS) or could not be checked for (NOT checked).
inline std::string Describe(const std::vector<Series>& series)
{
  std::string out;
  for (std::size_t i = 0; i < series.size(); ++i) {
    const Series& s = series[i];
    out += std::format("series {}: '{}'\n", i, s.label);
    for (const Origin& o : s.origins)
      out += std::format("  {} in {}\n", o.path, o.fileName);
    for (const std::string& note : s.notes)
      out += std::format("  {}\n", note);
    if (s.nonFiniteBins)
      out += std::format("  {} non-finite bin(s), left empty where drawn\n", s.nonFiniteBins);
  }
  for (std::size_t i = 1; i < series.size(); ++i) {
    const Comparison c = Compare(series.front(), series[i]);
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

// The facts that differ between the series label them; the ones every series has, with the same value, go in the
// header, in the order a reader expects (what it is, of what, where), the file last. A fact some series lack is not
// shared. File names are cut to the part that tells them apart: the words (between '/', '_' and '.') they all start or
// end with are removed.
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
    fileWords.push_back(words(s.origins.front().fileName));
  std::size_t prefix = 0, suffix = 0;
  if (fileWords.size() > 1) {
    const std::size_t shortest = std::ranges::min(fileWords | std::views::transform(&std::vector<std::string>::size));
    auto sameAt = [&](auto at) { return std::ranges::all_of(fileWords, [&](auto& w) { return at(w) == at(fileWords[0]); }); };
    while (prefix < shortest && sameAt([&](auto& w) { return w[prefix]; }))
      ++prefix;
    while (suffix < shortest - prefix && sameAt([&](auto& w) { return w[w.size() - 1 - suffix]; }))
      ++suffix;
  }

  using Field = std::optional<std::string> Facts::*;
  constexpr Field kOrder[] = {&Facts::kind, &Facts::species, &Facts::dy, &Facts::mult, &Facts::binning};
  constexpr std::size_t kFile = std::size(kOrder); // the file, after the facts
  std::vector<std::vector<std::optional<std::string>>> all;
  for (std::size_t i = 0; i < series.size(); ++i) {
    std::vector<std::optional<std::string>> values;
    for (Field field : kOrder)
      values.push_back(series[i].facts.*field);
    std::string kept;
    for (std::size_t w = prefix; w + suffix < fileWords[i].size(); ++w)
      kept += std::format("{}{}", kept.empty() ? "" : "_", fileWords[i][w]);
    values.push_back(kept.empty() ? std::nullopt : std::optional<std::string>(kept));
    all.push_back(std::move(values));
  }
  auto shared = [&](std::size_t f) { return series.size() > 1 && std::ranges::all_of(all, [&](auto& v) { return v[f] == all[0][f]; }); };

  LegendText text;
  for (std::size_t f = 0; f < kFile; ++f)
    if (shared(f) && all[0][f])
      text.header += std::format("{}{}", text.header.empty() ? "" : ", ", *all[0][f]);
  for (std::size_t i = 0; i < series.size(); ++i) {
    if (!series[i].label.empty()) {
      text.labels.push_back(series[i].label);
      continue;
    }
    std::string label;
    for (std::size_t f = 0; f <= kFile; ++f)
      if (!shared(f) && all[i][f])
        label += std::format("{}{}", label.empty() ? "" : ", ", *all[i][f]);
    text.labels.push_back(label);
  }
  return text;
}

// The owner of a plot's provenance in its .root.
inline const std::string kPlotOwner{"Plot"};

// The plot kept: 'outBase' with each extension of 'formats', and always 'outBase'.root with the canvas, a copy of each
// series as it was computed (Objects/series<i>, a non-finite bin as it is), and the provenance: the recipe (the
// composition or the specification, as the config_block), when, the code's commit and fingerprint, and per origin of
// each series its file, its path and its own provenance (RootIO::CopyProvenance). A .root that is not a plot written
// here, or that is open as an input, is never overwritten.
namespace Detail
{
// A plot's .root may replace only a plot's .root: never an input of the plot, never a file the plots did not write.
inline void RequirePlotOutput(const std::string& rootName, const std::string& ctx)
{
  if (!OpenFiles()[rootName].expired())
    throw std::runtime_error(std::format("[FATAL] {}: '{}' is open as an input of this plot.", ctx, rootName));
  if (std::filesystem::exists(rootName)) {
    std::unique_ptr<TFile> existing(TFile::Open(rootName.c_str(), "READ"));
    if (!existing || RootIO::ReadProvenance(existing.get(), kPlotOwner).empty())
      throw std::runtime_error(std::format("[FATAL] {}: '{}' exists and is not a plot written here; it is not overwritten. Choose another "
                                           "output.",
                                           ctx, rootName));
  }
  if (const std::filesystem::path dir = std::filesystem::path(rootName).parent_path(); !dir.empty())
    RootIO::CreateDirectoriesOrThrow(dir, ctx);
}
} // namespace Detail

inline void Save(TCanvas* canvas, const std::string& outBase, const std::vector<Series>& series, const std::string& recipe,
                 const std::vector<std::string>& formats = {"pdf", "png"})
{
  const std::string rootName = outBase + ".root";
  Detail::RequirePlotOutput(rootName, "PlotSeries::Save");
  for (const std::string& format : formats)
    canvas->SaveAs(std::format("{}.{}", outBase, format).c_str());

  std::unique_ptr<TFile> out = RootIO::OpenOrThrow(rootName, "RECREATE", "PlotSeries::Save");
  out->WriteTObject(canvas, canvas->GetName(), "Overwrite");
  RootIO::WriteProvenance(out.get(), kPlotOwner, RootIO::StartProvenance(recipe));
  for (std::size_t i = 0; i < series.size(); ++i) {
    RootIO::MakePath(out.get(), {"Objects", std::format("series{}", i)})->WriteTObject(series[i].hist.get(), series[i].hist->GetName(), "Overwrite");
    if (!series[i].notes.empty())
      RootIO::WriteProvenance(out.get(), kPlotOwner, {{"operations", Logger::Join(series[i].notes, "\n")}}, std::format("series{}", i));
    for (std::size_t j = 0; j < series[i].origins.size(); ++j) {
      const Origin& o = series[i].origins[j];
      const std::string record = std::format("series{}_origin{}", i, j);
      RootIO::WriteProvenance(out.get(), kPlotOwner, {{"file", o.fileName}, {"path", o.path}}, record);
      RootIO::CopyProvenance(o.file.get(), o.schemeDir ? o.schemeDir->GetName() : "", out.get(), kPlotOwner,
                             std::format("{}_input", record), o.fileName);
    }
  }
  out->Close();
}

// -----------------------------------------------------------------------------
// What a file holds
// -----------------------------------------------------------------------------
// The multiplicity classes of a file's scheme, from its stamp: what a plot of one curve per class iterates over.
inline std::vector<Interval> Intervals(const std::string& fileName, const std::string& scheme = "")
{
  auto file = Detail::Open(fileName);
  const std::vector<double> edges = Detail::MultBinning(Detail::Scheme(file.get(), scheme, fileName), fileName);
  std::vector<Interval> intervals;
  for (int multBin : BinningUtils::BinIndices(edges))
    intervals.push_back({edges[multBin], edges[multBin + 1]});
  return intervals;
}

struct Catalog {
  struct Section {
    std::string name;
    std::vector<std::string> species;
    std::map<std::string, std::vector<double>> ptAssocBinning; // per species, from its stamp
    std::optional<std::vector<double>> deltaYLimits;           // none: written before they were recorded
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
    if (!RootIO::IsSchemeDir(schemeDir))
      continue;
    Catalog::Scheme scheme{.name = schemeDir->GetName(),
                           .multBinning = RootIO::ReadBinningStamp(schemeDir, RootIO::kStampMult, fileName),
                           .ptPhiBinning = RootIO::ReadBinningStamp(schemeDir, RootIO::kStampPtPhi, fileName),
                           .provenance = RootIO::ReadProvenance(file.get(), schemeDir->GetName())};
    for (TObject* k : *schemeDir->GetListOfKeys()) {
      TDirectory* sectionDir = schemeDir->GetDirectory(k->GetName());
      if (!OutputNames::IsSectionDir(sectionDir))
        continue;
      Catalog::Section section{.name = sectionDir->GetName(),
                               .deltaYLimits = RootIO::ReadValueList(sectionDir, OutputNames::kDeltaYLimits, fileName)};
      for (TObject* s : *sectionDir->GetListOfKeys())
        if (TDirectory* speciesDir = sectionDir->GetDirectory(s->GetName()); OutputNames::IsSpeciesDir(speciesDir)) {
          section.species.emplace_back(s->GetName());
          section.ptAssocBinning[s->GetName()] = RootIO::ReadBinningStamp(speciesDir, RootIO::kStampPtAssoc, fileName);
        }
      scheme.sections.push_back(std::move(section));
    }
    catalog.schemes.push_back(std::move(scheme));
  }
  return catalog;
}

} // namespace PlotSeries
