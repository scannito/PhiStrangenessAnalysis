#pragma once

#include "TDirectory.h"
#include "TFile.h"

#include <memory>
#include <stdexcept>
#include <string>

struct TFileCloser {
  void operator()(TFile* f) const
  {
    if (f) {
      f->Close();
      delete f;
    }
  }
};

using TFileGuard = std::unique_ptr<TFile, TFileCloser>;

inline std::unique_ptr<TFile> OpenOrThrow(const std::string& path, const char* mode, const std::string& errCtx)
{
  // std::unique_ptr<TFile> f = std::make_unique<TFile>(path.c_str(), mode);
  std::unique_ptr<TFile> f(TFile::Open(path.c_str(), mode));
  if (!f || f->IsZombie())
    throw std::runtime_error("[FATAL] " + errCtx + ": Cannot open '" + path + "'");
  return f;
}

template <typename T>
inline T* GetOrThrow(TDirectory* dir, const std::string& objPath, const std::string& errCtx, bool detachFromFile = true)
{
  T* obj = static_cast<T*>(dir->Get(objPath.c_str()));
  if (!obj)
    throw std::runtime_error("[FATAL] " + errCtx + ": Missing object '" + objPath + "'");

  if constexpr (std::is_base_of_v<TH1, T>) {
    if (detachFromFile)
      obj->SetDirectory(0);
  }

  return obj;
}

template <typename T>
inline T* GetOrWarn(TDirectory* dir, const std::string& objPath, const std::string& warnCtx, bool detachFromFile = true)
{
  T* obj = static_cast<T*>(dir->Get(objPath.c_str()));
  if (!obj) {
    std::cerr << "[WARNING] " << warnCtx << ": Missing object '" << objPath << "'" << std::endl;
    return nullptr;
  }

  if constexpr (std::is_base_of_v<TH1, T>) {
    if (detachFromFile)
      obj->SetDirectory(0);
  }

  return obj;
}

template <typename T>
inline std::unique_ptr<T> GetUniqueOrThrow(TDirectory* dir, const std::string& objPath, const std::string& errCtx, bool detachFromFile = true)
{
  T* obj = static_cast<T*>(dir->Get(objPath.c_str()));
  if (!obj)
    throw std::runtime_error("[FATAL] " + errCtx + ": Missing object '" + objPath + "'");

  if constexpr (std::is_base_of_v<TH1, T>) {
    if (detachFromFile)
      obj->SetDirectory(0);
  }

  return std::unique_ptr<T>(obj);
}
