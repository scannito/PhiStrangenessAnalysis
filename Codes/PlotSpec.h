#pragma once

#include "BinningUtils.h"
#include "JsonConfigHelpers.h"
#include "OutputNames.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iterator>
#include <format>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// A plot specification: what to draw, named by meaning (kind, species, delta-y limit,
// multiplicity interval), never by object name. It is the one interface of the plotting
// tool: written by hand, by `phiplot quick`, or by a language model (Macros/phiplot_ask.py),
// it goes through the same checks here and in PlotRenderer, so how it was written does not
// change what it is allowed to mean.
//
// The keys are declared once, in the tables below, with their type and what they mean. The
// tables are what RequireKnownKeys checks a block against and what Schema() turns into a
// JSON schema for a model, so the parser, the schema and a misspelled key's error message
// cannot disagree.
namespace PlotSpec
{

enum class ValueType { String,
                       Number,
                       Integer,
                       Bool,
                       Range,
                       StringArray,
                       Object,
                       ObjectArray };

struct KeyInfo {
  std::string_view name;
  ValueType type;
  bool required;
  std::string_view description;
  std::span<const KeyInfo> sub{};               // Object, ObjectArray: the keys of the inner block
  std::span<const std::string_view> allowed{}; // String, StringArray: the accepted values, when closed
};

inline constexpr std::array<std::string_view, 4> kFormats{"pdf", "png", "svg", "eps"};
// What 'output' must not end with: the formats, and the .root that is always written.
inline constexpr std::array<std::string_view, 5> kOutputExtensions{"pdf", "png", "svg", "eps", "root"};
inline constexpr std::array<std::string_view, 2> kDivideOptions{"", "B"};

inline constexpr auto kKindNames = [] {
  std::array<std::string_view, OutputNames::kKinds.size()> names{};
  for (std::size_t i = 0; i < names.size(); ++i)
    names[i] = OutputNames::kKinds[i].name;
  return names;
}();

inline constexpr std::array<std::string_view, 2> kSections{OutputNames::kExtract1D, OutputNames::kExtract2D};

inline constexpr std::array<KeyInfo, 11> kSeriesKeys{{
  {"file", ValueType::String, true, "Path of a PhiAssocSpectra ROOT file."},
  {"kind", ValueType::String, true, "What to draw (see the catalog: phiplot list).", {}, kKindNames},
  {"species", ValueType::String, true, "Associated species, e.g. K0S, Xi, Pi; the numerator of a species ratio."},
  {"species_den", ValueType::String, false, "Denominator species: species ratios only, and required there."},
  {"dy", ValueType::Number, true, "The |delta y| limit, e.g. 1.0, 0.5."},
  {"mult", ValueType::Range, false, "Multiplicity interval [low, high] in percent, one bin of the file's binning_mult: spectra only, and required there."},
  {"binning_name", ValueType::String, false, "Scheme directory; needed only when the file holds more than one."},
  {"section", ValueType::String, false, "Engine section; needed only when the scheme holds both.", {}, kSections},
  {"label", ValueType::String, false, "Legend entry; by default built from what differs between the series."},
  {"color", ValueType::Integer, false, "ROOT colour index; by default from a fixed palette, in order."},
  {"marker", ValueType::Integer, false, "ROOT marker style; by default from a fixed list, in order."},
}};

inline constexpr std::array<KeyInfo, 4> kRatioKeys{{
  {"reference", ValueType::Integer, true, "Index (from 0) of the series every other one is divided by."},
  {"divide_option", ValueType::String, true, "TH1::Divide option: \"\" for independent samples, \"B\" when each numerator is a subset of the reference (a closure). Never guessed.", {}, kDivideOptions},
  {"y_range", ValueType::Range, false, "Y range of the ratio pad; by default fitted to the ratios."},
  {"y_title", ValueType::String, false, "Y title of the ratio pad."},
}};

inline constexpr std::array<KeyInfo, 2> kLabelKeys{{
  {"status", ValueType::String, true, "Text after ALICE, e.g. \"Work in progress\"."},
  {"system", ValueType::String, true, "Second line, e.g. \"pp, #sqrt{#it{s}} = 13.6 TeV\"."},
}};

inline constexpr std::array<KeyInfo, 11> kTopKeys{{
  {"output", ValueType::String, true, "Output path without extension: one file per format, plus <output>.root with the canvas, the objects and the provenance."},
  {"series", ValueType::ObjectArray, true, "What to draw, in order. Spectra (against pT) and the other kinds (against multiplicity) cannot be mixed in one plot.", kSeriesKeys},
  {"formats", ValueType::StringArray, false, "Image formats; by default pdf and png.", {}, kFormats},
  {"title", ValueType::String, false, "Plot title."},
  {"x_title", ValueType::String, false, "X axis title; by default the first object's."},
  {"y_title", ValueType::String, false, "Y axis title; by default the first object's."},
  {"log_y", ValueType::Bool, false, "Logarithmic Y; by default on for spectra, off otherwise."},
  {"x_range", ValueType::Range, false, "X range [low, high]."},
  {"y_range", ValueType::Range, false, "Y range [low, high]; by default fitted to the objects."},
  {"alice_label", ValueType::Object, false, "Draw the ALICE label.", kLabelKeys},
  {"ratio", ValueType::Object, false, "Add a pad with each series divided by a reference one.", kRatioKeys},
}};

namespace Detail
{
inline std::vector<std::string_view> Names(std::span<const KeyInfo> keys)
{
  std::vector<std::string_view> names;
  for (const KeyInfo& k : keys)
    names.push_back(k.name);
  return names;
}

// The table applied to a block: unknown keys, required keys, types and closed lists of
// values, in one pass and from the table alone, before any key is read. What the reads
// below then do is only take the values out. RequireKnownKeys is applied only where some
// key is optional (CLAUDE.md, Configuration): where all are required a typo already stops
// as a missing key.
inline void Validate(const rapidjson::Value& node, std::span<const KeyInfo> keys, std::string_view ctx)
{
  if (!node.IsObject())
    throw std::runtime_error(std::format("[FATAL] {}: not a JSON object.", ctx));
  if (std::ranges::any_of(keys, [](const KeyInfo& k) { return !k.required; }))
    JsonConfig::RequireKnownKeys(node, Names(keys), ctx);

  for (const KeyInfo& k : keys) {
    const std::string key(k.name);
    if (!JsonConfig::OptionalMember(node, key.c_str())) {
      if (k.required)
        JsonConfig::RequireMember(node, key.c_str(), ctx); // throws, with the usual message
      continue;
    }
    switch (k.type) {
      case ValueType::String: {
        const std::string v = JsonConfig::RequireString(node, key.c_str(), ctx);
        if (!k.allowed.empty())
          JsonConfig::RequireOneOf(v, k.allowed, k.name, ctx);
        break;
      }
      case ValueType::Number:
        JsonConfig::RequireDouble(node, key.c_str(), ctx);
        break;
      case ValueType::Integer:
        JsonConfig::RequireInt(node, key.c_str(), ctx);
        break;
      case ValueType::Bool:
        JsonConfig::RequireBool(node, key.c_str(), ctx);
        break;
      case ValueType::Range:
        JsonConfig::TryRange(node, key.c_str(), ctx);
        break;
      case ValueType::StringArray:
        for (const auto& v : JsonConfig::RequireArray(node, key.c_str(), ctx)) {
          if (!v.IsString())
            throw std::runtime_error(std::format("[FATAL] {}: '{}' must hold strings.", ctx, k.name));
          if (!k.allowed.empty())
            JsonConfig::RequireOneOf(v.GetString(), k.allowed, k.name, ctx);
        }
        break;
      case ValueType::Object:
        JsonConfig::RequireObject(node, key.c_str(), ctx);
        break;
      case ValueType::ObjectArray:
        JsonConfig::RequireArray(node, key.c_str(), ctx);
        break;
    }
  }
}
} // namespace Detail

struct AliceLabel {
  std::string status;
  std::string system;
};

struct Ratio {
  int reference{0};
  std::string divideOption;
  std::optional<std::pair<double, double>> yRange;
  std::optional<std::string> yTitle;
};

struct Series {
  std::string file;
  OutputNames::Kind kind{OutputNames::Kind::Trend};
  std::string species;
  std::string speciesDen;
  double dy{0.0};
  std::optional<std::pair<double, double>> mult;
  std::optional<std::string> binningName;
  std::optional<std::string> section;
  std::optional<std::string> label;
  std::optional<int> color;
  std::optional<int> marker;
};

struct Spec {
  std::string output;
  std::vector<std::string> formats;
  std::vector<Series> series;
  std::optional<std::string> title, xTitle, yTitle;
  std::optional<bool> logY;
  std::optional<std::pair<double, double>> xRange, yRange;
  std::optional<AliceLabel> alice;
  std::optional<Ratio> ratio;
  std::string json; // the specification as read, for the provenance of the plot

  bool PerMultBin() const { return OutputNames::Info(series.front().kind).perMultBin; }
};

inline Series ParseSeries(const rapidjson::Value& node, std::string_view ctx)
{
  Detail::Validate(node, kSeriesKeys, ctx);

  Series s;
  s.file = JsonConfig::RequireString(node, "file", ctx);
  s.kind = OutputNames::KindByName(JsonConfig::RequireString(node, "kind", ctx)).value(); // validated against kKindNames
  const OutputNames::KindInfo& info = OutputNames::Info(s.kind);
  s.species = JsonConfig::RequireString(node, "species", ctx);

  // The objects exist at the limits the configuration declared, named after their value to
  // two decimals (OutputNames::DyTag): 0.504 would find the 0.50 object, so it is refused
  // instead of rounded.
  const double dy = JsonConfig::RequireDouble(node, "dy", ctx);
  s.dy = std::stod(BinningUtils::FormatDeltaY(dy).first); // the value the object's name stands for
  if (dy <= 0 || std::abs(dy - s.dy) > BinningUtils::kEdgeEpsilon)
    throw std::runtime_error(std::format("[FATAL] {}: 'dy' is {}: it must be a positive |delta y| limit with at most two decimals, as the objects are named.", ctx, dy));

  // A key that selects nothing for this kind is refused, not ignored: it says the writer
  // meant something this kind cannot do (a multiplicity interval on a trend, which is
  // already a function of multiplicity).
  const auto species_den = JsonConfig::TryString(node, "species_den", ctx);
  if (info.speciesPair != species_den.has_value())
    throw std::runtime_error(std::format("[FATAL] {}: 'species_den' is {} for kind '{}'.", ctx,
                                         info.speciesPair ? "required" : "meaningless", info.name));
  s.speciesDen = species_den.value_or("");

  s.mult = JsonConfig::TryRange(node, "mult", ctx);
  if (info.perMultBin != s.mult.has_value())
    throw std::runtime_error(std::format("[FATAL] {}: 'mult' is {} for kind '{}'{}.", ctx,
                                         info.perMultBin ? "required" : "meaningless", info.name,
                                         info.perMultBin ? " (one spectrum per multiplicity interval)" : " (it is already a function of multiplicity)"));
  if (s.mult && !(s.mult->first < s.mult->second))
    throw std::runtime_error(std::format("[FATAL] {}: 'mult' must be [low, high] with low < high.", ctx));

  s.binningName = JsonConfig::TryString(node, "binning_name", ctx);
  s.section = JsonConfig::TryString(node, "section", ctx);
  s.label = JsonConfig::TryString(node, "label", ctx);
  s.color = JsonConfig::TryInt(node, "color", ctx);
  s.marker = JsonConfig::TryInt(node, "marker", ctx);
  return s;
}

inline Spec Parse(const rapidjson::Value& root, std::string_view ctx)
{
  Detail::Validate(root, kTopKeys, ctx);

  Spec spec;
  spec.json = JsonConfig::Serialize(root);
  spec.output = JsonConfig::RequireString(root, "output", ctx);
  if (spec.output.empty())
    throw std::runtime_error(std::format("[FATAL] {}: 'output' is empty.", ctx));
  // The extensions are added: an output ending in one would give "x.pdf.pdf", or "x.root.root".
  for (std::string_view ext : kOutputExtensions)
    if (spec.output.ends_with(std::format(".{}", ext)))
      throw std::runtime_error(std::format("[FATAL] {}: 'output' is the path WITHOUT extension ('{}'); the extensions come from 'formats', and .root is always written.", ctx, spec.output));

  if (auto formats = JsonConfig::TryArray(root, "formats", ctx)) {
    for (const auto& f : *formats)
      spec.formats.emplace_back(f.GetString());
    if (spec.formats.empty())
      throw std::runtime_error(std::format("[FATAL] {}: 'formats' is empty; leave it out for the default (pdf, png).", ctx));
  } else {
    spec.formats = {"pdf", "png"};
  }

  int idx = 0;
  for (const auto& s : JsonConfig::RequireArray(root, "series", ctx))
    spec.series.push_back(ParseSeries(s, std::format("{} series[{}]", ctx, idx++)));
  if (spec.series.empty())
    throw std::runtime_error(std::format("[FATAL] {}: 'series' is empty, so there is nothing to draw.", ctx));

  // One X quantity per plot: a spectrum is drawn against pT, everything else against
  // multiplicity, and the two cannot share an axis.
  for (const Series& s : spec.series)
    if (OutputNames::Info(s.kind).perMultBin != spec.PerMultBin())
      throw std::runtime_error(std::format("[FATAL] {}: the series mix spectra (against pT) with objects against multiplicity; "
                                           "they cannot share an X axis. Make two plots.", ctx));

  spec.title = JsonConfig::TryString(root, "title", ctx);
  spec.xTitle = JsonConfig::TryString(root, "x_title", ctx);
  spec.yTitle = JsonConfig::TryString(root, "y_title", ctx);
  spec.logY = JsonConfig::TryBool(root, "log_y", ctx);
  spec.xRange = JsonConfig::TryRange(root, "x_range", ctx);
  spec.yRange = JsonConfig::TryRange(root, "y_range", ctx);

  if (const rapidjson::Value* label = JsonConfig::TryObject(root, "alice_label", ctx)) {
    const std::string lctx = std::format("{} alice_label", ctx);
    Detail::Validate(*label, kLabelKeys, lctx);
    spec.alice = AliceLabel{JsonConfig::RequireString(*label, "status", lctx), JsonConfig::RequireString(*label, "system", lctx)};
  }

  if (const rapidjson::Value* ratio = JsonConfig::TryObject(root, "ratio", ctx)) {
    const std::string rctx = std::format("{} ratio", ctx);
    Detail::Validate(*ratio, kRatioKeys, rctx);
    Ratio r;
    r.reference = JsonConfig::RequireInt(*ratio, "reference", rctx);
    r.divideOption = JsonConfig::RequireString(*ratio, "divide_option", rctx);
    r.yRange = JsonConfig::TryRange(*ratio, "y_range", rctx);
    r.yTitle = JsonConfig::TryString(*ratio, "y_title", rctx);
    if (spec.series.size() < 2)
      throw std::runtime_error(std::format("[FATAL] {}: a ratio pad needs at least two series.", rctx));
    if (r.reference < 0 || r.reference >= static_cast<int>(spec.series.size()))
      throw std::runtime_error(std::format("[FATAL] {}: 'reference' is {}, but there are {} series (indices from 0).",
                                           rctx, r.reference, spec.series.size()));
    spec.ratio = r;
  }
  return spec;
}

inline Spec ParseFile(const std::string& path)
{
  std::ifstream in(path, std::ios::binary);
  if (!in)
    throw std::runtime_error(std::format("[FATAL] PlotSpec: cannot read '{}'.", path));
  const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  rapidjson::Document doc;
  doc.Parse(text.c_str());
  if (doc.HasParseError())
    throw std::runtime_error(std::format("[FATAL] PlotSpec: '{}' is not valid JSON (offset {}).", path, doc.GetErrorOffset()));
  return Parse(doc, std::format("plot spec '{}'", path));
}

// ---------------------------------------------------------------------------
// The schema, from the same tables: what a model is constrained to produce.
// ---------------------------------------------------------------------------
namespace Detail
{
inline rapidjson::Value SchemaOf(std::span<const KeyInfo> keys, rapidjson::Document::AllocatorType& a);

inline rapidjson::Value SchemaOfKey(const KeyInfo& k, rapidjson::Document::AllocatorType& a)
{
  rapidjson::Value v(rapidjson::kObjectType);
  const auto str = [&](std::string_view s) { return rapidjson::Value(s.data(), static_cast<rapidjson::SizeType>(s.size()), a); };
  const auto enumOf = [&](std::span<const std::string_view> allowed) {
    rapidjson::Value e(rapidjson::kArrayType);
    for (std::string_view s : allowed)
      e.PushBack(str(s), a);
    return e;
  };
  switch (k.type) {
    case ValueType::String:
      v.AddMember("type", "string", a);
      if (!k.allowed.empty())
        v.AddMember("enum", enumOf(k.allowed), a);
      break;
    case ValueType::Number:
      v.AddMember("type", "number", a);
      break;
    case ValueType::Integer:
      v.AddMember("type", "integer", a);
      break;
    case ValueType::Bool:
      v.AddMember("type", "boolean", a);
      break;
    case ValueType::Range: {
      v.AddMember("type", "array", a);
      rapidjson::Value items(rapidjson::kObjectType);
      items.AddMember("type", "number", a);
      v.AddMember("items", items, a);
      v.AddMember("minItems", 2, a);
      v.AddMember("maxItems", 2, a);
      break;
    }
    case ValueType::StringArray: {
      v.AddMember("type", "array", a);
      rapidjson::Value items(rapidjson::kObjectType);
      items.AddMember("type", "string", a);
      if (!k.allowed.empty())
        items.AddMember("enum", enumOf(k.allowed), a);
      v.AddMember("items", items, a);
      break;
    }
    case ValueType::Object:
      v = SchemaOf(k.sub, a);
      break;
    case ValueType::ObjectArray:
      v.AddMember("type", "array", a);
      v.AddMember("items", SchemaOf(k.sub, a), a);
      v.AddMember("minItems", 1, a);
      break;
  }
  v.AddMember("description", str(k.description), a);
  return v;
}

inline rapidjson::Value SchemaOf(std::span<const KeyInfo> keys, rapidjson::Document::AllocatorType& a)
{
  rapidjson::Value obj(rapidjson::kObjectType);
  obj.AddMember("type", "object", a);
  rapidjson::Value props(rapidjson::kObjectType);
  rapidjson::Value required(rapidjson::kArrayType);
  for (const KeyInfo& k : keys) {
    props.AddMember(rapidjson::Value(k.name.data(), static_cast<rapidjson::SizeType>(k.name.size()), a), SchemaOfKey(k, a), a);
    if (k.required)
      required.PushBack(rapidjson::Value(k.name.data(), static_cast<rapidjson::SizeType>(k.name.size()), a), a);
  }
  obj.AddMember("properties", props, a);
  obj.AddMember("required", required, a);
  obj.AddMember("additionalProperties", false, a);
  return obj;
}
} // namespace Detail

inline std::string Schema()
{
  rapidjson::Document doc;
  auto& a = doc.GetAllocator();
  rapidjson::Value schema = Detail::SchemaOf(kTopKeys, a);
  schema.AddMember("$schema", "https://json-schema.org/draft/2020-12/schema", a);
  schema.AddMember("title", "phiplot plot specification", a);
  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
  writer.SetIndent(' ', 2);
  schema.Accept(writer);
  return buffer.GetString();
}

} // namespace PlotSpec
