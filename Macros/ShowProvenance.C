// =============================================================================
// ShowProvenance.C  --  what produced the files in a directory
//
// The framework records, inside every output file it writes, the configuration
// block that produced it: see RootIO::WriteProvenance. That record lives in the
// file on purpose - it survives a copy, an upload to CCDB, a mail to a colleague,
// all the moments in which a JSON left alongside would have been lost.
//
// The price is that it is not greppable. This macro pays it back by DERIVING the
// grep-able view from the files themselves, on demand. It is not a second source
// of truth: delete its output and nothing is lost, run it again and it is back.
// A JSON written by the framework in parallel with the file would be the opposite
// - a copy that can silently disagree.
//
//   root -l 'ShowProvenance.C("../DataFile/pp/DeltaY/MC")'          // table
//   root -l 'ShowProvenance.C("../DataFile/pp/DeltaY/MC", "json")'  // machine
//   root -l 'ShowProvenance.C("Corrections.root", "full")'          // one file
//
// Depends on nothing but ROOT, like CompareAxes.C: it must stay usable on files
// the framework itself would refuse.
// =============================================================================

#include "TDirectory.h"
#include "TFile.h"
#include "TKey.h"
#include "TList.h"
#include "TNamed.h"
#include "TSystemDirectory.h"
#include "TSystemFile.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace
{

// The provenance sits one level below the binning scheme, whose name is not known
// in advance and may differ between files. Rather than guessing it, walk the top
// level and take whatever contains a "Provenance" directory: the layout is
// {schemeName}/Provenance, and the scheme name is exactly what we want to report.
std::map<std::string, std::map<std::string, std::string>> ReadAllProvenance(TFile* file)
{
  std::map<std::string, std::map<std::string, std::string>> perScheme;
  if (!file || !file->GetListOfKeys())
    return perScheme;

  for (TObject* obj : *file->GetListOfKeys()) {
    auto* key = static_cast<TKey*>(obj);
    TDirectory* scheme = file->GetDirectory(key->GetName());
    if (!scheme)
      continue;

    TDirectory* prov = scheme->GetDirectory("Provenance");
    if (!prov || !prov->GetListOfKeys())
      continue;

    std::map<std::string, std::string> facts;
    for (TObject* provObj : *prov->GetListOfKeys()) {
      auto* provKey = static_cast<TKey*>(provObj);
      TObject* entry = provKey->ReadObj();
      if (auto* named = dynamic_cast<TNamed*>(entry))
        facts[named->GetName()] = named->GetTitle();
      delete entry;
    }

    if (!facts.empty())
      perScheme[key->GetName()] = facts;
  }
  return perScheme;
}

// Pulls one value out of the serialised configuration block without parsing JSON:
// enough for the summary table, and it keeps this macro free of any dependency.
// Returns an empty string when the key is absent, which the caller renders as "-".
std::string PeekJsonString(const std::string& json, const std::string& key)
{
  const std::string needle = "\"" + key + "\":\"";
  const size_t start = json.find(needle);
  if (start == std::string::npos)
    return "";

  const size_t valueStart = start + needle.size();
  const size_t valueEnd = json.find('"', valueStart);
  if (valueEnd == std::string::npos)
    return "";

  return json.substr(valueStart, valueEnd - valueStart);
}

std::string BaseName(const std::string& path)
{
  const size_t slash = path.find_last_of('/');
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::vector<std::string> CollectRootFiles(const std::string& target)
{
  if (target.size() > 5 && target.substr(target.size() - 5) == ".root")
    return {target};

  std::vector<std::string> files;
  TSystemDirectory dir(target.c_str(), target.c_str());
  TList* entries = dir.GetListOfFiles();
  if (!entries) {
    std::cerr << "[ERROR] ShowProvenance: cannot list '" << target << "'" << std::endl;
    return files;
  }

  for (TObject* obj : *entries) {
    auto* entry = static_cast<TSystemFile*>(obj);
    std::string name = entry->GetName();
    if (entry->IsDirectory() || name.size() < 6 || name.substr(name.size() - 5) != ".root")
      continue;
    files.push_back(target + "/" + name);
  }

  delete entries;
  std::sort(files.begin(), files.end());
  return files;
}

} // namespace

// mode: "table" (default), "full", "json"
void ShowProvenance(const std::string& target, const std::string& mode = "table")
{
  const std::vector<std::string> files = CollectRootFiles(target);
  if (files.empty()) {
    std::cout << "No ROOT file found in '" << target << "'." << std::endl;
    return;
  }

  if (mode == "json")
    std::cout << "[" << std::endl;

  bool firstJsonEntry = true;
  int withProvenance = 0;

  if (mode == "table") {
    printf("%-46s %-24s %-22s %s\n", "file", "produced by", "at", "scheme");
    printf("%-46s %-24s %-22s %s\n", "----", "-----------", "--", "------");
  }

  for (const std::string& path : files) {
    std::unique_ptr<TFile> file(TFile::Open(path.c_str(), "READ"));
    if (!file || file->IsZombie()) {
      std::cerr << "[WARNING] cannot open '" << path << "'" << std::endl;
      continue;
    }

    const auto perScheme = ReadAllProvenance(file.get());
    if (perScheme.empty()) {
      if (mode == "table")
        printf("%-46s %-24s %-22s %s\n", BaseName(path).c_str(), "(none recorded)", "-", "-");
      continue;
    }
    ++withProvenance;

    for (const auto& [scheme, facts] : perScheme) {
      const std::string configBlock = facts.count("config_block") ? facts.at("config_block") : "";
      const std::string producedBy = PeekJsonString(configBlock, "_resolved_block_name");
      const std::string producedAt = facts.count("produced_at") ? facts.at("produced_at") : "";

      if (mode == "table") {
        printf("%-46s %-24s %-22s %s\n", BaseName(path).c_str(),
               (producedBy.empty() ? "-" : producedBy).c_str(),
               (producedAt.empty() ? "-" : producedAt).c_str(), scheme.c_str());
      } else if (mode == "full") {
        std::cout << "=== " << path << "  [" << scheme << "]" << std::endl;
        for (const auto& [key, value] : facts)
          std::cout << "  " << key << " = " << value << std::endl;
        std::cout << std::endl;
      } else if (mode == "json") {
        if (!firstJsonEntry)
          std::cout << "," << std::endl;
        firstJsonEntry = false;
        std::cout << "  {\"file\":\"" << path << "\",\"scheme\":\"" << scheme
                  << "\",\"produced_at\":\"" << producedAt
                  << "\",\"config_block\":" << (configBlock.empty() ? "null" : configBlock) << "}";
      }
    }
  }

  if (mode == "json") {
    std::cout << std::endl
              << "]" << std::endl;
    return;
  }

  std::cout << std::endl
            << withProvenance << " of " << files.size() << " file(s) carry provenance." << std::endl;

  if (withProvenance < static_cast<int>(files.size())) {
    std::cout << "The others were written before it existed, or by something else. "
                 "Re-running the task that produces them adds it." << std::endl;
  }
}
