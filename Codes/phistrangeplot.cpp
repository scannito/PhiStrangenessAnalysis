#include "PlotDesigns.h"
#include "PlotSummaries.h"

#include <cstdio>
#include <map>
#include <string>

// The plots command. For now one verb: the summaries a task draws, redrawn from a file it wrote, with no task and no
// configuration (which task wrote the file, and what the file holds, are read from the file):
//   phistrangeplot summaries <file.root> [--out <dir>] [--scheme <binning_name>] [--section <section>]
// A scheme or a section left out is the only one the file has; a section is for a spectra file. The summaries go to
// <dir>/<file stem>_<scheme>[_<section>]/ (PlotSummaries::Save). Exit codes: 0 done, 1 refused or failed, 2 wrong use,
// 3 not in the file (a scheme, a section, a record, an object).
namespace
{
int Usage()
{
  std::fprintf(stderr, "usage: phistrangeplot summaries <file.root> [--out <dir>] [--scheme <binning_name>] [--section <section>]\n");
  return 2;
}
} // namespace

int main(int argc, char* argv[])
{
  if (argc < 3 || std::string(argv[1]) != "summaries")
    return Usage();
  const std::string fileName = argv[2];
  std::map<std::string, std::string> options{{"--out", "summaries"}, {"--scheme", ""}, {"--section", ""}};
  for (int i = 3; i < argc; i += 2) {
    if (!options.contains(argv[i]) || i + 1 >= argc)
      return Usage();
    options[argv[i]] = argv[i + 1];
  }

  std::string recipe = "phistrangeplot";
  for (int i = 1; i < argc; ++i)
    recipe += std::format(" {}", argv[i]);
  return PlotDesigns::Run([&] {
    CanvasManager canvases;
    const PlotSeries::Where where = PlotSummaries::OfFile(canvases, fileName, {options["--scheme"], options["--section"]});
    PlotSummaries::Save(canvases, fileName, where, options["--out"], recipe);
  });
}
