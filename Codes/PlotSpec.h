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
// specification is validated against it before it is read, and a front end gives it to the model. It is built from the
// tables below, which the reader dispatches through, and every key is named once (the constants below), for the
// schema, the reader and the catalog of the files alike, so none of them can speak of a key the others do not know.
namespace PlotSpec
{

namespace Detail
{
// The keys of a specification, and of the catalog that tells what the files hold, named once.
inline constexpr const char* kDesign = "design";
inline constexpr const char* kSeries = "series";
inline constexpr const char* kOf = "of";
inline constexpr const char* kFile = "file";
inline constexpr const char* kScheme = "scheme";
inline constexpr const char* kSection = "section";
inline constexpr const char* kLabel = "label";
inline constexpr const char* kColor = "color";
inline constexpr const char* kMarker = "marker";
inline constexpr const char* kMarkerSize = "marker_size";
inline constexpr const char* kSpecies = "species";
inline constexpr const char* kDy = "dy";
inline constexpr const char* kMult = "mult";
inline constexpr const char* kExtended = "extended";
inline constexpr const char* kTrend = "trend";
inline constexpr const char* kNumerator = "numerator";
inline constexpr const char* kDenominator = "denominator";
inline constexpr const char* kPurityKey = "purity_key";
inline constexpr const char* kParticle = "particle";
inline constexpr const char* kMap = "map";
inline constexpr const char* kBinning = "binning";
inline constexpr const char* kTitle = "title";
inline constexpr const char* kXTitle = "x_title";
inline constexpr const char* kYTitle = "y_title";
inline constexpr const char* kYRange = "y_range";
inline constexpr const char* kLogY = "log_y";
inline constexpr const char* kLegend = "legend";
inline constexpr const char* kLegendBox = "legend_box";
inline constexpr const char* kLegendColumns = "legend_columns";
inline constexpr const char* kWidth = "width";
inline constexpr const char* kHeight = "height";
inline constexpr const char* kTextPx = "text_px";
inline constexpr const char* kDrawOption = "draw_option";
inline constexpr const char* kReference = "reference";
inline constexpr const char* kDivideOption = "divide_option";
inline constexpr const char* kRatioRange = "ratio_range";
inline constexpr const char* kRatioTitle = "ratio_title";
inline constexpr const char* kPanels = "panels";
inline constexpr const char* kNCols = "n_cols";
inline constexpr const char* kZeroBaseline = "zero_baseline";
inline constexpr const char* kHeadroom = "headroom";
inline constexpr const char* kColorBy = "color_by";
inline constexpr const char* kShapeBy = "shape_by";
inline constexpr const char* kFillBy = "fill_by";
inline constexpr const char* kColors = "colors";
inline constexpr const char* kKeys = "keys";
inline constexpr const char* kNotes = "notes";
inline constexpr const char* kFact = "fact";
inline constexpr const char* kShows = "shows";
inline constexpr const char* kBox = "box";
inline constexpr const char* kColumns = "columns";
inline constexpr const char* kLabels = "labels";
inline constexpr const char* kReferenceObject = "reference_object";
inline constexpr const char* kSystematic = "systematic";
inline constexpr const char* kPath = "path";
inline constexpr const char* kCells = "cells";
inline constexpr const char* kPt = "pt";
// The value of dy or mult that asks for every one the file records: a set of series in one entry.
inline constexpr std::string_view kAll = "all";
// The catalog's own: what holds the keys above.
inline constexpr const char* kSchemes = "schemes";
inline constexpr const char* kKind = "kind";
inline constexpr const char* kSeriesForms = "series_forms";
inline constexpr const char* kSections = "sections";
inline constexpr const char* kSpeciesRatios = "species_ratios";
inline constexpr const char* kProducedAt = "produced_at";
inline constexpr const char* kGitCommit = "git_commit";

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
inline constexpr std::pair<std::string_view, PlotDesigns::Key::Shows> kShowsNames[] = {{"color", PlotDesigns::Key::Shows::Color},
                                                                                          {"fill", PlotDesigns::Key::Shows::Fill}};

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

// A key of a form and the schema of its value.
struct KeySchema {
  const char* name;
  std::string schema;
};
inline const std::string kRangeSchema = R"({"type": "array", "items": {"type": "number"}, "minItems": 2, "maxItems": 2})";
inline const std::string kStringSchema = R"({"type": "string"})";
// The keys several forms of series take, each written once in the schema's definitions and referred to by name (Ref).
inline std::vector<KeySchema> SeriesSharedKeys()
{
  return {
    {kFile, R"({"type": "string", "description": "A file the framework wrote."})"},
    {kScheme, R"({"type": "string", "description": "The binning_name; the only one in the file when absent."})"},
    {kSection, R"({"type": "string", "description": "Extract1D or Extract2D; the only one in the file when absent."})"},
    {kLabel, R"({"type": "string", "description": "The legend's text; written from what the series is when absent."})"},
    {kColor, R"({"type": "integer", "description": "A ROOT colour number."})"},
    {kMarker, R"({"type": "integer", "description": "A ROOT marker style."})"},
    {kMarkerSize, R"({"type": "number"})"},
    {kDy, std::format(R"({{"oneOf": [{{"type": "number"}}, {{"type": "array", "items": {{"type": "number"}}, "minItems": 1}}, {{"enum": ["{}"]}}], "description": "The |Delta y| limit, one the file holds (two decimals); a list of them, or \"{}\" for every one, makes one series each."}})",
                      kAll, kAll)},
    {kMult, std::format(R"({{"oneOf": [{0}, {{"type": "array", "items": {0}, "minItems": 1}}, {{"enum": ["{1}"]}}], "description": "A multiplicity class, low and high edge in percent, as the file's binning has it; a list of them, or \"{1}\" for every class, makes one series each."}})",
                        kRangeSchema, kAll)},
    {kBinning, std::format(R"({{"enum": [{}], "description": "The pT binning; {} when absent."}})", EnumList(kBinnings | std::views::keys), kDefaultBinning)},
  };
}
inline KeySchema Ref(const char* name) { return {name, std::format(R"({{"$ref": "#/definitions/{}"}})", name)}; }
inline const std::vector<KeySchema> kSeriesCommon = {Ref(kFile), Ref(kScheme), Ref(kLabel), Ref(kColor), Ref(kMarker), Ref(kMarkerSize)};

// One object form of the schema: a discriminating key with one value, the keys it takes, the ones it requires.
inline std::string Form(const char* discriminator, std::string_view value, std::string_view description, const std::vector<KeySchema>& keys,
                        std::vector<std::string_view> required)
{
  std::string properties = std::format(R"("{}": {{"enum": ["{}"], "description": "{}"}})", discriminator, value, description);
  for (const KeySchema& k : keys)
    properties += std::format(",\n      \"{}\": {}", k.name, k.schema);
  required.insert(required.begin(), discriminator);
  return std::format("\n    {{\"type\": \"object\", \"additionalProperties\": false, \"required\": [{}],\n     \"properties\": {{\n      {}}}}}",
                     EnumList(required), properties);
}

// A form of series: its schema and how it is found. 'read' gets the series' object, its file and where in it.
struct SeriesForm {
  std::string_view of;
  PlotSeries::FileKind reads; // the kind of file it is found in
  std::string_view description;
  std::vector<KeySchema> keys;
  std::vector<std::string_view> required;
  PlotSeries::Series (*read)(const rapidjson::Value&, const std::string& file, const PlotSeries::Where& where);
};

inline const std::vector<SeriesForm>& SeriesForms()
{
  using namespace PlotSeries;
  static const std::vector<SeriesForm> forms = {
    {"spectrum", FileKind::Spectra,
     "The pT spectrum of an associated species in one multiplicity class; with extended, the same extended to pT = 0 with its fitted curve.",
     {{kSpecies, kStringSchema}, Ref(kDy), Ref(kMult), Ref(kSection), {kExtended, R"({"type": "boolean"})"}},
     {kSpecies, kDy, kMult},
     [](const rapidjson::Value& s, const std::string& file, const Where& where) {
       return (JsonConfig::OptionalBool(s, kExtended, false, kCtx) ? SpectrumExtended : Spectrum)(
         file, JsonConfig::RequireString(s, kSpecies, kCtx), JsonConfig::RequireDouble(s, kDy, kCtx), RequireRange<Interval>(s, kMult), where);
     }},
    {"trend", FileKind::Spectra, "The yield of a species against multiplicity: measured, extrapolated to pT = 0, or the one over the other.",
     {{kSpecies, kStringSchema}, Ref(kDy), {kTrend, std::format(R"({{"enum": [{}]}})", EnumList(kTrendKinds | std::views::keys))}, Ref(kSection)},
     {kSpecies, kDy, kTrend},
     [](const rapidjson::Value& s, const std::string& file, const Where& where) {
       return Trend(file, JsonConfig::RequireString(s, kSpecies, kCtx), JsonConfig::RequireDouble(s, kDy, kCtx),
                    Resolve(kTrendKinds, JsonConfig::RequireString(s, kTrend, kCtx), "trend"), where);
     }},
    // A species ratio is measured or extrapolated, the first two kinds of a trend.
    {"species_ratio", FileKind::Spectra, "The ratio of two species' yields against multiplicity, as the correlation task wrote it.",
     {{kNumerator, kStringSchema}, {kDenominator, kStringSchema}, Ref(kDy),
      {kTrend, std::format(R"({{"enum": [{}]}})", EnumList(std::span(kTrendKinds).first(2) | std::views::keys))}, Ref(kSection)},
     {kNumerator, kDenominator, kDy, kTrend},
     [](const rapidjson::Value& s, const std::string& file, const Where& where) {
       return SpeciesRatio(file, JsonConfig::RequireString(s, kNumerator, kCtx), JsonConfig::RequireString(s, kDenominator, kCtx),
                           JsonConfig::RequireDouble(s, kDy, kCtx), Resolve(kTrendKinds, JsonConfig::RequireString(s, kTrend, kCtx), "trend"),
                           where);
     }},
    {"purity", FileKind::Purity, "The purity S/(S+B) of a purity key against pT, in one multiplicity class.",
     {{kPurityKey, kStringSchema}, Ref(kMult), Ref(kBinning)},
     {kPurityKey, kMult},
     [](const rapidjson::Value& s, const std::string& file, const Where& where) {
       return Purity(file, JsonConfig::RequireString(s, kPurityKey, kCtx), RequireRange<Interval>(s, kMult),
                     Resolve(kBinnings, JsonConfig::OptionalString(s, kBinning, std::string(kDefaultBinning), kCtx), "binning"), where.scheme);
     }},
    {"correction_map", FileKind::Corrections,
     "A correction map of a particle against pT, in one multiplicity class or integrated over multiplicity (no mult).",
     {{kParticle, kStringSchema}, {kMap, std::format(R"({{"enum": [{}]}})", EnumList(kMaps | std::views::keys))}, Ref(kMult), Ref(kBinning)},
     {kParticle, kMap},
     [](const rapidjson::Value& s, const std::string& file, const Where& where) {
       return CorrectionMap(file, JsonConfig::RequireString(s, kParticle, kCtx), Resolve(kMaps, JsonConfig::RequireString(s, kMap, kCtx), "correction map"),
                            JsonConfig::TryRange<Interval>(s, kMult, kCtx),
                            Resolve(kBinnings, JsonConfig::OptionalString(s, kBinning, std::string(kDefaultBinning), kCtx), "binning"), where.scheme);
     }},
  };
  return forms;
}

// The keys of a panel besides its series, and of the frame, each written once in the definitions (every design with a
// panel at its top level, and the panel objects, take them).
inline std::vector<KeySchema> PanelValueKeys()
{
  const PlotDesigns::Panel panel;
  const std::string facts = R"({"$ref": "#/definitions/fact"})";
  return {
    {kTitle, R"({"type": "string", "description": "Written in the top left corner of the pad."})"},
    {kXTitle, kStringSchema},
    {kYTitle, kStringSchema},
    {kLogY, R"({"type": "boolean"})"},
    {kZeroBaseline, std::format(R"({{"type": "boolean", "description": "The automatic Y range starts at 0; {} when absent."}})", panel.zeroBaseline)},
    {kHeadroom, std::format(R"({{"type": "number", "minimum": 0, "description": "Room above the data, a fraction of their span; {} when absent."}})", panel.headroom)},
    {kLegend, std::format(R"({{"type": "boolean", "description": "The legend written from what the series are; {} when absent."}})", panel.legend)},
    {kColorBy, facts},
    {kShapeBy, facts},
    {kFillBy, facts},
    {kColors, R"({"type": "array", "items": {"type": "integer"}, "description": "The ROOT colours color_by gives the values, in order."})"},
    {kKeys, R"({"type": "array", "items": {"$ref": "#/definitions/key"}, "description": "Legends of the look: what the colour or the filling stands for, by the fact it was taken from."})"},
    {kNotes, R"({"type": "array", "items": {"type": "string"}, "description": "Lines of text under the title."})"},
  };
}

inline std::vector<KeySchema> FrameValueKeys()
{
  const PlotDesigns::Frame frame;
  return {
    {kLegendBox, R"({"type": "array", "items": {"type": "number"}, "minItems": 4, "maxItems": 4, "description": "x1, y1, x2, y2 of the legend, fractions of the pad."})"},
    {kLegendColumns, R"({"type": "integer", "minimum": 1})"},
    {kWidth, std::format(R"({{"type": "integer", "minimum": 200, "description": "Pixels; {} when absent."}})", frame.width)},
    {kHeight, std::format(R"({{"type": "integer", "minimum": 200, "description": "Pixels; {} when absent."}})", frame.height)},
    {kTextPx, std::format(R"({{"type": "integer", "minimum": 8, "description": "Size of the text in pixels; {} when absent."}})", frame.textPx)},
    {kDrawOption, std::format(R"({{"enum": [{}], "description": "E1 error bars with ends, E without, P markers only; {} when absent."}})",
                              EnumList(kDrawOptions), frame.drawOption)},
  };
}

// Every key written once in the definitions.
inline std::vector<KeySchema> SharedKeys()
{
  std::vector<KeySchema> keys = SeriesSharedKeys();
  for (auto more : {PanelValueKeys(), FrameValueKeys()})
    keys.insert(keys.end(), more.begin(), more.end());
  keys.push_back({kYRange, kRangeSchema});
  return keys;
}

// The keys of a panel: its series, at least 'minSeries' of them, how its axes read, its look by fact, its legends and
// notes. 'withYRange' is false where the range is the design's (joined pads).
inline std::vector<KeySchema> PanelKeys(int minSeries, bool withYRange)
{
  std::vector<KeySchema> keys = {
    {kSeries, std::format(R"({{"type": "array", "minItems": {}, "items": {{"$ref": "#/definitions/series"}}, "description": "What is drawn, each found by what it is in a file the framework wrote."}})",
                          minSeries)}};
  for (const KeySchema& k : PanelValueKeys())
    keys.push_back(Ref(k.name));
  if (withYRange)
    keys.push_back(Ref(kYRange));
  return keys;
}

// The keys of the frame: the canvas and its text, the draw option, the place of the legend.
inline std::vector<KeySchema> FrameKeys()
{
  std::vector<KeySchema> keys;
  for (const KeySchema& k : FrameValueKeys())
    keys.push_back(Ref(k.name));
  return keys;
}

inline std::vector<KeySchema> Joined(std::vector<KeySchema> a, const std::vector<KeySchema>& b)
{
  a.insert(a.end(), b.begin(), b.end());
  return a;
}

// The forms of a series, written once and referred to by every panel (#/definitions/series).
inline std::string SeriesDefinition()
{
  std::string forms;
  for (const SeriesForm& f : SeriesForms()) {
    std::vector<KeySchema> keys = kSeriesCommon;
    keys.insert(keys.end(), f.keys.begin(), f.keys.end());
    std::vector<std::string_view> required{kFile};
    required.insert(required.end(), f.required.begin(), f.required.end());
    forms += (forms.empty() ? "" : ",") + Form(kOf, f.of, f.description, keys, required);
  }
  return std::format(R"({{"oneOf": [{}]}})", forms);
}

// The facts a look or a key is taken from (#/definitions/fact), and a key (#/definitions/key).
// A fact is named as PlotSeries names it, or by the key of a series that gives it (the trend or the map tell the kind;
// the purity key, the particle or the numerator tell the species): what a person, or a model, writing a series calls it.
inline const std::vector<std::pair<std::string_view, PlotSeries::Fact>>& FactNames()
{
  static const std::vector<std::pair<std::string_view, PlotSeries::Fact>> names = [] {
    std::vector<std::pair<std::string_view, PlotSeries::Fact>> n(std::begin(PlotSeries::kFacts), std::end(PlotSeries::kFacts));
    n.insert(n.end(), {{kTrend, &PlotSeries::Facts::kind}, {kMap, &PlotSeries::Facts::kind}, {kPurityKey, &PlotSeries::Facts::species},
                       {kParticle, &PlotSeries::Facts::species}, {kNumerator, &PlotSeries::Facts::species}});
    return n;
  }();
  return names;
}
inline std::string FactDefinition() { return std::format(R"({{"enum": [{}]}})", EnumList(FactNames() | std::views::keys)); }
inline std::string KeyDefinition()
{
  return std::format(
    R"({{"type": "object", "additionalProperties": false, "required": ["{}", "{}", "{}"], "properties": {{"{}": {{"$ref": "#/definitions/fact"}}, "{}": {{"enum": [{}]}}, "{}": {{"type": "array", "items": {{"type": "number"}}, "minItems": 4, "maxItems": 4, "description": "x1, y1, x2, y2, fractions of the pad."}}, "{}": {{"type": "integer", "minimum": 1}}, "{}": {{"type": "array", "items": {{"type": "string"}}, "description": "The words of the entries, one per value of the fact in the order the series bring them; what tells the values apart when absent."}}}}}})",
    kFact, kShows, kBox, kFact, kShows, EnumList(kShowsNames | std::views::keys), kBox, kColumns, kLabels);
}

// An object of a file the framework did not write (a preliminary): by its path, only its X axis checked
// (#/definitions/object).
inline std::string ObjectDefinition()
{
  return std::format(
    R"({{"type": "object", "additionalProperties": false, "required": ["{}", "{}"], "properties": {{"{}": {{"type": "string"}}, "{}": {{"type": "string", "description": "The object's path in the file."}}, "{}": {{"type": "string"}}, "{}": {{"type": "integer"}}, "{}": {{"type": "integer"}}, "{}": {{"type": "number"}}}}}})",
    kFile, kPath, kFile, kPath, kLabel, kColor, kMarker, kMarkerSize);
}

// A panel as an object of its own, for the designs with several (#/definitions/panel, and joined_panel without its
// own Y range).
inline std::string PanelObject(bool withYRange)
{
  std::string properties;
  for (const KeySchema& k : PanelKeys(1, withYRange))
    properties += std::format("{}\n      \"{}\": {}", properties.empty() ? "" : ",", k.name, k.schema);
  return std::format("{{\"type\": \"object\", \"additionalProperties\": false, \"required\": [\"{}\"], \"properties\": {{{}}}}}", kSeries, properties);
}

// One form per design: a single panel's keys at the top for overlay and compare_with_ratio (which needs two series and
// its divide option); panels of their own for grid (and its columns) and joined_pads (and the one Y range they share).
inline std::string DesignForms()
{
  const std::vector<KeySchema> ratioKeys = {
    {kReference, R"({"type": "integer", "minimum": 0, "description": "The index of the series the others are divided by; 0 when absent."})"},
    {kDivideOption, std::format(R"({{"enum": [{}], "description": "\"\" for independent samples, \"B\" when the numerator is a subset of the denominator (a closure)."}})",
                                EnumList(kDivideOptions))},
    {kRatioRange, kRangeSchema}, {kRatioTitle, kStringSchema}};
  const std::vector<KeySchema> gridKeys = {
    {kPanels, R"({"type": "array", "minItems": 1, "items": {"$ref": "#/definitions/panel"}})"},
    {kNCols, R"({"type": "integer", "minimum": 1, "description": "Panels per row."})"}};
  const std::vector<KeySchema> joinedKeys = {
    {kPanels, R"({"type": "array", "minItems": 2, "items": {"$ref": "#/definitions/joined_panel"}})"},
    Ref(kYRange)};
  const std::string object = R"({"$ref": "#/definitions/object"})";
  const std::vector<KeySchema> referenceKeys = {
    {kReferenceObject, object}, {kSystematic, object}, ratioKeys[1], {kRatioRange, kRangeSchema}};
  const std::string interval = R"({"type": "array", "items": {"type": "number"}, "minItems": 2, "maxItems": 2})";
  const std::vector<KeySchema> fitKeys = {
    {kFile, R"({"type": "string", "description": "A purity file, or the phi fit file."})"},
    Ref(kScheme),
    {kPurityKey, R"({"type": "string", "description": "The purity key whose fits are shown; the phi's when absent."})"},
    {kCells, std::format(R"({{"type": "array", "minItems": 1, "items": {{"type": "object", "additionalProperties": false, "required": ["{}", "{}"], "properties": {{"{}": {}, "{}": {}, "{}": {{"type": "string"}}}}}}, "description": "The fits shown: a multiplicity class and a pT interval each, as the fits' binning has them."}})",
                         kMult, kPt, kMult, interval, kPt, interval, kLabel)},
    {kNCols, R"({"type": "integer", "minimum": 1, "description": "Fits per row."})"},
    Ref(kBinning), Ref(kWidth), Ref(kHeight), Ref(kTextPx)};
  return Form(kDesign, "overlay", "Every series on one pad.", Joined(PanelKeys(1, true), FrameKeys()), {kSeries}) + "," +
         Form(kDesign, "with_reference", "The series against an object of a file the framework did not write (a preliminary), with its systematic uncertainty as a band, and the ratio of each to it below.",
              Joined(Joined(PanelKeys(1, true), FrameKeys()), referenceKeys), {kSeries, kReferenceObject, kDivideOption}) + "," +
         Form(kDesign, "fit_canvas_grid", "The mass-fit canvases of some cells, already in the file, laid out in a grid.", fitKeys,
              {kFile, kCells, kNCols}) + "," +
         Form(kDesign, "compare_with_ratio", "The series on top, and below the ratio of each to the reference series.",
              Joined(Joined(PanelKeys(2, true), FrameKeys()), ratioKeys), {kSeries, kDivideOption}) + "," +
         Form(kDesign, "grid", "Panels in a grid, row by row, n_cols per row.", Joined(gridKeys, FrameKeys()), {kPanels, kNCols}) + "," +
         Form(kDesign, "joined_pads", "Panels side by side, joined, with one Y axis and one range for all.", Joined(joinedKeys, FrameKeys()), {kPanels});
}

inline std::string PointerText(const rapidjson::Pointer& pointer)
{
  rapidjson::StringBuffer buffer;
  pointer.Stringify(buffer);
  return buffer.GetString();
}

// Why 'value' fails 'schema', said in words: the key an object does not take, the keys it lacks, the values allowed, or
// for an object matching none of a oneOf's forms, its own form's failure (by its "design" or "of") or the forms there are.
inline std::string Explain(const rapidjson::Value& schema, const rapidjson::Value& value, const std::string& at, const rapidjson::Value& definitions)
{
  // A sub-schema checked on its own still needs the definitions its references point to.
  rapidjson::Document withDefinitions;
  withDefinitions.CopyFrom(schema, withDefinitions.GetAllocator());
  if (withDefinitions.IsObject() && !withDefinitions.HasMember("definitions"))
    withDefinitions.AddMember("definitions", rapidjson::Value(definitions, withDefinitions.GetAllocator()), withDefinitions.GetAllocator());
  rapidjson::SchemaDocument schemaDoc(withDefinitions);
  rapidjson::SchemaValidator validator(schemaDoc);
  if (value.Accept(validator))
    return "";
  const std::string keyword = validator.GetInvalidSchemaKeyword();
  const rapidjson::Pointer& docPointer = validator.GetInvalidDocumentPointer();
  const std::string inner = at + PointerText(docPointer);
  const std::string where = inner.empty() ? "/" : inner;
  const rapidjson::Value* failedSchema = validator.GetInvalidSchemaPointer().Get(withDefinitions);
  const rapidjson::Value* failedValue = docPointer.Get(value);
  // A key the object does not take: the validator points at the key itself, its schema is the object's.
  if (keyword == "additionalProperties" && failedSchema && failedSchema->HasMember("properties") && docPointer.GetTokenCount() > 0) {
    const rapidjson::Value& properties = (*failedSchema)["properties"];
    const std::string object = at + PointerText(rapidjson::Pointer(docPointer.GetTokens(), docPointer.GetTokenCount() - 1));
    const std::string key = docPointer.GetTokens()[docPointer.GetTokenCount() - 1].name;
    std::vector<std::string> known;
    for (const auto& p : properties.GetObject())
      known.emplace_back(p.name.GetString());
    // In a series, the forms that do take the key: what to change is often the form, not the key.
    std::string hint;
    if (properties.HasMember(kOf)) {
      std::vector<std::string_view> takenBy;
      for (const SeriesForm& f : SeriesForms())
        if (std::ranges::any_of(f.keys, [&](const KeySchema& k) { return key == k.name; }))
          takenBy.push_back(f.of);
      if (!takenBy.empty())
        hint = std::format(" ('{}' belongs to a series of: {})", key, Logger::Join(takenBy, ", "));
    }
    return std::format("{}: unknown key '{}'; the object takes {}{}", object.empty() ? "/" : object, key, Logger::Join(known, ", "), hint);
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
      for (const char* discriminator : {kDesign, kOf})
        if (form["properties"].HasMember(discriminator)) {
          const rapidjson::Value& name = form["properties"][discriminator]["enum"][0];
          if (failedValue->HasMember(discriminator) && (*failedValue)[discriminator] == name)
            return Explain(form, *failedValue, inner, definitions);
          kinds.push_back(std::format("{} {}", discriminator, name.GetString()));
        }
    return std::format("{}: matches none of the forms ({})", where, Logger::Join(kinds, ", "));
  }
  // A value of the wrong shape for a key that takes several (dy, mult): what the key takes, in its own words.
  if (keyword == "oneOf" && failedSchema && failedSchema->HasMember("description"))
    return std::format("{}: {}", where, (*failedSchema)["description"].GetString());
  return std::format("{}: fails '{}' (schema {})", where, keyword, PointerText(validator.GetInvalidSchemaPointer()));
}
} // namespace Detail

// The schema: what a specification may say. Checked to be JSON when it is built, before anyone is given it.
inline const std::string& Schema()
{
  static const std::string text = [] {
    std::string shared;
    for (const Detail::KeySchema& k : Detail::SharedKeys())
      shared += std::format(", \"{}\": {}", k.name, k.schema);
    std::string t = std::format("{{\n  \"$schema\": \"http://json-schema.org/draft-04/schema#\",\n"
                                "  \"title\": \"A plot of the phi-strangeness analysis: one design and the series it draws\",\n"
                                "  \"definitions\": {{\"series\": {}, \"panel\": {}, \"joined_panel\": {}, \"object\": {}, \"fact\": {}, \"key\": {}{}}},\n"
                                "  \"oneOf\": [{}]\n}}",
                                Detail::SeriesDefinition(), Detail::PanelObject(true), Detail::PanelObject(false), Detail::ObjectDefinition(),
                                Detail::FactDefinition(), Detail::KeyDefinition(), shared,
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
  if (const std::string why = Detail::Explain(schema, spec, "", schema["definitions"]); !why.empty())
    throw std::runtime_error(std::format("[FATAL] PlotSpec: '{}' is not a valid specification: {}.", origin, why));
  return spec;
}

namespace Detail
{
// The values a set key asks for: one, a list, or every one the file records ("all"); none when the key is absent.
template <class Value, class Every, class One>
std::vector<std::optional<Value>> SetValues(const rapidjson::Value& s, const char* key, Every every, One one)
{
  const rapidjson::Value* v = JsonConfig::OptionalMember(s, key);
  if (!v)
    return {std::nullopt};
  std::vector<std::optional<Value>> values;
  if (v->IsString())
    for (const Value& each : every())
      values.emplace_back(each);
  else if (v->IsArray() && !v->Empty() && (v->Begin()->IsArray() || std::string_view(key) == kDy))
    for (const auto& each : v->GetArray())
      values.emplace_back(one(each));
  else
    values.emplace_back(one(*v));
  return values;
}

// The series an entry asks for: one, or a set when its dy or mult is a list or "all", the classes outer and the |dy|
// inner; each with the entry's look and label.
inline std::vector<PlotSeries::Series> ReadSeries(const rapidjson::Value& s)
{
  const std::string of = JsonConfig::RequireString(s, kOf, kCtx);
  const auto form = std::ranges::find(SeriesForms(), of, &SeriesForm::of);
  if (form == SeriesForms().end())
    throw std::logic_error(std::format("[FATAL] PlotSpec: the schema let through a series of kind '{}'.", of));
  const std::string file = JsonConfig::RequireString(s, kFile, kCtx);
  const PlotSeries::Where where{JsonConfig::OptionalString(s, kScheme, "", kCtx), JsonConfig::OptionalString(s, kSection, "", kCtx)};
  const auto dys = SetValues<double>(s, kDy, [&] { return PlotSeries::DeltaYLimits(file, where); },
                                     [](const rapidjson::Value& v) { return JsonConfig::ToNumber(v, kDy, kCtx); });
  const auto mults = SetValues<PlotSeries::Interval>(s, kMult, [&] { return PlotSeries::Intervals(file, where.scheme); }, [](const rapidjson::Value& v) {
    const std::vector<double> edges = JsonConfig::ReadNumberArray(JsonConfig::ToArray(v, kMult, kCtx), kMult, kCtx);
    if (edges.size() != 2 || !(edges[0] < edges[1]))
      throw std::runtime_error(std::format("[FATAL] PlotSpec: a 'mult' is [low, high] with low < high, not [{}].", Logger::Join(edges, ", ")));
    return PlotSeries::Interval{edges[0], edges[1]};
  });
  const std::string label = JsonConfig::OptionalString(s, kLabel, "", kCtx);
  std::vector<PlotSeries::Series> series;
  for (const std::optional<PlotSeries::Interval>& mult : mults)
    for (const std::optional<double>& dy : dys) {
      // The entry with one value in place of the set, read by its form as any single series.
      rapidjson::Document one;
      one.CopyFrom(s, one.GetAllocator());
      if (dy)
        one[kDy].SetDouble(*dy);
      if (mult) {
        rapidjson::Value interval(rapidjson::kArrayType);
        interval.PushBack(mult->low, one.GetAllocator()).PushBack(mult->high, one.GetAllocator());
        one[kMult] = interval;
      }
      PlotSeries::Series read = form->read(one, file, where);
      // A label given to a set names what its series share; each adds what tells it apart.
      read.label = label;
      if (!label.empty() && dys.size() > 1)
        read.label += ", " + read.facts.dy.value_or("");
      if (!label.empty() && mults.size() > 1)
        read.label += ", " + read.facts.mult.value_or("");
      read.look = {.color = JsonConfig::TryInt(s, kColor, kCtx), .marker = JsonConfig::TryInt(s, kMarker, kCtx),
                   .markerSize = JsonConfig::TryDouble(s, kMarkerSize, kCtx)};
      series.push_back(std::move(read));
    }
  return series;
}

inline PlotSeries::Fact FactNamed(const std::string& name)
{
  for (const auto& [n, fact] : FactNames())
    if (n == name)
      return fact;
  throw std::logic_error(std::format("[FATAL] PlotSpec: the schema let through a fact '{}'.", name));
}

// A panel: its series, then their looks by fact (colour, shape, filling, in that order), its axes, legends and notes.
inline PlotDesigns::Panel ReadPanel(const rapidjson::Value& p)
{
  PlotDesigns::Panel panel;
  for (const auto& s : JsonConfig::RequireArray(p, kSeries, kCtx))
    std::ranges::move(ReadSeries(s), std::back_inserter(panel.series));
  if (auto by = JsonConfig::TryString(p, kColorBy, kCtx)) {
    std::vector<int> palette;
    if (auto colors = JsonConfig::TryArray(p, kColors, kCtx))
      for (double c : JsonConfig::ReadNumberArray(*colors, kColors, kCtx))
        palette.push_back(static_cast<int>(c));
    PlotDesigns::ColourBy(panel.series, FactNamed(*by), palette);
  }
  if (auto by = JsonConfig::TryString(p, kShapeBy, kCtx))
    PlotDesigns::ShapeBy(panel.series, FactNamed(*by));
  if (auto by = JsonConfig::TryString(p, kFillBy, kCtx))
    PlotDesigns::FillBy(panel.series, FactNamed(*by));
  panel.title = JsonConfig::OptionalString(p, kTitle, "", kCtx);
  panel.xTitle = JsonConfig::OptionalString(p, kXTitle, "", kCtx);
  panel.yTitle = JsonConfig::OptionalString(p, kYTitle, "", kCtx);
  panel.yRange = JsonConfig::TryRange<std::pair<double, double>>(p, kYRange, kCtx);
  panel.logY = JsonConfig::OptionalBool(p, kLogY, panel.logY, kCtx);
  panel.zeroBaseline = JsonConfig::OptionalBool(p, kZeroBaseline, panel.zeroBaseline, kCtx);
  panel.headroom = JsonConfig::OptionalDouble(p, kHeadroom, panel.headroom, kCtx);
  panel.legend = JsonConfig::OptionalBool(p, kLegend, panel.legend, kCtx);
  if (auto keys = JsonConfig::TryArray(p, kKeys, kCtx))
    for (const auto& k : *keys) {
      const std::vector<double> box = JsonConfig::ReadNumberArray(JsonConfig::RequireArray(k, kBox, kCtx), kBox, kCtx);
      const PlotSeries::Fact fact = FactNamed(JsonConfig::RequireString(k, kFact, kCtx));
      // The words, by position: the values of the fact in the order the series bring them, one each.
      std::vector<std::pair<std::string, std::string>> text;
      if (auto labels = JsonConfig::TryArray(k, kLabels, kCtx)) {
        std::vector<std::string> values;
        for (const PlotSeries::Series& series : panel.series)
          if (const auto& v = series.facts.*fact; v && std::ranges::find(values, *v) == values.end())
            values.push_back(*v);
        if (labels->Size() != values.size())
          throw std::runtime_error(std::format("[FATAL] PlotSpec: a key gives {} labels for {} values ({}).", labels->Size(), values.size(),
                                               Logger::Join(values, ", ")));
        for (std::size_t i = 0; i < values.size(); ++i)
          text.emplace_back(values[i], JsonConfig::ToString((*labels)[static_cast<rapidjson::SizeType>(i)], kLabels, kCtx));
      }
      panel.keys.push_back({.fact = fact,
                            .shows = Resolve(kShowsNames, JsonConfig::RequireString(k, kShows, kCtx), "key"),
                            .where = {box[0], box[1], box[2], box[3], JsonConfig::OptionalInt(k, kColumns, 1, kCtx)},
                            .text = std::move(text)});
    }
  if (auto notes = JsonConfig::TryArray(p, kNotes, kCtx))
    for (const auto& n : *notes)
      panel.notes.push_back(JsonConfig::ToString(n, kNotes, kCtx));
  return panel;
}

inline PlotDesigns::Frame ReadFrame(const rapidjson::Value& spec)
{
  PlotDesigns::Frame frame;
  frame.width = JsonConfig::OptionalInt(spec, kWidth, frame.width, kCtx);
  frame.height = JsonConfig::OptionalInt(spec, kHeight, frame.height, kCtx);
  frame.textPx = JsonConfig::OptionalInt(spec, kTextPx, frame.textPx, kCtx);
  frame.drawOption = JsonConfig::OptionalString(spec, kDrawOption, frame.drawOption, kCtx);
  if (auto box = JsonConfig::TryArray(spec, kLegendBox, kCtx)) {
    const std::vector<double> corners = JsonConfig::ReadNumberArray(*box, kLegendBox, kCtx);
    frame.legend = {corners[0], corners[1], corners[2], corners[3], frame.legend.columns};
  }
  frame.legend.columns = JsonConfig::OptionalInt(spec, kLegendColumns, frame.legend.columns, kCtx);
  return frame;
}

inline std::vector<PlotDesigns::Panel> ReadPanels(const rapidjson::Value& spec)
{
  std::vector<PlotDesigns::Panel> panels;
  for (const auto& p : JsonConfig::RequireArray(spec, kPanels, kCtx))
    panels.push_back(ReadPanel(p));
  return panels;
}
} // namespace Detail

// What a specification draws: its design run on its panels, on 'canvases'. The ALICE label is not the specification's
// to say (the collision system is not in the files): the caller gives it, or none.
inline PlotDesigns::Drawn Draw(const rapidjson::Document& spec, CanvasManager& canvases, const std::string& name,
                               const std::optional<std::pair<std::string, std::string>>& alice = std::nullopt)
{
  using namespace Detail;
  PlotDesigns::Frame frame = ReadFrame(spec);
  frame.alice = alice;
  const std::string design = JsonConfig::RequireString(spec, kDesign, kCtx);
  if (design == "overlay")
    return PlotDesigns::Overlay(canvases, name, ReadPanel(spec), frame);
  if (design == "compare_with_ratio")
    return PlotDesigns::CompareWithRatio(canvases, name, ReadPanel(spec), static_cast<std::size_t>(JsonConfig::OptionalInt(spec, kReference, 0, kCtx)),
                                         JsonConfig::RequireString(spec, kDivideOption, kCtx),
                                         JsonConfig::TryRange<std::pair<double, double>>(spec, kRatioRange, kCtx),
                                         JsonConfig::OptionalString(spec, kRatioTitle, "ratio", kCtx), frame);
  if (design == "with_reference") {
    auto object = [](const rapidjson::Value& o) {
      PlotSeries::Series s = PlotSeries::Object(JsonConfig::RequireString(o, kFile, kCtx), JsonConfig::RequireString(o, kPath, kCtx));
      s.label = JsonConfig::OptionalString(o, kLabel, "", kCtx);
      s.look = {.color = JsonConfig::TryInt(o, kColor, kCtx), .marker = JsonConfig::TryInt(o, kMarker, kCtx),
                .markerSize = JsonConfig::TryDouble(o, kMarkerSize, kCtx)};
      return s;
    };
    const rapidjson::Value* systematic = JsonConfig::OptionalMember(spec, kSystematic);
    return PlotDesigns::WithReference(canvases, name, ReadPanel(spec), object(JsonConfig::RequireMember(spec, kReferenceObject, kCtx)),
                                      systematic ? std::optional(object(*systematic)) : std::nullopt, JsonConfig::RequireString(spec, kDivideOption, kCtx),
                                      JsonConfig::TryRange<std::pair<double, double>>(spec, kRatioRange, kCtx), frame);
  }
  if (design == "fit_canvas_grid") {
    const std::string file = JsonConfig::RequireString(spec, kFile, kCtx);
    std::vector<PlotDesigns::FitCell> cells;
    for (const auto& c : JsonConfig::RequireArray(spec, kCells, kCtx))
      cells.push_back({RequireRange<PlotSeries::Interval>(c, kMult), RequireRange<PlotSeries::Interval>(c, kPt),
                       JsonConfig::OptionalString(c, kLabel, "", kCtx)});
    return PlotDesigns::FitCanvasGrid(canvases, name, file, PlotSeries::SchemeName(file, JsonConfig::OptionalString(spec, kScheme, "", kCtx)),
                                      JsonConfig::TryString(spec, kPurityKey, kCtx), cells, JsonConfig::OptionalInt(spec, kNCols, 1, kCtx),
                                      Resolve(kBinnings, JsonConfig::OptionalString(spec, kBinning, std::string(kDefaultBinning), kCtx), "binning"), frame);
  }
  if (design == "grid")
    return PlotDesigns::Grid(canvases, name, ReadPanels(spec), JsonConfig::OptionalInt(spec, kNCols, 1, kCtx), frame);
  if (design == "joined_pads")
    return PlotDesigns::JoinedPads(canvases, name, ReadPanels(spec), JsonConfig::TryRange<std::pair<double, double>>(spec, kYRange, kCtx), frame);
  throw std::logic_error(std::format("[FATAL] PlotSpec: the schema let through a design '{}' with nothing to draw it.", design));
}

// What the files hold, as JSON in the specification's own keys, with the forms of series each file can be asked for:
// what a front end gives the model along with the schema, so that it names only what is there; and what a person reads
// to write a specification by hand (phistrangeplot list).
inline std::string CatalogJson(const std::vector<PlotSeries::Catalog>& catalogs)
{
  using namespace Detail;
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
    file.AddMember(rapidjson::StringRef(kFile), str(catalog.fileName), a);
    for (const PlotSeries::Catalog::Scheme& scheme : catalog.schemes) {
      rapidjson::Value sc(rapidjson::kObjectType), classes(rapidjson::kArrayType), forms(rapidjson::kArrayType);
      sc.AddMember(rapidjson::StringRef(kScheme), str(scheme.name), a);
      sc.AddMember(rapidjson::StringRef(kKind), str(PlotSeries::Name(scheme.kind)), a);
      for (const SeriesForm& f : SeriesForms())
        if (f.reads == scheme.kind)
          forms.PushBack(str(f.of), a);
      sc.AddMember(rapidjson::StringRef(kSeriesForms), forms, a);
      for (const PlotSeries::Interval& c : scheme.classes) {
        rapidjson::Value interval(rapidjson::kArrayType);
        interval.PushBack(c.low, a).PushBack(c.high, a);
        classes.PushBack(interval, a);
      }
      sc.AddMember(rapidjson::StringRef(kMult), classes, a);
      switch (scheme.kind) {
        case PlotSeries::FileKind::Corrections:
          sc.AddMember(rapidjson::StringRef(kParticle), strings(scheme.particles), a);
          break;
        case PlotSeries::FileKind::Purity:
          sc.AddMember(rapidjson::StringRef(kPurityKey), strings(scheme.purityKeys), a);
          break;
        case PlotSeries::FileKind::Spectra: {
          rapidjson::Value sections(rapidjson::kArrayType);
          for (const PlotSeries::Catalog::Section& section : scheme.sections) {
            rapidjson::Value sec(rapidjson::kObjectType), dys(rapidjson::kArrayType), ratios(rapidjson::kArrayType);
            sec.AddMember(rapidjson::StringRef(kSection), str(section.name), a);
            sec.AddMember(rapidjson::StringRef(kSpecies), strings(section.species), a);
            for (double dy : section.deltaYLimits)
              dys.PushBack(dy, a);
            sec.AddMember(rapidjson::StringRef(kDy), dys, a);
            for (const auto& [num, den] : section.speciesRatios) {
              rapidjson::Value r(rapidjson::kObjectType);
              r.AddMember(rapidjson::StringRef(kNumerator), str(num), a).AddMember(rapidjson::StringRef(kDenominator), str(den), a);
              ratios.PushBack(r, a);
            }
            sec.AddMember(rapidjson::StringRef(kSpeciesRatios), ratios, a);
            sections.PushBack(sec, a);
          }
          sc.AddMember(rapidjson::StringRef(kSections), sections, a);
          break;
        }
      }
      sc.AddMember(rapidjson::StringRef(kProducedAt), str(scheme.producedAt), a);
      sc.AddMember(rapidjson::StringRef(kGitCommit), str(scheme.gitCommit), a);
      schemes.PushBack(sc, a);
    }
    file.AddMember(rapidjson::StringRef(kSchemes), schemes, a);
    doc.PushBack(file, a);
  }
  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
  writer.SetIndent(' ', 2);
  doc.Accept(writer);
  return buffer.GetString();
}

} // namespace PlotSpec
