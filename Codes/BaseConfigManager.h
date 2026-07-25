#pragma once

#include "AnalysisDataStructures.h"

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"

#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>

class BaseConfigManager
{
 protected:
  rapidjson::Document document;
  bool isLoaded{false};
  std::string managerName;

  // Constructor handles the rapidjson I/O stream reading.
  BaseConfigManager(const std::string& jsonPath, const std::string& managerName) : managerName(managerName)
  {
    FILE* fp = fopen(jsonPath.c_str(), "rb");
    if (!fp) {
      throw std::runtime_error("[FATAL ERROR] " + managerName +
                               ": Cannot open configuration file at path: " + jsonPath);
    }

    // Create a memory buffer (64KB) to read the file in chunks.
    // This makes reading significantly faster than reading line-by-line.
    char readBuffer[65536];
    rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));

    document.ParseStream(is);
    fclose(fp);

    // Check for parsing errors and validate the JSON structure.
    if (document.HasParseError()) {
      throw std::runtime_error("[FATAL ERROR] " + managerName +
                               ": JSON Syntax Error! Code: " + std::to_string(document.GetParseError()));
    }

    if (!document.IsObject()) {
      throw std::runtime_error("[FATAL ERROR] " + managerName +
                               ": Invalid JSON structure. Root element must be a JSON Object!");
    }

    isLoaded = true;
    std::cout << "[INFO] " << managerName << ": JSON successfully loaded into RAM." << std::endl;
  }

  // Universal parameter parser (free or fixed)
  void LoadParam(const rapidjson::Value& node, MathParam& param) const
  {
    if (node.IsArray() && node.Size() == 3) {
      // Free parameter: [initial_value, min, max]
      param.val = node[0].GetDouble();
      param.min = node[1].GetDouble();
      param.max = node[2].GetDouble();
      param.isConstant = false;
    } else if (node.IsNumber()) {
      // Fixed parameter: single number
      param.val = node.GetDouble();
      param.isConstant = true;
    } else {
      throw std::runtime_error("[FATAL ERROR] " + managerName + ": Invalid parameter format in JSON. Must be a Number or an Array of 3 Numbers.");
    }
  }
};
