#include "RunAnalysisChain.h"

#include "TROOT.h"

#include <iostream>
#include <string>

// Entry point of the compiled executable, and with the ROOT macro the only place
// aware of process-wide ROOT state: RunAnalysisChain stays free of ROOT globals.
int main(int argc, char* argv[])
{
  // Equivalent of `root -b` used by the interpreted mode: no windows, no display.
  gROOT->SetBatch(kTRUE);

  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <configFile> [baseConfigFile]\n";
    return 1;
  }

  const std::string configFile = argv[1];
  const std::string baseConfigFile = (argc > 2) ? argv[2] : "";

  return RunAnalysisChain(configFile, baseConfigFile);
}
