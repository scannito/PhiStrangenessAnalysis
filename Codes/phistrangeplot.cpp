#include "PlotDesigns.h"
#include "PlotSummaries.h"

#include <cstdio>
#include <filesystem>
#include <map>
#include <string>

// The plots command. For now one verb: the summaries a task draws, redrawn from a file it wrote, with no task and no
// configuration (what the file holds is read from the file):
//   phistrangeplot summaries {spectra|purity|corrections} <file.root> [--out <dir>] [--scheme <binning_name>] [--section <section>]
// Each summary goes to <dir>/<canvas>.png and .pdf, and all of them to <dir>/<file stem>_summaries.root at the paths the
// task gives them. A scheme or a section left out is the only one the file has. Exit codes: 0 done, 1 refused or
// failed, 2 wrong use, 3 not in the file (a scheme, a section, a record, an object).
namespace
{
int Usage()
{
  std::fprintf(stderr, "usage: phistrangeplot summaries {spectra|purity|corrections} <file.root> [--out <dir>] [--scheme <binning_name>] "
                       "[--section <section>]\n");
  return 2;
}
} // namespace

int main(int argc, char* argv[])
{
  if (argc < 4 || std::string(argv[1]) != "summaries")
    return Usage();
  const std::string kind = argv[2], fileName = argv[3];
  std::map<std::string, std::string> options{{"--out", "summaries"}, {"--scheme", ""}, {"--section", ""}};
  for (int i = 4; i < argc; i += 2) {
    if (!options.contains(argv[i]) || i + 1 >= argc)
      return Usage();
    options[argv[i]] = argv[i + 1];
  }
  if (kind != "spectra" && kind != "purity" && kind != "corrections")
    return Usage();
  if (kind != "spectra" && !options["--section"].empty())
    return Usage();

  std::string recipe = "phistrangeplot";
  for (int i = 1; i < argc; ++i)
    recipe += std::format(" {}", argv[i]);
  return PlotDesigns::Run([&] {
    CanvasManager canvases;
    std::string scheme;
    if (kind == "spectra") {
      const PlotSeries::Where where = PlotSeries::Resolve(fileName, {options["--scheme"], options["--section"]});
      scheme = where.scheme;
      PlotSummaries::Correlations(canvases, fileName, where.scheme, where.section);
    } else {
      scheme = PlotSeries::SchemeName(fileName, options["--scheme"]);
      if (kind == "purity")
        PlotSummaries::Purities(canvases, fileName, scheme);
      else
        PlotSummaries::Corrections(canvases, fileName, scheme);
    }
    PlotSummaries::Save(canvases, fileName, scheme, options["--out"], std::filesystem::path(fileName).stem().string() + "_summaries", recipe);
  });
}
