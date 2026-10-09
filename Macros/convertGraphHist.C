#include <TAxis.h>
#include <TDirectory.h>
#include <TFile.h>
#include <TGraphErrors.h>
#include <TH1.h>
#include <TH1D.h>
#include <TKey.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// Converts between TGraphErrors and TH1, in either direction.
//
// This exists because ComparisonTask divides histograms, and results produced before
// this framework are graphs. The conversion is deliberately NOT done inside the task,
// for a reason sharper than convenience: the graph's abscissa is generally a DIFFERENT
// QUANTITY from the histogram's axis - a mean multiplicity where the other has a
// percentile, or a bare index - so which point belongs to which bin cannot be worked
// out from the numbers. It has to be asserted by whoever knows what the old object
// meant.
//
// A converter that guessed would be the worst outcome available: it would place the
// points somewhere plausible, and the axis check in ComparisonTask would then be
// verifying a correspondence the converter had invented.
//
// So the correspondence is an argument, the target binning is copied from the new
// result, and the table of what went where is printed every time. The assumption is
// made here, in the open; the comparison downstream checks that the result landed on
// the axis it expects.

namespace
{

// Point i of the graph goes into bin binForPoint[i] of the target. The correspondence
// is GIVEN, never computed from the x values: the graph's abscissa is generally a
// different quantity from the target's axis - mean charged multiplicity against a
// percentile, or a plain index - so matching by position on the axis would place the
// points somewhere plausible and wrong.
//
// The x of the graph is printed and then discarded. It is documentation of where the
// old point sat in its own coordinates, and it plays no part in the conversion.
bool FillByMap(const TGraphErrors& graph, TH1& target, const std::vector<int>& binForPoint)
{
  const int nPoints = graph.GetN();
  if (static_cast<int>(binForPoint.size()) != nPoints) {
    std::cerr << "[ERROR] the map has " << binForPoint.size() << " entries for " << nPoints
              << " points." << std::endl;
    return false;
  }

  std::vector<int> pointForBin(target.GetNbinsX() + 2, -1);
  bool ok = true;

  std::cout << "  point      x (graph)        ->  bin   [ low , up ]        y +- ey" << std::endl;
  for (int i = 0; i < nPoints; ++i) {
    double x = 0.0, y = 0.0;
    graph.GetPoint(i, x, y);
    const int bin = binForPoint[i];

    if (bin < 1 || bin > target.GetNbinsX()) {
      std::cerr << "[ERROR] point " << i << " maps to bin " << bin << ", outside 1.."
                << target.GetNbinsX() << std::endl;
      ok = false;
      continue;
    }

    if (pointForBin[bin] >= 0) {
      std::cerr << "[ERROR] bin " << bin << " is claimed by both point " << pointForBin[bin]
                << " and point " << i << std::endl;
      ok = false;
      continue;
    }
    pointForBin[bin] = i;

    target.SetBinContent(bin, y);
    target.SetBinError(bin, graph.GetErrorY(i));

    printf("  %5d  %14g      ->  %3d   [%g, %g]  %g +- %g\n", i, x, bin,
           target.GetXaxis()->GetBinLowEdge(bin), target.GetXaxis()->GetBinUpEdge(bin),
           y, graph.GetErrorY(i));
  }

  for (int bin = 1; bin <= target.GetNbinsX(); ++bin) {
    if (pointForBin[bin] < 0) {
      std::cerr << "[ERROR] bin " << bin << " [" << target.GetXaxis()->GetBinLowEdge(bin) << ", "
                << target.GetXaxis()->GetBinUpEdge(bin) << "] receives no point." << std::endl;
      ok = false;
    }
  }

  return ok;
}

std::unique_ptr<TFile> OpenOrDie(const std::string& path, const char* mode)
{
  std::unique_ptr<TFile> f(TFile::Open(path.c_str(), mode));
  if (!f || f->IsZombie()) {
    std::cerr << "[FATAL] cannot open '" << path << "' in mode " << mode << std::endl;
    return nullptr;
  }
  return f;
}

} // namespace

// -----------------------------------------------------------------------------
// Graph -> histogram, on the binning of a reference histogram.
//
// 'referenceFile'/'referencePath' is normally the NEW result the graph will be
// compared against: the conversion lands on exactly the axis the comparison expects,
// so any disagreement surfaces here, naming the point, rather than later as a ratio
// that looks plausible.
//
// 'binForPoint' is the correspondence, one target bin per graph point, 1-based. Left
// empty it means positional - point i into bin i+1 - which requires the graph to have
// as many points as the target has bins, in the same order. That default is a
// convenience, not an inference: the printed table is there to be read before the
// result is used.
// -----------------------------------------------------------------------------
void graphToHist(const std::string& graphFile, const std::string& graphPath,
                 const std::string& referenceFile, const std::string& referencePath,
                 const std::string& outFile, const std::string& outName,
                 std::vector<int> binForPoint = {})
{
  auto fGraph = OpenOrDie(graphFile, "READ");
  auto fRef = OpenOrDie(referenceFile, "READ");
  if (!fGraph || !fRef)
    return;

  auto* graph = dynamic_cast<TGraphErrors*>(fGraph->Get(graphPath.c_str()));
  if (!graph) {
    std::cerr << "[FATAL] '" << graphPath << "' is not a TGraphErrors in '" << graphFile << "'" << std::endl;
    return;
  }

  auto* reference = dynamic_cast<TH1*>(fRef->Get(referencePath.c_str()));
  if (!reference) {
    std::cerr << "[FATAL] '" << referencePath << "' is not a TH1 in '" << referenceFile << "'" << std::endl;
    return;
  }

  // The axis is copied, never derived from the points: the whole purpose is to land on
  // the binning of the object this will be divided by.
  std::unique_ptr<TH1D> out(static_cast<TH1D*>(reference->Clone(outName.c_str())));
  out->SetDirectory(nullptr);
  out->Reset();
  out->SetTitle(graph->GetTitle());

  if (binForPoint.empty()) {
    if (graph->GetN() != out->GetNbinsX()) {
      std::cerr << "[FATAL] positional mapping asked for, but the graph has " << graph->GetN()
                << " points and the target " << out->GetNbinsX() << " bins. Pass the "
                << "correspondence explicitly as the last argument." << std::endl;
      return;
    }
    std::cout << "[INFO] no map given: point i -> bin i+1, in order. Read the table below."
              << std::endl;
    for (int i = 0; i < graph->GetN(); ++i)
      binForPoint.push_back(i + 1);
  }

  if (!FillByMap(*graph, *out, binForPoint)) {
    std::cerr << "[FATAL] conversion refused. Nothing was written." << std::endl;
    return;
  }

  auto fOut = OpenOrDie(outFile, "UPDATE");
  if (!fOut)
    return;

  fOut->cd();
  out->Write(outName.c_str(), TObject::kOverwrite);
  std::cout << "[INFO] wrote '" << outName << "' (" << out->GetNbinsX() << " bins) into '"
            << outFile << "'" << std::endl;
}

// -----------------------------------------------------------------------------
// Histogram -> graph, the other direction.
//
// The bin centres become x and the half widths ex, so the graph carries the histogram's
// own coordinates - NOT the ones an older graph might have used. There is no way back
// to a quantity this object never held, which is precisely why graphToHist takes the
// correspondence as an argument instead of reading it off the abscissa.
//
// Empty bins are dropped, because a graph cannot say "measured and zero" apart from
// "not measured".
// -----------------------------------------------------------------------------
void histToGraph(const std::string& histFile, const std::string& histPath,
                 const std::string& outFile, const std::string& outName,
                 bool keepEmptyBins = false)
{
  auto fHist = OpenOrDie(histFile, "READ");
  if (!fHist)
    return;

  auto* hist = dynamic_cast<TH1*>(fHist->Get(histPath.c_str()));
  if (!hist) {
    std::cerr << "[FATAL] '" << histPath << "' is not a TH1 in '" << histFile << "'" << std::endl;
    return;
  }

  auto graph = std::make_unique<TGraphErrors>();
  graph->SetName(outName.c_str());
  graph->SetTitle(hist->GetTitle());

  int n = 0, dropped = 0;
  for (int bin = 1; bin <= hist->GetNbinsX(); ++bin) {
    const double content = hist->GetBinContent(bin);
    const double error = hist->GetBinError(bin);

    if (!keepEmptyBins && content == 0.0 && error == 0.0) {
      ++dropped;
      continue;
    }

    graph->SetPoint(n, hist->GetBinCenter(bin), content);
    graph->SetPointError(n, 0.5 * hist->GetBinWidth(bin), error);
    ++n;
  }

  if (dropped > 0)
    std::cout << "[INFO] " << dropped << " empty bin(s) dropped. Pass keepEmptyBins = true"
              << " to keep them, if 'measured and zero' is what they mean." << std::endl;

  auto fOut = OpenOrDie(outFile, "UPDATE");
  if (!fOut)
    return;

  fOut->cd();
  graph->Write(outName.c_str(), TObject::kOverwrite);
  std::cout << "[INFO] wrote '" << outName << "' (" << n << " points) into '" << outFile << "'" << std::endl;
}

// Usage reminder, printed when the macro is run with no arguments.
void convertGraphHist()
{
  std::cout
    << "Converts between TGraphErrors and TH1, in either direction.\n\n"
    << "  graphToHist(graphFile, graphPath, referenceFile, referencePath,\n"
    << "              outFile, outName, binForPoint = {})\n\n"
    << "      The binning is COPIED from the reference histogram - normally the new\n"
    << "      result this will be divided by.\n\n"
    << "      'binForPoint' says which target bin each graph point goes into, 1-based.\n"
    << "      It is an argument and not something computed from the x values, because\n"
    << "      the graph's abscissa is usually a different quantity from the target's\n"
    << "      axis: matching by position would put the points somewhere plausible and\n"
    << "      wrong. Left empty it means point i -> bin i+1, in order, which requires\n"
    << "      as many points as bins.\n\n"
    << "      The table of what went where is printed every time. Read it: it is the\n"
    << "      only place the correspondence is visible.\n\n"
    << "      Refuses if a point maps outside the binning, if two points claim the same\n"
    << "      bin, or if a bin is left empty.\n\n"
    << "  histToGraph(histFile, histPath, outFile, outName, keepEmptyBins = false)\n\n"
    << "      Bin centres and half widths become x and ex, so the graph carries the\n"
    << "      histogram's own coordinates - not those of some older graph.\n"
    << std::endl;
}
