#pragma once

// Facts about the run itself: when it happened and which code produced it.
//
// Deliberately free of ROOT. It is the one header in the framework that compiles
// with the standard library alone, which is what makes it testable with a plain
// compiler instead of only through the interpreter - and the reason it is not in
// RootIOHelpers.h, where everything else touches a TFile.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace RunEnvironment
{
// FNV-1a, written out rather than reaching for std::hash: that one is not
// required to give the same answer across implementations or even across runs,
// and a fingerprint that changes on its own answers no question at all.
inline std::uint64_t Fnv1a(std::string_view data, std::uint64_t hash = 14695981039346656037ULL)
{
  for (unsigned char c : data) {
    hash ^= c;
    hash *= 1099511628211ULL;
  }
  return hash;
}

// Which commit the working tree is on, read straight from .git without invoking
// anything. Empty when there is no repository to read.
//
// Note what this does NOT say: whether the tree was clean. A run made on modified
// sources records the commit it was based on, which looks precise and is not -
// hence SourcesFingerprint below, which answers the question this one only seems
// to answer.
inline std::string GitCommit(const std::string& gitDir = "../.git")
{
  std::ifstream head(gitDir + "/HEAD");
  if (!head)
    return "";

  std::string line;
  std::getline(head, line);
  if (line.rfind("ref: ", 0) != 0)
    return line; // detached HEAD: the line already is the sha

  const std::string ref = line.substr(5);

  std::ifstream loose(gitDir + "/" + ref);
  if (loose) {
    std::string sha;
    std::getline(loose, sha);
    return sha;
  }

  // Freshly cloned or garbage-collected repositories keep refs in one file.
  std::ifstream packed(gitDir + "/packed-refs");
  while (std::getline(packed, line)) {
    const size_t space = line.find(' ');
    if (space != std::string::npos && line.substr(space + 1) == ref)
      return line.substr(0, space);
  }
  return "";
}

// One number over every header in the framework directory, in name order. This is
// what actually ran, committed or not, which is the difference that matters: most
// of a working session happens on a tree that no commit describes.
inline std::string SourcesFingerprint(const std::string& sourceDir = ".")
{
  std::vector<std::string> headers;
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(sourceDir, ec)) {
    if (entry.is_regular_file() && entry.path().extension() == ".h")
      headers.push_back(entry.path().string());
  }
  if (ec || headers.empty())
    return "";

  std::ranges::sort(headers);

  std::uint64_t hash = 14695981039346656037ULL;
  for (const std::string& path : headers) {
    std::ifstream file(path, std::ios::binary);
    if (!file)
      continue;
    // The name is folded in too, so that renaming a file changes the answer.
    hash = Fnv1a(std::filesystem::path(path).filename().string(), hash);
    const std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    hash = Fnv1a(content, hash);
  }

  char buffer[24];
  std::snprintf(buffer, sizeof(buffer), "%016llx", static_cast<unsigned long long>(hash));
  return buffer;
}

// Local time through strftime: std::format's chrono specifiers go through the
// same consteval validation that already forced snprintf on floating-point ones.
inline std::string TimestampNow()
{
  const std::time_t now = std::time(nullptr);
  char buffer[32];
  if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&now)) == 0)
    return "unknown";
  return buffer;
}

// The facts, gathered once per process: reading 330 kB of headers is cheap, doing
// it for every task is pointless.
//
// Both defaults assume the working directory is the framework's own, which
// runAnalysis.sh guarantees with its cd. Run from anywhere else they come back
// empty and nothing is recorded, which is the right outcome for documentation.
inline const std::map<std::string, std::string>& Facts()
{
  static const std::map<std::string, std::string> facts = [] {
    std::map<std::string, std::string> f;
    const std::string commit = GitCommit();
    const std::string sources = SourcesFingerprint();
    if (!commit.empty())
      f["git_commit"] = commit;
    if (!sources.empty())
      f["sources_fingerprint"] = sources;
    return f;
  }();
  return facts;
}

} // namespace RunEnvironment
