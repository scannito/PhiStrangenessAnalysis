#pragma once

// The configuration a Hyperloop train ran with, as Macros/downloadHYOutput.py saves it next to each data file, and the
// facts the framework reads from it (DESIGN_NOTES.md section 12).
//
// The record of DataFile/.../X.root is DataFile/.../X.train.json: next to the file, so that it goes wherever the file
// is copied, and found by one rule in both languages (RecordPath here, config_path_for in the script). It holds the
// configuration of every wagon of the train as it was when the train was composed:
//   {"hyperloop": {train_id, wagons_timestamp, dataset, package_tag, source},
//    "data_file": {name, downloaded_with_this_record, runs},
//    "wagons": {"<wagon id>": {"<DPL device>": {"<option>": value}}}}
// A file summed from several downloads (hadd) has a record that names it and lists the records of its parts instead,
// written by downloadHYOutput.py from a 'sum' entry of a batch file:
//   {"data_file": {"name": "<the summed file>", "summed_with_this_record": <bool>},
//    "merged_from": ["<part>.train.json", ...]}   (paths relative to it)
// summed_with_this_record is false for a sum made before its record, by hand: nothing then shows that it holds these
// parts. The options read must be the same in all the parts: a sum of trains that ran differently is not one sample.
// And the parts must not share events, which hadd would add twice (Detail::RequireDisjointParts).
//
// The options are written as DPL writes them, and read with JsonConfig::Dpl. An option missing from a record that
// exists means O2Physics renamed it: it stops the run, since a guess would be the silent failure this file is for.

#include "AnalysisStructures.h"
#include "IONames.h"
#include "JsonConfigHelpers.h"
#include "Logger.h"
#include "RootIOHelpers.h"

#include <TFile.h>
#include <TKey.h>

#include "rapidjson/document.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace TrainConfig
{
// One train's record, as read from its file.
struct Record {
  std::string path;
  rapidjson::Document doc;
};

// The wagons of a record, {wagon id: {device: options}}. Load requires them once, so that here it cannot fail.
inline const rapidjson::Value& Wagons(const Record& record)
{
  return JsonConfig::RequireObject(record.doc, "wagons", record.path);
}

// The device that wrote a directory of the data file, in one record: which wagon of the train it belongs to, and its
// options. 'options' points into the record's document, found and checked once by Devices: the documents no longer
// change or move by then, so it is a resolved reference, not a second copy of a fact.
struct Device {
  const Record* record;
  std::string wagonId;
  std::string name;
  const rapidjson::Value* options;
};

// The species of phiStrangeCorrelation's activeCorrelationTypes, in the framework's names, which are the task's
// (AssocParticleLabels): the order of its elements where it is a std::array, the names it lists where it is a list.
inline const auto kAssocSpecies = std::to_array<std::string>({"K0S", "Lambda", "AntiLambda", "Xi", "Omega", "Pi"});

// "<record>: wagon <id>, device '<name>'", for the messages.
inline std::string Source(const Device& device)
{
  return std::format("{}: wagon {}, device '{}'", device.record->path, device.wagonId, device.name);
}

// "Phi, Pi", or "none", for the messages.
inline std::string Names(const std::set<std::string>& names) { return names.empty() ? "none" : Logger::Join(names); }

// Where the record of a data file is: next to it, its extension replaced. The same rule as config_path_for in
// Macros/downloadHYOutput.py.
inline std::string RecordPath(const std::string& dataFile)
{
  return std::filesystem::path(dataFile).replace_extension(".train.json").string();
}

namespace Detail
{
// A directory of AnalysisResults.root, or the first component of a base path, split into the device and the wagon id
// that Hyperloop appends as _id<wagon id> when a train has the device in several wagons: "phi-strange-correlation_id66810"
// gives {"phi-strange-correlation", "66810"}; without the suffix the wagon is empty.
inline std::pair<std::string, std::optional<std::string>> SplitWagonId(const std::string& directory)
{
  static const std::regex withWagonId(R"(^(.+)_id(\d+)$)");
  std::smatch match;
  if (std::regex_match(directory, match, withWagonId))
    return {match[1].str(), match[2].str()};
  return {directory, std::nullopt};
}

// The block of one wagon in a record, or nullptr.
inline const rapidjson::Value* FindWagon(const Record& record, const std::string& wagonId)
{
  const rapidjson::Value* wagonBlock = JsonConfig::OptionalMember(Wagons(record), wagonId.c_str());
  return wagonBlock && wagonBlock->IsObject() ? wagonBlock : nullptr;
}

// Every directory of the data file named after a wagon (_id<wagon id>) must be a wagon of every record. For one train,
// a record next to a file of another train fails here, unless both ran the same wagons; the name check of Load covers
// that case. For a sum it is what makes the sum one: hadd adds directories by name, so a wagon missing from one part
// leaves its directory holding the other parts only. Skipped, and said, when the data file is not there, which the
// projection cache allows; a file that is there but cannot be opened stops the run.
inline void RequireWagonsKnown(const std::string& dataFile, const std::vector<Record>& records, std::string_view errCtx)
{
  if (!std::filesystem::exists(dataFile)) {
    Logger::Info("{}: '{}' is not here, its wagons are not compared with its train configuration.", errCtx, dataFile);
    return;
  }
  const std::unique_ptr<TFile> file = RootIO::OpenOrThrow(dataFile, "READ", errCtx);
  for (auto* object : *file->GetListOfKeys()) {
    const auto [device, wagonId] = SplitWagonId(object->GetName());
    if (!wagonId)
      continue;
    for (const auto& record : records) {
      if (!FindWagon(record, *wagonId)) {
        throw std::runtime_error(std::format(
          "[FATAL] {}: '{}' has the directory '{}', of wagon {}, which the train configuration '{}' does not list: the "
          "record is not of this file's train{}.",
          errCtx, dataFile, object->GetName(), *wagonId, record.path,
          records.size() > 1 ? ", or the parts of the sum did not run the same wagons" : ""));
      }
    }
  }
}

// What a record says of its data file ({name, ...}), which every record has
inline const rapidjson::Value& DataFileBlock(const Record& record)
{
  return JsonConfig::RequireObject(record.doc, "data_file", record.path);
}

// A record names the data file it belongs to, a sum's and each part's alike: one copied or renamed next to another file
// would stand for a file it does not describe.
inline void RequireNamesItsFile(const Record& record, std::string_view errCtx)
{
  const std::string name = JsonConfig::RequireString(DataFileBlock(record), "name", record.path);
  if (RecordPath(name) != std::filesystem::path(record.path).filename().string()) {
    throw std::runtime_error(std::format(
      "[FATAL] {}: '{}' is the train configuration of '{}', not of the file next to it. Download that file's again "
      "(Macros/downloadHYOutput.py, overwrite), or for a sum make it again from its batch entry.",
      errCtx, record.path, name));
  }
}

// The runs a record's data file holds, or nothing when they are not known: a record made for a file already on disk
// has runs null (downloadHYOutput.py).
inline std::optional<std::set<int>> RecordedRuns(const Record& record)
{
  const rapidjson::Value& runs = JsonConfig::RequireMember(DataFileBlock(record), "runs", record.path);
  if (runs.IsNull())
    return std::nullopt;
  std::set<int> recorded;
  int index = 0;
  for (const auto& run : JsonConfig::ToArray(runs, "data_file.runs", record.path))
    recorded.insert(static_cast<int>(JsonConfig::ToNumber(run, std::format("data_file.runs[{}]", index++), record.path)));
  return recorded;
}

// The dataset a record's train ran on, or nothing when Hyperloop did not give it (recorded as null)
inline std::optional<std::string> RecordedDataset(const Record& record)
{
  const rapidjson::Value& dataset =
    JsonConfig::RequireMember(JsonConfig::RequireObject(record.doc, "hyperloop", record.path), "dataset", record.path);
  if (dataset.IsNull())
    return std::nullopt;
  return JsonConfig::ToString(dataset, "hyperloop.dataset", record.path);
}

// The parts of a sum must not hold the same events: hadd adds them, and a run in two parts is counted twice with no
// sign of it. Compared by the runs the records list. A run in two parts of one dataset is the same events: it stops
// the run. A run in two parts of different datasets (or of a dataset not recorded) may or may not be: two passes of
// one period of data share their events, two MC productions anchored to the same runs do not, and the record does not
// say which is which; that is a warning. So is a part whose record does not list its runs (downloaded before its
// record): it is not compared at all.
inline void RequireDisjointParts(const std::vector<Record>& parts, const std::string& sumRecord, std::string_view errCtx)
{
  std::vector<std::string> notListed;
  std::set<int> acrossDatasets;
  std::map<int, std::vector<const Record*>> partsOfRun;
  for (const auto& part : parts) {
    const std::optional<std::set<int>> runs = RecordedRuns(part);
    if (!runs) {
      notListed.push_back(part.path);
      continue;
    }
    const std::optional<std::string> dataset = RecordedDataset(part);
    for (int run : *runs) {
      for (const Record* other : partsOfRun[run]) {
        const std::optional<std::string> otherDataset = RecordedDataset(*other);
        if (dataset && otherDataset && *dataset == *otherDataset) {
          throw std::runtime_error(std::format(
            "[FATAL] {}: run {} of {} is in two parts of the sum '{}', '{}' and '{}': hadd counted its events twice. "
            "Make the sum again from parts that do not share runs.",
            errCtx, run, *dataset, sumRecord, other->path, part.path));
        }
        acrossDatasets.insert(run);
      }
      partsOfRun[run].push_back(&part);
    }
  }
  if (!acrossDatasets.empty()) {
    Logger::Warn("{}: parts of different datasets of the sum '{}' share runs ({}): counted twice if they are two passes "
                 "of the same data, not if they are MC productions anchored to the same runs.",
                 errCtx, sumRecord, Logger::Join(acrossDatasets));
  }
  if (!notListed.empty()) {
    Logger::Warn("{}: that the parts of the sum '{}' do not share runs is not checked, as these do not list their "
                 "runs: {}. Downloading a part again with overwrite records them.",
                 errCtx, sumRecord, Logger::Join(notListed));
  }
}
} // namespace Detail

// The records of the train(s) a data file comes from: one, or one per part of a summed file. Empty when the file has
// no record, with a warning: the productions made before 12 November 2026 have none until downloadHYOutput.py is run on
// their batch again. A record that is unreadable, names another data file, or does not list a wagon the file has stops
// the run; for a sum, so does a part's record that names another file, a part that is itself a sum, or parts of one
// dataset sharing a run (Detail::RequireDisjointParts). A sum made before its record is a warning.
inline std::vector<Record> Load(const std::string& dataFile, std::string_view errCtx)
{
  const std::string path = RecordPath(dataFile);
  if (!std::filesystem::exists(path)) {
    Logger::Warn(
      "{}: no train configuration for '{}' (expected '{}'): how its train ran is not checked. "
      "Macros/downloadHYOutput.py saves it.",
      errCtx, dataFile, path);
    return {};
  }

  std::vector<Record> records(1);
  records.front().path = path;
  rapidjson::Document& own = records.front().doc;
  JsonConfig::LoadDocument(path, own, errCtx);

  Detail::RequireNamesItsFile(records.front(), errCtx);

  if (const rapidjson::Value* merged = JsonConfig::OptionalMember(own, "merged_from")) {
    if (!JsonConfig::RequireBool(Detail::DataFileBlock(records.front()), "summed_with_this_record", path)) {
      Logger::Warn("{}: '{}' was summed before its record was written: that it holds the parts the record lists is not "
                   "shown. Making it again from its batch entry with overwrite (Macros/downloadHYOutput.py) shows it.",
                   errCtx, dataFile);
    }
    const std::filesystem::path directory = std::filesystem::path(path).parent_path();
    std::vector<Record> parts;
    int index = 0;
    for (const auto& entry : JsonConfig::ToArray(*merged, "merged_from", path)) {
      Record& part = parts.emplace_back();
      part.path = (directory / JsonConfig::ToString(entry, std::format("merged_from[{}]", index++), path)).string();
      JsonConfig::LoadDocument(part.path, part.doc, errCtx);
      if (JsonConfig::OptionalMember(part.doc, "merged_from")) {
        throw std::runtime_error(std::format("[FATAL] {}: '{}' is itself a sum: list its parts in '{}' directly.",
                                             errCtx, part.path, path));
      }
      Detail::RequireNamesItsFile(part, errCtx);
    }
    if (parts.empty())
      throw std::runtime_error(std::format("[FATAL] {}: '{}' has an empty 'merged_from'.", errCtx, path));
    Detail::RequireDisjointParts(parts, path, errCtx);
    records = std::move(parts);
  }

  for (const auto& record : records)
    Wagons(record); // required once here, so that the later calls cannot fail
  Detail::RequireWagonsKnown(dataFile, records, errCtx);
  return records;
}

namespace Detail
{
// The device 'name' of a record, in whichever wagon has it: a producer is in every wagon that depends on it, with the
// same options, since the train runs it once. 'from' says in the message what asked for it.
inline Device FindInAnyWagon(const Record& record, const std::string& name, std::string_view from, std::string_view errCtx)
{
  std::optional<Device> found;
  for (const auto& entry : Wagons(record).GetObject()) {
    const rapidjson::Value* options = entry.value.IsObject() ? JsonConfig::OptionalMember(entry.value, name.c_str()) : nullptr;
    if (!options || !options->IsObject())
      continue;
    if (found && *found->options != *options) {
      throw std::runtime_error(std::format(
        "[FATAL] {}: '{}' has the device '{}' in wagons {} and {} with different options, while one train runs it once "
        "(a device in several wagons is a producer, and a task run twice is named with _id<wagon id>): the record is "
        "inconsistent. Download it again (Macros/downloadHYOutput.py, overwrite).",
        errCtx, record.path, name, found->wagonId, entry.name.GetString()));
    }
    if (!found) {
      found = Device{&record, entry.name.GetString(), name, options};
    }
  }
  if (!found) {
    throw std::runtime_error(std::format("[FATAL] {}: '{}' has no device '{}' in any wagon (from '{}').",
                                         errCtx, record.path, name, from));
  }
  return *found;
}
} // namespace Detail

// The device that wrote 'basePath' (base_path_data, base_path_me or a hist_name, as the task block writes it), in every
// record: each record holds its wagons' options at the date of its own train, and in a sum they may differ. The first
// component of the path names the device. With a _id<wagon id> suffix, the device of that wagon: Hyperloop adds it to
// an analysis task that runs in several wagons of one train. Without it, the device is in one wagon, or, a producer, in
// every wagon that depends on it: Hyperloop returns each wagon's configuration with its dependencies, and in the train
// the producer is one device (a table cannot be produced twice), so its options are the same in all of them.
inline std::vector<Device> Devices(const std::vector<Record>& records, std::string_view basePath, std::string_view errCtx)
{
  const auto [name, wagonId] = Detail::SplitWagonId(std::string(basePath.substr(0, basePath.find('/'))));

  std::vector<Device> devices;
  for (const auto& record : records) {
    if (wagonId) {
      const rapidjson::Value* wagonBlock = Detail::FindWagon(record, *wagonId);
      const rapidjson::Value* options = wagonBlock ? JsonConfig::OptionalMember(*wagonBlock, name.c_str()) : nullptr;
      if (!options || !options->IsObject()) {
        throw std::runtime_error(std::format("[FATAL] {}: '{}' has no device '{}' in wagon {} (from '{}').",
                                             errCtx, record.path, name, *wagonId, basePath));
      }
      devices.push_back({&record, *wagonId, name, options});
      continue;
    }
    devices.push_back(Detail::FindInAnyWagon(record, name, basePath, errCtx));
  }
  return devices;
}

// The particles whose efficiency phiStrangeCorrelation applied online, as a weight on the entries this device filled:
// the phi, the trigger, with applyPhiEfficiency; the associated species switched on in activeCorrelationTypes. Both
// only with applyEfficiency, without which the maps are not even loaded (phiStrangeCorrelation.cxx, init).
inline std::set<std::string> ParticlesWeightedOnline(const Device& device)
{
  const rapidjson::Value& options = *device.options;
  const std::string where = Source(device);
  std::set<std::string> weighted;
  if (!JsonConfig::Dpl::RequireBool(options, "applyEfficiency", where))
    return weighted;
  if (JsonConfig::Dpl::RequireBool(options, "applyPhiEfficiency", where)) {
    weighted.insert("Phi");
  }
  weighted.merge(JsonConfig::Dpl::RequireSelected(options, "activeCorrelationTypes", kAssocSpecies, where));
  return weighted;
}

// TEMPORARY, until every production read comes from the O2Physics task that no longer adds it (propagateEffError
// removed, O2Physics commit 15646e3d1 on the owner's branch phistrange-ccdb-ownership): whether this device added the
// uncertainty of the efficiency maps to the bin errors, entry by entry. Only where the option exists, and only with
// applyEfficiency on, since without the maps there is nothing added.
inline bool MapErrorAddedPerEntry(const Device& device)
{
  const std::string where = Source(device);
  const rapidjson::Value* option = JsonConfig::OptionalMember(*device.options, "propagateEffError");
  return option && JsonConfig::Dpl::RequireBool(*device.options, "applyEfficiency", where) &&
         JsonConfig::Dpl::ToBool(*option, "propagateEffError", where);
}

// A value read from every device, the same for all of them, with the first device as its source.
template <class T>
struct Sourced {
  T value;
  std::string source;
};

// The value 'valueOf' reads from every device, required the same for all of them: the same event and the mixed event,
// and the trains of a sum, are read as one sample only if they agree. The run stops naming two that differ, with 'what'
// and 'describe' saying what. Empty when there are no devices: not known, never a default.
// The value's type T is not written anywhere. The caller passes only the callable; the compiler deduces ValueOf from it
// and takes T as what calling it with a Device gives, T = decltype(valueOf(device)), which is what
// std::invoke_result_t<ValueOf, const Device&> says; the result is std::optional<Sourced<T>>. So one function serves a
// set of particles (WeightedOnline), a mass window (CheckOnlineMassWindows) or any later fact, a double for a PID
// threshold included, without a line changed here. T must support != and copying, and describe(T) must give something
// std::format can write; a T that does not fails to compile, not at run time. A template and not a std::function for
// the same reason: a std::function would fix the return type in advance and hide the callable behind type erasure.
template <class ValueOf, class Describe>
auto SameOnAllDevices(const std::vector<Device>& devices, ValueOf valueOf, Describe describe, std::string_view what,
                      std::string_view errCtx) -> std::optional<Sourced<std::invoke_result_t<ValueOf, const Device&>>>
{
  if (devices.empty()) {
    return std::nullopt;
  }
  const auto common = valueOf(devices.front());
  for (const auto& device : devices) {
    if (const auto value = valueOf(device); value != common) {
      throw std::runtime_error(std::format(
        "[FATAL] {}: the inputs differ in {}: {} by {}, {} by {}. The same event and the mixed event, and every train "
        "of a sum, must agree.",
        errCtx, what, describe(common), Source(devices.front()), describe(value), Source(device)));
    }
  }
  return Sourced<std::invoke_result_t<ValueOf, const Device&>>{common, Source(devices.front())};
}

// The particles, among 'of', that the devices weighted online, the same for all of them. Only 'of' is compared, so
// devices that differ in a particle nobody reads pass.
inline std::optional<Sourced<std::set<std::string>>> WeightedOnline(const std::vector<Device>& devices,
                                                                    const std::set<std::string>& of, std::string_view errCtx)
{
  const auto weightedOf = [&](const Device& device) {
    std::set<std::string> weighted;
    std::ranges::set_intersection(ParticlesWeightedOnline(device), of, std::inserter(weighted, weighted.end()));
    return weighted;
  };
  return SameOnAllDevices(devices, weightedOf, Names, "the particles weighted online", errCtx);
}

// phiStrangeCorrelation's AnalysisMode kDeltaYvsDeltaPhi: the only mode that fills the DeltaY-DeltaPhi containers the
// correlation tasks read, and the only one that applies the mass windows below.
inline constexpr int kDeltaYvsDeltaPhiMode = 2;

// The device ran in the DeltaY vs DeltaPhi mode, or the run stops: in the other modes phiStrangeCorrelation books the
// containers the correlation tasks read but leaves them empty.
inline void RequireDeltaYvsDeltaPhi(const Device& device)
{
  const std::string where = Source(device);
  const double mode = JsonConfig::ToNumber(JsonConfig::RequireMember(*device.options, "analysisMode", where), "analysisMode", where);
  if (mode != kDeltaYvsDeltaPhiMode) {
    throw std::runtime_error(std::format(
      "[FATAL] {}: analysisMode is {}, not {} (DeltaY vs DeltaPhi): the containers read are filled only in that mode.",
      where, mode, kDeltaYvsDeltaPhiMode));
  }
}

// phiStrangeCorrelation's isAssocValid, per associated species in the framework's names: the switch that turns its mass
// window on, and the window. Lambda and AntiLambda share theirs. The pion is cut in n sigma (pidTPCMax, pidTOFMax), not
// in mass, so it is not here.
struct MassWindowOptions {
  std::string species;
  std::string select;
  std::string range;
};

inline const std::array<MassWindowOptions, 5> kMassWindowOptions{{
  {"K0S", "selectK0sInSigRegion", "rangeMK0sSignal"},
  {"Lambda", "selectLambdaInSigRegion", "rangeMLambdaSignal"},
  {"AntiLambda", "selectLambdaInSigRegion", "rangeMLambdaSignal"},
  {"Xi", "selectXiInSigRegion", "rangeMXiSignal"},
  {"Omega", "selectOmegaInSigRegion", "rangeMOmegaSignal"},
}};

// The mass-window options of a species, or nullptr when it is not cut in mass online (the pion).
inline const MassWindowOptions* FindMassWindowOptions(std::string_view species)
{
  const auto options = std::ranges::find(kMassWindowOptions, species, &MassWindowOptions::species);
  return options == kMassWindowOptions.end() ? nullptr : &*options;
}

// A mass window option of the device, "key.first" and "key.second" in DPL's spelling
inline AnalysisStructures::Range RequireWindow(const Device& device, std::string_view key)
{
  const auto [low, high] = JsonConfig::Dpl::RequirePair(*device.options, key, Source(device));
  return {low, high};
}

// The mass window this device kept a species in, in the DeltaY vs DeltaPhi mode (RequireDeltaYvsDeltaPhi): empty when
// its select switch is off, and every mass the producer passed was then counted.
inline std::optional<AnalysisStructures::Range> OnlineMassWindow(const Device& device, const MassWindowOptions& options)
{
  if (!JsonConfig::Dpl::RequireBool(*device.options, options.select, Source(device))) {
    return std::nullopt;
  }
  return RequireWindow(device, options.range);
}

// "[0.475, 0.515]", or that no window was applied, for the messages.
inline std::string DescribeMassWindow(const std::optional<AnalysisStructures::Range>& window)
{
  return window ? std::format("[{}, {}]", window->min, window->max) : "none (every mass counted)";
}

// The phi mass windows of the correlation device: rangeMPhiSignal puts a pair into the signal containers, rangeMPhiSideband
// into the sideband ones (phiStrangeCorrelation, fillPhiAssocCorrelation). Always applied, with no switch: unlike the
// windows of the associated species, they cannot be off. Together, so that the devices are compared once.
struct PhiMassWindows {
  AnalysisStructures::Range signal;
  AnalysisStructures::Range sideband;
  bool operator==(const PhiMassWindows&) const = default; // exact, as Range's, for SameOnAllDevices
};
inline PhiMassWindows OnlinePhiMassWindows(const Device& device)
{
  return {RequireWindow(device, "rangeMPhiSignal"), RequireWindow(device, "rangeMPhiSideband")};
}
inline std::string DescribePhiMassWindows(const PhiMassWindows& windows)
{
  return std::format("signal {}, sideband {}", DescribeMassWindow(windows.signal), DescribeMassWindow(windows.sideband));
}

// The selection of the associated pion, as the options of a train make it (DESIGN_NOTES.md section 13, "The π"): the
// n sigma window phiStrangeCorrelation counts it in, and the TOF rule of pion-track-producer, whose tracks it reads.
// The facts, not their options: the producer's own pidTPCMax and pidTOFMax are arrays of its K/p rejection, not the
// window, and the rule is told by the name of the threshold's option (O2Physics renamed it when its meaning changed).
// A correction map holds the selection of the device that filled it, so mc_task stamps it next to the pion map and the
// correlation tasks compare it with that of their pairs (CorrelationTaskBase::CheckPionMapSelection).
struct PionSelection {
  // |nσ_TPC| < tpcMax, and |nσ_TOF| < tofMax where the TOF enters the PID; absent when selectPionInSigRegion is off,
  // and every pion the producer passed is counted.
  struct Window {
    double tpcMax;
    double tofMax;
    bool operator==(const Window&) const = default;
  };
  std::optional<Window> window;
  bool tofRequired;      // cfgIsTOFChecked: tracks without TOF rejected from minPtTOF up
  // The producer's threshold. Compared even where it acts on nothing (the old rule with the TOF not required, in no
  // train so far): a stop that was not needed, never a number let through.
  double minPtTOF;
  bool tofInPIDFromMinPt; // the rule of minPtTOFInPID: the TOF enters the PID (window, K/p rejection) from minPtTOF up
                          // and never below. Otherwise that of tofPIDThreshold: wherever the track has a TOF signal
  // Exact, as Range's: every value is read from the DPL text of a record by the same function, or written from one
  // as a double and read back unchanged.
  bool operator==(const PionSelection&) const = default;
};

inline const std::string kPionProducer{"pion-track-producer"};

// The selection of the pion counted by 'correlation', a phiStrangeCorrelation device, with the producer of its train.
// The device must have run in the DeltaY vs DeltaPhi mode, the only one in which isAssocValid applies the window: in
// another, a window switched on would be absent from what it counted, and the facts would say it is there.
inline PionSelection OnlinePionSelection(const Device& correlation, std::string_view errCtx)
{
  RequireDeltaYvsDeltaPhi(correlation);
  const rapidjson::Value& options = *correlation.options;
  const std::string where = Source(correlation);
  PionSelection selection{};
  if (JsonConfig::Dpl::RequireBool(options, "selectPionInSigRegion", where)) {
    selection.window = PionSelection::Window{
      JsonConfig::ToNumber(JsonConfig::RequireMember(options, "pidTPCMax", where), "pidTPCMax", where),
      JsonConfig::ToNumber(JsonConfig::RequireMember(options, "pidTOFMax", where), "pidTOFMax", where)};
  }

  const Device producer = Detail::FindInAnyWagon(*correlation.record, kPionProducer, where, errCtx);
  const rapidjson::Value& producerOptions = *producer.options;
  const std::string producerWhere = Source(producer);
  selection.tofRequired = JsonConfig::Dpl::RequireBool(producerOptions, "cfgIsTOFChecked", producerWhere);
  const rapidjson::Value* newRule = JsonConfig::OptionalMember(producerOptions, "minPtTOFInPID");
  const rapidjson::Value* oldRule = JsonConfig::OptionalMember(producerOptions, "tofPIDThreshold");
  if (!newRule == !oldRule) {
    throw std::runtime_error(std::format("[FATAL] {}: {} has {} of minPtTOFInPID and tofPIDThreshold, where one is "
                                         "expected: O2Physics changed the option again, or the record is broken.",
                                         errCtx, producerWhere, newRule ? "both" : "neither"));
  }
  selection.tofInPIDFromMinPt = newRule != nullptr;
  selection.minPtTOF = JsonConfig::ToNumber(newRule ? *newRule : *oldRule, newRule ? "minPtTOFInPID" : "tofPIDThreshold", producerWhere);
  return selection;
}

// For the messages: "|nσ_TPC| < 3, |nσ_TOF| < 3 where the TOF is used; TOF required from 0.5 GeV/c; the TOF in the PID
// from 0.5 GeV/c (minPtTOFInPID)".
inline std::string DescribePionSelection(const PionSelection& selection)
{
  const std::string window = selection.window ? std::format("n sigma window |nσ_TPC| < {}, |nσ_TOF| < {} where the TOF is used",
                                                            selection.window->tpcMax, selection.window->tofMax)
                                              : std::string("no n sigma window (selectPionInSigRegion off)");
  const std::string required = selection.tofRequired ? std::format("TOF required from {} GeV/c", selection.minPtTOF)
                                                     : std::string("TOF not required (cfgIsTOFChecked off)");
  const std::string rule = selection.tofInPIDFromMinPt
                             ? std::format("the TOF in the PID from {} GeV/c (minPtTOFInPID)", selection.minPtTOF)
                             : std::string("the TOF in the PID wherever the track has it (tofPIDThreshold, the rule before O2Physics 977ad4acc)");
  return std::format("{}; {}; {}", window, required, rule);
}

// The selection of the pion counted by every device, the same for all of them (SameOnAllDevices); empty without devices
inline std::optional<Sourced<PionSelection>> OnlinePionSelection(const std::vector<Device>& devices, std::string_view errCtx)
{
  return SameOnAllDevices(devices, [&](const Device& device) { return OnlinePionSelection(device, errCtx); },
                          DescribePionSelection, "the selection of the pion", errCtx);
}

// The selection a pion map was counted with, stamped next to it: MCTask writes it into Corrections.root, the correlation
// tasks read it back (CorrelationTaskBase::CheckPionMapSelection). One TParameter per fact, in the pion's
// IONames::OnlineSelectionPath, named here, next to their only writer and reader.
inline const std::string kStampPionNSigmaWindow{"NSigmaWindow"};
inline const std::string kStampPionTPCMax{"NSigmaTPCMax"};
inline const std::string kStampPionTOFMax{"NSigmaTOFMax"};
inline const std::string kStampPionTOFRequired{"TOFRequired"};
inline const std::string kStampPionMinPtTOF{"MinPtTOF"};
inline const std::string kStampPionTOFInPIDFromMinPt{"TOFInPIDFromMinPt"};

// The maxima only with the window, which they belong to.
inline void WritePionSelection(TDirectory* file, const std::string& binningName, const PionSelection& selection)
{
  TDirectory* dir = RootIO::MakePath(file, IONames::OnlineSelectionPath(binningName, "Pi"));
  RootIO::WriteParameter(dir, kStampPionNSigmaWindow, selection.window.has_value());
  if (selection.window) {
    RootIO::WriteParameter(dir, kStampPionTPCMax, selection.window->tpcMax);
    RootIO::WriteParameter(dir, kStampPionTOFMax, selection.window->tofMax);
  }
  RootIO::WriteParameter(dir, kStampPionTOFRequired, selection.tofRequired);
  RootIO::WriteParameter(dir, kStampPionMinPtTOF, selection.minPtTOF);
  RootIO::WriteParameter(dir, kStampPionTOFInPIDFromMinPt, selection.tofInPIDFromMinPt);
}

namespace Detail
{
// A fact of a stamp already found: missing, the stamp is not one WritePionSelection wrote, and the run stops.
template <class T>
T RequireFact(TDirectory* dir, const std::string& name, std::string_view where, std::string_view errCtx)
{
  const std::optional<T> value = RootIO::TryReadParameter<T>(dir, name, where, errCtx);
  if (!value) {
    throw std::runtime_error(std::format("[FATAL] {}: {} has the pion selection without '{}'. Re-run mc_task.", errCtx, where, name));
  }
  return *value;
}
} // namespace Detail

// The selection WritePionSelection wrote, or nullopt when there is none: a file from before the stamps, or from an MC
// input without a train configuration. 'where' names the file in the messages.
inline std::optional<PionSelection> ReadPionSelection(TDirectory* file, const std::string& binningName, std::string_view where,
                                                      std::string_view errCtx)
{
  TDirectory* dir = RootIO::FindPath(file, IONames::OnlineSelectionPath(binningName, "Pi"));
  const std::optional<bool> hasWindow = RootIO::TryReadParameter<bool>(dir, kStampPionNSigmaWindow, where, errCtx);
  if (!hasWindow)
    return std::nullopt;
  PionSelection selection{};
  if (*hasWindow) {
    selection.window = PionSelection::Window{Detail::RequireFact<double>(dir, kStampPionTPCMax, where, errCtx),
                                             Detail::RequireFact<double>(dir, kStampPionTOFMax, where, errCtx)};
  }
  selection.tofRequired = Detail::RequireFact<bool>(dir, kStampPionTOFRequired, where, errCtx);
  selection.minPtTOF = Detail::RequireFact<double>(dir, kStampPionMinPtTOF, where, errCtx);
  selection.tofInPIDFromMinPt = Detail::RequireFact<bool>(dir, kStampPionTOFInPIDFromMinPt, where, errCtx);
  return selection;
}

// The provenance key of an input's train configuration, for the role of the input it describes.
inline std::string ProvenanceKey(std::string_view role) { return std::format("train_config_{}", role); }

// For the provenance: per record, its train and data file as recorded, and the wagon, name and options of the device
// read, as one JSON text; "none" when the input has no train configuration. The record file is the archive of the rest.
// Documentation, so it never throws.
inline std::string Describe(const std::vector<Device>& devices)
{
  if (devices.empty()) {
    return "none";
  }
  rapidjson::Document out(rapidjson::kArrayType);
  auto& allocator = out.GetAllocator();
  for (const auto& device : devices) {
    rapidjson::Value entry(rapidjson::kObjectType);
    entry.AddMember("record", rapidjson::Value(device.record->path.c_str(), allocator), allocator);
    for (const char* key : {"hyperloop", "data_file"}) {
      if (const rapidjson::Value* member = JsonConfig::OptionalMember(device.record->doc, key))
        entry.AddMember(rapidjson::StringRef(key), rapidjson::Value(*member, allocator), allocator);
    }
    entry.AddMember("wagon", rapidjson::Value(device.wagonId.c_str(), allocator), allocator);
    entry.AddMember("device", rapidjson::Value(device.name.c_str(), allocator), allocator);
    entry.AddMember("options", rapidjson::Value(*device.options, allocator), allocator);
    out.PushBack(entry, allocator);
  }
  return JsonConfig::Serialize(out);
}
} // namespace TrainConfig
