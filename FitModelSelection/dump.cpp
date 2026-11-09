// What one output file of a purity or phi-fit run says about its fits, as two TSV tables for selection.py: the fit
// quality ({binning_name}/FitQuality, read by FitQualityRecorder::Read), one row per fit with the models it used,
// and the verdict FitQualityRecorder::Reasons gives (the one rule; the tables carry it, nobody downstream recomputes
// it); and the values the fits give, one row per cell (the
// purities, or the phi's raw signal and r = B(signal region)/B(sideband)). Cells are named by OutputNames::CellTag,
// which is also the key of a fitConfig.json override, and the object names come from OutputNames.
//
// Usage: dump purity|phi <directory> <run> <binning_name> <quality.tsv> <values.tsv>
// The file is the one the task writes there with output_prefix "<run>_" (OutputNames::kPurityFile, kPhiDataFile).
// The purity keys, and whether each has a purity at the source binning too (rebinning_pt), are those of the fits in
// the table: what the file holds, not what a configuration says it should.
#include "FitQualityRecorder.h"
#include "OutputNames.h"
#include "RootIOHelpers.h"

#include "TH2.h"

#include <cstdio>
#include <exception>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace
{
const std::string kWho{"FitModelSelection/dump"};

std::ofstream OpenTable(const std::string& path, const char* header)
{
  std::ofstream out(path);
  if (!out)
    throw std::runtime_error(std::format("[FATAL] {}: cannot write '{}'.", kWho, path));
  out.precision(17); // as %.17g: the NLL is compared between runs to 1e-3, at values up to 1e9
  out << header << '\n';
  return out;
}

// Returns the sections fitted, each with whether it was fitted at the source binning as well.
std::map<std::string, bool> DumpQuality(TFile& file, const std::string& binningName, const std::string& path)
{
  std::map<std::string, bool> sections;
  std::ofstream out = OpenTable(path, "section\tbinning\tcell\tsigModel\tbkgModel\tcounts\tnumFloating\tminNll\tstart\tdNLLconfig\toverride\treasons");
  for (const FitQualityRecorder::Row& row : FitQualityRecorder::Read(file.GetDirectory(binningName.c_str()), kWho)) {
    sections[row.section] |= (row.binning == FitQualityRecorder::kSourceBinning);
    std::string reasons;
    for (const std::string& reason : FitQualityRecorder::Reasons(row.fit))
      reasons += (reasons.empty() ? "" : "; ") + reason;
    const std::vector<double> mult{row.multLow, row.multHigh}, pt{row.ptLow, row.ptHigh};
    out << row.section << '\t' << row.binning << '\t' << OutputNames::CellTag(mult, 0, pt, 0) << '\t' << row.sigModel << '\t'
        << row.bkgModel << '\t' << row.fit.counts << '\t' << row.fit.numFloating << '\t' << row.fit.minNll << '\t' << row.fit.start
        << '\t' << row.fit.nllConfig - row.fit.minNll << '\t' << (row.binOverride.empty() ? "-" : row.binOverride) << '\t'
        << (reasons.empty() ? "-" : reasons) << '\n';
  }
  return sections;
}

void WriteValues(std::ofstream& out, const std::string& quantity, const std::string& binning, const std::string& cell, double value, double error)
{
  out << quantity << '\t' << binning << '\t' << cell << '\t' << value << '\t' << error << '\n';
}

void DumpPurities(TFile& file, const std::string& binningName, const std::map<std::string, bool>& sections, const std::string& path)
{
  const std::vector<std::string> summaryPath = OutputNames::SummaryPath(binningName);
  const std::vector<double> multBinning = RootIO::ReadBinningStamp(RootIO::FindPath(&file, {binningName}), RootIO::kStampMult, kWho);
  if (multBinning.empty())
    throw std::runtime_error(std::format("[FATAL] {}: no '{}' stamp in '{}'.", kWho, RootIO::kStampMult, file.GetName()));
  std::ofstream out = OpenTable(path, "quantity\tbinning\tcell\tvalue\terror");
  for (const auto& [key, withSource] : sections) {
    for (int multBin : BinningUtils::BinIndices(multBinning)) {
      const std::string name = OutputNames::Purity(key, OutputNames::MultTag(multBinning, multBin));
      auto dump = [&](const std::vector<std::string>& dirPath, OutputNames::SpectrumBinning which, const std::string& label) {
        auto h = RootIO::GetUniqueOrThrow<TH1>(RootIO::FindPath(&file, dirPath), name + std::string(OutputNames::BinningSuffix(which)), kWho);
        const std::vector<double> pt = BinningUtils::AxisEdges(h->GetXaxis());
        for (int ptBin : BinningUtils::BinIndices(pt))
          WriteValues(out, key, label, OutputNames::CellTag(multBinning, multBin, pt, ptBin), h->GetBinContent(BinningUtils::RootBin(ptBin)),
                      h->GetBinError(BinningUtils::RootBin(ptBin)));
      };
      dump(summaryPath, OutputNames::SpectrumBinning::Analysis, FitQualityRecorder::kAnalysisBinning);
      if (withSource)
        dump(OutputNames::SourceBinningPath(summaryPath), OutputNames::SpectrumBinning::Source, FitQualityRecorder::kSourceBinning);
    }
  }
}

void DumpPhi(TFile& file, const std::string& binningName, const std::string& path)
{
  TDirectory* summary = RootIO::FindPath(&file, OutputNames::SummaryPath(binningName));
  std::ofstream out = OpenTable(path, "quantity\tbinning\tcell\tvalue\terror");
  for (const auto& [quantity, name] : {std::pair{"signal", OutputNames::kTriggerSignal}, std::pair{"r", OutputNames::kTriggerBkgRatio}}) {
    auto h = RootIO::GetUniqueOrThrow<TH2>(summary, name, kWho);
    const std::vector<double> mult = BinningUtils::AxisEdges(h->GetXaxis()), pt = BinningUtils::AxisEdges(h->GetYaxis());
    for (int multBin : BinningUtils::BinIndices(mult))
      for (int ptBin : BinningUtils::BinIndices(pt)) {
        const int binX = BinningUtils::RootBin(multBin), binY = BinningUtils::RootBin(ptBin);
        WriteValues(out, quantity, FitQualityRecorder::kAnalysisBinning, OutputNames::CellTag(mult, multBin, pt, ptBin),
                    h->GetBinContent(binX, binY), h->GetBinError(binX, binY));
      }
  }
}
} // namespace

int main(int argc, char** argv)
{
  const std::vector<std::string> args(argv + 1, argv + argc);
  const bool purity = args.size() == 6 && args[0] == "purity", phi = args.size() == 6 && args[0] == "phi";
  if (!purity && !phi) {
    std::fprintf(stderr, "Usage: dump purity|phi <directory> <run> <binning_name> <quality.tsv> <values.tsv>\n");
    return 2;
  }
  try {
    const std::string& binningName = args[3];
    std::unique_ptr<TFile> file = RootIO::OpenOrThrow(
      std::format("{}/{}_{}", args[1], args[2], purity ? OutputNames::kPurityFile : OutputNames::kPhiDataFile), "READ", kWho);
    const std::map<std::string, bool> sections = DumpQuality(*file, binningName, args[4]);
    if (purity)
      DumpPurities(*file, binningName, sections, args[5]);
    else
      DumpPhi(*file, binningName, args[5]);
  } catch (const std::exception& e) {
    Logger::Error("{}", e.what());
    return 1;
  }
  return 0;
}
