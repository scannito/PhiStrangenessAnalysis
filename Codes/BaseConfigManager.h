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
      std::cerr << "[ERROR] " << managerName << ": Cannot open configuration file: " << jsonPath << std::endl;
      return;
    }

    // Create a memory buffer (64KB) to read the file in chunks.
    // This makes reading significantly faster than reading line-by-line.
    char readBuffer[65536];
    rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));

    document.ParseStream(is);
    fclose(fp);

    if (!document.HasParseError() && document.IsObject()) {
      isLoaded = true;
      std::cout << "[INFO] " << managerName << ": JSON successfully loaded into RAM." << std::endl;
    } else {
      std::cerr << "[ERROR] " << managerName << ": Syntax error or invalid object in the JSON file!" << std::endl;
    }
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
