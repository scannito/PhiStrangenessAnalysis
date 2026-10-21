#pragma once

#include "AnalysisDataStructures.h"
#include "JsonConfigHelpers.h"
#include "Logger.h"

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"

#include <cstdio>
#include <format>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
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
      throw std::runtime_error(std::format("[FATAL] {}: Cannot open configuration file at path: {}",
                                           managerName, jsonPath));
    }

    // Create a memory buffer (64KB) to read the file in chunks.
    // This makes reading significantly faster than reading line-by-line.
    char readBuffer[65536];
    rapidjson::FileReadStream is(fp.get(), readBuffer, sizeof(readBuffer));

    document.ParseStream(is);

    // Check for parsing errors and validate the JSON structure.
    if (document.HasParseError()) {
      throw std::runtime_error(std::format("[FATAL] {}: JSON Syntax Error! Code: {}",
                                           managerName, static_cast<int>(document.GetParseError())));
    }

    if (!document.IsObject()) {
      throw std::runtime_error(std::format("[FATAL] {}: Invalid JSON structure. Root element must be a JSON Object!",
                                           managerName));
    }

    isLoaded = true;
    Logger::Info("{}: JSON successfully loaded into RAM.", managerName);
  }

  // The entries of the "params" object of 'node', merged by name into 'params': an entry
  // replaces the parameter of that name and the others stay, since a bin override carries
  // only what it changes. Absent "params" leaves them all.
  static void MergeParams(const rapidjson::Value& node, std::map<std::string, MathParam>& params, std::string_view errCtx)
  {
    if (const rapidjson::Value* entries = JsonConfig::TryObject(node, "params", errCtx)) {
      for (auto& m : entries->GetObject()) {
        const char* name = m.name.GetString();
        params[name] = ReadParam(m.value, name, errCtx);
      }
    }
  }

 private:
  // [value, min, max] to let it float, a single value to fix it. Both forms accept numbers
  // written as strings, like every other number in these configurations. A whole parameter
  // every time: a fixed one has min and max at zero, not whatever an earlier pass left.
  static MathParam ReadParam(const rapidjson::Value& value, std::string_view paramName, std::string_view errCtx)
  {
    if (value.IsArray()) {
      const std::vector<double> values = JsonConfig::ReadNumberArray(value.GetArray(), paramName, errCtx);
      if (values.size() != 3) {
        throw std::runtime_error(std::format("[FATAL] {}: parameter '{}' is an array of {} entries; a free parameter "
                                             "needs exactly three, [value, min, max].",
                                             errCtx, paramName, values.size()));
      }
      return {.val = values[0], .min = values[1], .max = values[2], .isConstant = false};
    }

    // Anything else must be a single number: ToNumber says so by name if it is not.
    return {.val = JsonConfig::ToNumber(value, paramName, errCtx), .isConstant = true};
  }
};
