#pragma once

#include "JsonConfigHelpers.h"
#include "Logger.h"
#include "OutputNames.h"
#include "PlotDesigns.h"
#include "PlotSeries.h"

#include "rapidjson/document.h"
#include "rapidjson/pointer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/schema.h"
#include "rapidjson/stringbuffer.h"

#include <algorithm>
#include <format>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// A plot as data: one design of PlotDesigns and the series it draws, named the way a C++ composition names them
// (PlotSeries' finders), with the cosmetics of Panel and Frame. What a local model writes when a plot is asked for in
// words (DESIGN_NOTES section 1); it can only name what the components find and check, and every check is theirs: a
// specification is translated into the same calls a composition makes. The schema is the one description of what a
// specification may say, each design's and each series' rules included: `phistrangeplot schema` prints it, every
// specification is validated against it before it is read, and a front end passes it to the model as the shape its
// answer must have. It is built from the tables below, which the reader dispatches through, so the two cannot part.
namespace PlotSpec
{

namespace Detail
{
// The names a specification may use for each closed list, once.
inline constexpr std::pair<std::string_view, PlotSeries::TrendKind> kTrendKinds[] = {
  {"measured", PlotSeries::TrendKind::Measured}, {"extrapolated", PlotSeries::TrendKind::Extrapolated},
  {"extrap_over_meas", PlotSeries::TrendKind::ExtrapOverMeas}};
inline constexpr std::pair<std::string_view, OutputNames::SpectrumBinning> kBinnings[] = {
  {"analysis", OutputNames::SpectrumBinning::Analysis}, {"source", OutputNames::SpectrumBinning::Source}};
inline constexpr std::pair<std::string_view, OutputNames::CorrectionMap> kMaps[] = {
  {"efficiency", OutputNames::CorrectionMap::Efficiency}, {"signal_loss", OutputNames::CorrectionMap::SignalLoss},
  {"signal_loss_over_event_loss", OutputNames::CorrectionMap::SignalLossOverEventLoss}};
inline constexpr std::string_view kDrawOptions[] = {"E1", "E", "P"};
inline constexpr std::string_view kDivideOptions[] = {"", "B"};
inline constexpr std::string_view kDefaultBinning = "analysis";

// Names as the entries of a JSON "enum".
template <class Names>
std::string EnumList(Names&& names)
{
  return Logger::Join(names | std::views::transform([](std::string_view n) { return std::format("\"{}\"", n); }));
}

template <class E, std::size_t N>
E Resolve(const std::pair<std::string_view, E> (&table)[N], const std::string& name, std::string_view what)
{
  for (const auto& [n, value] : table)
    if (n == name)
      return value;
  throw std::runtime_error(std::format("[FATAL] PlotSpec: '{}' is not a {}.", name, what));
}

inline const std::string kCtx = "PlotSpec";

// A required [low, high] (the schema has checked it is there; TryRange checks low < high).
template <class RangeType>
RangeType RequireRange(const rapidjson::Value& node, const char* key)
{
  if (std::optional<RangeType> range = JsonConfig::TryRange<RangeType>(node, key, kCtx))
    return *range;
  throw std::runtime_error(std::format("[FATAL] PlotSpec: '{}' is required.", key));
}

// The schema text of the keys a form shares or owns.
inline std::string Key(std::string_view name, std::string_view schema) { return std::format("\"{}\": {}", name, schema); }
inline const std::string kRange = R"({"type": "array", "items": {"type": "number"}, "minItems": 2, "maxItems": 2})";
inline const std::string kSpecies = Key("species", R"({"type": "string"})");
inline const std::string kDy = Key("dy", R"({"type": "number", "description": "The |Delta y| limit, one the file holds (two decimals)."})");
inline const std::string kMult =
  Key("mult", R"({"type": "array", "items": {"type": "number"}, "minItems": 2, "maxItems": 2, "description": "A multiplicity class, low and high edge in percent, as the file's binning has it."})");
inline const std::string kSection = Key("section", R"({"type": "string", "description": "Extract1D or Extract2D; the only one in the file when absent."})");
inline std::string Binning()
{
  return Key("binning", std::format(R"({{"enum": [{}], "description": "The pT binning; {} when absent."}})", EnumList(kBinnings | std::views::keys),
                                    kDefaultBinning));
}
inline const std::vector<std::string> kSeriesCommon = {
  Key("file", R"({"type": "string", "description": "A file the framework wrote."})"),
  Key("scheme", R"({"type": "string", "description": "The binning_name; the only one in the file when absent."})"),
  Key("label", R"({"type": "string", "description": "The legend's text; written from what the series is when absent."})"),
  Key("color", R"({"type": "integer", "description": "A ROOT colour number."})"),
  Key("marker", R"({"type": "integer", "description": "A ROOT marker style."})"), Key("marker_size", R"({"type": "number"})")};

// One object form of the schema: a discriminating key with one value, the keys it takes, the ones it requires.
inline std::string Form(std::string_view discriminator, std::string_view value, std::string_view description, const std::vector<std::string>& keys,
                        std::vector<std::string_view> required)
{
  std::string properties = Key(discriminator, std::format(R"({{"enum": ["{}"], "description": "{}"}})", value, description));
  for (const std::string& k : keys)
    properties += ",\n      " + k;
  required.insert(required.begin(), discriminator);
  return std::format("\n    {{\"type\": \"object\", \"additionalProperties\": false, \"required\": [{}],\n     \"properties\": {{\n      {}}}}}",
                     EnumList(required), properties);
}

// A form of series: its schema and how it is found. 'read' gets the series' object, its file and where in it.
struct SeriesForm {
  std::string_view of;
  PlotSeries::FileKind reads; // the kind of file it is found in
  std::string_view description;
  std::vector<std::string> keys;
  std::vector<std::string_view> required;
  PlotSeries::Series (*read)(const rapidjson::Value&, const std::string& file, const PlotSeries::Where& where);
};

inline const std::vector<SeriesForm>& SeriesForms()
{
  using namespace PlotSeries;
  static const std::vector<SeriesForm> forms = {
    {"spectrum", PlotSeries::FileKind::Spectra, "The pT spectrum of an associated species in one multiplicity class; with extended, the same extended to pT = 0 with its fitted curve.",
     {kSpecies, kDy, kMult, kSection, Key("extended", R"({"type": "boolean"})")},
     {"species", "dy", "mult"},
     [](const rapidjson::Value& s, const std::string& file, const Where& where) {
       return (JsonConfig::OptionalBool(s, "extended", false, kCtx) ? SpectrumExtended : Spectrum)(
         file, JsonConfig::RequireString(s, "species", kCtx), JsonConfig::RequireDouble(s, "dy", kCtx), RequireRange<Interval>(s, "mult"), where);
     }},
    {"trend", PlotSeries::FileKind::Spectra, "The yield of a species against multiplicity: measured, extrapolated to pT = 0, or the one over the other.",
     {kSpecies, kDy, Key("trend", std::format(R"({{"enum": [{}]}})", EnumList(kTrendKinds | std::views::keys))), kSection},
     {"species", "dy", "trend"},
     [](const rapidjson::Value& s, const std::string& file, const Where& where) {
       return Trend(file, JsonConfig::RequireString(s, "species", kCtx), JsonConfig::RequireDouble(s, "dy", kCtx),
                    Resolve(kTrendKinds, JsonConfig::RequireString(s, "trend", kCtx), "trend"), where);
     }},
    // A species ratio is measured or extrapolated, the first two kinds of a trend.
    {"species_ratio", PlotSeries::FileKind::Spectra, "The ratio of two species' yields against multiplicity, as the correlation task wrote it.",
     {Key("numerator", R"({"type": "string"})"), Key("denominator", R"({"type": "string"})"), kDy,
      Key("trend", std::format(R"({{"enum": [{}]}})", EnumList(std::span(kTrendKinds).first(2) | std::views::keys))), kSection},
     {"numerator", "denominator", "dy", "trend"},
     [](const rapidjson::Value& s, const std::string& file, const Where& where) {
       return SpeciesRatio(file, JsonConfig::RequireString(s, "numerator", kCtx), JsonConfig::RequireString(s, "denominator", kCtx),
                           JsonConfig::RequireDouble(s, "dy", kCtx), Resolve(kTrendKinds, JsonConfig::RequireString(s, "trend", kCtx), "trend"),
                           where);
     }},
    {"purity", PlotSeries::FileKind::Purity, "The purity S/(S+B) of a purity key against pT, in one multiplicity class.",
     {Key("purity_key", R"({"type": "string"})"), kMult, Binning()},
     {"purity_key", "mult"},
     [](const rapidjson::Value& s, const std::string& file, const Where& where) {
       return Purity(file, JsonConfig::RequireString(s, "purity_key", kCtx), RequireRange<Interval>(s, "mult"),
                     Resolve(kBinnings, JsonConfig::OptionalString(s, "binning", std::string(kDefaultBinning), kCtx), "binning"), where.scheme);
     }},
    {"correction_map", PlotSeries::FileKind::Corrections, "A correction map of a particle against pT, in one multiplicity class or integrated over multiplicity (no mult).",
     {Key("particle", R"({"type": "string"})"), Key("map", std::format(R"({{"enum": [{}]}})", EnumList(kMaps | std::views::keys))), kMult, Binning()},
     {"particle", "map"},
     [](const rapidjson::Value& s, const std::string& file, const Where& where) {
       return CorrectionMap(file, JsonConfig::RequireString(s, "particle", kCtx), Resolve(kMaps, JsonConfig::RequireString(s, "map", kCtx), "correction map"),
                            JsonConfig::TryRange<Interval>(s, "mult", kCtx),
                            Resolve(kBinnings, JsonConfig::OptionalString(s, "binning", std::string(kDefaultBinning), kCtx), "binning"), where.scheme);
     }},
  };
  return forms;
}

// The keys of the panel and the frame every design takes, the series first, at least 'minSeries' of them.
inline std::vector<std::string> PanelAndFrameKeys(int minSeries)
{
  const PlotDesigns::Frame frame;
  const PlotDesigns::Panel panel;
  std::string seriesForms;
  for (const SeriesForm& f : SeriesForms()) {
    std::vector<std::string> keys = kSeriesCommon;
    keys.insert(keys.end(), f.keys.begin(), f.keys.end());
    std::vector<std::string_view> required{"file"};
    required.insert(required.end(), f.required.begin(), f.required.end());
    seriesForms += (seriesForms.empty() ? "" : ",") + Form("of", f.of, f.description, keys, required);
  }
  return {
    Key("series", std::format(R"({{"type": "array", "minItems": {}, "items": {{"oneOf": [{}]}}, "description": "What is drawn, each found by what it is in a file the framework wrote."}})",
                              minSeries, seriesForms)),
    Key("title", R"({"type": "string", "description": "Written in the top left corner of the pad."})"),
    Key("x_title", R"({"type": "string"})"),
    Key("y_title", R"({"type": "string"})"),
    Key("y_range", kRange),
    Key("log_y", R"({"type": "boolean"})"),
    Key("legend", std::format(R"({{"type": "boolean", "description": "The legend written from what the series are; {} when absent."}})", panel.legend)),
    Key("legend_box", R"({"type": "array", "items": {"type": "number"}, "minItems": 4, "maxItems": 4, "description": "x1, y1, x2, y2 of the legend, fractions of the pad."})"),
    Key("legend_columns", R"({"type": "integer", "minimum": 1})"),
    Key("width", std::format(R"({{"type": "integer", "minimum": 200, "description": "Pixels; {} when absent."}})", frame.width)),
    Key("height", std::format(R"({{"type": "integer", "minimum": 200, "description": "Pixels; {} when absent."}})", frame.height)),
    Key("text_px", std::format(R"({{"type": "integer", "minimum": 8, "description": "Size of the text in pixels; {} when absent."}})", frame.textPx)),
    Key("draw_option", std::format(R"({{"enum": [{}], "description": "E1 error bars with ends, E without, P markers only; {} when absent."}})",
                                   EnumList(kDrawOptions), frame.drawOption)),
  };
}

// One form per design: overlay takes no ratio key; compare_with_ratio needs two series and its divide option.
inline std::string DesignForms()
{
  std::vector<std::string> ratioKeys = PanelAndFrameKeys(2);
  ratioKeys.insert(ratioKeys.end(), {
    Key("reference", R"({"type": "integer", "minimum": 0, "description": "The index of the series the others are divided by; 0 when absent."})"),
    Key("divide_option", std::format(R"({{"enum": [{}], "description": "\"\" for independent samples, \"B\" when the numerator is a subset of the denominator (a closure)."}})",
                                     EnumList(kDivideOptions))),
    Key("ratio_range", kRange), Key("ratio_title", R"({"type": "string"})")});
  return Form("design", "overlay", "Every series on one pad.", PanelAndFrameKeys(1), {"series"}) + "," +
         Form("design", "compare_with_ratio", "The series on top, and below the ratio of each to the reference series.", ratioKeys,
              {"series", "divide_option"});
}

inline std::string PointerText(const rapidjson::Pointer& pointer)
{
  rapidjson::StringBuffer buffer;
  pointer.Stringify(buffer);
  return buffer.GetString();
}

// Why 'value' fails 'schema', said in words: the key an object does not take, the keys it lacks, the values allowed, or
// for an object matching none of a oneOf's forms, its own form's failure (by its "design" or "of") or the forms there are.
inline std::string Explain(const rapidjson::Value& schema, const rapidjson::Value& value, const std::string& at)
{
  rapidjson::SchemaDocument schemaDoc(schema);
  rapidjson::SchemaValidator validator(schemaDoc);
  if (value.Accept(validator))
    return "";
  const std::string keyword = validator.GetInvalidSchemaKeyword();
  const rapidjson::Pointer& docPointer = validator.GetInvalidDocumentPointer();
  const std::string inner = at + PointerText(docPointer);
  const std::string where = inner.empty() ? "/" : inner;
  const rapidjson::Value* failedSchema = validator.GetInvalidSchemaPointer().Get(schema);
  const rapidjson::Value* failedValue = docPointer.Get(value);
  // A key the object does not take: the validator points at the key itself, its schema is the object's.
  if (keyword == "additionalProperties" && failedSchema && failedSchema->HasMember("properties") && docPointer.GetTokenCount() > 0) {
    const std::string object = at + PointerText(rapidjson::Pointer(docPointer.GetTokens(), docPointer.GetTokenCount() - 1));
    std::vector<std::string> known;
    for (const auto& p : (*failedSchema)["properties"].GetObject())
      known.emplace_back(p.name.GetString());
    const std::string key = docPointer.GetTokens()[docPointer.GetTokenCount() - 1].name;
    // In a series, the forms that do take the key: what to change is often the form, not the key.
    std::vector<std::string_view> takenBy;
    for (const SeriesForm& f : SeriesForms())
      if (std::ranges::any_of(f.keys, [&](const std::string& k) { return k.starts_with(std::format("\"{}\"", key)); }))
        takenBy.push_back(f.of);
    return std::format("{}: unknown key '{}'; the object takes {}{}", object.empty() ? "/" : object, key, Logger::Join(known, ", "),
                       takenBy.empty() || !failedSchema->HasMember("properties") || !(*failedSchema)["properties"].HasMember("of")
                         ? ""
                         : std::format(" ('{}' belongs to a series of: {})", key, Logger::Join(takenBy, ", ")));
  }
  if (keyword == "required" && failedSchema && failedValue && failedValue->IsObject()) {
    std::vector<std::string> missing;
    for (const auto& key : (*failedSchema)["required"].GetArray())
      if (!failedValue->HasMember(key))
        missing.emplace_back(key.GetString());
    return std::format("{}: missing {}", where, Logger::Join(missing, ", "));
  }
  if (keyword == "minItems" && failedSchema)
    return std::format("{}: needs at least {} entries", where, (*failedSchema)["minItems"].GetInt());
  if (keyword == "enum" && failedSchema)
    return std::format("{}: not one of {}", where, JsonConfig::Serialize((*failedSchema)["enum"]));
  if (keyword == "oneOf" && failedSchema && failedValue && failedValue->IsObject()) {
    std::vector<std::string> kinds;
    for (const auto& form : (*failedSchema)["oneOf"].GetArray())
      for (const char* discriminator : {"design", "of"})
        if (form["properties"].HasMember(discriminator)) {
          const rapidjson::Value& name = form["properties"][discriminator]["enum"][0];
          if (failedValue->HasMember(discriminator) && (*failedValue)[discriminator] == name)
            return Explain(form, *failedValue, inner);
          kinds.push_back(std::format("{} {}", discriminator, name.GetString()));
        }
    return std::format("{}: matches none of the forms ({})", where, Logger::Join(kinds, ", "));
  }
  return std::format("{}: fails '{}' (schema {})", where, keyword, PointerText(validator.GetInvalidSchemaPointer()));
}
} // namespace Detail

// The schema: what a specification may say. Checked to be JSON when it is built, before anyone is given it.
inline const std::string& Schema()
{
  static const std::string text = [] {
    std::string t = std::format("{{\n  \"$schema\": \"http://json-schema.org/draft-04/schema#\",\n"
                                "  \"title\": \"A plot of the phi-strangeness analysis: one design and the series it draws\",\n"
                                "  \"oneOf\": [{}]\n}}",
                                Detail::DesignForms());
    rapidjson::Document check;
    if (check.Parse(t.c_str()).HasParseError())
      throw std::logic_error(std::format("[FATAL] PlotSpec: the schema built from the tables is not JSON (at offset {}).", check.GetErrorOffset()));
    return t;
  }();
  return text;
}

// A specification read and checked against the schema; a specification that fails it stops with why.
inline rapidjson::Document Parse(const std::string& text, const std::string& origin)
{
  rapidjson::Document spec;
  if (spec.Parse(text.c_str()).HasParseError())
    throw std::runtime_error(std::format("[FATAL] PlotSpec: '{}' is not JSON (at offset {}).", origin, spec.GetErrorOffset()));
  rapidjson::Document schema;
  schema.Parse(Schema().c_str());
  if (const std::string why = Detail::Explain(schema, spec, ""); !why.empty())
    throw std::runtime_error(std::format("[FATAL] PlotSpec: '{}' is not a valid specification: {}.", origin, why));
  return spec;
}

namespace Detail
{
// One series, by the form its "of" names, with its look.
inline PlotSeries::Series ReadSeries(const rapidjson::Value& s)
{
  const std::string of = JsonConfig::RequireString(s, "of", kCtx);
  const auto form = std::ranges::find(SeriesForms(), of, &SeriesForm::of);
  if (form == SeriesForms().end())
    throw std::logic_error(std::format("[FATAL] PlotSpec: the schema let through a series of kind '{}'.", of));
  PlotSeries::Series series = form->read(s, JsonConfig::RequireString(s, "file", kCtx),
                                         {JsonConfig::OptionalString(s, "scheme", "", kCtx), JsonConfig::OptionalString(s, "section", "", kCtx)});
  series.label = JsonConfig::OptionalString(s, "label", "", kCtx);
  series.look = {.color = JsonConfig::TryInt(s, "color", kCtx), .marker = JsonConfig::TryInt(s, "marker", kCtx),
                 .markerSize = JsonConfig::TryDouble(s, "marker_size", kCtx)};
  return series;
}
} // namespace Detail

// What a specification draws: its design run on its series, on 'canvases'. The ALICE label is not the specification's
// to say (the collision system is not in the files): the caller gives it, or none.
inline PlotDesigns::Drawn Draw(const rapidjson::Document& spec, CanvasManager& canvases, const std::string& name,
                               const std::optional<std::pair<std::string, std::string>>& alice = std::nullopt)
{
  using Detail::kCtx;
  PlotDesigns::Panel panel{.title = JsonConfig::OptionalString(spec, "title", "", kCtx),
                           .xTitle = JsonConfig::OptionalString(spec, "x_title", "", kCtx),
                           .yTitle = JsonConfig::OptionalString(spec, "y_title", "", kCtx),
                           .yRange = JsonConfig::TryRange<std::pair<double, double>>(spec, "y_range", kCtx),
                           .logY = JsonConfig::OptionalBool(spec, "log_y", false, kCtx)};
  panel.legend = JsonConfig::OptionalBool(spec, "legend", panel.legend, kCtx);
  for (const auto& s : JsonConfig::RequireArray(spec, "series", kCtx))
    panel.series.push_back(Detail::ReadSeries(s));

  PlotDesigns::Frame frame;
  frame.width = JsonConfig::OptionalInt(spec, "width", frame.width, kCtx);
  frame.height = JsonConfig::OptionalInt(spec, "height", frame.height, kCtx);
  frame.textPx = JsonConfig::OptionalInt(spec, "text_px", frame.textPx, kCtx);
  frame.drawOption = JsonConfig::OptionalString(spec, "draw_option", frame.drawOption, kCtx);
  if (auto box = JsonConfig::TryArray(spec, "legend_box", kCtx)) {
    const std::vector<double> corners = JsonConfig::ReadNumberArray(*box, "legend_box", kCtx);
    frame.legend = {corners[0], corners[1], corners[2], corners[3], frame.legend.columns};
  }
  frame.legend.columns = JsonConfig::OptionalInt(spec, "legend_columns", frame.legend.columns, kCtx);
  frame.alice = alice;

  const std::string design = JsonConfig::RequireString(spec, "design", kCtx);
  if (design == "overlay")
    return PlotDesigns::Overlay(canvases, name, panel, frame);
  if (design == "compare_with_ratio")
    return PlotDesigns::CompareWithRatio(canvases, name, panel, static_cast<std::size_t>(JsonConfig::OptionalInt(spec, "reference", 0, kCtx)),
                                         JsonConfig::RequireString(spec, "divide_option", kCtx),
                                         JsonConfig::TryRange<std::pair<double, double>>(spec, "ratio_range", kCtx),
                                         JsonConfig::OptionalString(spec, "ratio_title", "ratio", kCtx), frame);
  throw std::logic_error(std::format("[FATAL] PlotSpec: the schema let through a design '{}' with nothing to draw it.", design));
}

namespace Detail
{
inline constexpr std::pair<std::string_view, PlotSeries::FileKind> kFileKinds[] = {
  {"corrections", PlotSeries::FileKind::Corrections}, {"purity", PlotSeries::FileKind::Purity}, {"spectra", PlotSeries::FileKind::Spectra}};

inline std::string_view KindName(PlotSeries::FileKind kind)
{
  return std::ranges::find(kFileKinds, kind, &std::pair<std::string_view, PlotSeries::FileKind>::second)->first;
}
} // namespace Detail

// What the files hold, as JSON in the specification's own words (the keys a series takes, the forms each file can be
// asked for): what a front end gives the model along with the schema, so that it names only what is there.
inline std::string CatalogJson(const std::vector<PlotSeries::Catalog>& catalogs)
{
  rapidjson::Document doc(rapidjson::kArrayType);
  auto& a = doc.GetAllocator();
  auto str = [&](std::string_view text) { return rapidjson::Value(text.data(), static_cast<rapidjson::SizeType>(text.size()), a); };
  auto strings = [&](const auto& texts) {
    rapidjson::Value v(rapidjson::kArrayType);
    for (const auto& t : texts)
      v.PushBack(str(t), a);
    return v;
  };
  for (const PlotSeries::Catalog& catalog : catalogs) {
    rapidjson::Value file(rapidjson::kObjectType), schemes(rapidjson::kArrayType);
    file.AddMember("file", str(catalog.fileName), a);
    for (const PlotSeries::Catalog::Scheme& scheme : catalog.schemes) {
      rapidjson::Value sc(rapidjson::kObjectType), classes(rapidjson::kArrayType), forms(rapidjson::kArrayType);
      sc.AddMember("scheme", str(scheme.name), a);
      sc.AddMember("kind", str(Detail::KindName(scheme.kind)), a);
      for (const Detail::SeriesForm& f : Detail::SeriesForms())
        if (f.reads == scheme.kind)
          forms.PushBack(str(f.of), a);
      sc.AddMember("series_forms", forms, a);
      for (const PlotSeries::Interval& c : scheme.classes) {
        rapidjson::Value interval(rapidjson::kArrayType);
        interval.PushBack(c.low, a).PushBack(c.high, a);
        classes.PushBack(interval, a);
      }
      sc.AddMember("mult", classes, a);
      if (scheme.kind == PlotSeries::FileKind::Corrections)
        sc.AddMember("particle", strings(scheme.particles), a);
      if (scheme.kind == PlotSeries::FileKind::Purity)
        sc.AddMember("purity_key", strings(scheme.purityKeys), a);
      if (scheme.kind == PlotSeries::FileKind::Spectra) {
        rapidjson::Value sections(rapidjson::kArrayType);
        for (const PlotSeries::Catalog::Section& section : scheme.sections) {
          rapidjson::Value sec(rapidjson::kObjectType), dys(rapidjson::kArrayType), ratios(rapidjson::kArrayType);
          sec.AddMember("section", str(section.name), a);
          sec.AddMember("species", strings(section.species), a);
          for (double dy : section.deltaYLimits)
            dys.PushBack(dy, a);
          sec.AddMember("dy", dys, a);
          for (const auto& [num, den] : section.speciesRatios) {
            rapidjson::Value r(rapidjson::kObjectType);
            r.AddMember("numerator", str(num), a).AddMember("denominator", str(den), a);
            ratios.PushBack(r, a);
          }
          sec.AddMember("species_ratios", ratios, a);
          sections.PushBack(sec, a);
        }
        sc.AddMember("sections", sections, a);
      }
      for (const char* fact : {"produced_at", "git_commit"})
        if (auto it = scheme.provenance.find(fact); it != scheme.provenance.end())
          sc.AddMember(rapidjson::StringRef(fact), str(it->second), a);
      schemes.PushBack(sc, a);
    }
    file.AddMember("schemes", schemes, a);
    doc.PushBack(file, a);
  }
  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
  writer.SetIndent(' ', 2);
  doc.Accept(writer);
  return buffer.GetString();
}

} // namespace PlotSpec
