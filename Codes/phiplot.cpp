#include "Logger.h"
#include "PlotCatalog.h"
#include "PlotRenderer.h"
#include "PlotSpec.h"

#include "TROOT.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

#include <exception>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// Plots on demand from the files the correlation tasks write (README.md, Plots on demand).
// Compiled only: it is not part of the analysis chain and Cling is not asked to load it, so
// format specs such as {:<28} are allowed here.
//
//   phiplot list [--json] <file>...     what the files hold, by meaning
//   phiplot schema                      the JSON schema of a plot specification
//   phiplot describe <spec.json>        resolve and check; say what would be drawn
//   phiplot render <spec.json>          the same, then draw
//   phiplot quick <kind> ...            build a specification from options, save it, draw
//
// Every way of asking for a plot ends in a specification that goes through PlotSpec::Parse
// and PlotRenderer::Resolve: quick writes one and reads it back, and so does a front end
// driven by a language model.

namespace
{

int Usage()
{
  std::cerr << "Usage:\n"
               "  phiplot list [--json] <file>...\n"
               "  phiplot schema\n"
               "  phiplot describe <spec.json>\n"
               "  phiplot render <spec.json>\n"
               "  phiplot quick <kind> --species S [--den D] --dy X [--mult LO-HI] --file F[:LABEL]... --out STEM\n"
               "                [--ratio [B]] [--logy|--liny] [--title T] [--formats pdf,png] [--section Extract1D|Extract2D]\n"
               "                [--binning-name NAME] [--alice STATUS --system TEXT]\n"
               "    kinds:";
  for (const auto& k : OutputNames::kKinds)
    std::cerr << " " << k.name;
  std::cerr << "\n";
  return 2;
}

int List(const std::vector<std::string>& args)
{
  bool json = false;
  std::vector<std::string> files;
  for (const auto& a : args)
    (a == "--json" ? json = true : (files.push_back(a), false));
  if (files.empty())
    return Usage();
  // With --json the standard output is the document and nothing else; warnings still
  // reach the standard error.
  if (json)
    Logger::SetLevel(Logger::Level::Warn);
  std::vector<PlotCatalog::Catalog> cats;
  for (const auto& f : files)
    cats.push_back(PlotCatalog::Build(f));
  if (json) {
    std::cout << PlotCatalog::Json(cats) << std::endl;
    return 0;
  }
  for (const auto& c : cats)
    std::cout << PlotCatalog::Text(c) << std::endl;
  return 0;
}

int DescribeOrRender(const std::string& specPath, bool render)
{
  const PlotSpec::Spec spec = PlotSpec::ParseFile(specPath);
  PlotRenderer::Plan plan = PlotRenderer::Resolve(spec);
  std::cout << PlotRenderer::Describe(plan);
  if (render)
    PlotRenderer::Render(plan);
  return 0;
}

// --- quick: options to a specification -------------------------------------
std::vector<std::string> Split(const std::string& s, char sep)
{
  std::vector<std::string> out;
  std::string cur;
  for (char ch : s) {
    if (ch == sep) {
      out.push_back(cur);
      cur.clear();
    } else {
      cur += ch;
    }
  }
  out.push_back(cur);
  return out;
}

int Quick(const std::vector<std::string>& args)
{
  if (args.empty())
    return Usage();
  rapidjson::Document doc(rapidjson::kObjectType);
  auto& a = doc.GetAllocator();
  const auto str = [&](const std::string& s) { return rapidjson::Value(s.c_str(), a); };

  const std::string kind = args[0];
  std::string species, den, out, title, section, binningName, alice, system;
  std::optional<double> dy;
  std::optional<std::pair<double, double>> mult;
  std::vector<std::pair<std::string, std::string>> files;
  std::optional<std::string> ratio;
  std::optional<bool> logY;
  std::vector<std::string> formats;

  for (std::size_t i = 1; i < args.size(); ++i) {
    const std::string& o = args[i];
    const auto next = [&]() -> std::string {
      if (i + 1 >= args.size())
        throw std::runtime_error(std::format("[FATAL] phiplot quick: '{}' needs a value.", o));
      return args[++i];
    };
    if (o == "--species")
      species = next();
    else if (o == "--den")
      den = next();
    else if (o == "--dy")
      dy = std::stod(next());
    else if (o == "--mult") {
      const auto parts = Split(next(), '-');
      if (parts.size() != 2)
        throw std::runtime_error("[FATAL] phiplot quick: --mult is LO-HI, e.g. 0-10.");
      mult = std::pair{std::stod(parts[0]), std::stod(parts[1])};
    } else if (o == "--file") {
      const std::string v = next();
      const auto colon = v.rfind(':');
      files.emplace_back(colon == std::string::npos ? v : v.substr(0, colon), colon == std::string::npos ? "" : v.substr(colon + 1));
    } else if (o == "--out")
      out = next();
    else if (o == "--title")
      title = next();
    else if (o == "--section")
      section = next();
    else if (o == "--binning-name")
      binningName = next();
    else if (o == "--alice")
      alice = next();
    else if (o == "--system")
      system = next();
    else if (o == "--formats")
      formats = Split(next(), ',');
    else if (o == "--logy")
      logY = true;
    else if (o == "--liny")
      logY = false;
    else if (o == "--ratio")
      // The option of TH1::Divide is never guessed (ComparisonTask, divide_option): plain
      // division unless "B" is said.
      ratio = (i + 1 < args.size() && args[i + 1] == "B") ? (++i, std::string("B")) : std::string("");
    else
      throw std::runtime_error(std::format("[FATAL] phiplot quick: unknown option '{}'.", o));
  }
  if (out.empty() || files.empty() || species.empty() || !dy)
    return Usage();
  // The collision system is not in the files, so it is never assumed: a label that says
  // the wrong one would be a figure that lies.
  if (alice.empty() != system.empty())
    throw std::runtime_error("[FATAL] phiplot quick: --alice and --system go together (the label's two lines).");

  doc.AddMember("output", str(out), a);
  if (!title.empty())
    doc.AddMember("title", str(title), a);
  if (logY)
    doc.AddMember("log_y", *logY, a);
  if (!formats.empty()) {
    rapidjson::Value f(rapidjson::kArrayType);
    for (const auto& x : formats)
      f.PushBack(str(x), a);
    doc.AddMember("formats", f, a);
  }
  if (!alice.empty()) {
    rapidjson::Value l(rapidjson::kObjectType);
    l.AddMember("status", str(alice), a);
    l.AddMember("system", str(system), a);
    doc.AddMember("alice_label", l, a);
  }
  rapidjson::Value series(rapidjson::kArrayType);
  for (const auto& [file, label] : files) {
    rapidjson::Value s(rapidjson::kObjectType);
    s.AddMember("file", str(file), a);
    s.AddMember("kind", str(kind), a);
    s.AddMember("species", str(species), a);
    if (!den.empty())
      s.AddMember("species_den", str(den), a);
    s.AddMember("dy", *dy, a);
    if (mult) {
      rapidjson::Value m(rapidjson::kArrayType);
      m.PushBack(mult->first, a).PushBack(mult->second, a);
      s.AddMember("mult", m, a);
    }
    if (!section.empty())
      s.AddMember("section", str(section), a);
    if (!binningName.empty())
      s.AddMember("binning_name", str(binningName), a);
    if (!label.empty())
      s.AddMember("label", str(label), a);
    series.PushBack(s, a);
  }
  doc.AddMember("series", series, a);
  if (ratio) {
    rapidjson::Value r(rapidjson::kObjectType);
    r.AddMember("reference", 0, a);
    r.AddMember("divide_option", str(*ratio), a);
    doc.AddMember("ratio", r, a);
  }

  // Saved before it is used, so the plot can be made again from the file, and read back
  // through the same parser as any other specification.
  const std::string specPath = out + ".spec.json";
  {
    std::filesystem::path p(specPath);
    if (p.has_parent_path())
      std::filesystem::create_directories(p.parent_path());
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    writer.SetIndent(' ', 2);
    doc.Accept(writer);
    std::ofstream(specPath) << buffer.GetString() << "\n";
  }
  Logger::Info("phiplot quick: specification saved as {}", specPath);
  return DescribeOrRender(specPath, true);
}

} // namespace

int main(int argc, char* argv[])
{
  gROOT->SetBatch(kTRUE);
  if (argc < 2)
    return Usage();
  const std::string cmd = argv[1];
  const std::vector<std::string> args(argv + 2, argv + argc);

  int rc = 0;
  try {
    if (cmd == "list")
      rc = List(args);
    else if (cmd == "schema")
      std::cout << PlotSpec::Schema() << std::endl;
    else if ((cmd == "describe" || cmd == "render") && args.size() == 1)
      rc = DescribeOrRender(args[0], cmd == "render");
    else if (cmd == "quick")
      rc = Quick(args);
    else
      return Usage();
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    rc = 1;
  }
  // The end-of-run summary, as for the analysis chain, except where the standard output is
  // a document (list --json, schema): there the warnings were printed on the standard error.
  const bool document = cmd == "schema" || (cmd == "list" && std::find(args.begin(), args.end(), "--json") != args.end());
  if (!document)
    Logger::PrintSummary();
  return rc;
}
