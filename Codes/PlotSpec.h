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
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// A plot as data: one design of PlotDesigns and the series it draws, named the way a C++ composition names them
// (PlotSeries' finders), with the cosmetics of Panel and Frame. What a local model writes when a plot is asked for in
// words (PLOTS.md, section 11); it can only name what the components find and check, and every check is theirs: a
// specification is translated into the same calls a composition makes. The schema is the one description of what a
// specification may say, each design's and each series' rules included: `phistrangeplot schema` prints it, every
// specification is validated against it before it is read, and a front end gives it to the model. It is built from the
// tables below, which the reader dispatches through, and every key is named once (the constants below), for the
// schema, the reader and the catalog of the files alike, so none of them can speak of a key the others do not know;
// the schema of a key two forms share is written once, in the definitions, and referred to.
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
inline constexpr const char* kMultUnion = "mult_union";
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
inline constexpr const char* kZeroBaseline = "zero_baseline";
inline constexpr const char* kHeadroom = "headroom";
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
inline constexpr const char* kRatioGrid = "ratio_grid";
inline constexpr const char* kLegendHeader = "legend_header";
inline constexpr const char* kPanels = "panels";
inline constexpr const char* kNCols = "n_cols";
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
// The catalog's own: what holds the keys above.
inline constexpr const char* kSchemes = "schemes";
inline constexpr const char* kKind = "kind";
inline constexpr const char* kSeriesForms = "series_forms";
inline constexpr const char* kSections = "sections";
inline constexpr const char* kSpeciesRatios = "species_ratios";
inline constexpr const char* kProducedAt = "produced_at";
inline constexpr const char* kGitCommit = "git_commit";
// The value of dy or mult that asks for every one the file records: a set of series in one entry.
inline constexpr const char* kAll = "all";

// The names a specification may use for each closed list, once.
inline constexpr std::pair<std::string_view, PlotSeries::TrendKind> kTrendKinds[] = {
  {"measured", PlotSeries::TrendKind::Measured}, {"extrapolated", PlotSeries::TrendKind::Extrapolated},
  {"extrap_over_meas", PlotSeries::TrendKind::ExtrapOverMeas}};
inline constexpr std::pair<std::string_view, OutputNames::SpectrumBinning> kBinnings[] = {
  {"analysis", OutputNames::SpectrumBinning::Analysis}, {"source", OutputNames::SpectrumBinning::Source}};
inline constexpr const char* kDefaultBinning = "analysis";
inline constexpr std::pair<std::string_view, OutputNames::CorrectionMap> kMaps[] = {
  {"efficiency", OutputNames::CorrectionMap::Efficiency}, {"signal_loss", OutputNames::CorrectionMap::SignalLoss},
  {"signal_loss_over_event_loss", OutputNames::CorrectionMap::SignalLossOverEventLoss}};
inline constexpr std::string_view kDrawOptions[] = {"E1", "E", "P"};
inline constexpr std::string_view kDivideOptions[] = {"", "B"};
inline constexpr std::pair<std::string_view, PlotDesigns::Key::Shows> kShowsNames[] = {{"color", PlotDesigns::Key::Shows::Color},
                                                                                          {"fill", PlotDesigns::Key::Shows::Fill}};
// A fact is named as PlotSeries names it (kFacts), or by the key of a series that gives it alone (the trend or the map
// tell the kind; the purity key or the particle tell the species): what a person, or a model, writing a series calls
// it. Not the numerator: a species ratio's species is the pair.
inline const std::vector<std::pair<std::string_view, PlotSeries::Fact>>& FactNames()
{
  static const std::vector<std::pair<std::string_view, PlotSeries::Fact>> names = [] {
    std::vector<std::pair<std::string_view, PlotSeries::Fact>> all(std::begin(PlotSeries::kFacts), std::end(PlotSeries::kFacts));
    all.insert(all.end(), {{kTrend, &PlotSeries::Facts::kind},
                           {kMap, &PlotSeries::Facts::kind},
                           {kPurityKey, &PlotSeries::Facts::species},
                           {kParticle, &PlotSeries::Facts::species}});
    return all;
  }();
  return names;
}

// Names as the entries of a JSON "enum".
template <class Names>
std::string EnumList(Names&& names)
{
  return Logger::Join(names | std::views::transform([](std::string_view n) { return std::format("\"{}\"", n); }));
}

inline const std::string kCtx = "PlotSpec";

// The value of 'name' in one of the tables above, read under 'key' (JsonConfig::ResolveEnum, which lists what there is).
auto Resolve(const auto& table, std::string_view name, const char* key) { return JsonConfig::ResolveEnum(name, table, key, kCtx); }

inline OutputNames::SpectrumBinning ReadBinning(const rapidjson::Value& node)
{
  return Resolve(kBinnings, JsonConfig::OptionalString(node, kBinning, kDefaultBinning, kCtx), kBinning);
}

// A key of a form and the schema of its value.
struct KeySchema {
  const char* name;
  std::string schema;
};
inline KeySchema Ref(const char* name) { return {name, std::format(R"({{"$ref": "#/definitions/{}"}})", name)}; }
inline std::vector<KeySchema> Refs(const std::vector<KeySchema>& keys)
{
  std::vector<KeySchema> refs;
  for (const KeySchema& k : keys)
    refs.push_back(Ref(k.name));
  return refs;
}
inline std::vector<KeySchema> Joined(std::vector<KeySchema> a, const std::vector<KeySchema>& b)
{
  a.insert(a.end(), b.begin(), b.end());
  return a;
}
inline const std::string kRangeSchema = R"({"type": "array", "items": {"type": "number"}, "minItems": 2, "maxItems": 2})";
inline const std::string kStringSchema = R"({"type": "string"})";

// An object of the schema: the keys it takes, the ones it requires, and the keys that need others ("dependencies").
inline std::string ObjectSchema(const std::vector<KeySchema>& keys, const std::vector<std::string_view>& required,
                                const std::vector<std::pair<const char*, const char*>>& dependencies = {})
{
  std::string properties;
  for (const KeySchema& k : keys)
    properties += std::format("{}\"{}\": {}", properties.empty() ? "" : ", ", k.name, k.schema);
  std::string needs;
  for (const auto& [key, needed] : dependencies)
    needs += std::format("{}\"{}\": [\"{}\"]", needs.empty() ? "" : ", ", key, needed);
  return std::format(R"({{"type": "object", "additionalProperties": false, "required": [{}], "properties": {{{}}}{}}})", EnumList(required),
                     properties, needs.empty() ? "" : std::format(R"(, "dependencies": {{{}}})", needs));
}

// One form of a oneOf: the object with a discriminating key of one value first.
inline std::string Form(const char* discriminator, std::string_view value, std::string_view description, const std::vector<KeySchema>& keys,
                        std::vector<std::string_view> required, const std::vector<std::pair<const char*, const char*>>& dependencies = {})
{
  std::vector<KeySchema> all{{discriminator, std::format(R"({{"enum": ["{}"], "description": "{}"}})", value, description)}};
  all.insert(all.end(), keys.begin(), keys.end());
  required.insert(required.begin(), discriminator);
  return "\n    " + ObjectSchema(all, required, dependencies);
}

// The keys written once in the definitions, by their names: those of a series, of a panel, of the look by fact, of the
// frame, and of a ratio.
inline std::vector<KeySchema> SeriesValueKeys()
{
  return {
    {kFile, R"({"type": "string", "description": "A file the framework wrote."})"},
    {kScheme, R"({"type": "string", "description": "The binning_name; the only one in the file when absent."})"},
    {kSection, R"({"type": "string", "description": "Extract1D or Extract2D; the only one in the file when absent."})"},
    {kLabel, R"({"type": "string", "description": "The legend's text; written from what the series is when absent; given to a set, what its series share, each adding what tells it apart."})"},
    {kColor, R"({"type": "integer", "description": "A ROOT colour number."})"},
    {kMarker, R"({"type": "integer", "description": "A ROOT marker style."})"},
    {kMarkerSize, R"({"type": "number"})"},
    {kDy, std::format(R"({{"oneOf": [{{"type": "number"}}, {{"type": "array", "items": {{"type": "number"}}, "minItems": 1}}, {{"enum": ["{}"]}}], "description": "The |Delta y| limit, one the file holds (two decimals); a list of them, or \"{}\" for every one, makes one series each."}})",
                      kAll, kAll)},
    {kMult, std::format(R"({{"oneOf": [{0}, {{"type": "array", "items": {0}, "minItems": 1}}, {{"enum": ["{1}"]}}], "description": "A multiplicity class, low and high edge in percent, as the file's binning has it, or, for a purity or a correction map, the union of the classes (its interval, the catalog's {2}); a list of them, or \"{1}\" for every class, makes one series each."}})",
                        kRangeSchema, kAll, kMultUnion)},
    {kBinning, std::format(R"({{"enum": [{}], "description": "The pT binning; {} when absent."}})", EnumList(kBinnings | std::views::keys), kDefaultBinning)},
  };
}

inline std::vector<KeySchema> PanelValueKeys()
{
  const PlotDesigns::Panel panel;
  return {
    {kTitle, R"({"type": "string", "description": "Written in the top left corner of the pad."})"},
    {kXTitle, kStringSchema},
    {kYTitle, kStringSchema},
    {kLogY, R"({"type": "boolean"})"},
    {kZeroBaseline, std::format(R"({{"type": "boolean", "description": "The automatic Y range starts at 0; {} when absent."}})", panel.zeroBaseline)},
    {kHeadroom, std::format(R"({{"type": "number", "minimum": 0, "description": "Room above the data, a fraction of their span; {} when absent."}})", panel.headroom)},
    {kLegend, std::format(R"({{"type": "boolean", "description": "The legend written from what the series are; {} when absent."}})", panel.legend)},
    {kLegendHeader, R"({"type": "string", "description": "The legend's header, in place of what the series share."})"},
    {kKeys, R"({"type": "array", "items": {"$ref": "#/definitions/form_key"}, "description": "Legends of the look: what the colour or the filling stands for, by the fact it was taken from."})"},
    {kNotes, R"({"type": "array", "items": {"type": "string"}, "description": "Lines of text under the title."})"},
  };
}

// The look by fact: one value, one look, over every series of the design (all its panels).
inline std::vector<KeySchema> LookValueKeys()
{
  const std::string fact = R"({"$ref": "#/definitions/form_fact"})";
  return {
    {kColorBy, fact},
    {kShapeBy, fact},
    {kFillBy, fact},
    {kColors, R"({"type": "array", "items": {"type": "integer"}, "minItems": 1, "description": "The ROOT colours color_by gives the values, in order, one per value."})"},
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

inline std::vector<KeySchema> RatioValueKeys()
{
  return {
    {kDivideOption, std::format(R"({{"enum": [{}], "description": "\"\" for independent samples, \"B\" when the numerator is a subset of the denominator (a closure)."}})",
                                EnumList(kDivideOptions))},
    {kRatioRange, kRangeSchema},
    {kRatioTitle, std::format(R"({{"type": "string", "description": "The ratio pad's Y title; \"{}\" when absent."}})", PlotDesigns::RatioPad{}.title)},
    {kRatioGrid, R"({"type": "boolean", "description": "The ratio pad's horizontal grid, a dotted line at each labelled value; none when absent."})"},
    {kYRange, kRangeSchema},
  };
}

inline std::vector<KeySchema> SharedKeys()
{
  std::vector<KeySchema> keys = SeriesValueKeys();
  for (const auto& more : {PanelValueKeys(), LookValueKeys(), FrameValueKeys(), RatioValueKeys()})
    keys.insert(keys.end(), more.begin(), more.end());
  return keys;
}

// A form of series: its schema and how it is found. 'read' gets the series' object, its file, where in it, and the
// one |dy| and the one class of the series when its form has them (ReadSeries resolves the sets).
struct SeriesForm {
  std::string_view of;
  PlotSeries::FileKind reads; // the kind of file it is found in
  std::string_view description;
  std::vector<KeySchema> keys;
  std::vector<std::string_view> required;
  PlotSeries::Series (*read)(const rapidjson::Value&, const std::string& file, const PlotSeries::Where& where, std::optional<double> dy,
                             std::optional<PlotSeries::Interval> mult);
};

inline const std::vector<SeriesForm>& SeriesForms()
{
  using namespace PlotSeries;
  using JsonConfig::RequireString;
  static const std::vector<SeriesForm> forms = {
    {"spectrum", FileKind::Spectra,
     "The pT spectrum of an associated species in one multiplicity class; with extended, the same extended to pT = 0 with its fitted curve.",
     {{kSpecies, kStringSchema}, Ref(kDy), Ref(kMult), Ref(kSection), {kExtended, R"({"type": "boolean"})"}},
     {kSpecies, kDy, kMult},
     [](const rapidjson::Value& s, const std::string& file, const Where& where, std::optional<double> dy, std::optional<Interval> mult) {
       return (JsonConfig::OptionalBool(s, kExtended, false, kCtx) ? SpectrumExtended : Spectrum)(file, RequireString(s, kSpecies, kCtx), *dy, *mult, where);
     }},
    {"trend", FileKind::Spectra, "The yield of a species against multiplicity: measured, extrapolated to pT = 0, or the one over the other.",
     {{kSpecies, kStringSchema}, Ref(kDy), {kTrend, std::format(R"({{"enum": [{}]}})", EnumList(kTrendKinds | std::views::keys))}, Ref(kSection)},
     {kSpecies, kDy, kTrend},
     [](const rapidjson::Value& s, const std::string& file, const Where& where, std::optional<double> dy, std::optional<Interval>) {
       return Trend(file, RequireString(s, kSpecies, kCtx), *dy, Resolve(kTrendKinds, RequireString(s, kTrend, kCtx), kTrend), where);
     }},
    // A species ratio is measured or extrapolated, the first two kinds of a trend.
    {"species_ratio", FileKind::Spectra, "The ratio of two species' yields against multiplicity, as the correlation task wrote it.",
     {{kNumerator, kStringSchema}, {kDenominator, kStringSchema}, Ref(kDy),
      {kTrend, std::format(R"({{"enum": [{}]}})", EnumList(std::span(kTrendKinds).first(2) | std::views::keys))}, Ref(kSection)},
     {kNumerator, kDenominator, kDy, kTrend},
     [](const rapidjson::Value& s, const std::string& file, const Where& where, std::optional<double> dy, std::optional<Interval>) {
       return SpeciesRatio(file, RequireString(s, kNumerator, kCtx), RequireString(s, kDenominator, kCtx), *dy,
                           Resolve(kTrendKinds, RequireString(s, kTrend, kCtx), kTrend), where);
     }},
    {"purity", FileKind::Purity,
     "The purity S/(S+B) of a purity key against pT, in one multiplicity class or in the union of the classes.",
     {{kPurityKey, kStringSchema}, Ref(kMult), Ref(kBinning)},
     {kPurityKey, kMult},
     [](const rapidjson::Value& s, const std::string& file, const Where& where, std::optional<double>, std::optional<Interval> mult) {
       return Purity(file, RequireString(s, kPurityKey, kCtx), *mult, ReadBinning(s), where.scheme);
     }},
    {"correction_map", FileKind::Corrections,
     "A correction map of a particle against pT, in one multiplicity class or in the union of the classes.",
     {{kParticle, kStringSchema}, {kMap, std::format(R"({{"enum": [{}]}})", EnumList(kMaps | std::views::keys))}, Ref(kMult), Ref(kBinning)},
     {kParticle, kMap, kMult},
     [](const rapidjson::Value& s, const std::string& file, const Where& where, std::optional<double>, std::optional<Interval> mult) {
       return CorrectionMap(file, RequireString(s, kParticle, kCtx), Resolve(kMaps, RequireString(s, kMap, kCtx), kMap), *mult,
                            ReadBinning(s), where.scheme);
     }},
  };
  return forms;
}

// The keys of a panel: its series, at least 'minSeries' of them, and the panel's own keys; 'withYRange' is false where
// the range is the design's (joined pads).
inline std::vector<KeySchema> PanelKeys(int minSeries, bool withYRange)
{
  std::vector<KeySchema> keys = {
    {kSeries, std::format(R"({{"type": "array", "minItems": {}, "items": {{"$ref": "#/definitions/form_series"}}, "description": "What is drawn, each found by what it is in a file the framework wrote."}})",
                          minSeries)}};
  keys = Joined(keys, Refs(PanelValueKeys()));
  if (withYRange)
    keys.push_back(Ref(kYRange));
  return keys;
}

// The structural definitions (prefixed, so that no key's name can take their place), each referred to by its name.
inline std::vector<std::pair<std::string, std::string>> FormDefinitions()
{
  std::string forms;
  for (const SeriesForm& f : SeriesForms()) {
    std::vector<std::string_view> required{kFile};
    required.insert(required.end(), f.required.begin(), f.required.end());
    forms += (forms.empty() ? "" : ",") + Form(kOf, f.of, f.description, Joined({Ref(kFile), Ref(kScheme), Ref(kLabel), Ref(kColor), Ref(kMarker), Ref(kMarkerSize)}, f.keys), required);
  }
  return {
    {"form_series", std::format(R"({{"oneOf": [{}]}})", forms)},
    {"form_panel", ObjectSchema(PanelKeys(1, true), {kSeries})},
    {"form_joined_panel", ObjectSchema(PanelKeys(1, false), {kSeries})},
    // An object of a file the framework did not write (a preliminary): by its path, only its X axis checked.
    {"form_object", ObjectSchema({Ref(kFile), {kPath, R"({"type": "string", "description": "The object's path in the file."})"}, Ref(kLabel), Ref(kColor),
                                  Ref(kMarker), Ref(kMarkerSize)},
                                 {kFile, kPath})},
    {"form_fact", std::format(R"({{"enum": [{}]}})", EnumList(FactNames() | std::views::keys))},
    {"form_key", ObjectSchema({{kFact, R"({"$ref": "#/definitions/form_fact"})"},
                               {kShows, std::format(R"({{"enum": [{}]}})", EnumList(kShowsNames | std::views::keys))},
                               {kBox, R"({"type": "array", "items": {"type": "number"}, "minItems": 4, "maxItems": 4, "description": "x1, y1, x2, y2, fractions of the pad."})"},
                               {kColumns, R"({"type": "integer", "minimum": 1})"},
                               {kLabels, R"({"type": "array", "items": {"type": "string"}, "description": "The words of the entries, one per value of the fact in the order the pad's series bring them; what tells the values apart when absent."})"}},
                              {kFact, kShows, kBox})},
  };
}

// What the designs read, defined below with the rest of the reader.
inline std::vector<PlotDesigns::Panel> ReadPanels(const rapidjson::Value& spec, bool several);
inline PlotSeries::Series ReadObject(const rapidjson::Value& o);

// The ratio pad of a design that has one, its defaults RatioPad's.
inline PlotDesigns::RatioPad ReadRatioPad(const rapidjson::Value& spec)
{
  const PlotDesigns::RatioPad defaults;
  return {.divideOption = JsonConfig::RequireString(spec, kDivideOption, kCtx),
          .range = JsonConfig::TryRange<std::pair<double, double>>(spec, kRatioRange, kCtx),
          .title = JsonConfig::OptionalString(spec, kRatioTitle, defaults.title, kCtx),
          .grid = JsonConfig::OptionalBool(spec, kRatioGrid, defaults.grid, kCtx)};
}


// A design: its schema, and how it is drawn from a specification of it, on 'canvases' as 'name' in 'frame'. One table
// for both, so that a design the schema lets through is one the reader draws.
struct DesignForm {
  std::string_view name;
  std::string_view description;
  std::vector<KeySchema> keys;
  std::vector<std::string_view> required;
  std::vector<std::pair<const char*, const char*>> dependencies;
  PlotDesigns::Drawn (*draw)(const rapidjson::Value& spec, CanvasManager& canvases, const std::string& name, const PlotDesigns::Frame& frame);
};

// One form per design: a single panel's keys at the top for overlay, compare_with_ratio (two series and its divide
// option) and with_reference (its object); panels of their own for grid (and its columns) and joined_pads (and the one Y
// range they share), the look by fact at the top of all of them; the fits' own keys for fit_canvas_grid.
inline const std::vector<DesignForm>& DesignForms()
{
  using PlotDesigns::Frame;
  using Range = std::pair<double, double>;
  static const std::vector<DesignForm> forms = [] {
    const std::vector<KeySchema> look = Refs(LookValueKeys()), frame = Refs(FrameValueKeys());
    const std::vector<std::pair<const char*, const char*>> colorsNeedBy = {{kColors, kColorBy}};
    // The ratio pad's keys (PlotDesigns::RatioPad), the same in both designs that have one.
    const std::vector<KeySchema> ratioPad = {Ref(kDivideOption), Ref(kRatioRange), Ref(kRatioTitle), Ref(kRatioGrid)};
    const std::vector<KeySchema> ratio = Joined(
      {{kReference, R"({"type": "integer", "minimum": 0, "description": "The index of the series the others are divided by; 0 when absent."})"}}, ratioPad);
    const std::string object = R"({"$ref": "#/definitions/form_object"})";
    const std::vector<KeySchema> reference = Joined({{kReferenceObject, object}, {kSystematic, object}}, ratioPad);
    const std::vector<KeySchema> grid = {
      {kPanels, R"({"type": "array", "minItems": 1, "items": {"$ref": "#/definitions/form_panel"}})"},
      {kNCols, R"({"type": "integer", "minimum": 1, "description": "Panels per row."})"}};
    const std::vector<KeySchema> joined = {
      {kPanels, R"({"type": "array", "minItems": 2, "items": {"$ref": "#/definitions/form_joined_panel"}})"}, Ref(kYRange)};
    const std::vector<KeySchema> fits = {
      {kFile, R"({"type": "string", "description": "A purity file, or the phi fit file."})"},
      Ref(kScheme),
      {kPurityKey, R"({"type": "string", "description": "The purity key whose fits are shown; the phi's when absent."})"},
      {kCells, std::format(R"({{"type": "array", "minItems": 1, "items": {}, "description": "The fits shown: a multiplicity class and a pT interval each, as the fits' binning has them."}})",
                           ObjectSchema({{kMult, kRangeSchema}, {kPt, kRangeSchema}, {kLabel, kStringSchema}}, {kMult, kPt}))},
      {kNCols, R"({"type": "integer", "minimum": 1, "description": "Fits per row."})"},
      Ref(kBinning), Ref(kWidth), Ref(kHeight), Ref(kTextPx)};
    return std::vector<DesignForm>{
      {"overlay", "Every series on one pad.", Joined(Joined(PanelKeys(1, true), look), frame), {kSeries}, colorsNeedBy,
       [](const rapidjson::Value& spec, CanvasManager& canvases, const std::string& name, const Frame& f) {
         return PlotDesigns::Overlay(canvases, name, ReadPanels(spec, false).front(), f);
       }},
      {"compare_with_ratio", "The series on top, and below the ratio of each to the reference series.",
       Joined(Joined(Joined(PanelKeys(2, true), look), frame), ratio), {kSeries, kDivideOption}, colorsNeedBy,
       [](const rapidjson::Value& spec, CanvasManager& canvases, const std::string& name, const Frame& f) {
         return PlotDesigns::CompareWithRatio(canvases, name, ReadPanels(spec, false).front(),
                                              static_cast<std::size_t>(JsonConfig::OptionalInt(spec, kReference, 0, kCtx)), ReadRatioPad(spec), f);
       }},
      {"with_reference",
       "The series against an object of a file the framework did not write (a preliminary), with its systematic uncertainty as a band, and the ratio of each to it below.",
       Joined(Joined(Joined(PanelKeys(1, true), look), frame), reference), {kSeries, kReferenceObject, kDivideOption}, colorsNeedBy,
       [](const rapidjson::Value& spec, CanvasManager& canvases, const std::string& name, const Frame& f) {
         const rapidjson::Value* systematic = JsonConfig::OptionalMember(spec, kSystematic);
         return PlotDesigns::WithReference(canvases, name, ReadPanels(spec, false).front(), ReadObject(JsonConfig::RequireMember(spec, kReferenceObject, kCtx)),
                                           systematic ? std::optional(ReadObject(*systematic)) : std::nullopt, ReadRatioPad(spec), f);
       }},
      {"grid", "Panels in a grid, row by row, n_cols per row.", Joined(Joined(grid, look), frame), {kPanels, kNCols}, colorsNeedBy,
       [](const rapidjson::Value& spec, CanvasManager& canvases, const std::string& name, const Frame& f) {
         return PlotDesigns::Grid(canvases, name, ReadPanels(spec, true), JsonConfig::RequireInt(spec, kNCols, kCtx), f);
       }},
      {"joined_pads", "Panels side by side, joined, with one Y axis and one range for all.", Joined(Joined(joined, look), frame), {kPanels},
       colorsNeedBy,
       [](const rapidjson::Value& spec, CanvasManager& canvases, const std::string& name, const Frame& f) {
         return PlotDesigns::JoinedPads(canvases, name, ReadPanels(spec, true), JsonConfig::TryRange<Range>(spec, kYRange, kCtx), f);
       }},
      // The phi's fits have one binning: a binning is the purity key's.
      {"fit_canvas_grid", "The mass-fit canvases of some cells, already in the file, laid out in a grid.", fits, {kFile, kCells, kNCols},
       {{kBinning, kPurityKey}},
       [](const rapidjson::Value& spec, CanvasManager& canvases, const std::string& name, const Frame& f) {
         std::vector<PlotDesigns::FitCell> cells;
         for (const auto& c : JsonConfig::RequireArray(spec, kCells, kCtx))
           cells.push_back({JsonConfig::ToRange<PlotSeries::Interval>(JsonConfig::RequireMember(c, kMult, kCtx), kMult, kCtx),
                            JsonConfig::ToRange<PlotSeries::Interval>(JsonConfig::RequireMember(c, kPt, kCtx), kPt, kCtx),
                            JsonConfig::OptionalString(c, kLabel, "", kCtx)});
         return PlotDesigns::FitCanvasGrid(canvases, name, JsonConfig::RequireString(spec, kFile, kCtx), JsonConfig::TryString(spec, kPurityKey, kCtx),
                                           cells, JsonConfig::RequireInt(spec, kNCols, kCtx), ReadBinning(spec),
                                           JsonConfig::OptionalString(spec, kScheme, "", kCtx), f);
       }},
    };
  }();
  return forms;
}

inline std::string PointerText(const rapidjson::Pointer& pointer)
{
  rapidjson::StringBuffer buffer;
  pointer.Stringify(buffer);
  return buffer.GetString();
}

// Why 'value' fails 'schema', said in words: the key an object does not take, the keys it lacks or needs, the values
// allowed, or for an object matching none of a oneOf's forms, its own form's failure (by its "design" or "of") or the
// forms there are. 'definitions' are what the references of a sub-schema point to.
inline std::string Explain(const rapidjson::Value& schema, const rapidjson::Value& value, const std::string& at, const rapidjson::Value& definitions)
{
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
  if (keyword == "dependencies" && failedSchema && failedValue && failedValue->IsObject())
    for (const auto& d : (*failedSchema)["dependencies"].GetObject())
      if (failedValue->HasMember(d.name))
        for (const auto& needed : d.value.GetArray())
          if (!failedValue->HasMember(needed))
            return std::format("{}: '{}' needs '{}'", where, d.name.GetString(), needed.GetString());
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

// The schema: what a specification may say. Checked when it is built, before anyone is given it: JSON, and no two
// definitions under one name (the second would silently replace the first).
inline const std::string& Schema()
{
  static const std::string text = [] {
    std::string definitions;
    std::set<std::string> names;
    auto add = [&](const std::string& name, const std::string& schema) {
      if (!names.insert(name).second)
        throw std::logic_error(std::format("[FATAL] PlotSpec: two definitions named '{}' in the schema.", name));
      definitions += std::format("{}\n    \"{}\": {}", definitions.empty() ? "" : ",", name, schema);
    };
    for (const auto& [name, schema] : Detail::FormDefinitions())
      add(name, schema);
    for (const Detail::KeySchema& k : Detail::SharedKeys())
      add(k.name, k.schema);
    std::string t = std::format("{{\n  \"$schema\": \"http://json-schema.org/draft-04/schema#\",\n"
                                "  \"title\": \"A plot of the phi-strangeness analysis: one design and the series it draws\",\n"
                                "  \"definitions\": {{{}\n  }},\n  \"oneOf\": [{}]\n}}",
                                definitions, [] {
                                  std::string forms;
                                  for (const Detail::DesignForm& f : Detail::DesignForms())
                                    forms += (forms.empty() ? "" : ",") + Detail::Form(Detail::kDesign, f.name, f.description, f.keys, f.required, f.dependencies);
                                  return forms;
                                }());
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
// The |dy| an entry asks for: one, a list, or every limit the file records; none when its form has no dy.
inline std::vector<std::optional<double>> DyValues(const rapidjson::Value& s, const std::string& file, const PlotSeries::Where& where)
{
  const rapidjson::Value* v = JsonConfig::OptionalMember(s, kDy);
  if (!v)
    return {std::nullopt};
  std::vector<std::optional<double>> values;
  if (v->IsString())
    for (double dy : PlotSeries::DeltaYLimits(file, where))
      values.emplace_back(dy);
  else if (v->IsArray())
    for (const auto& each : v->GetArray())
      values.emplace_back(JsonConfig::ToNumber(each, kDy, kCtx));
  else
    values.emplace_back(JsonConfig::ToNumber(*v, kDy, kCtx));
  return values;
}

// The classes an entry asks for: one [low, high], a list of them, or every class of the file; none when absent.
inline std::vector<std::optional<PlotSeries::Interval>> MultValues(const rapidjson::Value& s, const std::string& file, const std::string& scheme)
{
  const rapidjson::Value* v = JsonConfig::OptionalMember(s, kMult);
  if (!v)
    return {std::nullopt};
  std::vector<std::optional<PlotSeries::Interval>> values;
  if (v->IsString())
    for (const PlotSeries::Interval& mult : PlotSeries::Intervals(file, scheme))
      values.emplace_back(mult);
  else if (v->IsArray() && !v->Empty() && v->Begin()->IsArray())
    for (const auto& each : v->GetArray())
      values.emplace_back(JsonConfig::ToRange<PlotSeries::Interval>(each, kMult, kCtx));
  else
    values.emplace_back(JsonConfig::ToRange<PlotSeries::Interval>(*v, kMult, kCtx));
  return values;
}

// The label and the look an entry gives, a series' or an object's.
inline void ReadLook(const rapidjson::Value& s, PlotSeries::Series& series)
{
  series.label = JsonConfig::OptionalString(s, kLabel, "", kCtx);
  series.look = {.color = JsonConfig::TryInt(s, kColor, kCtx), .marker = JsonConfig::TryInt(s, kMarker, kCtx),
                 .markerSize = JsonConfig::TryDouble(s, kMarkerSize, kCtx)};
}

// The series an entry asks for: one, or a set when its dy or mult is a list or "all", the classes outer and the |dy|
// inner; each with the entry's look, and its label followed by what tells the series of a set apart.
inline std::vector<PlotSeries::Series> ReadSeries(const rapidjson::Value& s)
{
  const std::string of = JsonConfig::RequireString(s, kOf, kCtx);
  const auto form = std::ranges::find(SeriesForms(), of, &SeriesForm::of);
  if (form == SeriesForms().end())
    throw std::logic_error(std::format("[FATAL] PlotSpec: the schema let through a series of kind '{}'.", of));
  const std::string file = JsonConfig::RequireString(s, kFile, kCtx);
  const PlotSeries::Where where{JsonConfig::OptionalString(s, kScheme, "", kCtx), JsonConfig::OptionalString(s, kSection, "", kCtx)};
  const auto dys = DyValues(s, file, where);
  const auto mults = MultValues(s, file, where.scheme);
  std::vector<PlotSeries::Series> series;
  for (const auto& mult : mults)
    for (const auto& dy : dys) {
      PlotSeries::Series read = form->read(s, file, where, dy, mult);
      ReadLook(s, read);
      if (!read.label.empty() && dys.size() > 1)
        read.label += ", " + read.facts.dy.value_or("");
      if (!read.label.empty() && mults.size() > 1)
        read.label += ", " + read.facts.mult.value_or("");
      series.push_back(std::move(read));
    }
  return series;
}

// A panel: its series, its axes, legends and notes. The words of a key go by position: the values of its fact in the
// order the pad's series bring them (PlotDesigns::FactValues, what the key itself lists), one each.
inline PlotDesigns::Panel ReadPanel(const rapidjson::Value& p)
{
  PlotDesigns::Panel panel;
  for (const auto& s : JsonConfig::RequireArray(p, kSeries, kCtx))
    std::ranges::move(ReadSeries(s), std::back_inserter(panel.series));
  panel.title = JsonConfig::OptionalString(p, kTitle, "", kCtx);
  panel.xTitle = JsonConfig::OptionalString(p, kXTitle, "", kCtx);
  panel.yTitle = JsonConfig::OptionalString(p, kYTitle, "", kCtx);
  panel.yRange = JsonConfig::TryRange<std::pair<double, double>>(p, kYRange, kCtx);
  panel.logY = JsonConfig::OptionalBool(p, kLogY, panel.logY, kCtx);
  panel.zeroBaseline = JsonConfig::OptionalBool(p, kZeroBaseline, panel.zeroBaseline, kCtx);
  panel.headroom = JsonConfig::OptionalDouble(p, kHeadroom, panel.headroom, kCtx);
  panel.legend = JsonConfig::OptionalBool(p, kLegend, panel.legend, kCtx);
  panel.legendHeader = JsonConfig::OptionalString(p, kLegendHeader, "", kCtx);
  if (auto keys = JsonConfig::TryArray(p, kKeys, kCtx))
    for (const auto& k : *keys) {
      const std::vector<double> box = JsonConfig::ReadNumberArray(JsonConfig::RequireArray(k, kBox, kCtx), kBox, kCtx);
      const PlotSeries::Fact fact = Resolve(FactNames(), JsonConfig::RequireString(k, kFact, kCtx), kFact);
      std::vector<std::pair<std::string, std::string>> text;
      if (auto labels = JsonConfig::TryArray(k, kLabels, kCtx)) {
        const std::vector<std::string> values = PlotDesigns::FactValues(panel.series, fact, "key");
        if (labels->Size() != values.size())
          throw std::runtime_error(std::format("[FATAL] PlotSpec: a key gives {} labels for {} values ({}).", labels->Size(), values.size(),
                                               Logger::Join(values, ", ")));
        for (std::size_t i = 0; i < values.size(); ++i)
          text.emplace_back(values[i], JsonConfig::ToString((*labels)[static_cast<rapidjson::SizeType>(i)], kLabels, kCtx));
      }
      panel.keys.push_back({.fact = fact,
                            .shows = Resolve(kShowsNames, JsonConfig::RequireString(k, kShows, kCtx), kShows),
                            .where = {box[0], box[1], box[2], box[3], JsonConfig::OptionalInt(k, kColumns, 1, kCtx)},
                            .text = std::move(text)});
    }
  if (auto notes = JsonConfig::TryArray(p, kNotes, kCtx))
    for (const auto& n : *notes)
      panel.notes.push_back(JsonConfig::ToString(n, kNotes, kCtx));
  return panel;
}

// The look by fact of a design, over every series of all its panels: one value, one look in every pad. What a look
// given by hand allows is PlotDesigns' rule (ColourBy, ShapeBy, FillBy), the same for a specification and a composition.
inline void ApplyLooks(const rapidjson::Value& spec, std::vector<PlotDesigns::Panel>& panels)
{
  if (auto by = JsonConfig::TryString(spec, kColorBy, kCtx)) {
    std::vector<int> palette;
    if (auto colors = JsonConfig::TryArray(spec, kColors, kCtx))
      for (const auto& c : *colors)
        palette.push_back(static_cast<int>(JsonConfig::ToNumber(c, kColors, kCtx)));
    PlotDesigns::ColourBy(panels, Resolve(FactNames(), *by, kColorBy), palette);
  }
  if (auto by = JsonConfig::TryString(spec, kShapeBy, kCtx))
    PlotDesigns::ShapeBy(panels, Resolve(FactNames(), *by, kShapeBy));
  if (auto by = JsonConfig::TryString(spec, kFillBy, kCtx))
    PlotDesigns::FillBy(panels, Resolve(FactNames(), *by, kFillBy));
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

// The panels of a design: its "panels", or the one panel its top level is; the looks by fact applied over all of them.
inline std::vector<PlotDesigns::Panel> ReadPanels(const rapidjson::Value& spec, bool several)
{
  std::vector<PlotDesigns::Panel> panels;
  if (several)
    for (const auto& p : JsonConfig::RequireArray(spec, kPanels, kCtx))
      panels.push_back(ReadPanel(p));
  else
    panels.push_back(ReadPanel(spec));
  ApplyLooks(spec, panels);
  return panels;
}

inline PlotSeries::Series ReadObject(const rapidjson::Value& o)
{
  PlotSeries::Series s = PlotSeries::Object(JsonConfig::RequireString(o, kFile, kCtx), JsonConfig::RequireString(o, kPath, kCtx));
  ReadLook(o, s);
  return s;
}

// JSON as the command prints it and a plot keeps it: indented by two.
inline std::string Pretty(const rapidjson::Value& value)
{
  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
  writer.SetIndent(' ', 2);
  value.Accept(writer);
  return buffer.GetString();
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
  const auto form = std::ranges::find(DesignForms(), design, &DesignForm::name);
  if (form != DesignForms().end())
    return form->draw(spec, canvases, name, frame);
  throw std::logic_error(std::format("[FATAL] PlotSpec: the schema let through a design '{}' with nothing to draw it.", design));
}

// A specification with its files replaced: the files it names (every "file" key, at any depth), in the order they first
// appear, become 'files', one for one; the rest is kept, except the words written for the files it named (the series'
// labels, the titles, the notes), which would describe the new ones wrongly ("offline" on a file that is not): the
// legend then comes from AutoLabels. What `phistrangeplot describe/render --file` do to a checked example, so that
// the same plot is drawn from other productions.
inline std::string WithFiles(const std::string& text, const std::string& origin, const std::vector<std::string>& files)
{
  using namespace Detail;
  rapidjson::Document spec = Parse(text, origin);
  std::vector<std::string> named;
  std::vector<rapidjson::Value*> slots;
  std::set<std::string> dropped;
  auto walk = [&](auto&& self, rapidjson::Value& v) -> void {
    if (v.IsArray())
      for (auto& e : v.GetArray())
        self(self, e);
    if (!v.IsObject())
      return;
    // Erased (in order, unlike RemoveMember, which would move the last member into the hole and change which file
    // comes first) before the file is found: erasing moves the members after it.
    for (const char* key : {kTitle, kRatioTitle, kNotes, kLegendHeader, v.HasMember(kFile) ? kLabel : nullptr})
      if (key && v.EraseMember(key))
        dropped.emplace(key);
    if (auto file = v.FindMember(kFile); file != v.MemberEnd()) {
      slots.push_back(&file->value);
      if (std::ranges::find(named, file->value.GetString()) == named.end())
        named.emplace_back(file->value.GetString());
    }
    for (auto& m : v.GetObject())
      self(self, m.value);
  };
  walk(walk, spec);
  if (named.size() != files.size())
    throw std::runtime_error(std::format("[FATAL] PlotSpec: '{}' names {} file(s) ({}); {} given.", origin, named.size(), Logger::Join(named, ", "),
                                         files.size()));
  for (rapidjson::Value* slot : slots) {
    const std::size_t i = static_cast<std::size_t>(std::ranges::find(named, slot->GetString()) - named.begin());
    slot->SetString(files[i].c_str(), spec.GetAllocator());
  }
  if (!dropped.empty())
    Logger::Info("PlotSpec: the words '{}' wrote for its own files are left out ({}).", origin, Logger::Join(dropped));
  return Pretty(spec);
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
      rapidjson::Value sc(rapidjson::kObjectType), forms(rapidjson::kArrayType);
      sc.AddMember(rapidjson::StringRef(kScheme), str(scheme.name), a);
      sc.AddMember(rapidjson::StringRef(kKind), str(PlotSeries::Name(scheme.kind)), a);
      for (const SeriesForm& f : SeriesForms())
        if (f.reads == scheme.kind)
          forms.PushBack(str(f.of), a);
      sc.AddMember(rapidjson::StringRef(kSeriesForms), forms, a);
      auto interval = [&](const PlotSeries::Interval& i) {
        rapidjson::Value v(rapidjson::kArrayType);
        v.PushBack(i.low, a).PushBack(i.high, a);
        return v;
      };
      auto intervals = [&](const std::vector<PlotSeries::Interval>& list) {
        rapidjson::Value v(rapidjson::kArrayType);
        for (const PlotSeries::Interval& i : list)
          v.PushBack(interval(i), a);
        return v;
      };
      sc.AddMember(rapidjson::StringRef(kMult), intervals(scheme.classes), a);
      if (scheme.classesUnion)
        sc.AddMember(rapidjson::StringRef(kMultUnion), interval(*scheme.classesUnion), a);
      switch (scheme.kind) {
        case PlotSeries::FileKind::Corrections:
          sc.AddMember(rapidjson::StringRef(kParticle), strings(scheme.particles), a);
          break;
        case PlotSeries::FileKind::Purity:
          sc.AddMember(rapidjson::StringRef(kPurityKey), strings(scheme.purityKeys), a);
          break;
        // The fits' cells, for a fit_canvas_grid: the classes above and these pT bins.
        case PlotSeries::FileKind::PhiFits:
          sc.AddMember(rapidjson::StringRef(kPt), intervals(scheme.ptIntervals), a);
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
  return Pretty(doc);
}

} // namespace PlotSpec
