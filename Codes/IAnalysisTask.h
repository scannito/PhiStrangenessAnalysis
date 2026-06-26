#pragma once

#include "AnalysisSettings.h"

#include "rapidjson/document.h"

#include <string>

class IAnalysisTask
{
 public:
  // Virtual destructor is mandatory for proper memory cleanup of derived classes
  virtual ~IAnalysisTask() = default;

  // Returns the exact name of the JSON node this task expects (e.g., "purity_task")
  virtual std::string GetName() const = 0;

  // Initializes the task with global settings AND its specific JSON branch in memory
  virtual void Init(const rapidjson::Value& taskConfig, const AnalysisSettings& globalSettings) = 0;

  // Execute the main analysis loop (e.g., fitting, integrating, projecting)
  virtual void Run() = 0;

  // Save results to the output files, clean up memory, and close file pointers
  virtual void Terminate() = 0;
};
