#pragma once

#include "BinningUtils.h"
#include "CanvasManager.h"
#include "HistogramUtils.h"
#include "Logger.h"
#include "OutputNames.h"
#include "PlotCatalog.h"
#include "PlotSpec.h"
#include "RootIOHelpers.h"
#include "RunEnvironment.h"

#include "TCanvas.h"
#include "TFile.h"
#include "TH1.h"
#include "TLegend.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

// From a specification to a plot, in two steps kept apart: Resolve finds every object and
// runs every check, Render draws what was resolved. `phiplot describe` stops after the first,
// so a front end can show in plain words what will be drawn, and have it confirmed, before
// anything is drawn - the same words whoever (or whatever) wrote the specification.
//
// What is checked, and why (CLAUDE.md, the rule the framework is built on):
// - a multiplicity interval is looked up in the file's binning_mult stamp, never taken as an
//   index: "multBin3" means whatever the producer's binning said;
// - dividing two series requires the same X axis (RequireSameAxis) and, when they come from
//   different schemes, the same trigger-pT binning (integrated away in everything here) and,
//   for objects against multiplicity, the same associated-pT binning of the species
//   (integrated away in a trend). Without a ratio, series that disagree there are drawn but
//   the disagreement goes through Logger::Warn: a plot does not divide, but whoever reads it
//   compares.
namespace PlotRenderer
{

// What was asked is not in these files: a species, a |dy| limit, an interval, a scheme or a
// section that they do not hold. Its own type, and phiplot's own exit code (kExitNotInFiles),
// because a front end must tell it from a specification that is malformed: the first is
// answered by saying what there is, the second by correcting the specification.
struct NotInFiles : std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct Resolved {
  const PlotSpec::Series* spec{nullptr};
  std::string scheme;
  std::string section;
  OutputNames::ObjectId id;
  std::string path; // inside the file
  std::unique_ptr<TH1> hist;  // the drawn copy: renamed and restyled
  std::unique_ptr<TH1> input; // as read, for Objects/ in the plot's file
  const PlotCatalog::Section* catalog{nullptr};
  std::string label;
};

struct Plan {
  const PlotSpec::Spec* spec{nullptr};
  std::map<std::string, PlotCatalog::Catalog> catalogs; // per file
  std::vector<Resolved> series;
  std::string legendHeader; // what all series share
  std::vector<std::string> checks;    // what was verified, for the description
  std::vector<std::string> unchecked; // what could not be checked (a stamp missing)
  std::vector<std::string> differs;   // what disagrees between series drawn together, not divided
};

namespace Detail
{
// Where the plot's own file keeps its specification; also how a file is recognised as one
// phiplot wrote, and may replace.
inline constexpr const char* kSpecKey = "plot_spec";
inline const std::string kSpecMark = std::format("{}/{}", OutputNames::kProvenanceDir, kSpecKey);

inline constexpr const char* kNoScheme = "the file has no <binning_name>/Extract1D or Extract2D directory (written before that layout existed, or not a PhiAssocSpectra file).";

inline std::string Stem(const std::string& path) { return std::filesystem::path(path).stem().string(); }

inline std::string Choose(const std::vector<std::string>& options, const std::optional<std::string>& requested,
                          const char* what, const std::string& ctx)
{
  std::string list;
  for (const auto& o : options)
    list += (list.empty() ? "" : ", ") + o;
  if (requested) {
    for (const auto& o : options)
      if (o == *requested)
        return o;
    throw NotInFiles(std::format("[FATAL] {}: no {} '{}'. Available: {}", ctx, what, *requested, list.empty() ? "none" : list));
  }
  if (options.size() == 1)
    return options.front();
  if (options.empty())
    throw NotInFiles(std::format("[FATAL] {}: no {} to choose from: {}", ctx, what, kNoScheme));
  throw std::runtime_error(std::format("[FATAL] {}: more than one {}, so it must be named. Available: {}", ctx, what, list));
}

// The bin of the stamp whose edges are the interval asked for, within the tolerance the
// stamps are compared with (BinningUtils::kEdgeEpsilon).
inline int MultBin(const std::vector<double>& edges, std::pair<double, double> interval, const std::string& ctx)
{
  if (edges.empty())
    throw NotInFiles(std::format("[FATAL] {}: the scheme carries no {} stamp, so there is no way to tell which "
                                         "spectrum is the interval asked for. Re-run the correlation task that wrote it.",
                                         ctx, OutputNames::kStampMult));
  for (int i = 0; i + 1 < static_cast<int>(edges.size()); ++i)
    if (std::abs(edges[i] - interval.first) <= BinningUtils::kEdgeEpsilon && std::abs(edges[i + 1] - interval.second) <= BinningUtils::kEdgeEpsilon)
      return i;
  std::set<int> all;
  for (int i = 0; i + 1 < static_cast<int>(edges.size()); ++i)
    all.insert(i);
  throw NotInFiles(std::format("[FATAL] {}: [{}, {}] is not a multiplicity bin of this file. Available: {}",
                                       ctx, BinningUtils::FormatEdge(interval.first), BinningUtils::FormatEdge(interval.second),
                                       PlotCatalog::Intervals(edges, all)));
}

// The inputs reduced to what tells them apart, for the legend: the tokens shared by all of
// them at the start and at the end are dropped - words split at '_' in the file names,
// directories split at '/' in the paths (so that "pp" and "pp_ref" stay whole) - so
// "Data_OfflineEff_2024_PhiAssocSpectra" and "..._2026_..." become "2024" and "2026". The
// file names are tried first; when two different paths have the same file name (pp and
// pp_ref write the same names), the whole paths are; and when that still leaves two equal or
// an empty one, the full paths are used. Two different files never share a label.
namespace Stems
{
inline std::vector<std::string> Tokens(const std::string& s, char sep)
{
  std::vector<std::string> t(1);
  for (char ch : s) {
    if (ch == sep)
      t.emplace_back();
    else
      t.back() += ch;
  }
  return t;
}

inline std::vector<std::string> Strip(const std::vector<std::string>& names, char sep)
{
  std::vector<std::vector<std::string>> tokens;
  for (const auto& n : names)
    tokens.push_back(Tokens(n, sep));
  std::size_t minLen = tokens.front().size();
  for (const auto& t : tokens)
    minLen = std::min(minLen, t.size());
  const auto sharedAt = [&](auto index) {
    for (const auto& t : tokens)
      if (t[index(t)] != tokens.front()[index(tokens.front())])
        return false;
    return true;
  };
  std::size_t head = 0, tail = 0;
  while (head < minLen && sharedAt([&](const auto&) { return head; }))
    ++head;
  while (tail < minLen - head && sharedAt([&](const auto& t) { return t.size() - 1 - tail; }))
    ++tail;
  std::vector<std::string> out;
  for (const auto& t : tokens) {
    std::string s;
    for (std::size_t k = head; k + tail < t.size(); ++k)
      s += (s.empty() ? "" : std::string(1, sep)) + t[k];
    out.push_back(s);
  }
  return out;
}

// True when the labels tell apart every pair of different paths, and none is empty.
inline bool Distinguish(const std::vector<std::string>& paths, const std::vector<std::string>& labels)
{
  for (std::size_t i = 0; i < paths.size(); ++i) {
    if (labels[i].empty())
      return false;
    for (std::size_t j = i + 1; j < paths.size(); ++j)
      if (paths[i] != paths[j] && labels[i] == labels[j])
        return false;
  }
  return true;
}
} // namespace Stems

inline std::vector<std::string> DistinctStems(const std::vector<std::string>& paths)
{
  std::vector<std::string> stems, whole;
  for (const auto& p : paths) {
    stems.push_back(Stem(p));
    whole.push_back((std::filesystem::path(p).parent_path() / Stem(p)).string());
  }
  for (const auto& candidate : {Stems::Strip(stems, '_'), Stems::Strip(whole, '/')})
    if (Stems::Distinguish(paths, candidate))
      return candidate;
  return paths;
}

inline std::string Who(const OutputNames::ObjectId& id)
{
  return OutputNames::Info(id.kind).speciesPair ? std::format("{}/{}", id.species, id.speciesDen) : id.species;
}

// What exists for this kind, for the message when the object asked for does not.
inline std::string Available(const PlotCatalog::Section& sec, OutputNames::Kind kind)
{
  std::string out;
  for (const auto& g : sec.groups) {
    if (g.kind != kind)
      continue;
    const auto& info = OutputNames::Info(kind);
    out += std::format("\n    {} |dy| < {}", info.speciesPair ? g.species + "/" + g.speciesDen : g.species, PlotCatalog::Dys(g.dy));
    if (info.perMultBin)
      out += std::format(", mult {}", PlotCatalog::Intervals(sec.mult, g.multBins));
  }
  return out.empty() ? " none of this kind" : out;
}

inline std::string MultText(const Resolved& r)
{
  const auto& edges = r.catalog->mult;
  return std::format("{}-{}%", BinningUtils::FormatEdge(edges[r.id.multBin]), BinningUtils::FormatEdge(edges[r.id.multBin + 1]));
}
} // namespace Detail

inline Plan Resolve(const PlotSpec::Spec& spec)
{
  Plan plan;
  plan.spec = &spec;

  // The plot's .root is written with RECREATE, so a file there that phiplot did not write -
  // one of the inputs, above all - is refused rather than replaced. Checked here, before
  // anything is drawn, so that describe says it too.
  const std::string outRoot = spec.output + ".root";
  if (std::filesystem::exists(outRoot)) {
    std::unique_ptr<TFile> previous(TFile::Open(outRoot.c_str(), "READ"));
    std::unique_ptr<TObject> mark(previous && !previous->IsZombie() ? previous->Get(Detail::kSpecMark.c_str()) : nullptr);
    if (!mark)
      throw std::runtime_error(std::format("[FATAL] phiplot: '{}' exists and was not written by phiplot (no {} in it); it is not "
                                           "replaced. Choose another 'output'.", outRoot, Detail::kSpecMark));
  }

  for (std::size_t i = 0; i < spec.series.size(); ++i) {
    const PlotSpec::Series& s = spec.series[i];
    const std::string ctx = std::format("phiplot series[{}] ({})", i, s.file);

    auto [it, inserted] = plan.catalogs.try_emplace(s.file);
    if (inserted)
      it->second = PlotCatalog::Build(s.file);
    const PlotCatalog::Catalog& cat = it->second;

    std::vector<std::string> schemes;
    for (const auto& sec : cat.sections)
      if (std::find(schemes.begin(), schemes.end(), sec.scheme) == schemes.end())
        schemes.push_back(sec.scheme);
    Resolved r;
    r.spec = &s;
    r.scheme = Detail::Choose(schemes, s.binningName, "binning_name", ctx);
    std::vector<std::string> sections;
    for (const auto& sec : cat.sections)
      if (sec.scheme == r.scheme)
        sections.push_back(sec.section);
    r.section = Detail::Choose(sections, s.section, "section", std::format("{} scheme '{}'", ctx, r.scheme));
    for (const auto& sec : cat.sections)
      if (sec.scheme == r.scheme && sec.section == r.section)
        r.catalog = &sec;

    r.id.kind = s.kind;
    r.id.species = s.species;
    r.id.speciesDen = s.speciesDen;
    r.id.dy = s.dy;
    if (s.mult)
      r.id.multBin = Detail::MultBin(r.catalog->mult, *s.mult, ctx);

    // The object must be one the catalog found, so that a miss says what there is.
    bool found = false;
    for (const auto& g : r.catalog->groups)
      if (g.kind == s.kind && g.species == s.species && g.speciesDen == s.speciesDen &&
          g.dy.contains(s.dy) && (r.id.multBin < 0 || g.multBins.contains(r.id.multBin)))
        found = true;
    if (!found)
      throw NotInFiles(std::format("[FATAL] {}: no {} for {} at |dy| < {}{} in '{}/{}'. What there is of this kind:{}",
                                           ctx, OutputNames::Info(s.kind).name, Detail::Who(r.id), BinningUtils::FormatDeltaY(s.dy).first,
                                           s.mult ? std::format(", mult {}-{}", BinningUtils::FormatEdge(s.mult->first), BinningUtils::FormatEdge(s.mult->second)) : "",
                                           r.scheme, r.section, Detail::Available(*r.catalog, s.kind)));

    std::vector<std::string> path{r.scheme, r.section};
    for (auto& node : OutputNames::Location(r.id))
      path.push_back(node);
    r.path = RootIO::MakeDirPath(path) + OutputNames::Name(r.id);
    std::unique_ptr<TFile> f = RootIO::OpenOrThrow(s.file, "READ", ctx);
    r.input = RootIO::GetUniqueOrThrow<TH1>(f.get(), r.path, ctx);
    r.hist.reset(static_cast<TH1*>(r.input->Clone(std::format("phiplot_series{}", i).c_str())));
    r.hist->SetDirectory(0);
    plan.series.push_back(std::move(r));
  }

  // --- Checks between series ------------------------------------------------
  // One comparison of two series, used on the same pairs whatever is drawn: each against
  // the reference when there is a ratio pad, every pair otherwise. A disagreement stops a
  // division and is warned about in an overlay; a stamp missing on either side is a check
  // that could not be made, warned about in both and listed in the description.
  const bool perMult = spec.PerMultBin();
  const auto compare = [&](std::size_t i, std::size_t j, bool dividing) {
    const Resolved& a = plan.series[i];
    const Resolved& b = plan.series[j];
    const std::string who = std::format("series[{}] and series[{}]", i, j);
    const auto stamp = [&](const std::vector<double>& x, const std::vector<double>& y, const std::string& what, const std::string& why) {
      if (x.empty() || y.empty()) {
        Logger::Warn("phiplot: a binning stamp is missing on one side, so two series could not be checked against each other; "
                     "see 'not checked' in the description");
        plan.unchecked.push_back(std::format("{}: {} (no stamp on one side)", who, what));
        return;
      }
      const std::string diff = BinningUtils::Compare(x, y, std::format("series[{}]", i), std::format("series[{}]", j));
      if (diff.empty())
        return;
      if (dividing)
        throw std::runtime_error(std::format("[FATAL] phiplot: {} differ in {}:\n{}{}", who, what, diff, why));
      Logger::Warn("phiplot: two series drawn together differ in a binning they are summed or integrated over; they are drawn, "
                   "not divided, but they do not mean the same thing (see the description)");
      plan.differs.push_back(std::format("{}: {}", who, what));
    };
    if (dividing)
      BinningUtils::RequireSameAxis(a.hist->GetXaxis(), b.hist->GetXaxis(), std::format("series[{}] {}", i, a.path),
                                    std::format("series[{}] {}", j, b.path));
    stamp(a.catalog->ptPhi, b.catalog->ptPhi, "the trigger-pT binning", " Both are summed over it: they would divide cleanly and mean different things.");
    // A trend is an integral over the associated pT: for a species the two have in common,
    // the binnings must agree even though the multiplicity axes do. For a spectrum the pT
    // is the X axis, and RequireSameAxis covers it.
    if (perMult)
      return;
    for (const std::string& sp : {a.id.species, a.id.speciesDen})
      if (!sp.empty() && (sp == b.id.species || sp == b.id.speciesDen)) {
        const auto ptOf = [&](const Resolved& r) { auto it = r.catalog->ptAssoc.find(sp); return it == r.catalog->ptAssoc.end() ? std::vector<double>{} : it->second; };
        stamp(ptOf(a), ptOf(b), std::format("the {} pT binning", sp), " Their multiplicity axes agree and their contents are integrals over different ranges.");
      }
  };
  if (spec.ratio) {
    for (std::size_t i = 0; i < plan.series.size(); ++i)
      if (static_cast<int>(i) != spec.ratio->reference)
        compare(i, spec.ratio->reference, true);
    plan.checks.push_back("each divided series has the reference's X axis, edge by edge");
  } else {
    for (std::size_t i = 0; i < plan.series.size(); ++i)
      for (std::size_t j = i + 1; j < plan.series.size(); ++j)
        compare(i, j, false);
  }
  // "Agree" only where a disagreement would have stopped the run; in an overlay the
  // comparison is reported, and what differs is listed under DIFFERS.
  const std::string compared = perMult ? "the trigger-pT binning (summed over)"
                                       : "the trigger-pT binning (summed over) and the pT binning of every species two series share (integrated over)";
  plan.checks.push_back(spec.ratio ? std::format("{} agree with the reference's, where both are stamped", compared)
                                   : std::format("{} compared between every pair of series, where both are stamped (an overlay does not require them to agree)", compared));
  if (perMult)
    plan.checks.push_back("each multiplicity interval was found in its file's binning_mult stamp, not taken as an index");

  // --- Labels: what differs goes in each entry, what is shared in the header -
  std::vector<std::string> files;
  for (const auto& r : plan.series)
    files.push_back(r.spec->file);
  const std::vector<std::string> stems = Detail::DistinctStems(files);
  std::map<const Resolved*, std::string> stemOf;
  for (std::size_t i = 0; i < plan.series.size(); ++i)
    stemOf[&plan.series[i]] = stems[i];
  const auto field = [&](const Resolved& r, int which) -> std::string {
    switch (which) {
      case 0:
        return std::string(OutputNames::Info(r.id.kind).name);
      case 1:
        return Detail::Who(r.id);
      case 2:
        return std::format("|#Deltay| < {}", BinningUtils::FormatDeltaY(r.id.dy).first);
      case 3:
        return r.id.multBin >= 0 ? Detail::MultText(r) : "";
      case 4:
        return stemOf.at(&r) + (r.section == OutputNames::kExtract2D ? " (2D)" : "");
    }
    return "";
  };
  std::vector<std::string> shared;
  std::vector<int> varying;
  for (int f = 0; f < 5; ++f) {
    bool same = true;
    for (const auto& r : plan.series)
      same = same && field(r, f) == field(plan.series.front(), f);
    if (same) {
      // The kind is in the axis titles; a file shared by every series is in the
      // description and the provenance, and its name is too long for a legend.
      if (f == 4 && plan.series.front().section == OutputNames::kExtract2D)
        shared.push_back("2D engine");
      else if (!field(plan.series.front(), f).empty() && f != 0 && f != 4)
        shared.push_back(field(plan.series.front(), f));
    } else {
      varying.push_back(f);
    }
  }
  for (const auto& s : shared)
    plan.legendHeader += (plan.legendHeader.empty() ? "" : ", ") + s;
  for (auto& r : plan.series) {
    if (r.spec->label) {
      r.label = *r.spec->label;
      continue;
    }
    for (int f : varying)
      r.label += (r.label.empty() ? "" : ", ") + field(r, f);
    if (r.label.empty())
      r.label = field(r, 1);
  }
  return plan;
}

// Plain words: what will be drawn, from where, with which checks. Built from the resolved
// plan, so it describes the objects found, not the request.
inline std::string Describe(const Plan& plan)
{
  const PlotSpec::Spec& spec = *plan.spec;
  std::string out = std::format("Plot '{}': {} series against {}{}\n", spec.title.value_or(spec.output), plan.series.size(),
                                spec.PerMultBin() ? "associated pT" : "multiplicity",
                                spec.ratio ? std::format(", with a ratio pad (each divided by series {}, TH1::Divide option \"{}\")",
                                                         spec.ratio->reference, spec.ratio->divideOption)
                                           : "");
  for (std::size_t i = 0; i < plan.series.size(); ++i) {
    const Resolved& r = plan.series[i];
    const auto prov = [&](const char* key) { auto it = r.catalog->provenance.find(key); return it == r.catalog->provenance.end() ? std::string("unknown") : it->second; };
    out += std::format("  [{}] '{}': {} ({}), {}, |dy| < {}{}\n      from {} : {}\n      produced {}, commit {}\n", i, r.label,
                       OutputNames::Info(r.id.kind).name, OutputNames::Info(r.id.kind).what, Detail::Who(r.id),
                       BinningUtils::FormatDeltaY(r.id.dy).first, r.id.multBin >= 0 ? ", multiplicity " + Detail::MultText(r) : "",
                       r.spec->file, r.path, prov("produced_at"), prov("git_commit").substr(0, 10));
  }
  for (const auto& c : plan.checks)
    out += std::format("  checked: {}\n", c);
  for (const auto& c : plan.unchecked)
    out += std::format("  NOT checked: {}\n", c);
  for (const auto& c : plan.differs)
    out += std::format("  DIFFERS (drawn, not divided): {}\n", c);
  std::string formats;
  for (const auto& f : spec.formats)
    formats += (formats.empty() ? "" : ", ") + f;
  out += std::format("  output: {}.{{{}}} and {}.root (canvas, objects, provenance)\n", spec.output, formats, spec.output);
  return out;
}

inline void Render(Plan& plan)
{
  const PlotSpec::Spec& spec = *plan.spec;
  static constexpr int kPalette[] = {kBlack, kRed + 1, kBlue + 1, kGreen + 2, kMagenta + 1, kOrange + 7, kCyan + 2, kViolet + 1, kAzure + 2, kPink + 9};
  static constexpr int kMarkers[] = {20, 21, 22, 23, 33, 34, 29, 47, 24, 25};
  const bool logY = spec.logY.value_or(spec.PerMultBin());

  // The ratios outlive the canvas that draws them: declared before the manager, destroyed after it.
  std::vector<std::unique_ptr<TH1>> ratios;
  CanvasManager canvases;
  const std::string cname = std::format("phiplot_{}", Detail::Stem(spec.output));
  TPad* top = nullptr;
  TPad* bottom = nullptr;
  double fontScale = 1.0;
  TCanvas* c = nullptr;
  if (spec.ratio) {
    auto rc = canvases.CreateRatioCanvas(cname, spec.title.value_or(""));
    c = rc.canvas;
    top = rc.topPad;
    bottom = rc.bottomPad;
    fontScale = rc.fontScale;
  } else {
    c = canvases.CreateCanvas(cname, spec.title.value_or(""));
    top = c;
  }
  CanvasManager::SetLog(top, false, logY);

  std::vector<TH1*> drawn;
  for (std::size_t i = 0; i < plan.series.size(); ++i) {
    Resolved& r = plan.series[i];
    const int color = r.spec->color.value_or(kPalette[i % std::size(kPalette)]);
    const int marker = r.spec->marker.value_or(kMarkers[i % std::size(kMarkers)]);
    CanvasManager::SetHistogramStyle(r.hist.get(), color, marker, 1.2, 2, 0);
    r.hist->SetStats(false);
    drawn.push_back(r.hist.get());
  }

  TH1* frame = drawn.front();
  frame->SetTitle(spec.title.value_or("").c_str());
  if (spec.xTitle)
    frame->GetXaxis()->SetTitle(spec.xTitle->c_str());
  if (spec.yTitle)
    frame->GetYaxis()->SetTitle(spec.yTitle->c_str());
  if (spec.xRange)
    frame->GetXaxis()->SetRangeUser(spec.xRange->first, spec.xRange->second);
  CanvasManager::YRangeOptions yopts;
  yopts.isLogY = logY;
  const auto [yMin, yMax] = spec.yRange.value_or(CanvasManager::ComputeGlobalYRange(drawn, yopts));
  frame->SetMinimum(yMin);
  frame->SetMaximum(yMax);
  if (spec.ratio)
    CanvasManager::FormatTopPadAxes(frame);
  else
    frame->GetYaxis()->SetTitleOffset(1.4); // SetHistogramStyle's 1.0 puts the title on the labels

  top->cd();
  for (std::size_t i = 0; i < drawn.size(); ++i)
    drawn[i]->Draw(i == 0 ? "E1 P" : "E1 P SAME");

  // Wide enough for the longest entry (about 0.012 of the pad per character at this size).
  std::size_t longest = plan.legendHeader.size();
  for (const auto& r : plan.series)
    longest = std::max(longest, r.label.size() + 6);
  const double x1 = std::clamp(0.92 - 0.012 * static_cast<double>(longest), 0.18, 0.62);
  TLegend* legend = CanvasManager::CreateLegend(x1, 0.88 - 0.06 * (drawn.size() + 1), 0.92, 0.88, 1, 0.035);
  if (!plan.legendHeader.empty())
    legend->SetHeader(plan.legendHeader.c_str());
  for (std::size_t i = 0; i < drawn.size(); ++i)
    legend->AddEntry(drawn[i], plan.series[i].label.c_str(), "lp");
  legend->Draw();
  if (spec.alice)
    CanvasManager::DrawAliceLabel(top, 0.18, 0.84, spec.alice->status, spec.alice->system);

  if (spec.ratio) {
    const Resolved& ref = plan.series[spec.ratio->reference];
    std::vector<TH1*> ratioViews;
    for (std::size_t i = 0; i < plan.series.size(); ++i) {
      if (static_cast<int>(i) == spec.ratio->reference)
        continue;
      ratios.push_back(HistogramUtils::MakeRatioHist(plan.series[i].hist.get(), ref.hist.get(), std::format("phiplot_ratio{}", i), "", 1.0, 1.0,
                                                     spec.ratio->divideOption.c_str()));
      ratioViews.push_back(ratios.back().get());
    }
    CanvasManager::YRangeOptions ropts;
    ropts.topMargin = 0.1;
    const auto [rMin, rMax] = spec.ratio->yRange.value_or(CanvasManager::ComputeGlobalYRange(ratioViews, ropts));
    TH1* rframe = ratioViews.front();
    // Named after the reference when its label is short enough for the pad.
    const std::string defaultTitle = ref.label.size() <= 14 ? std::format("Ratio to {}", ref.label) : std::format("Ratio to [{}]", spec.ratio->reference);
    CanvasManager::FormatBottomPadAxes(rframe, spec.ratio->yTitle.value_or(defaultTitle), rMin, rMax, fontScale);
    rframe->GetXaxis()->SetTitle(frame->GetXaxis()->GetTitle());
    if (spec.xRange)
      rframe->GetXaxis()->SetRangeUser(spec.xRange->first, spec.xRange->second);
    bottom->cd();
    for (std::size_t i = 0; i < ratioViews.size(); ++i)
      ratioViews[i]->Draw(i == 0 ? "E1 P" : "E1 P SAME");
    CanvasManager::DrawHorizontalLine(bottom, rframe->GetXaxis()->GetBinLowEdge(rframe->GetXaxis()->GetFirst()),
                                      rframe->GetXaxis()->GetBinUpEdge(rframe->GetXaxis()->GetLast()), 1.0);
  }
  c->Modified();
  c->Update();

  for (const auto& fmt : spec.formats)
    CanvasManager::ExportCanvas(c, std::format("{}.{}", spec.output, fmt));

  // The ROOT file: the canvas, what was drawn, and how it was made. Provenance never
  // throws (CLAUDE.md), and an input without any is recorded as such.
  std::unique_ptr<TFile> out = RootIO::OpenOrThrow(spec.output + ".root", "RECREATE", "phiplot");
  CanvasManager::WriteCanvas(c, out.get());
  TDirectory* objects = RootIO::GetOrCreatePath(out.get(), {"Objects"});
  // The inputs as read, one directory per series (two series may share an object name);
  // what was drawn is in the canvas.
  for (std::size_t i = 0; i < plan.series.size(); ++i) {
    RootIO::GetOrCreatePath(objects, {std::format("series{}", i)})->cd();
    plan.series[i].input->Write(nullptr, TObject::kOverwrite);
  }
  objects->cd();
  for (auto& h : ratios)
    h->Write(nullptr, TObject::kOverwrite);

  std::map<std::string, std::string> facts = RunEnvironment::Facts();
  facts["produced_at"] = RunEnvironment::TimestampNow();
  facts[Detail::kSpecKey] = spec.json;
  RootIO::WriteProvenance(RootIO::GetOrCreatePath(out.get(), {std::string(OutputNames::kProvenanceDir)}), facts);
  for (std::size_t i = 0; i < plan.series.size(); ++i) {
    const Resolved& r = plan.series[i];
    std::map<std::string, std::string> input = r.catalog->provenance;
    if (input.empty())
      input["no_provenance"] = "the input carries none under its scheme's Provenance directory";
    input["input_file"] = r.spec->file;
    input["input_object"] = r.path;
    RootIO::WriteProvenance(RootIO::GetOrCreatePath(out.get(), {std::format("{}_series{}", OutputNames::kProvenanceDir, i)}), input);
  }
  out->Close();
  Logger::Info("phiplot: written {}.{{{}}} and {}.root", spec.output, [&] { std::string f; for (const auto& x : spec.formats) f += (f.empty() ? "" : ",") + x; return f; }(), spec.output);
}

} // namespace PlotRenderer
