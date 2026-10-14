#include "RunAnalysisChain.h"

#include "TSystem.h"

#include <string>

// Entry point for the ROOT interpreter, and the only place aware of gSystem.
// A macro cannot propagate its return value to the process exit status, so the
// translation from status code to process termination happens here.
void PhiStrangenessCorrelation(const std::string& configFile = "globalConfig.json", const std::string& baseConfigFile = "")
{
  if (RunAnalysisChain(configFile, baseConfigFile) != 0) {
    gSystem->Exit(1);
  }
}
