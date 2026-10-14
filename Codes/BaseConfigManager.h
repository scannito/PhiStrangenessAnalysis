#pragma once

#include "AnalysisDataStructures.h"
#include "JsonConfigHelpers.h"
#include "Logger.h"

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"

#include <cstdio>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

class BaseConfigManager
{
 protected:
  rapidjson::Document document;
  bool isLoaded{false};
  std::string managerName;

  // Constructor handles the rapidjson I/O stream reading.
  BaseConfigManager(const std::string& jsonPath, const std::string& managerName) : managerName(managerName)
  {
    std::unique_ptr<FILE, int (*)(FILE*)> fp(fopen(jsonPath.c_str(), "rb"), &fclose);
    if (!fp) {
      throw std::runtime_error(std::format("[FATAL ERROR] {}: Cannot open configuration file at path: {}",
                                           managerName, jsonPath));
    }

    // Create a memory buffer (64KB) to read the file in chunks.
    // This makes reading significantly faster than reading line-by-line.
    char readBuffer[65536];
    rapidjson::FileReadStream is(fp.get(), readBuffer, sizeof(readBuffer));

    document.ParseStream(is);

    // Check for parsing errors and validate the JSON structure.
    if (document.HasParseError()) {
      throw std::runtime_error(std::format("[FATAL ERROR] {}: JSON Syntax Error! Code: {}",
                                           managerName, static_cast<int>(document.GetParseError())));
    }

    if (!document.IsObject()) {
      throw std::runtime_error(std::format("[FATAL ERROR] {}: Invalid JSON structure. Root element must be a JSON Object!",
                                           managerName));
    }

    isLoaded = true;
    Logger::Info("{}: JSON successfully loaded into RAM.", managerName);
  }

  // Universal parameter parser: [value, min, max] to let it float, a single value
  // to fix it. Both forms accept numbers written as strings, like every other
  // number in these configurations.
  void LoadParam(const rapidjson::Value& node, const std::string& paramName, MathParam& param) const
  {
    const std::string what = std::format("parameter '{}'", paramName);

    if (node.IsArray()) {
      const std::vector<double> values = JsonConfig::ReadNumberArray(node.GetArray(), what, managerName);
      if (values.size() != 3) {
        throw std::runtime_error(std::format("[FATAL] {}: {} is an array of {} entries; a free parameter needs "
                                             "exactly three, [value, min, max].",
                                             managerName, what, values.size()));
      }
      param.val = values[0];
      param.min = values[1];
      param.max = values[2];
      param.isConstant = false;
      return;
    }

    // Anything else must be a single number: ToNumber says so by name if it is not.
    param.val = JsonConfig::ToNumber(node, what, managerName);
    param.isConstant = true;
  }
};
