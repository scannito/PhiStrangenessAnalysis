#pragma once

#include "JsonConfigHelpers.h"
#include "Logger.h"
#include "OutputNames.h"
#include "PlotDesigns.h"
#include "PlotSeries.h"

#include "rapidjson/document.h"
#include "rapidjson/pointer.h"
#include "rapidjson/schema.h"
#include "rapidjson/stringbuffer.h"

#include <format>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// A plot as data: one design of PlotDesigns and the series it draws, named the way a C++ composition names them
// (PlotSeries' finders), with the cosmetics of Panel and Frame. What a local model writes when a plot is asked for in
// words (DESIGN_NOTES section 1); it can only name what the components find and check, and every check is theirs: a
// specification is translated into the same calls a composition makes. The schema below is the one description of
// what a specification may say: `phistrangeplot schema` prints it, every specification is validated against it before
// it is read, and a front end passes it to the model as the shape its answer must have.
namespace PlotSpec
{

namespace Detail
{
// The names a specification may use for each closed list, once: the schema is written from them and the reader
// resolves through them.
inline constexpr std::pair<std::string_view, PlotSeries::TrendKind> kTrendKinds[] = {
  {"measured", PlotSeries::TrendKind::Measured}, {"extrapolated", PlotSeries::TrendKind::Extrapolated},
  {"extrap_over_meas", PlotSeries::TrendKind::ExtrapOverMeas}};
inline constexpr std::pair<std::string_view, OutputNames::SpectrumBinning> kBinnings[] = {
  {"analysis", OutputNames::SpectrumBinning::Analysis}, {"source", OutputNames::SpectrumBinning::Source}};
inline constexpr std::pair<std::string_view, OutputNames::CorrectionMap> kMaps[] = {
  {"efficiency", OutputNames::CorrectionMap::Efficiency}, {"signal_loss", OutputNames::CorrectionMap::SignalLoss},
  {"signal_loss_over_event_loss", OutputNames::CorrectionMap::SignalLossOverEventLoss}};
inline constexpr std::string_view kDesigns[] = {"overlay", "compare_with_ratio"};
inline constexpr std::string_view kDrawOptions[] = {"E1", "E", "P"};
inline constexpr std::string_view kDivideOptions[] = {"", "B"};

// The names of a closed list as JSON strings, for an "enum" of the schema.
template <class Names>
std::string EnumList(const Names& names)
{
  std::string out;
  for (const auto& entry : names) {
    std::string_view name;
    if constexpr (requires { entry.first; })
      name = entry.first;
    else
      name = entry;
    out += std::format("{}\"{}\"", out.empty() ? "" : ", ", name);
  }
  return out;
}

template <class E, std::size_t N>
E Resolve(const std::pair<std::string_view, E> (&table)[N], const std::string& name, std::string_view what)
{
  for (const auto& [n, value] : table)
    if (n == name)
      return value;
  throw std::runtime_error(std::format("[FATAL] PlotSpec: '{}' is not a {}.", name, what));
}

inline void Replace(std::string& text, std::string_view token, const std::string& with)
{
  for (std::size_t at = text.find(token); at != std::string::npos; at = text.find(token, at + with.size()))
    text.replace(at, token.size(), with);
}

// The schema's text, with @TOKENS@ for what is written once elsewhere.
inline constexpr std::string_view kSchemaText = R"({
  "$schema": "http://json-schema.org/draft-04/schema#",
  "title": "A plot of the phi-strangeness analysis: one design and the series it draws",
  "type": "object",
  "additionalProperties": false,
  "required": ["design", "series"],
  "properties": {
    "design": {"enum": [@DESIGNS@],
               "description": "overlay: every series on one pad. compare_with_ratio: the series on top, and below the ratio of each to the reference series."},
    "series": {"type": "array", "minItems": 1, "items": {"oneOf": [@FORMS@]},
               "description": "What is drawn, each found by what it is in a file the framework wrote."},
    "title": {"type": "string", "description": "Written in the top left corner of the pad."},
    "x_title": {"type": "string"},
    "y_title": {"type": "string"},
    "y_range": {"type": "array", "items": {"type": "number"}, "minItems": 2, "maxItems": 2, "description": "Low and high; the series' own when absent."},
    "log_y": {"type": "boolean"},
    "legend": {"type": "boolean", "description": "The legend written from what the series are (true when absent)."},
    "legend_box": {"type": "array", "items": {"type": "number"}, "minItems": 4, "maxItems": 4, "description": "x1, y1, x2, y2 of the legend, fractions of the pad."},
    "legend_columns": {"type": "integer", "minimum": 1},
    "width": {"type": "integer", "minimum": 200},
    "height": {"type": "integer", "minimum": 200},
    "text_px": {"type": "integer", "minimum": 8, "description": "Size of the text in pixels."},
    "draw_option": {"enum": [@DRAW_OPTIONS@], "description": "E1 error bars with ends (default), E without, P markers only."},
    "reference": {"type": "integer", "minimum": 0, "description": "compare_with_ratio: the index of the series the others are divided by."},
    "divide_option": {"enum": [@DIVIDE_OPTIONS@], "description": "compare_with_ratio, required there: \"\" for independent samples, \"B\" when the numerator is a subset of the denominator (a closure)."},
    "ratio_range": {"type": "array", "items": {"type": "number"}, "minItems": 2, "maxItems": 2},
    "ratio_title": {"type": "string"}
  }
})";

// One form per finder; the keys every form shares, then each form's own.
inline constexpr std::string_view kCommon = R"(
      "file": {"type": "string", "description": "A file the framework wrote."},
      "scheme": {"type": "string", "description": "The binning_name; the only one in the file when absent."},
      "label": {"type": "string", "description": "The legend's text; written from what the series is when absent."},
      "color": {"type": "integer", "description": "A ROOT colour number."},
      "marker": {"type": "integer", "description": "A ROOT marker style."},
      "marker_size": {"type": "number"})";
inline constexpr std::string_view kMult =
  R"("mult": {"type": "array", "items": {"type": "number"}, "minItems": 2, "maxItems": 2, "description": "A multiplicity class, low and high edge in percent, as the file's binning has it."})";
inline constexpr std::string_view kSection = R"("section": {"type": "string", "description": "Extract1D or Extract2D; the only one in the file when absent."})";
inline constexpr std::string_view kDy = R"("dy": {"type": "number", "description": "The |Delta y| limit, one the file holds."})";

inline std::string Form(std::string_view of, std::string_view description, std::vector<std::string> own, std::vector<std::string> required)
{
  std::string properties = std::format(R"("of": {{"enum": ["{}"], "description": "{}"}},{})", of, description, kCommon);
  for (const std::string& p : own)
    properties += ",\n      " + p;
  required.insert(required.begin(), {"of", "file"});
  return std::format(R"(
    {{"type": "object", "additionalProperties": false, "required": [{}],
     "properties": {{
      {}}}}})",
                     EnumList(required), properties);
}

inline std::string SchemaText()
{
  const std::string trends = std::format(R"("trend": {{"enum": [{}]}})", EnumList(kTrendKinds));
  // A species ratio is measured or extrapolated, the first two kinds of a trend.
  const std::string ratioTrends = std::format(R"("trend": {{"enum": [{}]}})", EnumList(std::span(kTrendKinds).first(2)));
  const std::string binning = std::format(R"("binning": {{"enum": [{}], "description": "The pT binning; analysis when absent."}})", EnumList(kBinnings));
  const std::vector<std::string> forms = {
    Form("spectrum", "The pT spectrum of an associated species in one multiplicity class.", {"\"species\": {\"type\": \"string\"}", std::string(kDy), std::string(kMult), std::string(kSection)},
         {"species", "dy", "mult"}),
    Form("spectrum_extended", "The same spectrum extended to pT = 0, with its fitted curve.",
         {"\"species\": {\"type\": \"string\"}", std::string(kDy), std::string(kMult), std::string(kSection)}, {"species", "dy", "mult"}),
    Form("trend", "The yield of a species against multiplicity: measured, extrapolated to pT = 0, or the one over the other.",
         {"\"species\": {\"type\": \"string\"}", std::string(kDy), trends, std::string(kSection)}, {"species", "dy", "trend"}),
    Form("species_ratio", "The ratio of two species' yields against multiplicity, as the correlation task wrote it.",
         {"\"numerator\": {\"type\": \"string\"}", "\"denominator\": {\"type\": \"string\"}", std::string(kDy), ratioTrends, std::string(kSection)},
         {"numerator", "denominator", "dy", "trend"}),
    Form("purity", "The purity S/(S+B) of a purity key against pT, in one multiplicity class.",
         {"\"purity_key\": {\"type\": \"string\"}", std::string(kMult), binning}, {"purity_key", "mult"}),
    Form("correction_map", "A correction map of a particle against pT, in one multiplicity class or integrated (no mult).",
         {"\"particle\": {\"type\": \"string\"}", std::format(R"("map": {{"enum": [{}]}})", EnumList(kMaps)), std::string(kMult), binning},
         {"particle", "map"}),
  };
  std::string formsText;
  for (const std::string& f : forms)
    formsText += (formsText.empty() ? "" : ",") + f;
  std::string text(kSchemaText);
  Replace(text, "@DESIGNS@", EnumList(kDesigns));
  Replace(text, "@DRAW_OPTIONS@", EnumList(kDrawOptions));
  Replace(text, "@DIVIDE_OPTIONS@", EnumList(kDivideOptions));
  Replace(text, "@FORMS@", formsText);
  return text;
}

inline std::string PointerText(const rapidjson::Pointer& pointer)
{
  rapidjson::StringBuffer buffer;
  pointer.Stringify(buffer);
  return buffer.GetString();
}

// Why 'value' fails 'schema', said in words: the keys a closed object does not take, the form a series of a known kind
// does not match, or the keyword and where.
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
  const rapidjson::Value* failedSchema = validator.GetInvalidSchemaPointer().Get(const_cast<rapidjson::Value&>(schema));
  const rapidjson::Value* failedValue = docPointer.Get(const_cast<rapidjson::Value&>(value));
  auto keysOf = [](const rapidjson::Value& properties) {
    std::vector<std::string> keys;
    for (const auto& p : properties.GetObject())
      keys.emplace_back(p.name.GetString());
    return Logger::Join(keys, ", ");
  };
  // A key the object does not take: the validator points at the key itself, its schema is the object's.
  if (keyword == "additionalProperties" && failedSchema && failedSchema->HasMember("properties") && docPointer.GetTokenCount() > 0) {
    const auto& last = docPointer.GetTokens()[docPointer.GetTokenCount() - 1];
    const std::string object = inner.substr(0, inner.size() - last.length - 1);
    return std::format("{}: unknown key '{}'; the object takes {}", object.empty() ? "/" : object, last.name,
                       keysOf((*failedSchema)["properties"]));
  }
  if (keyword == "required" && failedSchema && failedValue && failedValue->IsObject()) {
    std::vector<std::string> missing;
    for (const auto& key : (*failedSchema)["required"].GetArray())
      if (!failedValue->HasMember(key))
        missing.emplace_back(key.GetString());
    return std::format("{}: missing {}", where, Logger::Join(missing, ", "));
  }
  if (keyword == "enum" && failedSchema)
    return std::format("{}: not one of {}", where, JsonConfig::Serialize((*failedSchema)["enum"]));
  // A series that matches no form: explained against the form its "of" names, or the kinds there are.
  if (keyword == "oneOf" && failedSchema && failedValue && failedValue->IsObject()) {
    std::vector<std::string> kinds;
    for (const auto& form : (*failedSchema)["oneOf"].GetArray()) {
      if (failedValue->HasMember("of") && form["properties"]["of"]["enum"][0] == (*failedValue)["of"])
        return Explain(form, *failedValue, inner);
      kinds.emplace_back(form["properties"]["of"]["enum"][0].GetString());
    }
    return std::format("{}: 'of' must be one of {}", where, Logger::Join(kinds, ", "));
  }
  return std::format("{}: fails '{}' (schema {})", where, keyword, PointerText(validator.GetInvalidSchemaPointer()));
}
} // namespace Detail

// The schema: what a specification may say.
inline const std::string& Schema()
{
  static const std::string text = Detail::SchemaText();
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
  if (schema.HasParseError())
    throw std::logic_error(std::format("[FATAL] PlotSpec: the schema itself is not JSON (at offset {}).", schema.GetErrorOffset()));
  if (const std::string why = Detail::Explain(schema, spec, ""); !why.empty())
    throw std::runtime_error(std::format("[FATAL] PlotSpec: '{}' is not a valid specification: {}.", origin, why));
  return spec;
}

namespace Detail
{
inline PlotSeries::Interval Mult(const rapidjson::Value& v) { return {v[0].GetDouble(), v[1].GetDouble()}; }

inline std::pair<double, double> Range(const rapidjson::Value& v) { return {v[0].GetDouble(), v[1].GetDouble()}; }

// One series, by the finder its "of" names, with its look.
inline PlotSeries::Series Series(const rapidjson::Value& s)
{
  const std::string ctx = "PlotSpec series";
  const std::string of = s["of"].GetString(), file = s["file"].GetString();
  const std::string scheme = JsonConfig::OptionalString(s, "scheme", "", ctx);
  const PlotSeries::Where where{scheme, JsonConfig::OptionalString(s, "section", "", ctx)};
  auto binning = [&] { return Resolve(kBinnings, JsonConfig::OptionalString(s, "binning", "analysis", ctx), "binning"); };
  PlotSeries::Series series;
  if (of == "spectrum")
    series = PlotSeries::Spectrum(file, s["species"].GetString(), s["dy"].GetDouble(), Mult(s["mult"]), where);
  else if (of == "spectrum_extended")
    series = PlotSeries::SpectrumExtended(file, s["species"].GetString(), s["dy"].GetDouble(), Mult(s["mult"]), where);
  else if (of == "trend")
    series = PlotSeries::Trend(file, s["species"].GetString(), s["dy"].GetDouble(), Resolve(kTrendKinds, s["trend"].GetString(), "trend"), where);
  else if (of == "species_ratio")
    series = PlotSeries::SpeciesRatio(file, s["numerator"].GetString(), s["denominator"].GetString(), s["dy"].GetDouble(),
                                      Resolve(kTrendKinds, s["trend"].GetString(), "trend"), where);
  else if (of == "purity")
    series = PlotSeries::Purity(file, s["purity_key"].GetString(), Mult(s["mult"]), binning(), scheme);
  else if (of == "correction_map")
    series = PlotSeries::CorrectionMap(file, s["particle"].GetString(), Resolve(kMaps, s["map"].GetString(), "correction map"),
                                       s.HasMember("mult") ? std::optional(Mult(s["mult"])) : std::nullopt, binning(), scheme);
  else
    throw std::logic_error(std::format("[FATAL] PlotSpec: the schema let through a series of kind '{}'.", of));
  series.label = JsonConfig::OptionalString(s, "label", "", ctx);
  if (s.HasMember("color"))
    series.look.color = s["color"].GetInt();
  if (s.HasMember("marker"))
    series.look.marker = s["marker"].GetInt();
  if (s.HasMember("marker_size"))
    series.look.markerSize = s["marker_size"].GetDouble();
  return series;
}
} // namespace Detail

// What a specification draws: its design run on its series, on 'canvases'. The ALICE label is not the specification's
// to say (the collision system is not in the files): the caller gives it, or none.
inline PlotDesigns::Drawn Draw(const rapidjson::Document& spec, CanvasManager& canvases, const std::string& name,
                               const std::optional<std::pair<std::string, std::string>>& alice = std::nullopt)
{
  const std::string ctx = "PlotSpec";
  PlotDesigns::Panel panel{.title = JsonConfig::OptionalString(spec, "title", "", ctx),
                           .xTitle = JsonConfig::OptionalString(spec, "x_title", "", ctx),
                           .yTitle = JsonConfig::OptionalString(spec, "y_title", "", ctx),
                           .logY = JsonConfig::OptionalBool(spec, "log_y", false, ctx),
                           .legend = JsonConfig::OptionalBool(spec, "legend", true, ctx)};
  for (const auto& s : spec["series"].GetArray())
    panel.series.push_back(Detail::Series(s));
  if (spec.HasMember("y_range"))
    panel.yRange = Detail::Range(spec["y_range"]);

  PlotDesigns::Frame frame;
  frame.width = JsonConfig::OptionalInt(spec, "width", frame.width, ctx);
  frame.height = JsonConfig::OptionalInt(spec, "height", frame.height, ctx);
  frame.textPx = JsonConfig::OptionalInt(spec, "text_px", frame.textPx, ctx);
  frame.drawOption = JsonConfig::OptionalString(spec, "draw_option", frame.drawOption, ctx);
  if (spec.HasMember("legend_box")) {
    const auto& box = spec["legend_box"];
    frame.legend = {box[0].GetDouble(), box[1].GetDouble(), box[2].GetDouble(), box[3].GetDouble(), frame.legend.columns};
  }
  frame.legend.columns = JsonConfig::OptionalInt(spec, "legend_columns", frame.legend.columns, ctx);
  frame.alice = alice;

  const std::string design = spec["design"].GetString();
  if (design == "overlay")
    return PlotDesigns::Overlay(canvases, name, panel, frame);
  // compare_with_ratio: the division's option is stated, never guessed (PlotSeries::Ratio)
  if (!spec.HasMember("divide_option"))
    throw std::runtime_error("[FATAL] PlotSpec: compare_with_ratio needs 'divide_option': \"\" for independent samples, \"B\" for a closure.");
  return PlotDesigns::CompareWithRatio(canvases, name, panel, static_cast<std::size_t>(JsonConfig::OptionalInt(spec, "reference", 0, ctx)),
                                       spec["divide_option"].GetString(),
                                       spec.HasMember("ratio_range") ? std::optional(Detail::Range(spec["ratio_range"])) : std::nullopt,
                                       JsonConfig::OptionalString(spec, "ratio_title", "ratio", ctx), frame);
}

} // namespace PlotSpec
