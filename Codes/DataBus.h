#pragma once

#include "TObject.h"

#include <iostream>
#include <string>
#include <unordered_map>

class DataBus
{
 private:
  // Map that associates a string key (e.g., "Eff_Phi_Bin0") with a generic ROOT object pointer
  std::unordered_map<std::string, TObject*> memoryStore;

 public:
  // Publishes an object to the Bus
  void Publish(const std::string& key, TObject* obj)
  {
    if (!obj)
      return;
    memoryStore[key] = obj;
    std::cout << "[BUS] Published object: " << key << std::endl;
  }

  // Retrieves an object from the Bus, automatically casting it to the requested type (e.g., TH1F)
  template <typename T>
  T* Retrieve(const std::string& key)
  {
    auto it = memoryStore.find(key);
    if (it != memoryStore.end()) {
      std::cout << "[BUS] Retrieved object: " << key << " from RAM!" << std::endl;
      return dynamic_cast<T*>(it->second);
    }
    return nullptr; // Returns nullptr if the object is not found
  }

  // Clears the memory at the end of the workflow
  void Clear()
  {
    for (auto& pair : memoryStore) {
      delete pair.second; // The Bus takes ownership and deletes the objects to prevent memory leaks
    }
    memoryStore.clear();
  }
};
