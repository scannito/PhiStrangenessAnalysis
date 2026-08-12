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
#include <format>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
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

    // Resolved once and handed to both: they read the same node.
    const auto& workflowNode = JsonConfig::RequireMember(document, "workflow", "Master JSON");
    ParseGlobalSettings(workflowNode);
    ParseWorkflowTasks(workflowNode);
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

  // 'global_binning' lives inside 'workflow' because the top level of these files
  // is the namespace MergeTaskConfiguration searches for blocks: anything there can
  // be the target of an "inherits". Settings sat in it by convention only.
  void ParseGlobalSettings(const rapidjson::Value& workflowNode)
  {
    const rapidjson::Value* config = JsonConfig::TryObject(workflowNode, "global_binning", "workflow");
    if (!config) {
      // Not "using defaults": there are none. AnalysisSettings deliberately ships an
      // empty speciesPtBinning, because the correlation and MC productions have
      // different binnings and either one built in would be a false alarm for the
      // other family. Empty means VerifyPtBinning has nothing to compare against, so
      // the axes found in the input files are used unverified - which is the check
      // this framework is built on, switched off.
      std::cout << "[INFO] WorkflowManager: no 'global_binning' in 'workflow'. The pT and multiplicity "
                   "axes found in the input files will be used WITHOUT verification."
                << std::endl;
      return;
    }

    globalSettings.binningName = JsonConfig::OptionalString(*config, "binning_name", globalSettings.binningName, "global_binning");

    // Override multiplicity binning if specified in JSON
    if (auto multBins = JsonConfig::TryArray(*config, "multiplicity_binning", "global_binning")) {
      globalSettings.binsMult = JsonConfig::ReadNumberArray(*multBins, "multiplicity_binning", "global_binning");
      std::cout << "  -> Overridden 'multiplicity' binning from JSON." << std::endl;
    }

    // pT binning per species (e.g. Phi, K0S, Pi) can be overridden in the JSON as well
    if (const rapidjson::Value* ptBinning = JsonConfig::TryObject(*config, "pt_binning", "global_binning")) {
      for (auto& m : ptBinning->GetObject()) {
        std::string species = m.name.GetString();
        globalSettings.speciesPtBinning[species] =
          JsonConfig::ReadNumberArray(JsonConfig::ToArray(m.value, species, "global_binning.pt_binning"),
                                      species, "global_binning.pt_binning");
        std::cout << "  -> Overridden pT binning for species '" << species << "' from JSON." << std::endl;
      }
    }
  }

  void ParseWorkflowTasks(const rapidjson::Value& workflowNode)
  {
    // Missing and present-but-not-an-array used to give the same message; now the
    // two are told apart, which matters when the mistake is a typo in the type.
    for (auto& taskValue : JsonConfig::RequireArray(workflowNode, "active_tasks", "workflow"))
      activeTasksList.push_back(taskValue.GetString());
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
