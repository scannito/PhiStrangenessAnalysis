#include "WorkflowManager.h"

#include "TSystem.h"

#include <iostream>
#include <string>

// Runs the whole analysis chain and reports the outcome as a status code.
// Deliberately free of any ROOT global: this body can be reused as-is inside a
// compiled main(), where "return status" is all that is needed.
int RunAnalysisChain(const std::string& configFile, const std::string& baseConfigFile)
{
  std::cout << "======================================================" << std::endl;
  std::cout << "        ALICE Data Analysis - Phi-Strangeness         " << std::endl;
  std::cout << "======================================================" << std::endl;

  try {
    // 1. Create the Workflow Manager.
    // Upon creation, it parses the JSON file, reads the paths,
    // and safely stores them in the GlobalSettings struct.
    WorkflowManager manager(configFile, baseConfigFile);

    // 2. Build the task pipeline.
    // Here the manager sets the execution order (e.g., Purity -> MC -> Data).
    manager.BuildWorkflow();

    // 3. Run the analysis!
    // The manager will take each task, pass the settings to it (Init),
    // execute it (Run), and then safely save and clean up (Terminate),
    // one after the other.
    manager.RunWorkflow();

    std::cout << "======================================================" << std::endl;
    std::cout << " [SUCCESS] Entire analysis chain completed!           " << std::endl;
    std::cout << "======================================================" << std::endl;
  } catch (const std::exception& e) {
    // If ANYTHING goes wrong with a file, memory allocation, or JSON parsing,
    // the exception is safely caught here without crashing the ROOT interpreter.
    // The manager is destroyed during stack unwinding, so every output file has
    // already been closed properly by the time we get here.
    std::cerr << "\n[FATAL ERROR] Analysis aborted due to an exception:" << std::endl;
    std::cerr << " -> " << e.what() << std::endl;
    std::cerr << "======================================================" << std::endl;
    return 1;
  }

  return 0;
}

// Entry point for the ROOT interpreter, and the only place aware of gSystem.
// A macro cannot propagate its return value to the process exit status, so the
// translation from status code to process termination happens here.
void PhiStrangenessCorrelation(const std::string& configFile = "globalConfig.json", const std::string& baseConfigFile = "")
{
  if (RunAnalysisChain(configFile, baseConfigFile) != 0) {
    gSystem->Exit(1);
  }
}
