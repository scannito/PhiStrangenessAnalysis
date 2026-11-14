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
// A file summed from several trains (hadd) has a record that names it and lists the records of its parts instead,
// written by whoever makes the sum:
//   {"data_file": {"name": "<the summed file>"}, "merged_from": ["<part>.train.json", ...]}   (paths relative to it)
// The options read must then be the same in all the parts: a sum of trains that ran differently is not one sample.
//
// The options are written as DPL writes them, and read with JsonConfig::Dpl. An option missing from a record that
// exists means O2Physics renamed it: it stops the run, since a guess would be the silent failure this file is for.

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

// phiStrangeCorrelation's activeCorrelationTypes, element by element (K0S, Lambda, AntiLambda, Xi, Omega, Pion), in the
// framework's names.
inline const std::array<std::string, 6> kAssocSpecies{"K0S", "Lambda", "AntiLambda", "Xi", "Omega", "Pi"};

// "<record>: wagon <id>, device '<name>'", for the messages.
inline std::string Source(const Device& device)
{
  return std::format("{}: wagon {}, device '{}'", device.record->path, device.wagonId, device.name);
}

// "Phi, Pi", or "none", for the messages.
inline std::string Names(const std::set<std::string>& names) { return names.empty() ? "none" : Logger::Join(names); }

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
} // namespace Detail

// Where the record of a data file is: next to it, its extension replaced. The same rule as config_path_for in
// Macros/downloadHYOutput.py.
inline std::string RecordPath(const std::string& dataFile)
{
  return std::filesystem::path(dataFile).replace_extension(".train.json").string();
}

// The records of the train(s) a data file comes from: one, or one per part of a summed file. Empty when the file has
// no record, with a warning: the productions made before 12 November 2026 have none until downloadHYOutput.py is run on
// their batch again. A record that is unreadable, names another data file, or does not list a wagon the file has stops
// the run.
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

  // The record names the file it belongs to, a sum's too: a record copied or renamed next to another file fails here
  const std::string recorded = JsonConfig::RequireString(JsonConfig::RequireObject(own, "data_file", path), "name", path);
  const std::string actual = std::filesystem::path(dataFile).filename().string();
  if (recorded != actual) {
    throw std::runtime_error(std::format("[FATAL] {}: '{}' is the train configuration of '{}', not of '{}'.",
                                         errCtx, path, recorded, actual));
  }

  if (const rapidjson::Value* merged = JsonConfig::OptionalMember(own, "merged_from")) {
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
    }
    if (parts.empty())
      throw std::runtime_error(std::format("[FATAL] {}: '{}' has an empty 'merged_from'.", errCtx, path));
    records = std::move(parts);
  }

  for (const auto& record : records)
    Wagons(record); // required once here, so that the later calls cannot fail
  Detail::RequireWagonsKnown(dataFile, records, errCtx);
  return records;
}

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
                                           errCtx, record.path, name, basePath));
    }
    devices.push_back(*found);
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

// The particles, among 'of', that the devices weighted online, and the first device as their source. The same for all
// the devices (the same event and the mixed event, the trains of a sum, are read together and must have been weighted
// alike), or the run stops naming two that differ. Only 'of' is compared, so devices that differ in a particle nobody
// reads pass. Empty when there are no devices, which is "not known", never "nothing weighted".
struct WeightedParticles {
  std::set<std::string> particles;
  std::string source;
};

inline std::optional<WeightedParticles> WeightedOnline(const std::vector<Device>& devices, const std::set<std::string>& of,
                                                       std::string_view errCtx)
{
  if (devices.empty()) {
    return std::nullopt;
  }
  const auto weightedOf = [&](const Device& device) {
    std::set<std::string> weighted;
    std::ranges::set_intersection(ParticlesWeightedOnline(device), of, std::inserter(weighted, weighted.end()));
    return weighted;
  };
  const std::set<std::string> common = weightedOf(devices.front());
  for (const auto& device : devices) {
    if (const std::set<std::string> weighted = weightedOf(device); weighted != common) {
      throw std::runtime_error(std::format(
        "[FATAL] {}: the inputs were weighted online for different particles: {} by {}, {} by {}. The same event and "
        "the mixed event, and every train of a sum, must be weighted alike.",
        errCtx, Names(common), Source(devices.front()), Names(weighted), Source(device)));
    }
  }
  return WeightedParticles{common, Source(devices.front())};
}

// The provenance key of an input's train configuration, for its role ("data", "mixed_event").
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
