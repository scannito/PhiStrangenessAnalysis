#include "PlotDesigns.h"
#include "PlotSpec.h"
#include "PlotSummaries.h"

#include <cstdio>
#include <map>
#include <string>

// The plots command:
//   phistrangeplot summaries <file.root> [--out <dir>] [--scheme <binning_name>] [--section <section>]
//     the summaries a task draws, redrawn from a file it wrote, with no task and no configuration (which task wrote the
//     file, and what it holds, are read from it); they go to <dir>/<file stem>_<scheme>[_<section>]/ (PlotSummaries::Save).
//   phistrangeplot schema
//     what a plot specification may say (PlotSpec::Schema).
//   phistrangeplot describe <spec.json>
//     what the specification resolves to, every series and what the series disagree on, without keeping anything.
//   phistrangeplot render <spec.json> --out <base> [--alice <status> --system <collision system>]
//     the plot, as <base>.pdf, .png and .root, the specification kept as its recipe. The ALICE label only when both its
//     status and its system are given: the system is not in the files.
// Exit codes: 0 done, 1 refused or failed, 2 wrong use, 3 not in the files (a scheme, a section, a record, an object).
namespace
{
int Usage()
{
  std::fprintf(stderr, "usage: phistrangeplot summaries <file.root> [--out <dir>] [--scheme <binning_name>] [--section <section>]\n"
                       "       phistrangeplot schema\n"
                       "       phistrangeplot describe <spec.json>\n"
                       "       phistrangeplot render <spec.json> --out <base> [--alice <status> --system <collision system>]\n");
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
} // namespace

int main(int argc, char* argv[])
{
  if (argc < 2)
    return Usage();
  const std::string verb = argv[1];
  std::string recipe = "phistrangeplot";
  for (int i = 1; i < argc; ++i)
    recipe += std::format(" {}", argv[i]);

  if (verb == "schema" && argc == 2) {
    std::printf("%s\n", PlotSpec::Schema().c_str());
    return 0;
  }
  if ((verb == "describe" || verb == "render") && argc >= 3) {
    std::map<std::string, std::string> options{{"--out", ""}, {"--alice", ""}, {"--system", ""}};
    if (!ReadOptions(argc, argv, 3, options) || (verb == "render") == options["--out"].empty() ||
        options["--alice"].empty() != options["--system"].empty())
      return Usage();
    const std::string specFile = argv[2];
    return PlotDesigns::Run([&] {
      const std::string text = JsonConfig::ReadFileText(specFile);
      const rapidjson::Document spec = PlotSpec::Parse(text, specFile);
      std::optional<std::pair<std::string, std::string>> alice;
      if (!options["--alice"].empty())
        alice = std::pair{options["--alice"], options["--system"]};
      CanvasManager canvases;
      const PlotDesigns::Drawn drawn = PlotSpec::Draw(spec, canvases, "plot", alice);
      std::printf("%s", PlotSeries::Describe(drawn.series).c_str());
      if (verb == "render")
        PlotDesigns::Save(drawn, options["--out"], std::format("{}\n{}", recipe, text));
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
