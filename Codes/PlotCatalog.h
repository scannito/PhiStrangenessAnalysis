#pragma once

#include "BinningUtils.h"
#include "Logger.h"
#include "OutputNames.h"
#include "RootIOHelpers.h"

#include "TClass.h"
#include "TDirectory.h"
#include "TFile.h"
#include "TH1.h"
#include "TKey.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

#include <format>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

// What a PhiAssocSpectra file holds, by meaning: for each scheme and section, which kinds of
// object exist for which species, at which delta-y limits and for which multiplicity
// intervals, with the binnings the stamps record. It is what a reader needs to ask for a
// plot without knowing how the objects are named, and what the plotting tool resolves a
// request against (PlotSpec).
//
// Read only, and nothing here throws on what a file holds: a file is described as it is.
// Objects whose name OutputNames::Parse does not recognise are listed as such, never
// guessed at.
namespace PlotCatalog
{

// One kind of object for one species (or pair), over the delta-y limits and multiplicity
// bins present.
struct Group {
  OutputNames::Kind kind;
  std::string species;
  std::string speciesDen;
  std::set<double> dy;
  std::set<int> multBins; // per-multiplicity kinds only
};

struct Section {
  std::string scheme;  // the binning_name
  std::string section; // Extract1D or Extract2D
  std::vector<double> mult;  // binning_mult stamp of the scheme, empty when absent
  std::vector<double> ptPhi; // binning_ptPhi stamp of the scheme
  std::map<std::string, std::vector<double>> ptAssoc; // per species
  std::map<std::string, std::string> provenance;      // of the scheme
  std::vector<Group> groups;
  std::vector<std::string> unrecognised; // histograms whose names OutputNames does not parse
};

struct Catalog {
  std::string file;
  std::vector<Section> sections;
};

namespace Detail
{
inline std::vector<std::pair<std::string, std::string>> Keys(TDirectory* dir)
{
  std::vector<std::pair<std::string, std::string>> out;
  if (!dir || !dir->GetListOfKeys())
    return out;
  std::set<std::string> seen; // a key written twice has two cycles; one entry is enough
  for (TObject* obj : *dir->GetListOfKeys()) {
    auto* key = static_cast<TKey*>(obj);
    if (seen.insert(key->GetName()).second)
      out.emplace_back(key->GetName(), key->GetClassName());
  }
  return out;
}

inline bool IsDirectory(const std::string& cls) { return cls == "TDirectoryFile" || cls == "TDirectory"; }

inline bool IsHistogram(const std::string& cls)
{
  TClass* c = TClass::GetClass(cls.c_str());
  return c && c->InheritsFrom(TH1::Class());
}

// Histograms of one directory, stamps excluded (they are TH1D too).
inline void Collect(TDirectory* dir, const std::string& where, Section& sec,
                    std::map<std::tuple<OutputNames::Kind, std::string, std::string>, Group>& groups)
{
  for (const auto& [name, cls] : Keys(dir)) {
    if (!IsHistogram(cls))
      continue;
    const auto id = OutputNames::Parse(name);
    if (!id) {
      sec.unrecognised.push_back(where + name);
      continue;
    }
    Group& g = groups[{id->kind, id->species, id->speciesDen}];
    g.kind = id->kind;
    g.species = id->species;
    g.speciesDen = id->speciesDen;
    g.dy.insert(id->dy);
    if (id->multBin >= 0)
      g.multBins.insert(id->multBin);
  }
}
} // namespace Detail

inline Section DescribeSection(TDirectory* schemeDir, const std::string& scheme, const std::string& section)
{
  Section sec;
  sec.scheme = scheme;
  sec.section = section;
  sec.mult = RootIO::ReadBinningStamp(schemeDir, std::string(OutputNames::kStampMult));
  sec.ptPhi = RootIO::ReadBinningStamp(schemeDir, std::string(OutputNames::kStampPtPhi));
  sec.provenance = RootIO::ReadProvenance(schemeDir->GetDirectory(std::string(OutputNames::kProvenanceDir).c_str()));

  TDirectory* secDir = schemeDir->GetDirectory(section.c_str());
  std::map<std::tuple<OutputNames::Kind, std::string, std::string>, Group> groups;
  for (const auto& [name, cls] : Detail::Keys(secDir)) {
    if (!Detail::IsDirectory(cls))
      continue;
    TDirectory* sub = secDir->GetDirectory(name.c_str());
    if (name == OutputNames::kRatiosDir) {
      Detail::Collect(sub, name + "/", sec, groups);
      continue;
    }
    if (!OutputNames::IsSpeciesDir(sub)) {
      sec.unrecognised.push_back(name + "/ (a directory that is neither a species nor Ratios)");
      continue;
    }
    // A species directory: its stamp, then what it holds per kind.
    sec.ptAssoc[name] = RootIO::ReadBinningStamp(sub, std::string(OutputNames::kStampPtAssoc));
    for (const std::string_view leaf : {OutputNames::kSpectraDir, OutputNames::kTrendsDir})
      Detail::Collect(sub->GetDirectory(std::string(leaf).c_str()), std::format("{}/{}/", name, leaf), sec, groups);
  }
  for (auto& [key, g] : groups)
    sec.groups.push_back(std::move(g));
  return sec;
}

// Every scheme directory with a section below it. A scheme is recognised by holding an
// Extract1D or Extract2D directory, not by its name, which is the configuration's.
inline Catalog Build(const std::string& path)
{
  Catalog cat;
  cat.file = path;
  std::unique_ptr<TFile> f = RootIO::OpenOrThrow(path, "READ", "PlotCatalog");
  for (const auto& [scheme, cls] : Detail::Keys(f.get())) {
    if (!Detail::IsDirectory(cls))
      continue;
    TDirectory* schemeDir = f->GetDirectory(scheme.c_str());
    for (const std::string_view section : {OutputNames::kExtract1D, OutputNames::kExtract2D})
      if (schemeDir->GetDirectory(std::string(section).c_str()))
        cat.sections.push_back(DescribeSection(schemeDir, scheme, std::string(section)));
  }
  for (const Section& s : cat.sections)
    if (!s.unrecognised.empty())
      Logger::Warn("PlotCatalog: some histograms in '{}' have names OutputNames does not recognise; they are listed, "
                   "not plotted (written by an older producer, or a new kind of object nobody added to OutputNames)",
                   path);
  return cat;
}

// "0-1, 1-5, ..." from a list of edges, or the indices when there is no stamp to say what
// they mean.
inline std::string Intervals(const std::vector<double>& edges, const std::set<int>& bins)
{
  std::string out;
  for (int b : bins) {
    if (!out.empty())
      out += ", ";
    if (b + 1 < static_cast<int>(edges.size()))
      out += BinningUtils::FormatEdge(edges[b]) + "-" + BinningUtils::FormatEdge(edges[b + 1]);
    else
      out += std::format("bin {} (no stamp)", b);
  }
  return out;
}

inline std::string Edges(const std::vector<double>& edges)
{
  if (edges.empty())
    return "no stamp";
  std::string out;
  for (double e : edges)
    out += (out.empty() ? "" : " ") + BinningUtils::FormatEdge(e);
  return out;
}

inline std::string Dys(const std::set<double>& dys)
{
  std::string out;
  for (double d : dys)
    out += (out.empty() ? "" : ", ") + BinningUtils::FormatDeltaY(d).first;
  return out;
}

inline std::string Text(const Catalog& cat)
{
  std::string out = std::format("{}\n", cat.file);
  if (cat.sections.empty())
    out += "  no <binning_name>/Extract1D or Extract2D directory: written before that layout existed, or not a PhiAssocSpectra file.\n";
  for (const Section& s : cat.sections) {
    const auto prov = [&](const char* key) { auto it = s.provenance.find(key); return it == s.provenance.end() ? std::string("?") : it->second; };
    out += std::format("\n  scheme '{}', section {}   (produced {}, commit {})\n", s.scheme, s.section, prov("produced_at"), prov("git_commit").substr(0, 10));
    out += std::format("    multiplicity edges: {}\n", Edges(s.mult));
    out += std::format("    trigger pT edges:   {}\n", Edges(s.ptPhi));
    for (const auto& [species, edges] : s.ptAssoc)
      out += std::format("    {} pT edges: {}\n", species, Edges(edges));
    for (const Group& g : s.groups) {
      const auto& info = OutputNames::Info(g.kind);
      const std::string who = info.speciesPair ? std::format("{}/{}", g.species, g.speciesDen) : g.species;
      out += std::format("    {:<28} {:<8} |dy| < {}", info.name, who, Dys(g.dy));
      if (info.perMultBin)
        out += std::format("   mult {}", Intervals(s.mult, g.multBins));
      out += "\n";
    }
    if (!s.unrecognised.empty()) {
      out += std::format("    not recognised ({}):", s.unrecognised.size());
      for (const std::string& n : s.unrecognised)
        out += " " + n;
      out += "\n";
    }
  }
  return out;
}

// The same as JSON: what a front end hands to a language model as the list of what exists.
inline std::string Json(const std::vector<Catalog>& cats)
{
  rapidjson::Document doc(rapidjson::kArrayType);
  auto& a = doc.GetAllocator();
  const auto str = [&](const std::string& s) { return rapidjson::Value(s.c_str(), a); };
  const auto nums = [&](const auto& xs) { rapidjson::Value v(rapidjson::kArrayType); for (double x : xs) v.PushBack(x, a); return v; };

  for (const Catalog& cat : cats) {
    rapidjson::Value file(rapidjson::kObjectType);
    file.AddMember("file", str(cat.file), a);
    rapidjson::Value sections(rapidjson::kArrayType);
    for (const Section& s : cat.sections) {
      rapidjson::Value sec(rapidjson::kObjectType);
      sec.AddMember("binning_name", str(s.scheme), a);
      sec.AddMember("section", str(s.section), a);
      sec.AddMember("multiplicity_edges", nums(s.mult), a);
      sec.AddMember("trigger_pt_edges", nums(s.ptPhi), a);
      rapidjson::Value pt(rapidjson::kObjectType);
      for (const auto& [species, edges] : s.ptAssoc)
        pt.AddMember(str(species), nums(edges), a);
      sec.AddMember("assoc_pt_edges", pt, a);
      for (const char* key : {"produced_at", "git_commit"})
        if (auto it = s.provenance.find(key); it != s.provenance.end())
          sec.AddMember(rapidjson::StringRef(key), str(it->second), a);
      rapidjson::Value objects(rapidjson::kArrayType);
      for (const Group& g : s.groups) {
        const auto& info = OutputNames::Info(g.kind);
        rapidjson::Value o(rapidjson::kObjectType);
        o.AddMember("kind", rapidjson::StringRef(info.name.data(), info.name.size()), a);
        o.AddMember("species", str(g.species), a);
        if (info.speciesPair)
          o.AddMember("species_den", str(g.speciesDen), a);
        o.AddMember("dy", nums(g.dy), a);
        if (info.perMultBin) {
          rapidjson::Value mult(rapidjson::kArrayType);
          for (int b : g.multBins)
            if (b + 1 < static_cast<int>(s.mult.size())) {
              rapidjson::Value iv(rapidjson::kArrayType);
              iv.PushBack(s.mult[b], a).PushBack(s.mult[b + 1], a);
              mult.PushBack(iv, a);
            }
          o.AddMember("mult", mult, a);
        }
        objects.PushBack(o, a);
      }
      sec.AddMember("objects", objects, a);
      sections.PushBack(sec, a);
    }
    file.AddMember("sections", sections, a);
    doc.PushBack(file, a);
  }
  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
  writer.SetIndent(' ', 2);
  doc.Accept(writer);
  return buffer.GetString();
}

} // namespace PlotCatalog
