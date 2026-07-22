#pragma once

#include "CorrelationTask.h"
#include "CorrelationWPDGTask.h"
#include "IAnalysisTask.h"
#include "MCTask.h"
#include "PhiFitTask.h"
#include "PurityTask.h"

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"

#include <cstdio>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

class WorkflowManager
{
 public:
  WorkflowManager(const std::string& configFile = "globalConfig.json")
  {
    LoadConfiguration(configFile); // Read the JSON file from disk immediately
  }

  void BuildWorkflow()
  {
    std::cout << "[INFO] WorkflowManager: Building analysis workflow..." << std::endl;

    // Define our "Task Registry" (Factory Pattern)
    // It is a list of pairs: { "task_prefix_name", Lambda_function_to_create_the_task }
    using TaskFactory = std::function<std::unique_ptr<IAnalysisTask>()>;

    // NOTE: Order matters! Place the longest/most specific names first to avoid overlaps.
    // (e.g., "correlation_wpdg_task" MUST be checked before "correlation_task")
    std::vector<std::pair<std::string, TaskFactory>> taskRegistry = {
      {"purity_task", []() { return std::make_unique<PurityTask>(); }},
      {"mc_task", []() { return std::make_unique<MCTask>(); }},
      {"phi_fit_task", []() { return std::make_unique<PhiFitTask>(); }},
      {"correlation_wpdg_task", []() { return std::make_unique<CorrelationWPDGTask>(); }},
      {"correlation_task", []() { return std::make_unique<CorrelationTask>(); }}};

    // Map to track used prefixes per task type ---
    std::map<std::string, std::set<std::string>> usedPrefixes;

    // Loop over the tasks requested by the JSON configuration
    for (const std::string& taskName : activeTasksList) {
      bool taskFound = false;

      for (const auto& [baseName, factory] : taskRegistry) {
        // Check if the requested task name STARTS with the registered base name (index == 0).
        // This allows task aliasing (e.g., "correlation_task_nominal") without false positives.
        if (taskName.find(baseName) == 0) {
          activeTasks.push_back(factory());
          taskFound = true;
          break; // Task instantiated successfully, move to the next JSON item
        }
      }

      if (!taskFound) {
        std::cerr << "[WARNING] Unknown task requested: '" << taskName << "'. Skipping." << std::endl;
      }
    }

    std::cout << "[INFO] Workflow built successfully. Number of active tasks: " << activeTasks.size() << std::endl;
  }

  void RunWorkflow()
  {
    std::cout << "WorkflowManager: Running analysis workflow..." << std::endl;
    std::cout << "------------------------------------------------" << std::endl;

    for (size_t i = 0; i < activeTasks.size(); ++i) {
      auto& task = activeTasks[i];

      // We DO NOT use task->GetName() anymore. We use the exact name the user typed in the JSON array.
      std::string configBlockName = activeTasksList[i];
      std::cout << "\n>>> [STARTING TASK] " << configBlockName << " <<<" << std::endl;

      // Generate the final merged JSON configuration for this specific task (handling 'inherits')
      rapidjson::Value finalTaskConfig = MergeTaskConfiguration(configBlockName);

      // Pass the fully assembled config to the task
      task->Init(finalTaskConfig, globalSettings);
      task->Run();
      task->Terminate();

      std::cout << ">>> [TASK COMPLETED] " << configBlockName << " <<<" << std::endl;
      std::cout << "------------------------------------------------" << std::endl;
    }

    std::cout << "==================================================" << std::endl;
    std::cout << "[INFO] WorkflowManager: ALL TASKS COMPLETED SUCCESSFULLY!" << std::endl;
    std::cout << "==================================================" << std::endl;
  }

 private:
  rapidjson::Document document; // The full JSON tree loaded into RAM

  // Contains the raw strings from the JSON (e.g., "correlation_task_test")
  std::vector<std::string> activeTasksList;
  // Contains the actual C++ pointers in the EXACT same order
  std::vector<std::unique_ptr<IAnalysisTask>> activeTasks;

  AnalysisSettings globalSettings; // Holds the runtime configuration

  void LoadConfiguration(const std::string& configFile)
  {
    std::cout << "[INFO] WorkflowManager: Parsing master config file " << configFile << "..." << std::endl;

    FILE* fp = fopen(configFile.c_str(), "rb");
    if (!fp) {
      throw std::runtime_error("[FATAL] Cannot open config file: " + configFile);
    }

    char readBuffer[65536];
    rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
    document.ParseStream(is);
    fclose(fp); // Close the file immediately. Everything is now in RAM.

    if (document.HasParseError()) {
      throw std::runtime_error("[FATAL] Invalid JSON syntax or missing 'global_settings' node!");
    }

    // 1. Load Global Settings
    ParseGlobalSettings();

    // 2. Load Workflow Tasks
    if (!document.HasMember("workflow")) {
      throw std::runtime_error("[FATAL ERROR] Missing 'workflow' block in JSON!");
    }

    const auto& workflowNode = document["workflow"];
    if (workflowNode.HasMember("active_tasks") && workflowNode["active_tasks"].IsArray()) {
      for (auto& taskValue : workflowNode["active_tasks"].GetArray()) {
        activeTasksList.push_back(taskValue.GetString());
      }
    } else {
      throw std::runtime_error("[FATAL ERROR] 'active_tasks' array missing or invalid!");
    }

    std::cout << "[INFO] WorkflowManager: Master configuration loaded successfully." << std::endl;
  }

  void ParseGlobalSettings()
  {
    if (!document.HasMember("global_binning")) {
      std::cout << "[INFO] WorkflowManager: 'global_binning' missing in JSON. Using default historical binning." << std::endl;
      return;
    }

    const auto& config = document["global_binning"];

    if (config.HasMember("binning_name")) {
      globalSettings.binningName = config["binning_name"].GetString();
    }

    // Override multiplicity binning if specified in JSON
    if (config.HasMember("multiplicity") && config["multiplicity"].IsArray()) {
      globalSettings.binsMult.clear();
      for (const auto& v : config["multiplicity"].GetArray())
        globalSettings.binsMult.push_back(v.GetDouble());
      std::cout << "  -> Overridden 'multiplicity' binning from JSON." << std::endl;
    }

    // pT binning per species (e.g., Phi, K0S, Pi) can be overridden in the JSON as well
    if (config.HasMember("pt_binning") && config["pt_binning"].IsObject()) {
      for (auto& m : config["pt_binning"].GetObject()) {
        std::string species = m.name.GetString();
        std::vector<double> bins;

        for (const auto& v : m.value.GetArray()) {
          if (v.IsNumber()) {
            // If it is already a number in the JSON, parse it directly
            bins.push_back(v.GetDouble());
          } else if (v.IsString()) {
            // If it is a string, attempt to cast it to a double
            try {
              bins.push_back(std::stod(v.GetString()));
            } catch (const std::invalid_argument& e) {
              throw std::runtime_error("[FATAL ERROR] Cannot cast the string '" +
                                       std::string(v.GetString()) + "' to a number for species: " + species);
            }
          } else {
            // Unsupported type (e.g., boolean, null, or a nested array)
            throw std::runtime_error("[FATAL ERROR] Invalid binning value for species: " + species +
                                     ". It must be either a number or a string.");
          }
        }

        globalSettings.speciesPtBinning[species] = std::move(bins);
        std::cout << "  -> Overridden pT binning for species '" << species << "' from JSON." << std::endl;
      }
    }

    globalSettings.UpdateBinCounts();
  }

  // =========================================================================
  // HELPER: Merge base settings with task-specific overrides (Inheritance)
  // =========================================================================
  rapidjson::Value MergeTaskConfiguration(const std::string& taskName)
  {
    // Check if the specific task block exists in the JSON
    if (!document.HasMember(taskName.c_str())) {
      throw std::runtime_error("[FATAL ERROR] Missing JSON config block for: " + taskName);
    }

    rapidjson::Value& specificConfig = document[taskName.c_str()];

    // If it doesn't inherit anything, just return a deep copy of itself
    if (!specificConfig.HasMember("inherits")) {
      return rapidjson::Value(specificConfig, document.GetAllocator());
    }

    // Get the name of the base configuration block
    std::string baseName = specificConfig["inherits"].GetString();
    if (!document.HasMember(baseName.c_str())) {
      throw std::runtime_error("[FATAL ERROR] Base config block '" + baseName + "' not found!");
    }

    // 1. Create a deep copy of the BASE block in memory
    rapidjson::Value mergedConfig;
    mergedConfig.CopyFrom(document[baseName.c_str()], document.GetAllocator());

    // 2. Iterate over the SPECIFIC block and overwrite/add members to the base copy
    for (auto& m : specificConfig.GetObject()) {
      std::string keyName = m.name.GetString();

      // Skip the "inherits" keyword itself, the task doesn't need to read it
      if (keyName == "inherits")
        continue;

      // If the key already exists in the base block, remove it to overwrite it
      if (mergedConfig.HasMember(m.name)) {
        mergedConfig.RemoveMember(m.name);
      }

      // Add the overriding member (doing a deep copy to avoid memory allocation issues)
      rapidjson::Value keyCopy(m.name, document.GetAllocator());
      rapidjson::Value valCopy(m.value, document.GetAllocator());
      mergedConfig.AddMember(keyCopy, valCopy, document.GetAllocator());
    }

    return mergedConfig;
  }
};
