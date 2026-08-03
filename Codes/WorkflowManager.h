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
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

class WorkflowManager
{
 public:
  // Constructor supporting both single-file and dual-file (Base + Master) configurations.
  WorkflowManager(const std::string& masterConfigFile = "globalConfig.json",
                  const std::string& baseConfigFile = "")
  {
    // 1. Load the Master File (Always required)
    LoadJsonFile(masterConfigFile, document, "MasterConfig");

    // 2. Load the Base File (Only if provided by the user)
    if (!baseConfigFile.empty()) {
      LoadJsonFile(baseConfigFile, baseDocument, "BaseConfig");
      hasBaseFile = true;
      std::cout << "[INFO] WorkflowManager: Dual-file configuration mode activated." << std::endl;
    } else {
      hasBaseFile = false;
      std::cout << "[INFO] WorkflowManager: Single-file configuration mode activated." << std::endl;
    }

    ParseGlobalSettings();
    ParseWorkflowTasks();
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

    // Loop over the tasks requested by the JSON configuration
    for (const std::string& taskName : activeTasksList) {
      bool taskFound = false;

      for (const auto& [baseName, factory] : taskRegistry) {
        // Task aliasing: "correlation_task_nominal" is served by "correlation_task".
        if (taskName.starts_with(baseName)) {
          // Store the JSON block name TOGETHER with the instance: a skipped
          // unknown task must not shift the configuration of the following ones.
          activeTasks.push_back(ScheduledTask{taskName, factory()});
          taskFound = true;
          break; // Task instantiated successfully, move to the next JSON item
        }
      }

      // An unrecognized name means the workflow would silently run fewer tasks
      // than requested: abort instead, listing what the registry accepts.
      if (!taskFound) {
        std::string knownPrefixes;
        for (const auto& [baseName, factory] : taskRegistry) {
          knownPrefixes += (knownPrefixes.empty() ? "" : ", ") + baseName;
        }
        throw std::runtime_error("[FATAL] WorkflowManager: Unknown task requested: '" + taskName +
                                 "'. A task name must start with one of the registered prefixes: " + knownPrefixes);
      }
    }

    std::cout << "[INFO] Workflow built successfully. Number of active tasks: " << activeTasks.size() << std::endl;
  }

  void RunWorkflow()
  {
    std::cout << "WorkflowManager: Running analysis workflow..." << std::endl;
    std::cout << "------------------------------------------------" << std::endl;

    for (auto& [configBlockName, task] : activeTasks) {
      std::cout << "\n>>> [STARTING TASK] " << configBlockName << " <<<" << std::endl;

      // Generate the final merged JSON configuration (resolving N-levels of inheritance)
      rapidjson::Value finalTaskConfig = MergeTaskConfiguration(configBlockName);

      // Which block this is - "mc_task_2024", not the "mc_task" prefix the registry
      // matched. Only this loop knows it, and a task that records what produced its
      // output needs it. Added to the merged block rather than passed alongside:
      // that object is already synthetic, with inheritance resolved, so the resolved
      // identity belongs in it and travels wherever it goes. The leading underscore
      // marks a key nobody wrote in the JSON.
      if (finalTaskConfig.IsObject()) {
        rapidjson::Value key("_resolved_block_name", document.GetAllocator());
        rapidjson::Value value(configBlockName.c_str(), document.GetAllocator());
        if (finalTaskConfig.HasMember(key))
          finalTaskConfig.RemoveMember(key);
        finalTaskConfig.AddMember(key, value, document.GetAllocator());
      }

      // Pass the fully assembled config and global settings to the task
      task->Init(finalTaskConfig, globalSettings);
      task->Run();
      task->Terminate();

      // Destroy the task as soon as it is done. Everything it owns (output
      // files, canvases, loaded histograms) is released here instead of
      // surviving until the end of the workflow. Tasks communicate through
      // ROOT files, never in memory, so nothing downstream needs this instance.
      task.reset();

      std::cout << ">>> [TASK COMPLETED] " << configBlockName << " <<<" << std::endl;
      std::cout << "------------------------------------------------" << std::endl;
    }

    std::cout << "==================================================" << std::endl;
    std::cout << "[INFO] WorkflowManager: ALL TASKS COMPLETED SUCCESSFULLY!" << std::endl;
    std::cout << "==================================================" << std::endl;
  }

 private:
  rapidjson::Document document;     // Holds the master execution JSON tree
  rapidjson::Document baseDocument; // Holds the base settings JSON tree (if provided)
  bool hasBaseFile{false};

  // Pairs an instantiated task with the JSON configuration block it must read.
  // Keeping the two in a single struct makes it impossible for them to drift
  // apart, which is what happened when an unknown task name was skipped.
  struct ScheduledTask {
    std::string configBlockName;
    std::unique_ptr<IAnalysisTask> task;
  };

  // Contains the raw strings from the JSON (e.g., "correlation_task_test")
  std::vector<std::string> activeTasksList;
  // Contains the instantiated tasks, each carrying its own config block name
  std::vector<ScheduledTask> activeTasks;

  AnalysisSettings globalSettings; // Holds the runtime configuration

  // Safe file loader helper utilizing exceptions
  void LoadJsonFile(const std::string& path, rapidjson::Document& doc, std::string_view context)
  {
    std::unique_ptr<FILE, int (*)(FILE*)> fp(fopen(path.c_str(), "rb"), &fclose);
    if (!fp) {
      throw std::runtime_error(std::format("[FATAL] {}: cannot open configuration file at: {}", context, path));
    }

    // 64KB read buffer for fast chunked I/O
    char readBuffer[65536];
    rapidjson::FileReadStream is(fp.get(), readBuffer, sizeof(readBuffer));
    doc.ParseStream(is);

    if (doc.HasParseError()) {
      // RapidJSON provides the specific error code and the exact byte offset!
      throw std::runtime_error(std::format("[FATAL] {}: JSON syntax error in file '{}' at byte offset {}",
                                           context, path, doc.GetErrorOffset()));
    }

    if (!doc.IsObject()) {
      throw std::runtime_error(std::format("[FATAL] {}: invalid structure in {}. The root must be a JSON object.",
                                           context, path));
    }
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

  }

  void ParseWorkflowTasks()
  {
    if (!document.HasMember("workflow")) {
      throw std::runtime_error("[FATAL] Missing 'workflow' block in Master JSON!");
    }

    const auto& workflowNode = document["workflow"];
    if (workflowNode.HasMember("active_tasks") && workflowNode["active_tasks"].IsArray()) {
      for (auto& taskValue : workflowNode["active_tasks"].GetArray()) {
        activeTasksList.push_back(taskValue.GetString());
      }
    } else {
      throw std::runtime_error("[FATAL] 'active_tasks' array missing or invalid in workflow block!");
    }
  }

  // =========================================================================
  // HELPER: Merge base settings with task-specific overrides (Inheritance)
  // Supports N-levels of inheritance via recursion.
  // =========================================================================
  rapidjson::Value MergeTaskConfiguration(const std::string& taskName)
  {
    bool isInMaster = document.HasMember(taskName.c_str());
    bool isInBase = hasBaseFile && baseDocument.HasMember(taskName.c_str());

    if (!isInMaster && !isInBase) {
      throw std::runtime_error("[FATAL] JSON configuration block '" + taskName + "' not found in any file!");
    }

    // Target the document where the block was found (Master takes precedence)
    rapidjson::Value& currentSrc = isInMaster ? document[taskName.c_str()] : baseDocument[taskName.c_str()];

    // BASE CASE: No inheritance requested. Return a standalone deep copy.
    if (!currentSrc.HasMember("inherits")) {
      return rapidjson::Value(currentSrc, document.GetAllocator());
    }

    // RECURSIVE CASE: Resolve the parent chain first
    std::string baseName = currentSrc["inherits"].GetString();
    rapidjson::Value mergedConfig = MergeTaskConfiguration(baseName);

    // Apply the local overrides from the child to the resolved parent
    for (auto& m : currentSrc.GetObject()) {
      std::string keyName = m.name.GetString();

      // Skip the "inherits" keyword itself, the task doesn't need to read it
      if (keyName == "inherits")
        continue;

      // Remove the existing key if we are overriding it
      if (mergedConfig.HasMember(m.name)) {
        mergedConfig.RemoveMember(m.name);
      }

      // Perform a safe deep copy of both the key and the value to avoid memory corruption
      rapidjson::Value keyCopy(m.name, document.GetAllocator());
      rapidjson::Value valCopy(m.value, document.GetAllocator());

      mergedConfig.AddMember(keyCopy, valCopy, document.GetAllocator());
    }

    return mergedConfig;
  }
};
