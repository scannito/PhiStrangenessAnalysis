#include "PlotDesigns.h"
#include "PlotSpec.h"
#include "PlotSummaries.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <vector>
#include <string>

// The plots command:
//   phistrangeplot summaries <file.root> [--out <dir>] [--scheme <binning_name>] [--section <section>]
//     the summaries a task draws, redrawn from a file it wrote, with no task and no configuration (which task wrote the
//     file, and what it holds, are read from it); they go to <dir>/<file stem>_<scheme>[_<section>]/ (PlotSummaries::Save).
//   phistrangeplot list <file.root>...
//     what the files hold, per binning scheme, as JSON in the specification's keys (PlotSpec::CatalogJson): the task
//     that wrote it, the multiplicity classes, the particles, purity keys or sections (species, |dy| limits, species
//     ratios), the forms of series a specification can ask of it. Nothing else on the standard output.
//   phistrangeplot schema
//     what a plot specification may say (PlotSpec::Schema).
//   phistrangeplot describe <spec.json>
//     what the specification resolves to, every series and what the series disagree on, without keeping anything.
//   phistrangeplot render <spec.json> --out <base> [--alice <status> --system <collision system>] [--note <text>]
//     the plot, as <base>.pdf, .png and .root, the specification kept as its recipe and the note, when one is given, as
//     a fact of its own (a front end records there the request and the model that wrote the specification). The ALICE label only when
//     both its status and its system are given: the system is not in the files.
//   phistrangeplot quick <example.json> --file <file.root> [--file <file.root>...] --out <base> [--alice <status> --system <system>]
//     a checked example (PlotSpecs/examples) drawn from other files: the files it names, in the order they first appear,
//     replaced one for one by those given (PlotSpec::WithFiles); the specification made so is kept as <base>.spec.json,
//     next to the plot, and as its recipe.
// Exit codes: 0 done, 1 refused or failed, 2 wrong use, 3 not in the files (a scheme, a section, a record, an object).
namespace
{
int Usage()
{
  std::fprintf(stderr, "usage: phistrangeplot summaries <file.root> [--out <dir>] [--scheme <binning_name>] [--section <section>]\n"
                       "       phistrangeplot list <file.root>...\n"
                       "       phistrangeplot schema\n"
                       "       phistrangeplot describe <spec.json>\n"
                       "       phistrangeplot render <spec.json> --out <base> [--alice <status> --system <collision system>] [--note <text>]\n"
                       "       phistrangeplot quick <example.json> --file <file.root>... --out <base> [--alice <status> --system <collision system>]\n");
  return 2;
}

// The options after the positional arguments, each "--name value", from those 'options' lists; false on a wrong one.
bool ReadOptions(int argc, char* argv[], int first, std::map<std::string, std::string>& options)
{
  for (int i = first; i < argc; i += 2) {
    if (!options.contains(argv[i]) || i + 1 >= argc)
      return false;
    options[argv[i]] = argv[i + 1];
  }
  return true;
}
// Draws a specification, prints what it resolved to, and keeps the plot when 'out' is given (with the note, if any).
void DrawSpec(const std::string& text, const std::string& origin, const std::string& out, const std::string& recipe,
              const std::optional<std::pair<std::string, std::string>>& alice, const std::string& note)
{
  const rapidjson::Document spec = PlotSpec::Parse(text, origin);
  CanvasManager canvases;
  const PlotDesigns::Drawn drawn = PlotSpec::Draw(spec, canvases, "plot", alice);
  std::printf("%s", PlotSeries::Describe(drawn.series).c_str());
  if (!out.empty())
    PlotDesigns::Save(drawn, out, std::format("{}\n{}", recipe, text),
                      note.empty() ? std::map<std::string, std::string>{} : std::map<std::string, std::string>{{"note", note}});
}

std::optional<std::pair<std::string, std::string>> Alice(const std::string& status, const std::string& system)
{
  if (status.empty())
    return std::nullopt;
  return std::pair{status, system};
}
} // namespace

int main(int argc, char* argv[])
{
  if (argc < 2)
    return Usage();
  const std::string verb = argv[1];
  // The command line as the recipe, without the note: that is recorded as a fact of its own.
  std::string recipe = "phistrangeplot";
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--note") {
      ++i;
      continue;
    }
    recipe += std::format(" {}", argv[i]);
  }

  if (verb == "list" && argc >= 3) {
    const std::vector<std::string> files(argv + 2, argv + argc);
    // The standard output is a document: the progress lines stay out of it (the warnings go to stderr).
    Logger::SetLevel(Logger::Level::Warn);
    return PlotDesigns::Run([&] {
      std::vector<PlotSeries::Catalog> catalogs;
      for (const std::string& file : files)
        catalogs.push_back(PlotSeries::ReadCatalog(file));
      std::printf("%s\n", PlotSpec::CatalogJson(catalogs).c_str());
    });
  }
  if (verb == "schema" && argc == 2) {
    std::printf("%s\n", PlotSpec::Schema().c_str());
    return 0;
  }
  if ((verb == "describe" || verb == "render") && argc >= 3) {
    std::map<std::string, std::string> options{{"--out", ""}, {"--alice", ""}, {"--system", ""}, {"--note", ""}};
    const bool render = verb == "render";
    if (!ReadOptions(argc, argv, 3, options))
      return Usage();
    // render needs where to write, describe writes nothing; the ALICE label needs its system with its status.
    if (render != !options["--out"].empty() || options["--alice"].empty() != options["--system"].empty())
      return Usage();
    const std::string specFile = argv[2];
    return PlotDesigns::Run([&] {
      DrawSpec(JsonConfig::ReadFileText(specFile), specFile, options["--out"], recipe, Alice(options["--alice"], options["--system"]), options["--note"]);
    });
  }
  if (verb == "quick" && argc >= 3) {
    // --file repeats, so the options are read here.
    std::vector<std::string> files;
    std::map<std::string, std::string> options{{"--out", ""}, {"--alice", ""}, {"--system", ""}};
    for (int i = 3; i < argc; i += 2) {
      if (i + 1 >= argc)
        return Usage();
      if (std::string(argv[i]) == "--file")
        files.emplace_back(argv[i + 1]);
      else if (options.contains(argv[i]))
        options[argv[i]] = argv[i + 1];
      else
        return Usage();
    }
    if (files.empty() || options["--out"].empty() || options["--alice"].empty() != options["--system"].empty())
      return Usage();
    const std::string example = argv[2];
    return PlotDesigns::Run([&] {
      const std::string text = PlotSpec::WithFiles(JsonConfig::ReadFileText(example), example, files);
      const std::filesystem::path kept = options["--out"] + ".spec.json";
      if (kept.has_parent_path())
        RootIO::CreateDirectoriesOrThrow(kept.parent_path(), "phistrangeplot");
      std::ofstream(kept) << text << "\n";
      DrawSpec(text, kept.string(), options["--out"], recipe, Alice(options["--alice"], options["--system"]), "");
    });
  }
  if (verb == "summaries" && argc >= 3) {
    std::map<std::string, std::string> options{{"--out", "summaries"}, {"--scheme", ""}, {"--section", ""}};
    if (!ReadOptions(argc, argv, 3, options))
      return Usage();
    const std::string fileName = argv[2];
    return PlotDesigns::Run([&] {
      CanvasManager canvases;
      const PlotSeries::Where where = PlotSummaries::OfFile(canvases, fileName, {options["--scheme"], options["--section"]});
      PlotSummaries::Save(canvases, fileName, where, options["--out"], recipe);
    });
  }
  return Usage();
}
