#include "WorkflowManager.h"

#include <iostream>
#include <string>

void PhiStrangenessCorrelation(const std::string& configFile = "globalConfig.json", const std::string& baseConfigFile = "")
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
    std::cerr << "\n[FATAL ERROR] Analysis aborted due to an exception:" << std::endl;
    std::cerr << " -> " << e.what() << std::endl;
    std::cerr << "======================================================" << std::endl;
  }
}
