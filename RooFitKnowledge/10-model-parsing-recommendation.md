# 10 - Replacing the hand-written model parsing: a decision document (14 October 2026)

For the project owner. Written by the researcher of notes 01 and 02 after the second pass. The evidence is in notes 01 (factory) and 02 (HS3); every number below was produced by a program in `SC2 = /Users/scannito/alice/Analysis/RooFitKnowledge/scratch/study0102` (`proto/`, `hs3/`, `misc/`). Tags: [verified] run here, [source] code read, [doc] documentation, [inferred] not tested.

## The decision on one page

**Question.** `DynamicRooFitter` builds its models by hand (`ParseModelString`, `CreatePdfComponent`, `CreateVar`: about 150 of the 366 code lines of `DynamicRooFitter.h`). Can RooFit's own machinery replace it?

**Short answer.** Yes, two ways work and give exactly the same fits as today; neither is clearly better than what you have for the 10 models you use now. Recommendation: **keep the hand-written parser (A) and add a validation layer to it now; move to factory strings (B) when the model list starts to grow; do not adopt HS3 JSON (C).**

| | A: keep hand-written | B: factory strings from `fitConfig.json` | C: HS3 JSON as the model description |
|---|---|---|---|
| Result equivalence | reference | **identical in every field on 1440 real cells** (k0s 340, xi 260, pi_tpc 360, pi_tof 340, phi 70 + 70), also with window variants and composite models | **identical on the same 1440 cells** (needs a 65-line importer/exporter pair for Chebychev and Voigtian) |
| `fitConfig.json` | unchanged | **unchanged**: model strings, params, windows, overrides as now | cannot be used as is: HS3 has no per-bin override, window, purity or sideband; you need a translator, or one HS3 file per cell plus a settings file |
| Errors and validation | clear messages for a missing parameter or unknown model; **runs silently** on min>max, start outside limits, a Chebychev that starts negative, a fraction above 1 | same messages; RooFit errors captured and rethrown with the statement; **a bad `EXPR::` formula kills the process** | RooFit's importer **accepts silently** 7 of the 16 defects tested (clamped value, missing parameter becomes 0, infinite range, wrong `extended`, unknown key, min>max); a validator you write is needed |
| Adding a model | an `else if` block (about 8 lines + include) | **one table row** (verified with `Bernstein2`: same minNll as the hand-built model) | a type name + importer/exporter if RooFit lacks it + a translator row + a validator row |
| Construction per fitter | **0.005 ms** (phi 0.075) | 3.4-5.4 ms uncached (1.7 ms per class, Cling); **0.02 ms** with a workspace cache (phi 0.11) | 0.65-4.8 ms uncached; 0.02 ms cached |
| Code | 366 lines in `DynamicRooFitter.h` | 215 there + 221 in a new `FactoryModel.h`: **net +70**, about 60 of it validation | 215 + 154 translator + 65 pair + 92 validator: **net +160** |
| Version / API dependence | public C++ constructors: a change fails at compile time | factory grammar, documented but with doc/source mismatches; needs Cling at run time (the project has it already); string-typed, so a class typo fails at run time | **experimental** (RooFit's own word); HS3 v0.2.9 "can still undergo minor and major changes"; RooFit writes "0.2"; names differ from the standard; `Detail` namespace; no validator exists yet |
| Cling (interpreted) | works today | **works** (K0S, phi, pi_tpc verified); needs `snprintf` instead of `std::format("{:.17g}")` | **works**: the full `DynamicRooFitterC` interpreted on 4 real cells, identical to 17 digits (third pass) |
| `DrawFit` (single pad and pulls): PNG and curves | reference | **byte-identical PNG, identical curve points**, 5 real cells x 2 canvases | **identical after one line**: HS3 carries no variable title, so the axis label was the name until `obs->SetTitle(cfg.obs.title)` was set |

**Why not simply B?** B removes about 150 lines of parsing and adds about 220 of generator, cache and validation. What you buy is data-driven models (a row, not a code block) and RooFit's own error text. What it costs is real: a run-time string layer where there was compile-time typing, 1.7 ms per class of Cling time that forces a cache, and one failure (a bad formula) that terminates the process. The clearest gain of this study is not B itself but the **validation layer, which works with A as well** (verified on the hand-written class: `proto/out-aplus.txt`).

**What to do.**
1. Now, with no change of route: add to the existing code (a) a config check (min < max, start value inside limits) and (b) a check that the model is finite and non-negative at the start values at every bin centre, before `fitTo` (about 30 lines together; the code is `CheckConfig` and `RequireFiniteAtStart` in section 6). They turn the four silent numeric defects of this study (min > max, start outside limits, a negative Chebychev start, a fraction above 1) into a clear error before the fit.
2. When you add the third or fourth new model, or want the model library as one visible table, switch to B using the prototype in section 6 (bit-identical, Cling-safe, cached). The project's window / purity / sideband code stays untouched, as in the prototype.
3. Use HS3 as an OUTPUT, not as the model description (new, third pass, section 8): an opt-in provenance record per fit (about 5.5 KB and 0.13 ms per cell) that records the status/edm/covQual/minNll, the fitted parameters with errors and limits, the windows, the yields and a hash of the input histogram, and re-imports to the same fitted model and NLL (1000 of 1000 real cells). If only the diagnostic record is wanted, its side block alone needs no HS3 at all. Keep the records in one JSON-lines file per task run, flushed after each record (section 9).
4. Revisit C as an *input* only if an external tool must be driven by the same file; the evidence still says no.

**What I would NOT do.** (a) Make HS3 JSON the model description (as an output record it is fine, section 8): experimental, unable to express half of `fitConfig.json`, silent on bad input, and it needs custom importers for two of your five classes (one of them, Chebychev, only in 6.36; from 6.38 it is native). (b) Trust the pointer returned by `factory()`, or a `try`/`catch` around it: the pointer is non-null after some failures, values outside their range are silently clipped, and under Cling a `catch` does not catch (verified). (c) Put a new formula in `EXPR::` without compiling it once on its own: a formula that does not compile terminates the process. (d) Use `CEXPR::` (it writes a `.so` into the current directory and cannot be saved with a workspace). (e) Format numbers for factory strings with `std::format("{:.17g}")` (does not compile in this Cling). (f) Rely on `$Typedef`, `$Match` or `$REMOVE()` from the tutorials: the first two do not exist, the third did nothing in my test. (g) Build a factory workspace per fitter without a cache, or share the cache between threads without a lock (section 9: crashes and a hang). (h) Replace the parser to save code: B is +70 lines and C +160.

## 1. Result equivalence

[verified] Method: `proto/DynamicRooFitterB.h` and `proto/DynamicRooFitterC.h` are copies of the project's `DynamicRooFitter.h` in which only the model construction was replaced; `DoFit`, `ExtractYieldsAndPurity`, `CalculateIntegrationLimits` (fixed window, n sigma, snap to bin), sideband, purity and drawing are the original lines. `proto/cmp.cpp` and `proto/cmpC.cpp` run the original and the copy on every real cell of `AnalysisResults_136_2026_PtMod_23luglio.root` that has at least 1000 entries and compare with `==`: status, covQual, edm, minNll, every fitted value and error, S, its error, B, its error, purity, its error, sideband and ratio.

| section | cells | B identical | C identical |
|---|---|---|---|
| k0s (`h3K0sCandidatesMass`; 20 cells with the Chebychev2 override) | 340 | 340 | 340 |
| xi | 260 | 260 | 260 |
| pi_tpc | 360 | 360 | 360 |
| pi_tof | 340 | 340 | 340 |
| phi offline (Voigtian + BkgSourav2, `integration_range` + histogram sideband) | 70 | 70 | 70 |
| phi online (non-integer weights) | 70 | 70 | 70 |
| k0s / pi_tpc / phi with `n_sigma_integration = 3` and `snap_to_bin` forced | 340 / 360 / 70 | all | all |
| k0s with `Gaussian+Gaussian` signal; `Chebychev1*Exponential` and `Chebychev1+Exponential` background (made-up extra parameters) | 340 each | all | all |

Outputs: `proto/out-cmp-*.txt`, `out-cmpC-*.txt`, `out-nsigma-snap.txt`, `out-composites.txt`. The K0S (0,2), phi (0,2) and pi_tpc (3,10) cells also ran interpreted under `root -l -b -q` (`out-cling-b.txt`): S and its error agree to 17 digits between A and B. The fits keep their known problems (status 3 on most K0S cells, notes 03 and 07): equivalence means the replacement neither fixes nor worsens them.

## 2. Error messages and validation

[verified] `proto/defects.cpp` (output `out-defects.txt`) on K0S cell (0,2) and phi cell (0,2), the configuration modified; "A" is the project code, "B" the prototype, "A+" the hand-written class with the two checks of section 6 added (`proto/aplus.cpp`, `out-aplus.txt`):

| defect | A | B | A+ |
|---|---|---|---|
| missing parameter / `nsig` / unknown model / trailing `+` / sum without a fraction | exception, clear text | the same text | as A |
| observable starts below 2 m_K (phi) | exception (`RequireThresholdBelowRange`) | same check, shorter text | as A |
| `min > max` | runs, status 3, S = 6.289e6 against 6.137e6 | rejected | rejected |
| start value outside its limits | runs | rejected | rejected |
| Chebychev2 starting at a negative shape | runs, S = 5.31e6 against 6.14e6 | rejected (NaN at start) | rejected |
| fraction 1.5 in a sum | runs, NaN messages at every evaluation | rejected | not run (the same check applies) |
| observable name `m inv`, parameter name `c1,bkg` | runs | rejected (would break the string) | not needed: A builds no strings |
| unused parameter (typo) | ignored | ignored | ignored |

So most of the validation gain is independent of the route. What B adds is the capture of RooFit's own errors per statement. `proto/fac-errors.cpp` (output `out-fac-errors.txt`) runs faulty statements through the raw factory and through the checked wrapper: unknown argument, too few arguments, unknown class, unbalanced parenthesis, non-numeric value, min>max and a redeclared variable are rejected; `SUM::` without coefficients is caught as an exception (compiled code only); **three pass silently through `factory()`** (a value outside its range is clipped, a start value outside the range inside a `{}` list, a duplicate pdf name; the first two are closed by the value/limit comparison in `Check`); **two kill the process** (an `EXPR::` formula that does not compile, and one with an `@index` out of range; `proto/expr-abort.cpp`: the same constructor called directly throws a catchable exception). With C every silent defect of note 02 section 4e has to be closed by a validator you own (`hs3/Hs3Validate.h`, 92 lines, catches all of them except the two variants that are legitimate HS3; `out-validate.txt`); for hand-edited JSON that validator, not RooFit, is what stands between a typo and a wrong fit.

## 3. Maintainability: adding a new model

Example: a `Bernstein2` background (coefficients `c0_bkg`, `c1_bkg`, `c2_bkg`).
- **A**: a new block `else if (modelType == "Bernstein2") { RooRealVar* c0 = CreateVar("c0" + suffix); ...; pdf = Own<RooBernstein>(name.c_str(), "Bernstein2", *obs, RooArgList(*c0, *c1, *c2)); }` and `#include "RooBernstein.h"`: 7-8 lines in a 500-line class [source: the pattern of the existing blocks; not run].
- **B** [verified]: one row, `{"Bernstein2", {"Bernstein::$(name)($(x),{$(c0),$(c1),$(c2)})", false}},`. On K0S cell (3,10) with a made-up config B gave status 3, minNll -473253928.2620059252, and the same model built by hand with `RooBernstein` gave status 3, minNll -473253928.2620059252 (`proto/newmodel.cpp`, `out-newmodel.txt`). No include. Any class the factory can construct by name works, a custom compiled class too if it has a dictionary (note 01, `misc/custom/`).
- **C**: `bernstein_poly_dist` exists natively in 6.36, so you add a row to the translator and one to the validator's allowed-key table. For a class without a native HS3 type (Chebychev and Voigtian today) you write an importer/exporter pair (65 lines for the two, `hs3/hs3-pair.h`, note 02) and invent a type name outside the standard.

## 4. Dependencies, ROOT versions, Cling

- **A**: public constructors of RooFit classes; an API change shows up as a compile error. Nothing at run time.
- **B**: [verified] needs Cling at run time (`createArg` compiles a constructor call per class); the project is already interpreted or links Cling, the cost is the 1.7 ms per class. [source] The factory grammar is old and documented, but doc and source disagree in places (`$Typedef` is in a tutorial, only `$Alias` exists). [doc, note 08, paraphrased and unverified] ROOT 6.38 makes `RooRealVar::setVal` throw outside the range instead of clipping; through the factory that exception would be thrown inside Cling-built code, which terminates the process, so the start-value check matters on upgrade (it is also true for A, where the exception would be catchable); `Optimize()` is deprecated in 6.40 and removed in 6.42 (affects `DoFit` in every option equally).
- **C, other ROOT versions** [doc and source of the tags `v6-38-06`, `v6-40-04`, nothing run; note 02 section 10.4]: the 6.38 and 6.40 release notes announce no HS3 change (only bug-list entries); `hs3_version` is still "0.2" in 6.40.04; 6.38 adds the native export keys for `RooChebychev` (the registration of note 02 5.1 becomes unnecessary from 6.38; `RooVoigtian` is in none of the versions); the registration API is the same in 6.38 and gains string-key overloads in 6.40; the `mixture_dist` importer still ignores `extended` in 6.40; the 6.38 `setVal` exception probably does not reach the importer (value is set before the range, on a fresh variable) [inferred].
- **C**: [doc] "This library is still at an experimental stage"; HS3 v0.2.9 "can still undergo minor and major changes"; RooFit writes `hs3_version: "0.2"` and only checks that the field exists; its names already differ from the standard (`ARGUS_dist`, `crystalball_doublesided_dist`, `generic_function`, `errors`) and `voigtian_dist` is project-local. The standard's authoring rules (defaults must not depend on software version; an unsupported option must be rejected) are not what the 6.36 importer does with `extended` or unknown keys. No official validator or schema exists yet (the standard says so). YML is not available (verified: `exportYMLtoString` throws). The preprint (arXiv:2606.01760, June 2026; abstract only) states conversion to and from RooFit and says nothing on stability. [source] The JSON layer lives in `RooFit::Detail`.
- **Cling**: [verified] B and the C importer pair run interpreted. Two constraints from the project's own `DESIGN_NOTES.md` section 11 and `root-issue-cling-eh/issue.md` apply and were checked: `std::format` with a floating-point spec does not compile (use `snprintf`), and a `catch` in interpreted code does not catch an exception from compiled code (verified with `SUM::` without coefficients: the process terminates). So validation must not depend on catching RooFit exceptions: the prototype only generates well-formed statements and relies on the null and ERROR-text checks.

## 5. Performance and code size

[verified] `proto/bench-build.cpp` (`out-bench-build.txt`): CPU time of one thread, median of 5 repeats of 60-300 builds, other jobs on the machine (load 2-4), so read the third digit with care. Milliseconds per fitter construction:

| model | A | B uncached | B cached | C uncached | C cached |
|---|---|---|---|---|---|
| k0s DSCB+Chebychev1 | 0.0051 | 3.44 | 0.017 | 1.74 | 0.021 |
| k0s DSCB+Chebychev2 | 0.0057 | 3.69 | 0.019 | 2.04 | 0.024 |
| pi_tpc DSCB+Gaussian | 0.0059 | 4.38 | 0.017 | 4.82 | 0.023 |
| phi Voigtian+BkgSourav2 | 0.0753 | 5.42 | 0.110 | 0.65 | 0.118 |

`proto/prof-build.cpp`: each class constructed by the factory costs 1.6-1.8 ms; variables and `SUM::` cost microseconds. C is cheaper where its importers are C++ classes, which do not go through Cling (note 02, 9.3). The cache is a static map from the statement text to a validated template workspace; a `RooWorkspace` copy costs 0.012 ms and is independent of the template. For scale, one fit costs 0.5 ms (pi_tof), 1.8-2.1 ms (xi), 3.6-6.5 ms (K0S, pi_tpc) and 9-12 ms (phi) in the `cmp` runs. For about 1000 fitters, B uncached adds 3.4-5.4 s, cached 0.02 s; A 0.005 s.

Code size, non-blank non-comment lines [verified, `wc` and `grep`]: `DynamicRooFitter.h` 366; the same file patched for B or C 215 (-151); `FactoryModel.h` 221 with the model library (11 rows), the generator, the error capture and about 60 lines of validation (net +70 for B); C adds `Hs3Model.h` 154 + `hs3-pair.h` 65 + `Hs3Validate.h` 92 (net +160). The ordered-teardown destructor disappears in both (the workspace owns the objects); no crash in 2880 prototype runs (1440 cells x 2 options) plus 770 window-variant runs and 1020 composite-model runs.

## 6. The prototype of the preferred replacement (B), `proto/FactoryModel.h`

Not in the project. The patch to `DynamicRooFitter` (`proto/DynamicRooFitterB.h`): the constructor body after `RooFitMessages::Configure();` becomes
```cpp
    ws = FactoryModel::MakeWorkspace(cfg, h->GetNbinsX());
    obs = ws->var(cfg.obs.name.c_str());
    sigPdf = ws->pdf(cfg.model.sigModel.find_first_of("+*") == std::string::npos ? (cfg.model.sigModel + "_sig").c_str() : "sig");
    bkgPdf = ws->pdf(cfg.model.bkgModel.find_first_of("+*") == std::string::npos ? (cfg.model.bkgModel + "_bkg").c_str() : "bkg");
    model = ws->pdf("model");
```
The members `ownedObjects`, the destructor, `Own`, `CreateVar`, `CreatePdfComponent`, `ParseModelString`, `RequireThresholdBelowRange` and `TwoKaonThreshold` are removed and one member `std::unique_ptr<RooWorkspace> ws;` is added; `FindVar`, `DoFit`, `ExtractYieldsAndPurity`, `CalculateIntegrationLimits` and `DrawFitOnPads` are unchanged. The two validations that also work for A are `CheckConfig` and `RequireFiniteAtStart` (for A they are called with the config and with `model`/`obs`, see `proto/aplus.cpp`).

The complete `FactoryModel.h` (the `Bernstein2` row is the "new model" experiment):
```cpp
#pragma once
// Prototype of "option B": build the RooFit model of DynamicRooFitter from RooWorkspace::factory
// strings generated from the project's own FitConfig (fitConfig.json is NOT changed).
//
// What stays outside this file (unchanged project code): the signal window (fixed / n-sigma /
// snap_to_bin), yields, purity, sideband, per-bin overrides (FitConfigManager), drawing.
#include "AnalysisConstants.h"
#include "AnalysisDataStructures.h"

#include "RooAbsPdf.h"
#include "RooGlobalFunc.h"
#include "RooHelpers.h"
#include "RooRealVar.h"
#include "RooWorkspace.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <format>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace FactoryModel
{

// ---------------------------------------------------------------------------------------------
// The model library. ONE LINE PER MODEL. $(x) is the observable, $(name) the object name, $(thr)
// the K+K- threshold, every other $(key) is the parameter  key + suffix  of the FitConfig
// (suffix "_sig", "_bkg_0", ...). A key that is not in the config is reported by name.
// Adding a model = adding a line (any class that RooWorkspace::factory can construct, or EXPR::).
// ---------------------------------------------------------------------------------------------
struct Entry {
  const char* text;
  bool needsThreshold; // formula undefined below 2*m_K: the observable must start above it
};

inline const std::map<std::string, Entry>& Library()
{
  static const std::map<std::string, Entry> lib = {
    {"DSCB", {"CrystalBall::$(name)($(x),$(mean),$(sigma),$(alpha1),$(n1),$(alpha2),$(n2))", false}},
    {"Gaussian", {"Gaussian::$(name)($(x),$(mean),$(sigma))", false}},
    {"Voigtian", {"Voigtian::$(name)($(x),$(mean),$(width),$(sigma))", false}},
    {"Chebychev1", {"Chebychev::$(name)($(x),{$(c1)})", false}},
    {"Chebychev2", {"Chebychev::$(name)($(x),{$(c1),$(c2)})", false}},
    {"Chebychev3", {"Chebychev::$(name)($(x),{$(c1),$(c2),$(c3)})", false}},
    {"Exponential", {"Exponential::$(name)($(x),$(slope))", false}},
    {"Bernstein2", {"Bernstein::$(name)($(x),{$(c0),$(c1),$(c2)})", false}}, // added as the "new model" experiment
    {"BkgSourav1", {"EXPR::$(name)('1.0 + @1*@0 + @2*sqrt(@0 - $(thr))',$(x),$(c1),$(c2))", true}},
    {"BkgSourav2", {"EXPR::$(name)('1.0 + @1*@0 + @2*@0*@0 + @3*sqrt(@0 - $(thr))',$(x),$(c1),$(c2),$(c3))", true}},
    {"BkgMattia",
     {"EXPR::$(name)('pow(@0 - $(thr), @1) * exp(@2*(@0 - $(thr)) + @3*pow(@0 - $(thr), 2) + @4*pow(@0 - $(thr), 3))',"
      "$(x),$(n),$(c1),$(c2),$(c3))",
      true}},
  };
  return lib;
}

// "%.17g" through snprintf, not std::format("{:.17g}"): the project's DESIGN_NOTES section 11 says a
// floating-point format spec does not compile under Cling. 17 digits round-trip a double exactly.
inline std::string G17(double v)
{
  char b[40];
  std::snprintf(b, sizeof b, "%.17g", v);
  return b;
}

inline double TwoKaonThreshold() { return 2.0 * AnalysisConstants::GetMass("K"); }

// ---------------------------------------------------------------------------------------------
// Builder: turns a FitConfig into a list of factory statements, then executes them with every
// RooFit error captured and turned into one exception that says which statement failed.
// ---------------------------------------------------------------------------------------------
class Builder
{
 public:
  explicit Builder(const FitConfig& cfg) : cfg(cfg) {}

  // Pure string work, nothing touches RooFit: observable, signal, background, extended sum.
  // The objects are then looked up by name ("model", "sig"/"bkg" or <Type>_sig, "nsig", "nbkg").
  const std::vector<std::string>& Plan()
  {
    CheckConfig();
    Emit(std::format("{}[{},{}]", cfg.obs.name, G17(cfg.obs.min), G17(cfg.obs.max)));
    declared.insert(cfg.obs.name);
    const std::string sig = Model(cfg.model.sigModel, "sig");
    const std::string bkg = Model(cfg.model.bkgModel, "bkg");
    Emit(std::format("SUM::model({}*{},{}*{})", Par("nsig"), sig, Par("nbkg"), bkg));
    return statements;
  }
  // Executes the plan in a fresh workspace and validates the result.
  void Realize(RooWorkspace& ws)
  {
    for (const auto& st : statements)
      Exec(ws, st);
    Check(ws);
    ws.var(cfg.obs.name.c_str())->SetTitle(cfg.obs.title.c_str());
  }

 private:
  const FitConfig& cfg;
  std::set<std::string> declared;
  std::vector<std::string> statements;
  std::string sigName, bkgName;

  // value[min,max]-style declaration the first time a parameter is used, bare name afterwards.
  std::string Par(const std::string& name)
  {
    auto it = cfg.model.params.find(name);
    if (it == cfg.model.params.end())
      throw std::runtime_error(std::format("[FATAL ERROR] Missing parameter in JSON: {}", name));
    if (!declared.insert(name).second)
      return name;
    const MathParam& p = it->second;
    return p.isConstant ? std::format("{}[{}]", name, G17(p.val))
                        : std::format("{}[{},{},{}]", name, G17(p.val), G17(p.min), G17(p.max));
  }

  std::string Component(const std::string& type, const std::string& suffix)
  {
    auto it = Library().find(type);
    if (it == Library().end())
      throw std::runtime_error(std::format("[FATAL ERROR] Unknown model type: {}", type));
    if (it->second.needsThreshold && cfg.obs.min < TwoKaonThreshold())
      throw std::runtime_error(std::format(
        "[FATAL] background model '{}' is only defined above the K+K- threshold {}, but the observable starts at {}.",
        type, TwoKaonThreshold(), cfg.obs.min));
    const std::string name = type + suffix;
    std::string text = it->second.text, out;
    for (size_t i = 0; i < text.size();) {
      if (text.compare(i, 2, "$(") == 0) {
        size_t j = text.find(')', i);
        std::string key = text.substr(i + 2, j - i - 2);
        if (key == "x") out += cfg.obs.name;
        else if (key == "name") out += name;
        else if (key == "thr") out += G17(TwoKaonThreshold());
        else out += Par(key + suffix);
        i = j + 1;
      } else {
        out += text[i++];
      }
    }
    Emit(out);
    return name;
  }

  // Same grammar as ParseModelString: "A+B" sum with fractions frac_<role>_<i>, "A*B" product.
  std::string Model(std::string str, const std::string& role)
  {
    str.erase(std::remove(str.begin(), str.end(), ' '), str.end());
    auto split = [](const std::string& s, char d) {
      std::vector<std::string> o; size_t a = 0, b;
      while ((b = s.find(d, a)) != std::string::npos) { o.push_back(s.substr(a, b - a)); a = b + 1; }
      o.push_back(s.substr(a)); return o;
    };
    std::string result;
    if (str.find('+') != std::string::npos || str.find('*') != std::string::npos) {
      const bool sum = str.find('+') != std::string::npos;
      auto toks = split(str, sum ? '+' : '*');
      std::string spec;
      for (size_t i = 0; i < toks.size(); ++i) {
        const std::string n = Component(toks[i], std::format("_{}_{}", role, i));
        if (i) spec += ",";
        if (sum && i + 1 < toks.size()) spec += Par(std::format("frac_{}_{}", role, i)) + "*";
        spec += n;
      }
      Emit(std::format("{}::{}({})", sum ? "SUM" : "PROD", role, spec));
      result = role;
    } else {
      result = Component(str, "_" + role);
    }
    (role == "sig" ? sigName : bkgName) = result;
    return result;
  }

  void Emit(const std::string& stmt) { statements.push_back(stmt); }

  // One statement: every RooFit ERROR message during it is captured; a null result, an escaped
  // C++ exception (SUM:: without coefficients throws std::invalid_argument, which the factory
  // does not catch; NOT catchable under Cling) or any captured error becomes a std::runtime_error
  // with the statement. A bad EXPR:: formula aborts the process and cannot be caught at all.
  static void Exec(RooWorkspace& ws, const std::string& stmt)
  {
    RooHelpers::HijackMessageStream msgs(RooFit::ERROR, static_cast<RooFit::MsgTopic>(0x3FFFF));
    RooAbsArg* r = nullptr;
    try {
      r = ws.factory(stmt.c_str());
    } catch (const std::exception& e) {
      throw std::runtime_error(std::format("[FATAL] factory statement '{}' threw: {}", stmt, e.what()));
    }
    if (!r || !msgs.str().empty())
      throw std::runtime_error(std::format("[FATAL] factory statement '{}' failed: {}", stmt, msgs.str()));
  }

  // Cheap checks of the INPUT, before anything is built (what CreateVar did not check at all).
  void CheckConfig() const
  {
    if (cfg.obs.name.empty() || cfg.obs.name.find_first_of(" ,[](){}'\"-+*/") != std::string::npos || std::isdigit((unsigned char)cfg.obs.name[0]))
      throw std::runtime_error(std::format("[FATAL] observable name '{}' is not usable in a factory string", cfg.obs.name));
    for (const auto& [n, p] : cfg.model.params) {
      if (!p.isConstant && !(p.min < p.max))
        throw std::runtime_error(std::format("[FATAL] parameter {}: min {} is not below max {}", n, p.min, p.max));
      if (!p.isConstant && (p.val < p.min || p.val > p.max))
        throw std::runtime_error(std::format("[FATAL] parameter {}: start value {} outside [{}, {}]", n, p.val, p.min, p.max));
      if (n.find_first_of(" ,[](){}'\"") != std::string::npos || n.empty() || std::isdigit((unsigned char)n[0]))
        throw std::runtime_error(std::format("[FATAL] parameter name '{}' is not usable in a factory string", n));
    }
  }

  // Checks of the OUTPUT, after the build and before the fit: the objects are the ones asked for,
  // every parameter has exactly the value, limits and constness of the config (catches a number
  // the factory parsed differently), and the pdf is finite and positive at every bin centre.
  void Check(RooWorkspace& ws) const
  {
    RooAbsPdf* model = ws.pdf("model");
    if (!model || !model->canBeExtended())
      throw std::runtime_error("[FATAL] factory model is missing or not extendable");
    for (const auto& name : declared) {
      auto* v = ws.var(name.c_str());
      if (!v) throw std::runtime_error(std::format("[FATAL] parameter {} was not created", name));
      auto it = cfg.model.params.find(name);
      if (it == cfg.model.params.end()) continue; // the observable
      const MathParam& p = it->second;
      const bool same = v->getVal() == p.val && v->isConstant() == p.isConstant &&
                        (p.isConstant || (v->getMin() == p.min && v->getMax() == p.max));
      if (!same)
        throw std::runtime_error(std::format("[FATAL] parameter {} in the workspace differs from the config", name));
    }
  }
};

// After the data are known: the model must be finite and positive at the start values at every
// bin centre (this is what the "p.d.f value is less than zero" / NaN messages are about).
inline void RequireFiniteAtStart(RooWorkspace& ws, const std::string& obsName, int nBins)
{
  auto* x = ws.var(obsName.c_str());
  auto* model = ws.pdf("model");
  RooArgSet nset(*x);
  const double lo = x->getMin(), hi = x->getMax(), keep = x->getVal();
  for (int i = 0; i < nBins; ++i) {
    x->setVal(lo + (i + 0.5) * (hi - lo) / nBins);
    const double v = model->getVal(&nset);
    if (!std::isfinite(v) || v < 0.0) {
      const double at = x->getVal();
      x->setVal(keep);
      throw std::runtime_error(std::format("[FATAL] model is {} at {}={} with the start parameters", v, obsName, at));
    }
  }
  x->setVal(keep);
}

// ---------------------------------------------------------------------------------------------
// Entry point. The factory costs ~1.6 ms per class it constructs (Cling compiles the constructor
// call), 3.4 ms for a signal+background model, against 0.005 ms for the hand-written build. The
// statements are a pure function of the FitConfig, so the validated workspace is built ONCE per
// distinct statement list and every fitter gets a copy (RooWorkspace copy: 0.012 ms).
// ---------------------------------------------------------------------------------------------
inline std::unique_ptr<RooWorkspace> MakeWorkspace(const FitConfig& cfg, int nBins, bool useCache = true)
{
  Builder b(cfg);
  std::string key = std::to_string(nBins);
  for (const auto& st : b.Plan()) key += "\n" + st;
  static std::map<std::string, std::unique_ptr<RooWorkspace>> cache;
  auto it = cache.find(key);
  if (!useCache || it == cache.end()) {
    auto ws = std::make_unique<RooWorkspace>("ws");
    b.Realize(*ws);
    RequireFiniteAtStart(*ws, cfg.obs.name, nBins);
    if (!useCache) return ws;
    it = cache.emplace(key, std::move(ws)).first;
  }
  return std::make_unique<RooWorkspace>(*it->second);
}

} // namespace FactoryModel
```

Reproduce: `cd SC2/proto; source ../../rooenv.sh; ./cmp k0s k0s-reduced-cand-producer/k0sReducedCandidates/h3K0sCandidatesMass` (other sections as in note 01); `./defects`; `./aplus`; `./bench-build`; `./newmodel`; `root -l -b -q cling-b.C`.

## 7. Limits of this study

- The prototypes were tested on the data file named in the brief and on the configuration as it is today; `n_sigma_integration` and `snap_to_bin` are not used by the current `fitConfig.json` and were exercised only by forcing them.
- The drawing code was run on the replacement workspaces in the third pass (section 8): identical PNG and curves on 5 cells; other cells and the other drawing tasks were not run.
- The cache is process-wide static state; it was tested under two threads in the fourth pass (section 9): unsafe without a lock, safe with one lock around construction and destruction. The fitter is not used from threads today.
- ROOT 6.38 and 6.40 behaviour comes from the primary release-note files and source files of the tags, not from a run (only 6.36.10 is installed).
- "Net lines" depends on style; what is firm is the direction: neither replacement shrinks the code.

## 8. Third pass: HS3 as an output, DrawFit, Cling, ROOT 6.38/6.40 (14 October 2026)

Details, code and the real JSON of one K0S and one phi cell are in note 02, section 10. What bears on this decision:

**Provenance record (HS3 as output).** A function that takes the fitter's `model`, `obs`, `fitResult`, `config` and the input histogram, copies the model into a temporary workspace, exports it with `RooJSONFactoryWSTool`, adds the parameter errors (`err`, which the importer reads) and a side block `misc.dynamic_roofitter` with status, covQual, edm, minNll, the covariance, the configuration (model strings, window mode, start values), the windows used, the yields, purity and sideband, and the identity of the input (file, histogram path, bin indices, entries, integral, FNV-1a hash of the contents). [verified, 1000 real cells: k0s 340, xi 260, pi_tpc 360, phi 40] The record imported into a fresh workspace reproduces every value, limit, constant flag and error exactly and the extended NLL exactly (also equal to the fit's `minNll`); mean size 5.5 KB (3.5 KB without covariance, 6.7 KB with the contents embedded), `Record()` 0.125 ms per cell compiled and 0.8-0.9 ms interpreted, against 3.5 ms for a fit; the named windows `signalRegion` and `sidebandRegion` are exported as domains with no extra code; the observable range recorded is the one after `Import` (phi: [0.9945, 1.101], not the configured [0.995, 1.1]).
**Worth adding?** [inferred] Yes, as an opt-in, because no output of the project records the fit status today and the notes found status 3 in most K0S cells; it is small and cheap. Hook: a new public method `ProvenanceRecord(const Source&)` of `DynamicRooFitter`, called by `PhiFitTask.h:129` and `PurityTask.h:327` right after `ExtractYieldsAndPurity` (the tasks know file, histogram path and bin indices; the fitter would keep the last yields and window). It needs `libRooFitHS3`, exporters for `RooChebychev` (native only from 6.38) and `RooVoigtian` (the 65-line pair), and the #23221 recipe. The cheaper variant without HS3 (the side block plus a parameter table, about 2.5 KB) keeps most of the value.
**Models in a workspace, #23221.** RooGenericPdf backgrounds (BkgSourav1, BkgSourav2, BkgMattia) with the phi Voigtian export and re-import with identical NLL and identical window yield on the real phi cell. #23221 (empty export, silently) occurs only when a `createIntegral` of the TOP pdf is alive; the project integrates only `sigPdf` and `bkgPdf`, which does not trigger it. Recipe: export a temporary copy made with `RooWorkspace::import(model)` and require a non-empty `distributions`.
**Drawing and Cling.** `DrawFit` on the factory and HS3 workspaces gives byte-identical PNGs and identical curve points (5 cells, single pad and pulls), after setting the observable title for the HS3 route. The full HS3 fitter runs interpreted on 4 cells with identical results.
**Does this change the recommendation?** No for the model description: A with validation now, B when models grow, C not as input. It adds one item, the opt-in provenance record, which is independent of the choice among A, B and C and works on the hand-written class (verified: the harness drove `DynamicRooFitter` itself).

## 9. Fourth pass (15 October 2026): the workspace cache under threads, and smaller results

**The cache under two threads** [verified, `SC2/proto/threads.cpp`, output `SC2/proto/out-threads.txt`; construction and destruction only, no fit]. Two `std::thread`s each build and destroy 40 workspaces through `FactoryModel::MakeWorkspace` for four real configurations (k0s (0,2) and (0,4), pi_tpc, phi), each run in its own process, with and without `ROOT::EnableThreadSafety()`:

| mode | runs | outcome |
|---|---|---|
| cold cache (both threads fill the static map and call the factory), no lock | 3 plain + 5 with EnableThreadSafety | **8 of 8 failed**: segmentation violations (one in `RooFactoryWSTool::processExpression` called from the prototype's `Exec`), one uncaught `std::runtime_error` from the prototype's own error capture (a factory statement failed only because of the concurrency), deaths without a message |
| no cache (every construction goes through the factory), no lock | 3 + 3 | **6 of 6 failed** (segmentation violations, one death without a message) |
| warm cache (templates built in the main thread first; the threads only find them in the map and copy the workspace), no lock | 9 plain + 3 with EnableThreadSafety | **3 of 12 failed**: one SIGTRAP, one segmentation violation, one **hang** (98 % CPU for over two minutes, killed); 9 completed with all 80 workspaces |
| any of the three, with one `std::mutex` held around `MakeWorkspace` and around the destruction of the workspace | 3 + 3 each, 18 runs | **18 of 18 completed**, 80 complete workspaces each |

`ROOT::EnableThreadSafety()` changed nothing. Reading: the factory itself (Cling) cannot run in two threads at once, and even copying a `RooWorkspace` from a template that nobody modifies is not safe concurrently (copying touches shared RooFit state; [inferred], the source of the race was not located). Together with note 06 (fits of different cells in different threads crash or hang; a mutex around each whole fit works) **the safe pattern is: no RooFit in more than one thread of a process.** If threads are ever wanted, one lock must cover every RooFit construction, copy, fit and destruction, which leaves nothing to run in parallel; parallelism, if needed at all (the 960 cells take seconds serially), belongs in separate processes. The prototype's static cache therefore needs no lock as long as the project stays single-threaded, and must not be shared between threads.

**Smaller results** (details in notes 01 and 02):
- The plot of the factory-built model: axis titles, legend texts and the objects they point to, every curve point, and the PNG bytes are identical to the project's `DrawFit` on the hand-written model, for five real cells and both canvas layouts [verified, note 01 "Third pass"].
- Where to keep the provenance records of section 8: one JSON-lines file per task run, flushed per record, is the easier choice (readable without ROOT, a crash costs at most the record being written); a `TObjString` per cell in the task's ROOT output is 2.3 times smaller and survives a crash through ROOT's recovery, but needs ROOT to read [verified on 600 real cells, note 02 section 11.1]. The ModelConfig / RooSimultaneous HS3 round trip works for the model but silently re-bins combined binned data to the observable's 100 bins unless `setBins` matches the histograms; it does not affect the project, which fits each cell alone [verified, note 02 section 11.2].
- A workspace containing a custom compiled class crashes the reading process unless the class's library is loaded first, and `importClassCode` does not embed the code in 6.36 [verified, note 01 "Third pass"]: one more reason to keep the model to RooFit's own classes, which all options A, B and C do today.

**Does this change the recommendation?** No. A with validation now; B when the model list grows (single-threaded, which is how the project runs); HS3 only as an output record (JSON lines); C not as the model description.

