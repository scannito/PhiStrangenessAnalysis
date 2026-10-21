// How ExtrapConfigManager reads the "high" block and the "params" of a node (README, "The extrapolated
// yield"; DESIGN_HISTORY.md, "A second fit for the high side"). Each case is a small extrapConfig written
// to the scratch directory, read for multiplicity classes 0, 3 and 4, and compared with what it must give:
// either a configuration or a [FATAL] containing the given text. PASS if every case matches.
//
// Usage: configHigh <scratch directory>
#include "ExtrapConfigManager.h"

#include <cstdio>
#include <format>
#include <fstream>
#include <string>
#include <vector>

namespace
{
struct Case {
  std::string name;
  std::string json;     // the "Pi" section
  std::string expected; // what the summary of classes 0, 3, 4 must be, or the text the [FATAL] must contain
};

const std::string kParams = R"("params": {"n": [5.0, 2.0, 20.0], "T": [0.3, 0.05, 0.8], "norm": [1.5, 0.0, 1000.0]})";
const std::string kNode = R"("model": "LevyTsallis", "domain_range": [0.0, 10.0], "fit_range": [0.2, 1.0], )" + kParams;
const std::string kHigh = R"({"model": "LevyTsallis", "fit_range": [1.0, 3.0], )" + kParams + "}";

// One fit, in one line: model, range, parameters (a fixed one with its min and max, which must be zero).
std::string Describe(const ExtrapFit& fit)
{
  std::string out = std::format("{} {:g}-{:g}", fit.model, fit.fitRange.first, fit.fitRange.second);
  for (const auto& [name, p] : fit.params)
    out += std::format(" {}={:g}{}", name, p.val, p.isConstant ? std::format("(fixed, {:g}..{:g})", p.min, p.max) : "");
  return out;
}

// The configuration of classes 0, 3 and 4, in one line.
std::string Summary(const ExtrapConfigManager& manager)
{
  std::string out;
  for (int bin : {0, 3, 4}) {
    const ExtrapConfig c = manager.GetConfig("Pi", bin);
    out += std::format("[{}] {}", bin, Describe(c));
    if (c.high)
      out += " | high " + Describe(*c.high);
    out += "; ";
  }
  return out;
}
} // namespace

int main(int argc, char** argv)
{
  if (argc != 2) {
    fprintf(stderr, "usage: %s <scratch directory>\n", argv[0]);
    return 1;
  }
  const std::vector<Case> cases = {
    // A bin override can switch the high fit off with null, or replace it whole (no params: it does not
    // inherit the default's, so "params" is required there and the replacement has its own).
    {"high, null and a replacement in two bins",
     R"({"default": {)" + kNode + R"(, "high": )" + kHigh + R"(}, "bins": {"multBin3": {"high": null}, "multBin4": {"high": {"model": "LevyTsallis", "fit_range": [0.8, 3.0], "params": {"n": 6.0, "T": 0.3, "norm": 2.0}}}}})",
     "[0] LevyTsallis 0.2-1 T=0.3 n=5 norm=1.5 | high LevyTsallis 1-3 T=0.3 n=5 norm=1.5; [3] LevyTsallis 0.2-1 T=0.3 n=5 norm=1.5; "
     "[4] LevyTsallis 0.2-1 T=0.3 n=5 norm=1.5 | high LevyTsallis 0.8-3 T=0.3(fixed, 0..0) n=6(fixed, 0..0) norm=2(fixed, 0..0); "},
    // Parameters are merged by name: the override replaces two, keeps the third; a fixed one has min and
    // max at zero, not what the default had; a number written as a string is a number.
    {"params merged by name, fixed, as a string",
     R"({"default": {)" + kNode + R"(}, "bins": {"multBin3": {"params": {"n": 7.5, "T": "0.25"}}}})",
     "[0] LevyTsallis 0.2-1 T=0.3 n=5 norm=1.5; [3] LevyTsallis 0.2-1 T=0.25(fixed, 0..0) n=7.5(fixed, 0..0) norm=1.5; [4] LevyTsallis 0.2-1 T=0.3 n=5 norm=1.5; "},
    {"high not an object", R"({"default": {)" + kNode + R"(, "high": 5}})", "section 'Pi', high: key 'high' must be an object, or null for none"},
    {"high without model", R"({"default": {)" + kNode + R"(, "high": {"fit_range": [1.0, 3.0], )" + kParams + "}}}", "section 'Pi', high: 'model' missing"},
    {"high without params", R"({"default": {)" + kNode + R"(, "high": {"model": "LevyTsallis", "fit_range": [1.0, 3.0]}}})", "section 'Pi', high: 'params' missing"},
    {"high with fit_range a string", R"({"default": {)" + kNode + R"(, "high": {"model": "LevyTsallis", "fit_range": "1-3", )" + kParams + "}}}", "section 'Pi', high: 'fit_range' is not an array"},
    {"misspelled key next to high", R"({"default": {)" + kNode + R"(, "hihg": )" + kHigh + "}}", "section 'Pi': unknown key 'hihg'"},
    {"parameter array of two", R"({"default": {"model": "LevyTsallis", "fit_range": [0.2, 1.0], "params": {"n": [5.0, 2.0]}}})", "section 'Pi': parameter 'n' is an array of 2 entries"},
    {"parameter array of two inside high", R"({"default": {)" + kNode + R"(, "high": {"model": "LevyTsallis", "fit_range": [1.0, 3.0], "params": {"n": [1, 2]}}}})", "section 'Pi', high: parameter 'n' is an array of 2 entries"},
    {"parameter of the wrong type", R"({"default": {"model": "LevyTsallis", "fit_range": [0.2, 1.0], "params": {"T": true}}})", "section 'Pi': 'T' is neither a number nor a string"},
  };

  int failed = 0;
  for (size_t i = 0; i < cases.size(); ++i) {
    const std::string path = std::format("{}/configHigh_{}.json", argv[1], i);
    std::ofstream(path) << R"({"Pi": )" << cases[i].json << "}";
    std::string got;
    try {
      ExtrapConfigManager manager(path);
      got = Summary(manager);
    } catch (const std::exception& e) {
      got = e.what();
    }
    const bool ok = got == cases[i].expected || (got.starts_with("[FATAL]") && got.find(cases[i].expected) != std::string::npos);
    failed += !ok;
    printf("%-4s %s\n     %s\n", ok ? "ok" : "FAIL", cases[i].name.c_str(), got.c_str());
    if (!ok)
      printf("     expected: %s\n", cases[i].expected.c_str());
  }
  printf("%s: %zu cases, %d failed\n", failed ? "FAIL" : "PASS", cases.size(), failed);
  return failed ? 2 : 0;
}
