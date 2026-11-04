#include "PlotDesigns.h"
#include "PlotSpec.h"
#include "PlotSummaries.h"

#include <cstdio>
#include <filesystem>
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
//   phistrangeplot describe <spec.json> [--file <file.root>...]
//     what the specification resolves to, every series and what the series disagree on, without keeping anything.
//   phistrangeplot render <spec.json> [--file <file.root>...] --out <base> [--alice <status> --system <collision system>] [--note <text>]
//     the plot, as <base>.pdf, .png and .root, the specification kept as its recipe and the note, when one is given, as
//     a fact of its own (a front end records there the request and the model that wrote the specification). The ALICE label only when
//     both its status and its system are given: the system is not in the files.
//   With --file, a specification (a checked example of PlotSpecs/examples) is drawn from other files: the files it names,
//   in the order they first appear, are replaced one for one by those given (PlotSpec::WithFiles), and the specification
//   made so is the one described and kept as the recipe.
// Exit codes: 0 done, 1 refused or failed, 2 wrong use, 3 not in the files (a scheme, a section, a record, an object).
namespace
{
int Usage()
{
  std::fprintf(stderr, "usage: phistrangeplot summaries <file.root> [--out <dir>] [--scheme <binning_name>] [--section <section>]\n"
                       "       phistrangeplot list <file.root>...\n"
                       "       phistrangeplot schema\n"
                       "       phistrangeplot describe <spec.json> [--file <file.root>...]\n"
                       "       phistrangeplot render <spec.json> [--file <file.root>...] --out <base> [--alice <status> --system <collision system>] [--note <text>]\n");
  return 2;
}

// The options after the positional arguments, each "--name value", from those 'options' lists, and every "--file value"
// into 'files' when it is given; false on a wrong one.
bool ReadOptions(int argc, char* argv[], int first, std::map<std::string, std::string>& options, std::vector<std::string>* files = nullptr)
{
  for (int i = first; i < argc; i += 2) {
    if (i + 1 >= argc)
      return false;
    if (files && std::string_view(argv[i]) == "--file")
      files->emplace_back(argv[i + 1]);
    else if (options.contains(argv[i]))
      options[argv[i]] = argv[i + 1];
    else
      return false;
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

// The file of Codes/ (a header, or this one) changed after this binary was built, if any: the binary would draw with code
// the tree no longer holds and record the tree's commit and fingerprint (RunEnvironment::Facts) as if it did not.
// CMake rebuilds it when any of them changes (CMakeLists.txt), so after a build this is empty. Not checked, with a
// warning, when the binary was found through PATH and argv[0] does not say where it is.
std::string Stale(const char* argv0)
{
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path self(argv0);
  const fs::file_time_type built = fs::last_write_time(self, ec);
  if (!self.has_parent_path() || ec) {
    Logger::Warn("phistrangeplot: where '{}' is cannot be told; not checked against Codes/ for a rebuild.", argv0);
    return "";
  }
  const fs::path source = fs::absolute(__FILE__, ec);
  for (const fs::directory_entry& entry : fs::directory_iterator(source.parent_path(), ec))
    if ((entry.path().extension() == ".h" || entry.path() == source) && entry.last_write_time(ec) > built)
      return entry.path().filename().string();
  return "";
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
  if (const std::string changed = Stale(argv[0]); !changed.empty()) {
    Logger::Error("[FATAL] phistrangeplot: Codes/{} changed after this binary was built; rebuild it: cmake --build build --target phistrangeplot", changed);
    return 1;
  }
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
    std::vector<std::string> files;
    const bool render = verb == "render";
    if (!ReadOptions(argc, argv, 3, options, &files))
      return Usage();
    // render needs where to write, describe writes nothing; the ALICE label needs its system with its status.
    if (render != !options["--out"].empty() || options["--alice"].empty() != options["--system"].empty())
      return Usage();
    const std::string specFile = argv[2];
    return PlotDesigns::Run([&] {
      const std::string text = JsonConfig::ReadFileText(specFile);
      DrawSpec(files.empty() ? text : PlotSpec::WithFiles(text, specFile, files), specFile, options["--out"], recipe,
               Alice(options["--alice"], options["--system"]), options["--note"]);
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
