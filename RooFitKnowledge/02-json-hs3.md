# 02 - The native JSON / HS3 interface of RooFit (ROOT 6.36.10, ALICE build)

Status of this note: COMPLETED on 14 October 2026; a fourth pass on 15 October added section 11 (storing the provenance records: TObjString in a ROOT file against JSON lines, including a crash test; the ModelConfig / RooSimultaneous round trip). A third pass on 14 October added section 10 (HS3 as an output record of each fit, RooGenericPdf/Voigtian export and #23221, DrawFit and Cling on the replacement workspaces, ROOT 6.38/6.40, the rest of the standard). The text of the second pass follows. The first pass was paused partway; the second pass ran the Cling / PyROOT test, a compiled C++ Importer/Exporter pair, the YML test, the real phi / pi_tpc / pi_tof data (all 1440 real cells through an HS3 prototype), the unread HS3 chapters and the preprint, a JSON validator against the 16 defective files, and timings (section 9 below). What is still open is under the last NOT DONE heading. The decision document that uses this note is `10-model-parsing-recommendation.md`. Second-pass scratch: `SC2 = /Users/scannito/alice/Analysis/RooFitKnowledge/scratch/study0102` (`hs3/`, `proto/`).

Provenance tags: **[doc]** documentation or Doxygen comment, **[source]** read in the source tree, **[verified]** run here, **[inferred]** concluded, not tested.
Source tree: `S=~/alice/sw/SOURCES/ROOT/v6-36-10-alice2/v6-36-10-alice2`. Install: `~/alice/sw/osx_arm64/ROOT/v6-36-10-alice4-local1`.

## 1. Summary

- RooFit does have native JSON import/export, but it is **workspace (de)serialisation in the HS3 format**, not a "parse a model description" API. The entry point is `RooJSONFactoryWSTool` (library `RooFitHS3`, header `RooFitHS3/RooJSONFactoryWSTool.h`). It exports a `RooWorkspace` to HS3 JSON and imports an HS3 JSON into a `RooWorkspace`. [source] `roofit/hs3/inc/RooFitHS3/RooJSONFactoryWSTool.h:38-150`, [verified] below.
- Maturity: the ROOT developer doc says "This library is still at an experimental stage" [doc] `roofit/doc/developers/roofit_hs3.md:9`. The HS3 standard itself says it "can still undergo minor and major changes" and is unstable before 1.x (v0.2.9 at the time of reading) [doc] https://hep-statistics-serialization-standard.github.io/ and https://github.com/hep-statistics-serialization-standard/hep-statistics-serialization-standard. This build writes `hs3_version: "0.2"` [source] `RooJSONFactoryWSTool.cxx:110`, [verified] in exported JSON.
- **For the project's models, 6.36.10 supports natively:** `RooCrystalBall` (DSCB), `RooGaussian`, `RooExponential`, `RooGenericPdf` (all three BkgSourav1/2 and BkgMattia formulas), `RooAddPdf` (extended), `RooProdPdf`, `RooPolynomial`. [verified] round trip, NLL identical.
- **It does NOT support natively:** `RooChebychev` and `RooVoigtian`. Export of these prints a message to stderr, writes an empty stub `{"name":"..."}` into the JSON, **still returns true**, and the later import throws. [verified] Both can be added in about 10 lines of JSON via the proxy-based mechanism (5.1) or with a 65-line C++ Importer/Exporter pair (5.2, compiled and tested). [verified]
- With those two registered, DSCB+Cheb1, DSCB+Cheb2, Voigtian+Cheb2, Gauss+Exp and the generic-formula backgrounds all round-trip with **NLL difference exactly 0 and identical fitted parameters** (see section 4). [verified]
- HS3 covers: distributions, parameters (value, const, domain bounds), binned/unbinned data, likelihoods, analyses (only via ModelConfig+RooSimultaneous in RooFit). It does not cover the project's analysis-level settings (n-sigma window, snap_to_bin, purity, sideband, per-bin overrides). [inferred] from the spec and from what RooFit exports/imports; section 6.
- Importer validation is weak: several mistakes import silently with a changed meaning (section 8). Recommendation in section 7: HS3 could replace the model-description part only if the project accepts those gaps and keeps its own validation layer; it does not remove the need for project code.

## 2. What exists in this build

[verified] `ls $ROOTSYS/lib`: `libRooFitHS3.so`, `libRooFitJSONInterface.so` (plus `libRooFit`, `libRooFitCore`, `libRooFitMore`, `libRooFitCodegen`, `libRooFitXRooFit`, `libHistFactory`, `libRooStats`). Headers: `$ROOTSYS/include/RooFitHS3/{JSONIO.h,RooJSONFactoryWSTool.h}` and `$ROOTSYS/include/RooFit/Detail/JSONInterface.h`.

- **Backend:** nlohmann-json only. [verified] `JSONTree::getBackend()` returns `nlohmann-json`, `hasBackend("rapidyaml")` is 0, `hasBackend("nlohmann-json")` is 1. [source] `roofit/jsoninterface/CMakeLists.txt` compiles only `JSONInterface.cxx` and `JSONParser.cxx` (the RYML file is built only with `ROOFIT_WITH_RYML`). [doc] `roofit_hs3.md:19-26`: RYML "was supported in the past ... too high" effort to maintain. Consequence: `exportYML`/`importYML` exist in the header but `JSONNode::writeYML` throws "YML not supported" in the base class [source] `JSONInterface.h` (writeYML). [verified, second pass, `SC2/hs3/yml.cpp`, `out-yml.txt`] `exportYMLtoString()` throws `YML not supported`; `importYMLfromString("metadata: {hs3_version: '0.2'}")` does not say so: it tries to parse the text as JSON and fails with a nlohmann parse error. `hasBackend("rapidyaml")` is 0. So there is no YAML in this build.
- **Number formatting:** nlohmann writes doubles in shortest round-trip form, so values survive export/import exactly. [verified] NLL identical to the last digit. Note `JSONNode::val()` for a float uses `std::to_string` (6 digits) [source] `JSONParser.cxx:~251`; this only matters where a number is read as a string (for example a literal number used as a coefficient name). [inferred]
- **Key classes** [source]:
  - `RooJSONFactoryWSTool(RooWorkspace&)`; `importJSON(filename|istream)`, `importJSONfromString(str)`, `exportJSON(filename|ostream)`, `exportJSONtoString()`, `importJSONElement(name, jsonString)`, `importFunction(jsonString, importAllDependants)`, `createNewJSONTree()`. Static helpers `name(node)`, `isValidName`, `appendNamedChild`, `findNamedChild`, `fillSeq`, `error`, `warning`; template helpers `request<T>`, `requestArg<T>`, `requestArgList<T>`, `requestArgSet<T>`; `wsImport`, `wsEmplace<T>`.
  - `RooFit::JSONIO` (`JSONIO.h`): `Importer`, `Exporter`, `registerImporter`, `registerExporter`, `removeImporters/Exporters`, `printImporters/Exporters`, `loadFactoryExpressions`, `loadExportKeys`, `clearFactoryExpressions`, `clearExportKeys`, `printFactoryExpressions`, `printExportKeys`.
  - `RooFit::Detail::JSONNode` / `JSONTree` (`RooFit/Detail/JSONInterface.h`): abstract tree (`operator<<`, `operator[]`, `children()`, `has_child`, `val()`, `val_double()`, `is_seq()`, `set_map()`...). `JSONTree::create(istream|string)` parses; `writeJSON(ostream)` writes. `Detail` namespace signals it is not a stable public API. [source]
- **Export logic** [source] `RooJSONFactoryWSTool.cxx:1847-1937`: exports all top-level pdfs and functions (those without clients), attributes, all datasets in the workspace, ModelConfigs (only if the pdf is a `RooSimultaneous`, otherwise warns and skips, line 1737), parameter snapshots, domains. A plain `RooAddPdf` with no ModelConfig produces **no `analyses`/`likelihoods`** block. [verified] (output below has none).
- **Import logic** [source] `RooJSONFactoryWSTool.cxx:2129-2216`: requires `metadata.hs3_version` (only presence is checked, not value); reads `domains` (requires a domain named `default_domain`, `Domains.cxx:69-82`); imports variables from `parameter_points`; imports distributions/functions by name on demand; applies `parameter_points` snapshots; imports `data`; builds ModelConfigs from `analyses`+`likelihoods` if present.
- **How to call from C++** [verified], build line (env sourced, see the pitfall on `alienv` in section 8):
  ```
  clang++ -std=c++20 $(root-config --cflags | sed 's/-std=[^ ]*//') x.cpp -o x $(root-config --libs) \
      -lRooFit -lRooFitCore -lRooFitHS3 -lRooFitJSONInterface -lHistFactory -lRooStats -lRooFitMore -Wl,-rpath,$ROOTSYS/lib
  ```
  ```cpp
  #include <RooFitHS3/RooJSONFactoryWSTool.h>
  RooWorkspace ws("ws");  /* ... import pdf (and optionally data) ... */
  std::string js = RooJSONFactoryWSTool{ws}.exportJSONtoString();   // or .exportJSON("file.json")
  RooWorkspace ws2("ws2");
  RooJSONFactoryWSTool{ws2}.importJSON("file.json");                // or importJSONfromString(js)
  ```
- **How to call from a Cling macro / Python:** [doc] `RooJSONFactoryWSTool.cxx:54-86` and tutorial `tutorials/roofit/roofit/rf515_hfJSON.py` show `tool = ROOT.RooJSONFactoryWSTool(ws); tool.importJSON("x.json")`. [verified, second pass] Cling: `SC2/hs3/cling-hs3.C` (output `out-cling-hs3.txt`) runs under `root -l -b -q` in three modes on a real K0S cell: (0) stock classes, Gaussian + Exponential; (1) Chebychev registered through the proxy-key route with `std::stringstream` (`loadExportKeys`/`loadFactoryExpressions` work in Cling); (2) the C++ Importer/Exporter classes of section 5.2, defined in an interpreted header (`#include "hs3-pair.h"` after `R__ADD_INCLUDE_PATH`). In all three the export works, the import works, the imported model is a RooChebychev/RooExponential and the fit gives minNll identical to the last digit and the same status (modes: -617378171.0260620117, -617497215.7617501020, -617497215.7617501020; diff 0). PyROOT: `SC2/hs3/pyroot-hs3.py` (output `out-pyroot-hs3.txt`) works with the Python-modules python3 (3.14.7) **only after `export DYLD_LIBRARY_PATH=$ROOTSYS/lib`** (the alienv environment leaves it empty and `import ROOT` fails on `libcppyy_backend.so`); the C++ registration helper is declared with `ROOT.gInterpreter.Declare`; export, import and fit identical (diff 0).
- **Tests shipped with ROOT** [source] `roofit/hs3/test/testRooFitHS3.cxx` (round-trip tests for RooAddPdf, ArgusBG, BifurGauss, CBShape, ConstVar, Exponential, LegacyExpPoly, Gamma, Gaussian, Bernstein, GenericPdf, HistPdf, Landau, Lognormal, MultiVarGaussian, Poisson, Polynomial, PowerSum, RealIntegral, Uniform, simultaneous Gaussians, scientific notation) and `testHS3SimultaneousFit.cxx`. No test for RooChebychev, RooVoigtian or RooCrystalBall. [source] (grep of `^TEST`).

## 3. Supported types in this build (exact names)

All enumerated at runtime with `RooFit::JSONIO::print*()` [verified] (program `list-types.cpp`) and cross-checked in source [source] `roofit/hs3/src/RooFitHS3_wsexportkeys.cxx`, `RooFitHS3_wsfactoryexpressions.cxx`, `JSONFactories_RooFitCore.cxx:756-793`, `JSONFactories_HistFactory.cxx:1445-1454`.

### 3.1 Distributions / functions: HS3 `type` string -> RooFit class (JSON keys)

| HS3 type | RooFit class | JSON keys | mechanism |
|---|---|---|---|
| `gaussian_dist`, `normal_dist` (import alias) | RooGaussian | x, mean, sigma | proxy keys |
| `crystalball_dist` | RooCBShape (single-sided) | m, m0, sigma, alpha, n | proxy keys |
| `crystalball_doublesided_dist` | RooCrystalBall | m, m0, sigma_L, sigma_R, alpha_L, n_L, alpha_R, n_R | proxy keys |
| `ARGUS_dist` | RooArgusBG | mass, resonance, slope, power | proxy keys |
| `bernstein_poly_dist` | RooBernstein | x, coefficients | proxy keys |
| `bifurkated_gaussian_dist` | RooBifurGauss | x, mean, sigmaL, sigmaR | proxy keys |
| `gamma_dist` | RooGamma | x, gamma, beta, mu | proxy keys |
| `landau_dist` | RooLandau | x, mean, sigma | proxy keys |
| `power_sum_dist` | RooPowerSum | x, coefficients, exponents | proxy keys |
| `uniform_dist` | RooUniform | x | proxy keys |
| `product_dist` | RooProdPdf | factors | proxy keys (but see HistFactory streamer, below) |
| `product` | RooProduct | factors | proxy keys |
| `sum` | RooAddition | summands | proxy keys |
| `step` | ParamHistFunc | variables, parameters | proxy keys |
| `interpolation0d` | HistFactory::FlexibleInterpVar | vars, nom, low, high | factory expression + C++ streamer |
| `mixture_dist` | RooAddPdf | summands, coefficients, extended | C++ |
| `exponential_dist` | RooExponential | x, c (c = -slope, see 4.3) | C++ |
| `polynomial_dist` | RooPolynomial | x, coefficients | C++ |
| `legacy_exp_poly_dist` | RooLegacyExpPoly | x, coefficients | C++ (not an HS3 standard name) |
| `lognormal_dist` | RooLognormal | x, mu, sigma | C++ |
| `poisson_dist` | RooPoisson | x, mean, integer | C++ |
| `multivariate_normal_dist` | RooMultiVarGaussian | x, mean, covariances | C++ |
| `generic_dist` | RooGenericPdf | expression | C++ |
| `generic_function` | RooFormulaVar | expression | C++ (also exports RooTFnBinding) |
| `histogram_dist` / `histogram` | RooHistPdf / RooHistFunc | data | C++ |
| `weighted_sum_dist` / `weighted_sum` | RooRealSumPdf / RooRealSumFunc | samples, coefficients | C++ |
| `integral` | RooRealIntegral | integrand, variables, domain, normalization | C++ |
| `binsampling_dist`, `binwidth` | RooBinSamplingPdf, RooBinWidthFunction | pdf/observable/epsilon; histogram/divideByBinWidth | C++ |
| `histfactory_dist`, `interpolation` | HistFactory (RooRealSumPdf + RooProdPdf), PiecewiseInterpolation | HistFactory samples/modifiers | C++ |

Exporters registered (class -> streamer) [verified]: RooAddPdf, RooBinSamplingPdf, RooBinWidthFunction, RooLegacyExpPoly, RooExponential, RooFormulaVar, RooGenericPdf, RooHistFunc, RooHistPdf, RooLognormal, RooMultiVarGaussian, RooPoisson, RooPolynomial, RooRealSumFunc, RooRealSumPdf (HistFactory streamer first, then generic), RooTFnBinding, RooRealIntegral, FlexibleInterpVar, PiecewiseInterpolation, RooProdPdf (HistFactory streamer first). Plus the proxy-key classes of the table.

**Not present in 6.36.10 [verified]:** `RooChebychev`, `RooVoigtian`, `RooBreitWigner`, `RooSimultaneous` as a distribution (exported only as ROOT-internal info under `misc.ROOT_internal.combined_distributions`, [source] `RooJSONFactoryWSTool.cxx:1056-1075`), `RooDecay`/resolution models, `RooExtendPdf`. Anything with no entry is reported on stderr and exported as an empty node (see 8).

### 3.2 Standard vs RooFit naming (differences found)
[doc] HS3 standard, fundamental distributions page https://hep-statistics-serialization-standard.github.io/chapters/2.1.1_fundamental_distributions/ lists: `argus_dist`, `chebychev_dist` (x, coefficients), `crystalball_dist` (one type covering both tails with optional sigma_L/sigma_R/alpha_L/alpha_R/n_L/n_R), `exponential_dist`, `gaussian_dist`/`normal_dist`, `landau_dist`, `lognormal_dist`, `poisson_dist`, `polynomial_dist`, `uniform_dist`, `generalized_normal_dist`, `decay_dist`, `relativistic_breit_wigner_dist`, `multivariate_normal_dist`, `generic_dist`, `histfactory_dist`, `barlow_beeston_lite_poisson_constraint_dist`. Composite: `mixture_dist`, `mixture_model`, `product_dist`, `rate_extended_dist`, etc. Functions: `product`, `sum`, `polynomial`, `generic` (with `generic_function` as the legacy spelling), `integral`, `derivative`, ...
Mismatches with this RooFit build [inferred from comparing the two lists]:
- RooFit writes `ARGUS_dist`; the standard says `argus_dist`.
- RooFit writes the double-sided Crystal Ball as `crystalball_doublesided_dist`; the current standard has it inside `crystalball_dist` (and RooFit's `crystalball_dist` is the single-sided RooCBShape with different key names). A JSON written by RooFit for the DSCB is therefore not standard-conformant, and a standard-conformant double-sided `crystalball_dist` is not readable by this build. 
- `chebychev_dist` is in the standard but not implemented in 6.36.10. The name used in section 5 for the custom registration follows the standard (`x`, `coefficients`).
- `voigtian_dist` used in section 5 is NOT in the standard; it is a project-local name (the standard has `relativistic_breit_wigner_dist`, a different function).
- RooFit uses a `generic_function` for `RooFormulaVar` and `generic_dist` for `RooGenericPdf`; the standard page lists `generic` for functions. [doc]

### 3.3 Data, domains, likelihoods, analyses
- Data [source] `RooJSONFactoryWSTool.cxx:517-577,1490-1573`: `type: "binned"` (axes with min/max/nbins or edges; `contents`; optional `errors`), `type: "unbinned"` (`axes`, `entries`, optional `weights`). `RooDataHist` exports as binned. Combined datasets with a category are split per channel with ROOT-internal bookkeeping. Note the standard names the binned errors `uncertainty` (with `sigma`), RooFit uses `errors`. [doc] https://hep-statistics-serialization-standard.github.io/chapters/2.3_data/ vs [source] `readBinnedData`.
- Domains: only `type: "product_domain"` is supported, with `axes` of `name`/`min`/`max`; the domain named `default_domain` is mandatory on import and carries the variable bounds. Extra domains register named ranges on variables. [source] `Domains.cxx:69-82,126-143`.
- Parameter points: `parameter_points[{name:"default_values", parameters:[{name,value,const?,nbins?,err?,relErr?}]}]`; other named snapshots are saved as workspace snapshots. [source] `RooJSONFactoryWSTool.cxx:226-245,2160-2174`. Constants appear as `"const": true`.
- Likelihoods/analyses: imported only when present (`importAnalysis`, lines 594-721): builds a `RooStats::ModelConfig` from `analyses[].likelihood`, `parameters_of_interest`, `domains`, and `likelihoods[].distributions/data/aux_distributions`. Exported only for a `RooSimultaneous`. [source]
- Misc: RooFit stores private information under `misc.ROOT_internal` (attributes/tags such as `Constant` or `roofit_skip`, ModelConfigs, combined data). [source], [verified] in exported JSON.

## 4. Experiments (all run; scratch dir `SC=/Users/scannito/alice/Analysis/RooFitKnowledge/scratch/json-hs3`)

Common setup [verified]: real K0S cell from `AnalysisResults_136_2026_PtMod_23luglio.root` (read-only), `k0s-reduced-cand-producer/k0sReducedCandidates/h3K0sCandidatesMass`: axes centFT0M 10 bins [0,100], pT 34 bins [0,10], mass 200 bins [0.45,0.55]. Cell used: `ProjectionZ(.., 6,6, 26,26)` i.e. mult bin 5, pT bin 25 (4.22e6 entries). Models built with the same RooFit calls and parameter ranges/initial values as `DynamicRooFitter.h`/`fitConfig.json` (k0s default). Fit: `fitTo(dh, Optimize(1), Extended(1), Save(1), PrintLevel(-1))`. Procedure (`exp-rt.cpp`): copy the workspace before fitting, export the unfitted copy (so initial values are exported), fit the original, import the JSON in a fresh workspace, compare NLL at the initial values and after the fit.

### 4a. DSCB (RooCrystalBall) + RooChebychev, extended RooAddPdf
Without any registration (stock 6.36.10) [verified]:
```
unable to export class 'RooChebychev' - no export keys available!
 ... (long advice text) ...
IMPORT FAILED: RooJSONFactoryWSTool() no type given for function 'Chebychev1_bkg', skipping.
```
Export "succeeds" (returns true, JSON contains `{"name":"Chebychev1_bkg"}` without type); the exception comes at import.

With the project-local Chebychev registration (section 5) [verified], exported JSON (data contents truncated; keys are sorted alphabetically by nlohmann; the observable limits show float noise, see 8):
```json
{"data":[{"axes":[{"max":0.550000011920929,"min":0.44999998807907104,"name":"mass","nbins":200}],
          "contents":[3687,3765,3835,"... 200 values"],"name":"dataHist","type":"binned"}],
 "distributions":[
  {"coefficients":["nsig","nbkg"],"extended":true,"name":"model",
   "summands":["DSCB_sig","Chebychev1_bkg"],"type":"mixture_dist"},
  {"coefficients":["c1_bkg"],"name":"Chebychev1_bkg","type":"chebychev_dist","x":"mass"},
  {"alpha_L":"alpha1_sig","alpha_R":"alpha2_sig","m":"mass","m0":"mean_sig","n_L":"n1_sig","n_R":"n2_sig",
   "name":"DSCB_sig","sigma_L":"sigma_sig","sigma_R":"sigma_sig","type":"crystalball_doublesided_dist"}],
 "domains":[{"axes":[{"max":5.0,"min":0.5,"name":"alpha1_sig"}, {"max":5.0,"min":0.5,"name":"alpha2_sig"},
   {"max":1.0,"min":-1.0,"name":"c1_bkg"}, {"max":0.55,"min":0.45,"name":"mass"},
   {"max":0.505,"min":0.49,"name":"mean_sig"}, {"max":150.0,"min":1.0,"name":"n1_sig"},
   {"max":150.0,"min":1.0,"name":"n2_sig"}, {"max":25000000.0,"min":0.0,"name":"nbkg"},
   {"max":100000000.0,"min":0.0,"name":"nsig"}, {"max":0.008,"min":0.002,"name":"sigma_sig"}],
   "name":"default_domain","type":"product_domain"}],
 "metadata":{"hs3_version":"0.2","packages":[{"name":"ROOT","version":"6.36.10"}]},
 "parameter_points":[{"name":"default_values","parameters":[
   {"name":"mass","value":0.5},{"name":"c1_bkg","value":-0.8},{"name":"mean_sig","value":0.497},
   {"name":"sigma_sig","value":0.004},{"name":"alpha1_sig","value":1.2},{"name":"n1_sig","value":5.0},
   {"name":"alpha2_sig","value":1.2},{"name":"n2_sig","value":5.0},{"name":"nbkg","value":50000.0},{"name":"nsig","value":100000.0}]}]}
```
(Domain bounds were shortened to 0.55/0.45 above for readability; the real file has the float-noise values.) The symmetric `sigma` of the project's DSCB shows up as `sigma_L` = `sigma_R` = the same variable. Full file: `$SC/rt-dscb-cheb1.json`.

Result [verified]: NLL at initial values original = imported = -49530032.5304 (diff 0); after fit both `minNll = -56966878.94`, all nine parameters identical (max relative difference 0). Both fits report `status=3 covQual=2` (so the HS3 route does not change the fit behaviour; the status is a property of this cell/model, not of the JSON).

### 4b. Other models (same cell, same procedure) [verified]
| model | native in 6.36.10? | NLL initial diff | minNll diff after fit | param diff |
|---|---|---|---|---|
| DSCB + Chebychev2 | DSCB yes, Chebychev no (needs registration) | 0 | 0 (status 0) | 0 |
| Voigtian + Chebychev2 (width_sig constant) | neither (needs registration) | 0 | 0 (status 0) | 0 |
| Gaussian + Exponential | **yes, fully native** | 0 | 0 (status 0) | 0 |
Without registration Voigtian+Cheb2 fails at import (`no type given for function 'Voigtian_sig'`), Gauss+Exp round-trips with stock RooFit.

Gauss+Exp exported JSON (key parts) [verified]:
```json
{"c":"slope_bkg_exponential_inverted","name":"Exponential_bkg","type":"exponential_dist","x":"mass"}
 "functions":[{"expression":"-slope_bkg","name":"slope_bkg_exponential_inverted","type":"generic_function"}]
 "misc":{"ROOT_internal":{"attributes":{"slope_bkg_exponential_inverted":{"tags":["roofit_skip"]}}}}
```
The HS3 exponential is exp(-c x), RooFit's is exp(slope x); RooFit therefore writes a helper function `-slope_bkg` and tags it `roofit_skip`, so on re-import it is skipped and the original `slope_bkg` is used. [source] `JSONFactories_RooFitCore.cxx:289-336`. A non-RooFit reader sees `c = -slope_bkg`.

Voigtian JSON (after registration) [verified]: `{"mean":"mean_sig","name":"Voigtian_sig","sigma":"sigma_sig","type":"voigtian_dist","width":"width_sig","x":"mass"}`; a constant parameter is written as `{"const":true,"name":"width_sig","value":0.0005}` in `parameter_points` and as a `Constant` tag in `misc.ROOT_internal.attributes`, and has no entry in `domains` (infinite range).

### 4c. RooGenericPdf (the BkgSourav1/2 and BkgMattia formulas) [verified]
Synthetic phi-like histogram (200 bins in [0.995,1.1], 1.2e6 events generated from Voigtian + background; the real phi data were not used), `exp-generic.cpp`. RooGenericPdf with `sqrt`, `pow`, `exp` exports as `generic_dist` with the TFormula index notation `x[i]`/`@i` replaced by variable names, and imports back:
```json
{"expression":"1.0 + c1_bkg*mass + c2_bkg*sqrt(mass - 0.987354)","name":"BkgSourav1_bkg","type":"generic_dist"}
{"expression":"1.0 + c1_bkg*mass + c2_bkg*mass*mass + c3_bkg*sqrt(mass - 0.987354)","name":"BkgSourav2_bkg","type":"generic_dist"}
{"expression":"pow(mass - 0.987354, n_bkg) * exp(c1_bkg*(mass - 0.987354) + c2_bkg*pow(mass - 0.987354, 2) + c3_bkg*pow(mass - 0.987354, 3))","name":"BkgMattia_bkg","type":"generic_dist"}
```
Results: minNll original = imported for all three (-18329724.64, -18327780.61, -18351683.56), diff 0. The imported variables are found by name scanning of the expression (`extractArguments`, [source] `JSONFactories_RooFitCore.cxx:70-103`): identifiers not followed by `(`, scientific notation guarded. The functions used (`sqrt`, `pow`, `exp`, `+ - * /`) are all in the HS3 generic-expression list (adds `^`, `log`, `abs`, `min`, `max`, trig, `erf`, `ifelse`) [doc] https://hep-statistics-serialization-standard.github.io/chapters/3.1_generic_expressions/. Note: the project's `2*m_K` threshold is baked into the expression as a literal number (not a named parameter), and the `RequireThresholdBelowRange` check would remain project code. [inferred]

### 4d. Hand-written model-only JSON [verified]
`$SC/good-model.json` (about 40 lines: `metadata.hs3_version`, `distributions`, `domains` with `default_domain`, `parameter_points.default_values`; no data block) imports with `RooJSONFactoryWSTool::importJSON`. Then `ws.var("mass")` is the observable; a `RooDataHist` from the K0S TH1 is built on it, imported, and `ws.pdf("model")->fitTo(...)` gives `minNll = -56966878.94`, identical to the hand-built model (`delta = 0`). Program: `exp-handjson.cpp`.

### 4e. Failure modes of the importer [verified] (`exp-errors.cpp`, files in `$SC/variants/`)
| # | defect in JSON | result |
|---|---|---|
| v01 | no `metadata.hs3_version` | exception with a clear message and an example |
| v02 | no `default_domain` | exception `"domains" do not contain "default_domain"` |
| v03 | unknown `type` | exception with a long list of possible reasons |
| v04 | missing key (`sigma_R`) | exception `node 'DSCB_sig' is missing key 'sigma_R'` |
| v05 | misspelt key (`mo` for `m0`) | exception `... is missing key 'm0'` |
| v06 | invalid name `mean-sig` | exception `name 'mean-sig' is not valid!` |
| v07 | variable used by a pdf but absent from `parameter_points` | **imports silently**, variable created from its domain with value 0 (c1_bkg=0, range [-1,1]) |
| v08 | extra unknown key (`foo`) on a pdf | **silently ignored** |
| v09 | value outside its domain (c1_bkg=5, domain [-1,1]) | **silently clamped** to 1 |
| v10 | no domain entry for a parameter | **silently** infinite range [-inf, inf] |
| v11 | `mixture_dist` with 2 summands but 1 coefficient | **imports as a non-extended fraction model** (`canBeExtended=0`), `extended` flag not honoured [source] `RooAddPdfFactory` ignores `extended` |
| v13 | `extended:false`, 1 coefficient | same non-extended RooAddPdf |
| v16 | `extended:false`, 2 coefficients | still extended (`canBeExtended=1`), the flag is ignored |
| v14 | domain min > max | only RooFit warnings; variable collapses to range [0.51,0.51], import "OK" |
| v15 | JSON syntax error (trailing comma) | nlohmann exception with line and column |
| v12 | `"const": true` | works, `isConstant()==1` |
Also: RooFit INFO messages `importing RooRealVar::...` are printed for every import unless the message level is raised (the project's `RooFitMessages::Configure()` would handle that).

## 5. Custom importers/exporters for classes HS3 lacks

### 5.1 Proxy-based, no C++ class (VERIFIED, used for Chebychev and Voigtian above)
[doc] `roofit/doc/developers/roofit_hs3.md:56-118`. Export keys map a class's proxy names to JSON keys (class must have only proxies as relevant members); factory expressions map a type name to a constructor call through `RooWorkspace::factory`. Load from an istream or a file at program start:
```cpp
// hs3-extra.h (file: $SC/hs3-extra.h)
#include <RooFitHS3/JSONIO.h>
#include <sstream>
inline void registerProjectHS3Extras() {
  static bool done = false; if (done) return; done = true;
  { std::stringstream keys; keys << R"({
      "RooChebychev": {"type": "chebychev_dist", "proxies": {"x": "x", "coefList": "coefficients"}},
      "RooVoigtian":  {"type": "voigtian_dist",  "proxies": {"x": "x", "mean": "mean", "width": "width", "sigma": "sigma"}} })";
    RooFit::JSONIO::loadExportKeys(keys); }
  { std::stringstream expr; expr << R"({
      "chebychev_dist": {"class": "RooChebychev", "arguments": ["x", "coefficients"]},
      "voigtian_dist":  {"class": "RooVoigtian",  "arguments": ["x", "mean", "width", "sigma"]} })";
    RooFit::JSONIO::loadFactoryExpressions(expr); }
}
```
How it was derived: the proxy names are `x`, `coefList` for RooChebychev and `x`, `mean`, `width`, `sigma` for RooVoigtian [source] `roofit/roofit/inc/RooChebychev.h:46-47`, `RooVoigtian.h:42-45`; the factory argument order must equal the constructor order [source] `RooJSONFactoryWSTool.cxx:343-375`. Limits: non-proxy members are lost; RooVoigtian's `_doFast` flag (default false) and RooChebychev's `_refRangeName` are not exported. [source]/[inferred] A key in `loadExportKeys` whose class is unknown to ROOT is skipped with a message on stderr. [source] `JSONIO.cxx:228-234`.

### 5.2 C++ importer/exporter classes (compiled and tested in the second pass)

[doc] `roofit_hs3.md:120-206`, pattern as in [source] `JSONFactories_RooFitCore.cxx:124-133,420-432`. **The doc snippet is out of date for 6.36**: it shows `registerImporter("<key>", new MyClassFactory(), true)` but the real signatures take `std::unique_ptr<const Importer>` or are templates:
```cpp
// [source] JSONIO.h:85-97
template<class T> bool registerImporter(const std::string &key, bool topPriority = true);   // std::make_unique<T>()
template<class T> bool registerExporter(const TClass *key, bool topPriority = true);
bool registerImporter(const std::string &key, std::unique_ptr<const Importer> f, bool topPriority = true);
bool registerExporter(const TClass *key, std::unique_ptr<const Exporter> f, bool topPriority = true);
```
Rules [source]: C++ exporters/importers registered for a key take precedence over factory expressions/export keys (`importFunction` lines 1241-1283, `exportObject` lines 1093-1118); several importers per key are tried in order until one returns true; `Exporter::autoExportDependants()` is true by default.

**A working pair for RooChebychev and RooVoigtian** [verified: compiled with `clang++ -std=c++20`, `-lRooFitHS3 -lRooFitJSONInterface`; file `SC2/hs3/hs3-pair.h`, test `test-pair.cpp`, output `out-test-pair.txt`; also interpreted in Cling, see section 2]. The complete file:
```cpp
#pragma once
// C++ Importer/Exporter pairs for the two classes RooFitHS3 (ROOT 6.36.10) cannot (de)serialise:
// RooChebychev ("chebychev_dist", the name the HS3 standard uses) and RooVoigtian ("voigtian_dist",
// project-local: HS3 has no Voigt profile). Register once at program start with RegisterHs3Extras().
#include <RooFitHS3/JSONIO.h>
#include <RooFitHS3/RooJSONFactoryWSTool.h>
#include <RooChebychev.h>
#include <RooVoigtian.h>
#include <RooRealProxy.h>

namespace Hs3Extras {
using RooFit::Detail::JSONNode;

// ----------------------------------------------------------------- RooChebychev
struct ChebychevImporter : RooFit::JSONIO::Importer {
  bool importArg(RooJSONFactoryWSTool* tool, const JSONNode& p) const override
  {
    std::string name(RooJSONFactoryWSTool::name(p));
    if (!p.has_child("coefficients"))
      RooJSONFactoryWSTool::error("no coefficients given in '" + name + "'");
    RooAbsReal* x = tool->requestArg<RooAbsReal>(p, "x");
    RooArgList coefs;
    for (const auto& c : p["coefficients"].children())
      coefs.add(*tool->request<RooAbsReal>(c.val(), name));
    tool->wsEmplace<RooChebychev>(name, *x, coefs);
    return true;
  }
};
struct ChebychevExporter : RooFit::JSONIO::Exporter {
  std::string const& key() const override { static const std::string k = "chebychev_dist"; return k; }
  bool exportObject(RooJSONFactoryWSTool*, const RooAbsArg* f, JSONNode& elem) const override
  {
    auto* pdf = static_cast<const RooChebychev*>(f);
    elem["type"] << key();
    elem["x"] << pdf->x().GetName();
    auto& coefs = elem["coefficients"].set_seq();
    for (const auto* c : pdf->coefList()) coefs.append_child() << c->GetName();
    return true;
  }
};

// ----------------------------------------------------------------- RooVoigtian
// RooVoigtian keeps x, mean, width, sigma in protected proxies with no accessors, so the exporter
// reads them through RooAbsArg::getProxy() (registration order of the constructor). The "fast"
// algorithm flag is private and is NOT exported: the default (slow, exact) one is what both
// DynamicRooFitter and this importer use.
struct VoigtianImporter : RooFit::JSONIO::Importer {
  bool importArg(RooJSONFactoryWSTool* tool, const JSONNode& p) const override
  {
    std::string name(RooJSONFactoryWSTool::name(p));
    tool->wsEmplace<RooVoigtian>(name, *tool->requestArg<RooAbsReal>(p, "x"), *tool->requestArg<RooAbsReal>(p, "mean"),
                                 *tool->requestArg<RooAbsReal>(p, "width"), *tool->requestArg<RooAbsReal>(p, "sigma"));
    return true;
  }
};
struct VoigtianExporter : RooFit::JSONIO::Exporter {
  std::string const& key() const override { static const std::string k = "voigtian_dist"; return k; }
  bool exportObject(RooJSONFactoryWSTool*, const RooAbsArg* f, JSONNode& elem) const override
  {
    elem["type"] << key();
    for (int i = 0; i < f->numProxies(); ++i)
      if (auto* rp = dynamic_cast<RooRealProxy*>(f->getProxy(i)))
        elem[rp->name()] << rp->arg().GetName();   // proxy names are x, mean, width, sigma
    return true;
  }
};

inline void RegisterHs3Extras()
{
  static bool done = false;
  if (done) return;
  done = true;
  RooFit::JSONIO::registerImporter<ChebychevImporter>("chebychev_dist", true);
  RooFit::JSONIO::registerExporter<ChebychevExporter>(RooChebychev::Class(), true);
  RooFit::JSONIO::registerImporter<VoigtianImporter>("voigtian_dist", true);
  RooFit::JSONIO::registerExporter<VoigtianExporter>(RooVoigtian::Class(), true);
}
} // namespace Hs3Extras
```
Test (`test-pair.cpp`): a factory-built workspace is exported, the JSON imported into a fresh workspace, both fitted on real K0S cell (3,4) [mult 3, source pT bin 4] with `Optimize(1), Extended(1)`:
| model | import | minNll original | minNll imported | difference | status | max abs diff of values + errors |
|---|---|---|---|---|---|---|
| DSCB + Chebychev2 | ok | -617912966.2198921442 | -617912966.2198921442 | 0 | 3 / 3 | 0 |
| Voigtian (width constant) + Chebychev3 | ok | -617747099.7739195824 | -617747099.7739195824 | 0 | 0 / 0 | 0 |
The Chebychev3 case also settles the "Chebychev3 inferred" item of the first pass. Exported nodes: `{"coefficients":["c1_bkg","c2_bkg","c3_bkg"],"name":"bkg","type":"chebychev_dist","x":"mass"}` and `{"mean":"mean_sig","name":"sig","sigma":"sigma_sig","type":"voigtian_dist","width":"width_sig","x":"mass"}`. Differences from the proxy-key route of 5.1: ~65 lines of C++ instead of 10 lines of JSON; the exporter can take the data from public accessors (`RooChebychev::x()`, `coefList()`) and from `RooAbsArg::getProxy(i)` for the Voigtian, whose members are protected without accessors (proxy names `x, mean, width, sigma`); neither route exports `RooVoigtian::_doFast` (private, default false) or `RooChebychev::_refRangeName`. The proxy-key route produced the identical JSON and results in all tests, so the C++ pair is only worth it where the importer needs logic (checks, defaults). One thing to remember when you register: the `true` (topPriority) argument makes the importer be tried before any other for that key.

## 6. Mapping of the project's `fitConfig.json` onto HS3

Project config recap [read] `JSONConfigs/fitConfig.json`, `Codes/DynamicRooFitter.h`: per particle `observable{name,title,min,max}`, `fit_setup.default` and `fit_setup.bins.<multN_ptM>` overrides; each has `sig_model`, `bkg_model`, `params` (scalar = constant, `[val,min,max]` = free), `integration_range` or `n_sigma_integration`, `snap_to_bin`, `calculate_purity`, `sideband`.

| project element | HS3/RooFit representation in 6.36.10 | status |
|---|---|---|
| `observable` name, min, max | variable + `domains[default_domain].axes[{name,min,max}]` | represented [verified]; `title` not represented (RooFit title lost on re-import: names only) [inferred] |
| `sig_model`/`bkg_model` DSCB, Gaussian, Exponential | `crystalball_doublesided_dist`, `gaussian_dist`, `exponential_dist` | native [verified] (non-standard name for DSCB, sign flip for exponential) |
| Chebychev1/2/3 | `chebychev_dist` | **needs registration** (section 5.1) [verified for 1 and 2; 3 inferred] |
| Voigtian | no standard name | **needs registration**, project-local type [verified] |
| BkgSourav1/2, BkgMattia | `generic_dist` with formula | native [verified]; threshold literal baked in |
| `A+B` sum of components in a model string, `A*B` | `mixture_dist` (non-extended fractions) / `product_dist` | native [verified, second pass: on all 340 K0S cells, with made-up extra parameters in the config (`EXTRA` env of `cmp`), signal `Gaussian+Gaussian`, background `Chebychev1*Exponential` and `Chebychev1+Exponential` went through the HS3 prototype and the factory prototype and were identical to `DynamicRooFitter` in every field; `proto/out-composites.txt`] |
| total model RooAddPdf(sig,bkg;nsig,nbkg) | `mixture_dist` with `extended:true` and coefficients `[nsig,nbkg]` | native [verified]; `extended` flag ignored on import (section 4e) |
| `params` value, min, max | `parameter_points.default_values` value + `domains` min/max | represented [verified] |
| scalar param = constant | `"const": true` in `parameter_points` | represented [verified] |
| `integration_range` (fixed window) | no HS3 construct; a `domains` entry named like a range would register a named RooFit range (`setRange(name,...)`, [source] `Domains::registerBinnings`) | partially, by convention only [inferred] |
| `n_sigma_integration` | none | **not represented** (needs `mean_sig`/`sigma_sig` lookups, project code) |
| `snap_to_bin`, `calculate_purity`, `sideband` (range, from_fit) | none | **not represented**; could live in `misc` (free-form, [doc] https://hep-statistics-serialization-standard.github.io/chapters/2.9_misc/) but RooFit would ignore it |
| per-bin overrides (`bins.mult0_pt4`) | none (HS3 is one model per file, no inheritance/override) | **not represented**; one JSON per cell or project-side merging |
| data (TH1 cell) | `data` binned | representable [verified] but one would not store 680 histograms in the model file; the model-only JSON plus a data attach step works [verified, 4d] |
| likelihood / analysis | `likelihoods`, `analyses` | only produced from a `RooSimultaneous` ModelConfig in RooFit; not needed for a single-histogram fit [source] |

## 7. Recommendation (no advocacy)

Could HS3 replace the model-description part of `DynamicRooFitter` (ParseModelString/CreatePdfComponent/CreateVar)? **Technically yes for the pdf structure and the parameters**, with these conditions: [verified] items are experiments above, the rest [inferred].
- Needed project code in any case: registration of `chebychev_dist` and `voigtian_dist` at start-up (10 lines); the whole analysis layer (signal window incl. n-sigma, snap_to_bin, purity, sideband, yield/error propagation, drawing); per-bin override logic (merge of default and bin settings before producing a JSON, or generating the JSON per cell); the `RequireThresholdBelowRange` sanity check; validation (see below); handling of the `nsig`/`nbkg` and `mean_sig`/`sigma_sig` conventions (`FindVar` by name keeps working because HS3 import preserves variable names, [verified] names unchanged).
- Possible gains: a documented, tool-independent interchange format; RooFit's own import resolves dependencies by name; models with `generic_dist` already round-trip; HS3 files could be read by pyhf/zfit-type tools only for the standard types (not DSCB under the RooFit name, not Voigtian).
- Costs/risks:
  1. Schema churn: the standard is pre-1.0 (v0.2.9) and RooFit writes "0.2"; names already diverge from the current standard (`ARGUS_dist`, `crystalball_doublesided_dist`, `generic_function`, `errors`). Upgrading ROOT could change the file format; importer only checks that `hs3_version` exists. [doc]/[source]
  2. Validation is weaker than the current hand-written `CreateVar` ("Missing parameter in JSON"): silent clamping, silent zero default for a parameter missing in `parameter_points`, silent infinite range, ignored `extended`, ignored unknown keys, min>max accepted (section 4e). A project validator (or a schema check) would still be required. Error messages for missing keys and unknown types are good.
  3. Export of unsupported classes does not fail: it writes an empty node and returns true; the failure appears only at import. [verified]
  4. Documentation of the API is outdated in places (registerImporter signature) [doc vs source].
  5. Verbosity: three required blocks (`domains`, `parameter_points`, `distributions`) versus the current compact `[val,min,max]` arrays; a human-edited per-cell config would be longer, so a generator or a thin translation from the current compact format would probably be kept. [inferred]
  6. The observable limits can pick up float noise (0.550000011920929) when a `RooDataHist` is imported into the workspace before export (the data import rewrites the observable binning), so export the model workspace before attaching data. [verified] (seen in the JSON); the cause is [inferred].
- Honest bottom line: HS3 can serve as the container for the pdf graph and parameters, and the round trips here are exact. It cannot represent the analysis settings, so the project would end up with two files per fit (HS3 model + analysis settings) or HS3 plus `misc`, plus a validator. Whether that is better than the current ~120-line factory is a maintenance judgment, not a technical necessity. [inferred]

## 8. Pitfalls (all observed unless stated)

- **`alienv printenv` is not safe to run concurrently** (several processes at once): it failed repeatedly with `rm: .../MODULES/osx_arm64: Directory not empty` and `Unable to locate a modulefile`, leaving the environment empty. Workaround used: run it once when quiet, save its output to `$SC/env.sh`, and `source` that. Building with `root-config` from an incomplete environment produced a crash in `TFile::Open` ("no interpreter information for class THnBase", segmentation violation) and a manual minimal `ROOTSYS/PATH` environment failed on `Vc.pcm`. With the full `env.sh`, standalone programs run but still print many `Error in <TClass::LoadClassInfo>: no interpreter information for class THnBase/THnSparse` lines (filtered with `grep -v LoadClassInfo` in these experiments); the cause was not investigated.
- Unsupported class on export: stderr text only, empty node in JSON, `exportJSON` returns true (section 4a).
- Silent import defects listed in 4e; `extended` ignored on import.
- Names must match `[A-Za-z_][A-Za-z0-9_]*` (ASCII). Invalid names make import throw; export only warns when `allowExportInvalidNames` is true (default) [source] `RooJSONFactoryWSTool.cxx:871-904`.
- `RooJSONFactoryWSTool::error` logs to RooMsgService and throws `std::runtime_error`; `DependencyMissingError` is a plain `std::exception` subclass thrown for an undefined reference. [source]
- The exported JSON key order is alphabetical (nlohmann std::map), not the definition order; a diff against a hand-written file needs normalisation.
- Round-trip fit statuses (`status=3` for DSCB+Cheb1 on this cell) are identical before and after; do not attribute them to HS3.
- `RooJSONFactoryWSTool` lives in a `Detail`-namespaced JSON layer (`RooFit::Detail::JSONNode`), signalling an unstable internal API. [source]
- Importing needs `default_domain` even for a model whose parameters you do not want to bound. [source]/[verified v02]

## 9. Second pass: real data, a full HS3 prototype, validator, timing, standard

### 9.1 All real cells through HS3 (option C prototype)
[verified] `SC2/hs3/Hs3Model.h` translates the project's `FitConfig` into an HS3 JSON text (one table row per model; `mixture_dist` for sums and for the extended top level, `product_dist` for products, `generic_dist` for BkgSourav1/2 and BkgMattia with the threshold as a 17-digit literal, `exponential_dist` with `c = slope_bkg_exponential_inverted` plus a helper `generic_function` `-slope_bkg`, `default_domain` axes for the observable and every free parameter, `parameter_points` with `"const": true` for scalars). `import` uses `RooJSONFactoryWSTool::importJSONfromString` with the C++ pair of 5.2 registered, under a `HijackMessageStream` at WARNING level that rejects any message. `DynamicRooFitterC.h` is the same patched copy of `DynamicRooFitter.h` as for the factory prototype (note 01), only the workspace comes from HS3. `proto/cmpC.cpp` compares it field by field (`==`) with the project's `DynamicRooFitter` on every real cell (outputs `proto/out-cmpC-*.txt`):

| section | cells | identical | different |
|---|---|---|---|
| k0s (DSCB+Chebychev1, 20 cells Chebychev2) | 340 | 340 | 0 |
| xi | 260 | 260 | 0 |
| pi_tpc (DSCB+Gaussian) | 360 | 360 | 0 |
| pi_tof (Gaussian+Exponential, i.e. the `exponential_dist` sign convention) | 340 | 340 | 0 |
| phi offline (Voigtian+BkgSourav2, real data) | 70 | 70 | 0 |
| phi online (non-integer weights) | 70 | 70 | 0 |
| k0s, pi_tpc, phi with `n_sigma_integration=3` and `snap_to_bin` forced | 340 / 360 / 70 | all | 0 |
| k0s with a composite model: signal `Gaussian+Gaussian`; background `Chebychev1*Exponential`; background `Chebychev1+Exponential` (extra parameters made up) | 340 each | all | 0 |

So the "real phi data and Voigtian+BkgSourav case" and the "real n-sigma (pion) histograms" of the first pass's NOT DONE list are done, and HS3 reproduces the project to the last bit, including the observable name `nsigma` and the 105-bin phi histogram whose limits are not bin edges. Generated JSON for phi cell (0,2) (`proto/../hs3/out-json-phi-0-2.txt`, 1286 bytes): distributions `{"name":"Voigtian_sig","type":"voigtian_dist","x":"mass","mean":"mean_sig","width":"width_sig","sigma":"sigma_sig"}`, `{"name":"BkgSourav2_bkg","type":"generic_dist","expression":"1.0 + c1_bkg*mass + c2_bkg*mass*mass + c3_bkg*sqrt(mass - 0.98735399999999995)"}`, `{"name":"model","type":"mixture_dist","summands":["Voigtian_sig","BkgSourav2_bkg"],"coefficients":["nsig","nbkg"],"extended":true}`; `width_sig` appears in `parameter_points` as `{"name":"width_sig","value":0.00426,"const":true}` and has no domain axis. pi_tof: `{"name":"Exponential_bkg","type":"exponential_dist","x":"nsigma","c":"slope_bkg_exponential_inverted"}` and `{"name":"slope_bkg_exponential_inverted","type":"generic_function","expression":"-slope_bkg"}`. A reader other than RooFit sees types that are not in the standard (`voigtian_dist`; `crystalball_doublesided_dist` is RooFit's own name).

### 9.2 A JSON-level validator against the 16 defective files
[verified] `SC2/hs3/Hs3Validate.h` (92 lines of code, uses the public-but-`Detail` `RooFit::Detail::JSONTree`) checks, on the text before import: `hs3_version` present, `default_domain` present, domain min < max, every referenced variable (found in key values, lists and in generic expressions) has a `parameter_points` value, every free variable has a domain, the value lies inside its domain, no unknown keys on a distribution, unknown types, and the count of `coefficients` against `summands` for `extended` true/false. `test-validate.cpp` runs it and the bare RooFit import over note 4e's files (`out-validate.txt`):

| file | RooFit import | validator |
|---|---|---|
| v01 no hs3_version, v02 no default_domain, v03 unknown type, v05 misspelt key, v15 syntax error | throws (clear message) | complains |
| v04 missing key `sigma_R`, v06 invalid name `mean-sig` | throws (clear message) | no complaint (RooFit's message is enough; not duplicated) |
| v07 variable missing in parameter_points | **accepts** (value 0) | complains |
| v08 extra unknown key | **accepts** | complains |
| v09 value outside domain | **accepts** (clamped) | complains |
| v10 no domain for a free parameter | **accepts** (infinite range) | complains |
| v11 extended mixture with 1 coefficient for 2 summands; v16 non-extended with 2 coefficients | **accepts** (model silently changes between extended and fractions) | complains |
| v14 domain min > max | **accepts** with warnings | complains |
| v12 constant sigma; v13 non-extended with one coefficient | accepts | no complaint: both are legitimate HS3; a project that needs "an extended mixture with `nsig` and `nbkg`" has to say so itself |

So every silent acceptance of note 4e can be closed with about 90 lines owned by the project; none of it is provided by RooFit or by the standard (see 9.4: no validator exists yet). In the prototype the check is partly redundant because the JSON is generated from `FitConfig`, whose loader already validates ranges; it matters when the JSON is hand-edited, which is the point of option C.

### 9.3 Timing
[verified, `SC2/hs3/t-export.cpp`, `SC2/proto/bench-build.cpp`; machine shared with three other jobs, load 2-4, CPU time of one thread; medians of 5 repeats] Export of a 4-object workspace: 0.024 ms, 1358 bytes. `importJSONfromString` of that model: 1.6 ms. Per fitter construction, `bench-build` (`out-bench-build.txt`): hand-written 0.005-0.006 ms (phi 0.075); HS3 without cache 1.7 ms (K0S DSCB+Chebychev1), 2.0 ms (DSCB+Chebychev2), 4.8 ms (pi_tpc DSCB+Gaussian), 0.65 ms (phi Voigtian+BkgSourav2); factory without cache 3.4-5.4 ms. The spread is explained by how the import is done [source `RooJSONFactoryWSTool.cxx:343-375`, `generate()`]: every type served by export keys / factory expressions (`gaussian_dist`, `crystalball_doublesided_dist`, and the proxy-registered extras) is imported by building a `RooWorkspace::factory` expression and so pays ~1.7 ms of Cling each, while types with a C++ importer (`generic_dist`, `mixture_dist`, `exponential_dist`, the Chebychev and Voigtian importers of 5.2, `product_dist`) do not. Hence K0S (DSCB via factory + C++ Chebychev) 1.7 ms, phi (two C++ importers) 0.65 ms, pi_tpc (two factory classes) 4.8 ms. With a copy of a cached workspace (`RooWorkspace` copy 0.012 ms, [verified], note 01) both prototypes cost 0.017-0.024 ms per fitter for K0S/pi_tpc and 0.11-0.12 ms for phi; JSON text generation alone 0.007-0.012 ms. The cache gives bit-identical fits (tables above were run with it).

### 9.4 The standard and the preprint (read in the second pass)
[doc] https://hep-statistics-serialization-standard.github.io/ (v0.2.9): the home page says the document and the standard "are still in development and can still undergo minor and major changes in the future" and, under Validators: "Future versions of this standard will recommend official validator implementations and schemata. Currently, these have not been finalized." Chapter 2 (top levels): nine top-level components (`distributions`, `functions`, `data`, `likelihoods`, `domains`, `parameter_points`, `analyses`, `metadata`, `misc`); only `metadata` is required; the chapter does not say how unknown or extra keys are treated; `misc` is for "optimizer settings or plotting colors". Chapter 2.8 (metadata): `hs3_version` is the only required field "for now"; no format, no compatibility rules, no statement of what a tool must do on a version mismatch (consistent with RooFit only checking that it exists). Chapter 3.2 (authoring primitives): rules for specifying NEW primitives (mathematical semantics normative; defaults must not depend on implementation or software version; an unsupported method must be rejected or its fallback reported, never silently substituted); it gives no naming convention for project-local types. Reading this against the importer: silently ignoring unknown keys and `extended` is not in the spirit of the authoring rules, but nothing obliges RooFit's importer today. [doc] arXiv:2606.01760, "HS3: A Descriptive, Interoperable Serialization Standard for Statistical Models in High-Energy Physics" (Burgard, Schulz, Stark, Rembser, Cello, Grunwald; submitted 1 June 2026, v2 3 June): HS3 "is convertible from and to ROOT/RooFit" and "is a superset of pyhf"; implementations in C++, Python and Julia; designed to be extended by backward-compatible additions. Only the abstract was read at this point; the full text was read in the third pass (10.5), which adds a semantic-versioning promise but no statement on a validator or on recording fit results. [inferred] For this project the preprint changes nothing: the model description needs Voigtian and the DSCB as RooFit writes it, neither of which is in the standard.

### 9.5 Other ROOT versions (superseded by 10.4 for 6.38/6.40, kept for the first reading)
Only 6.36.10 (alice4) runs here (6.36.04 does not start on this SDK, per the project's `root-issue-cling-eh/issue.md`). From note 08 [doc, the 6.38/6.40 items came through a paraphrasing fetch and are unverified]: 6.38 `RooRealVar::setVal` throws instead of clipping when the value is outside the range (this turns v09 of the defects, and a start value outside the limits in a config, from silent into an exception); 6.40 deprecates `Optimize()` (removal in 6.42; unrelated to HS3); a bug list entry for 6.36.00 mentions "Warnings from the deserialization of RooCrystalBall" (#17163). [source] `RooFitHS3` keeps the `Detail` namespace for its JSON layer in 6.36. Nothing in the 6.26-6.36 release notes announces a stable HS3 API.

## 10. Third pass (completed 14 October 2026): HS3 as an OUTPUT, more models, drawing, Cling, ROOT 6.38/6.40, the rest of the standard

Scratch: `SC2/prov/` (`Provenance.h`, `prov-main.cpp`, `phi-models.cpp`, `draw.cpp`, `cling-prov.C`, outputs `out-*.txt`, `example-*.json`), `SC2/proto/` (`DynamicRooFitterC.h` patched, `cling-c.C`, `out-cling-c.txt`), `SC2/dl/` (primary files of ROOT tags v6-36-10, v6-38-06, v6-40-04 fetched with curl from raw.githubusercontent.com; read with grep only).

### 10.1 A provenance record of every fit: native HS3 + one side block

**Idea.** Keep fitting with the hand-written model, and after `DoFit()` and `ExtractYieldsAndPurity()` write one JSON per cell that records what was fitted and what came out. The HS3 part is the *output* of RooFit's exporter (never read back as a model description), so the weaknesses of 9.2 (silent import) do not matter; it only has to round-trip.

**What native HS3 carries** [verified, `prov-main.cpp`]: the pdf structure (`mixture_dist` extended with `nsig`/`nbkg`, `crystalball_doublesided_dist`, `chebychev_dist`, `voigtian_dist`, `generic_dist`), every parameter's fitted value and constant flag (`parameter_points.default_values`), every free parameter's limits (`domains.default_domain`), the observable and its range *as it is after `Import(*h)`* (for the phi cell `mass` is [0.9945, 1.101], not the configured [0.995, 1.1]: RooFit moved the range outward to bin edges, now visible in the record, note 05), and the named ranges that `ExtractYieldsAndPurity` created: `signalRegion` and `sidebandRegion` appear as additional domains, so the integration windows are recorded without any code (the k0s record has `signalRegion` [0.475, 0.515], the phi record `signalRegion` [1.0, 1.05] and `sidebandRegion` [1.06, 1.08]).
**What it does not carry, and where it goes.** The parameter errors: the exporter writes none [source `RooJSONFactoryWSTool.cxx:977-1001`: value, const, nbins], but the importer reads `err` of a parameter point [source `configureVariable`, line 236-238], so the record adds `"err"` to each free parameter in `default_values` and the errors come back on import [verified: every `getError()` identical after import]. The `RooFitResult` (status, covQual, edm, minNll, covariance), the start values of the fit, the model strings and window mode of the configuration, the yields, purity and sideband numbers and the identity of the data are not exportable and go in a side block `misc.dynamic_roofitter` (HS3's `misc` is the free-form place: [doc] "optimizer settings or plotting colors", and the preprint says the standard does not address storing uncertainties or covariance, 10.5). RooFit tolerates the extra `misc` key and the `err` entries on import [verified, 1000 cells].
**The data** are identified, not stored: input file, histogram path, bin indices, projection name, entries, integral, number of bins and axis range of the TH1, and an FNV-1a 64-bit hash of its bin contents (a changed input file changes the hash). An option embeds the 200 contents (+1.2 KB).

The code (`SC2/prov/Provenance.h`, complete). The fitter's `model` is copied into a temporary workspace with `RooWorkspace::import`, so the fitter's objects get no new client and GitHub #23221 cannot bite (10.2); a document without distributions raises:
```cpp
#pragma once
// Fit provenance record: HS3 JSON (native export of the fitted model) + one side block of our own.
// Hook (not in the project): called after DoFit() and ExtractYieldsAndPurity() with objects the fitter owns.
#include "AnalysisDataStructures.h"
#include <RooFit/Detail/JSONInterface.h>
#include <RooFitHS3/RooJSONFactoryWSTool.h>
#include "RooAbsPdf.h"
#include "RooFitResult.h"
#include "RooRealVar.h"
#include "RooWorkspace.h"
#include "TH1.h"
#include "TMatrixDSym.h"
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <variant>

namespace Prov {
using RooFit::Detail::JSONNode;
using RooFit::Detail::JSONTree;

struct Source { std::string file, histPath; int multBin = -1, ptBin = -1, firstBin = 1, lastBin = -1; };
struct Yields { double S{}, eS{}, B{}, eB{}, P{}, eP{}, SB{}, eSB{}, ratio{}, eRatio{}; };

// FNV-1a over the bin contents (as 8-byte doubles): identifies the histogram that was fitted.
inline std::string Fnv(const TH1& h)
{
  uint64_t x = 1469598103934665603ULL;
  for (int i = 1; i <= h.GetNbinsX(); ++i) {
    double c = h.GetBinContent(i); unsigned char b[8]; std::memcpy(b, &c, 8);
    for (int k = 0; k < 8; ++k) { x ^= b[k]; x *= 1099511628211ULL; }
  }
  char s[24]; std::snprintf(s, sizeof s, "%016llx", (unsigned long long)x); return s;
}

// Returns the record. `model` and `obs` are the fitter's own objects; they are copied into a temporary
// workspace (RooWorkspace::import clones the whole tree with its current values, errors and limits), so
// the fitter's objects get no new client and GitHub #23221 (empty export while an integral is alive) cannot
// bite. Throws if the exported document has no distributions.
inline std::string Record(RooAbsPdf& model, const RooRealVar& obs, const RooFitResult& fr, const FitConfig& cfg,
                          const TH1& h, const Source& src, const Yields& y, double winLo, double winHi,
                          bool withCovariance = true, bool embedData = false)
{
  RooWorkspace ws("ws");
  ws.import(model, RooFit::Silence());
  std::string js = RooJSONFactoryWSTool{ws}.exportJSONtoString();
  auto tree = JSONTree::create(js);
  JSONNode& root = tree->rootnode();
  if (!root.has_child("distributions") || root["distributions"].num_children() == 0)
    throw std::runtime_error("[FATAL] HS3 export has no distributions (unregistered class, or GitHub #23221)");

  // 1. errors: the importer reads "err" of a parameter point, the exporter does not write it
  for (auto& set : root["parameter_points"].children())
    if (set["name"].val() == "default_values")
      for (auto& p : set["parameters"].children())
        if (auto* v = ws.var(p["name"].val().c_str()); v && !v->isConstant() && v->getError() > 0.0)
          p["err"] << v->getError();

  // 2. side block
  JSONNode& b = root["misc"]["dynamic_roofitter"];
  b["record_version"] << 1;
  JSONNode& f = b["fit"]; f.set_map();
  f["status"] << fr.status(); f["covQual"] << fr.covQual(); f["edm"] << fr.edm(); f["minNll"] << fr.minNll();
  f["nFloatingParameters"] << static_cast<int>(fr.floatParsFinal().size());
  f["extended"] << true;
  if (withCovariance) {
    auto& names = f["covariance_parameters"].set_seq();
    for (auto* a : fr.floatParsFinal()) names.append_child() << a->GetName();
    const TMatrixDSym& C = fr.covarianceMatrix();
    auto& rows = f["covariance"].set_seq();
    for (int i = 0; i < C.GetNrows(); ++i) { auto& row = rows.append_child(); row.set_seq(); for (int j = 0; j < C.GetNcols(); ++j) row.append_child() << C(i, j); }
  }
  JSONNode& c = b["config"]; c.set_map();
  c["sig_model"] << cfg.model.sigModel; c["bkg_model"] << cfg.model.bkgModel;
  c["observable"] << cfg.obs.name; c["observable_min"] << cfg.obs.min; c["observable_max"] << cfg.obs.max;
  c["snap_to_bin"] << cfg.integration.snapToBin; c["calculate_purity"] << cfg.integration.calculatePurity;
  if (auto* fx = std::get_if<FixedRange>(&cfg.integration.signalWindow)) { c["window"]["mode"] << "fixed"; c["window"]["min"] << fx->min; c["window"]["max"] << fx->max; }
  else { c["window"]["mode"] << "n_sigma"; c["window"]["n"] << std::get<NSigma>(cfg.integration.signalWindow).n; }
  if (cfg.integration.sideband) { c["sideband"]["min"] << cfg.integration.sideband->range.first; c["sideband"]["max"] << cfg.integration.sideband->range.second; c["sideband"]["from_fit"] << cfg.integration.sideband->fromFit; }
  auto& st = c["start_values"].set_seq();  // the configuration the fit started from (the HS3 block holds the fitted values)
  for (const auto& [n, p] : cfg.model.params) { auto& e = st.append_child(); e.set_map(); e["name"] << n; e["value"] << p.val; if (p.isConstant) e["const"] << true; else { e["min"] << p.min; e["max"] << p.max; } }
  JSONNode& w = b["window_used"]; w.set_map(); w["min"] << winLo; w["max"] << winHi;
  JSONNode& yy = b["yields"]; yy.set_map();
  yy["signal"] << y.S; yy["signal_error"] << y.eS; yy["background"] << y.B; yy["background_error"] << y.eB;
  if (cfg.integration.calculatePurity) { yy["purity"] << y.P; yy["purity_error"] << y.eP; }
  if (cfg.integration.sideband) { yy["background_in_sideband"] << y.SB; yy["background_in_sideband_error"] << y.eSB; yy["background_ratio"] << y.ratio; yy["background_ratio_error"] << y.eRatio; }
  JSONNode& s = b["source"]; s.set_map();
  s["file"] << src.file; s["histogram"] << src.histPath;
  if (src.multBin >= 0) { s["mult_bin"] << src.multBin; s["pt_bin"] << src.ptBin; }
  s["projection_name"] << h.GetName(); s["entries"] << h.GetEntries(); s["integral"] << h.Integral();
  s["nbins"] << h.GetNbinsX(); s["xmin"] << h.GetXaxis()->GetXmin(); s["xmax"] << h.GetXaxis()->GetXmax();
  s["contents_fnv1a64"] << Fnv(h);
  if (embedData) {
    auto& cont = b["data_contents"].set_seq();
    for (int i = 1; i <= h.GetNbinsX(); ++i) cont.append_child() << h.GetBinContent(i);
  }
  std::ostringstream os; root.writeJSON(os);
  return os.str();
}
} // namespace Prov
```

**Real records.** K0S cell (mult 0, pT 2; DSCB + Chebychev1, status 3, edm 0.84; 5326 bytes with covariance), file `SC2/prov/example-k0s-0-2.json`, one top-level key per line:
```
{
"distributions": [{"coefficients":["nsig","nbkg"],"extended":true,"name":"model","summands":["DSCB_sig","Chebychev1_bkg"],"type":"mixture_dist"},{"coefficients":["c1_bkg"],"name":"Chebychev1_bkg","type":"chebychev_dist","x":"mass"},{"alpha_L":"alpha1_sig","alpha_R":"alpha2_sig","m":"mass","m0":"mean_sig","n_L":"n1_sig","n_R":"n2_sig","name":"DSCB_sig","sigma_L":"sigma_sig","sigma_R":"sigma_sig","type":"crystalball_doublesided_dist"}],
"domains": [{"axes":[{"max":5.0,"min":0.5,"name":"alpha1_sig"},{"max":5.0,"min":0.5,"name":"alpha2_sig"},{"max":1.0,"min":-1.0,"name":"c1_bkg"},{"max":0.550000011920929,"min":0.44999998807907104,"name":"mass"},{"max":0.505,"min":0.49,"name":"mean_sig"},{"max":150.0,"min":1.0,"name":"n1_sig"},{"max":150.0,"min":1.0,"name":"n2_sig"},{"max":25000000.0,"min":0.0,"name":"nbkg"},{"max":100000000.0,"min":0.0,"name":"nsig"},{"max":0.008,"min":0.002,"name":"sigma_sig"}],"name":"default_domain","type":"product_domain"},{"axes":[{"max":0.515,"min":0.475,"name":"mass"}],"name":"signalRegion","type":"product_domain"}],
"metadata": {"hs3_version":"0.2","packages":[{"name":"ROOT","version":"6.36.10"}]},
"misc": {"dynamic_roofitter":{"config":{"bkg_model":"Chebychev1","calculate_purity":true,"observable":"mass","observable_max":0.55,"observable_min":0.45,"sig_model":"DSCB","snap_to_bin":false,"start_values":[{"max":5.0,"min":0.5,"name":"alpha1_sig","value":1.2},{"max":5.0,"min":0.5,"name":"alpha2_sig","value":1.2},{"max":1.0,"min":-1.0,"name":"c1_bkg","value":-0.8},{"max":0.505,"min":0.49,"name":"mean_sig","value":0.497},{"max":150.0,"min":1.0,"name":"n1_sig","value":5.0},{"max":150.0,"min":1.0,"name":"n2_sig","value":5.0},{"max":25000000.0,"min":0.0,"name":"nbkg","value":50000.0},{"max":100000000.0,"min":0.0,"name":"nsig","value":100000.0},{"max":0.008,"min":0.002,"name":"sigma_sig","value":0.004}],"window":{"max":0.515,"min":0.475,"mode":"fixed"}},"fit":{"covQual":3,"covariance":[[9.546874400655673e-06,2.1193010398575813e-06,2.0513577916897472e-06,-3.6439749045665826e-09,-0.00036907667416555246,-9.244384424381774e-05,-1.2624423188934302,1.2633718762812662,4.188676836241142e-09],[2.1193010398575813e-06,9.787152878204096e-06,1.7227504903070994e-07,-2.7420627982216526e-11,-2.0289721265240965e-05,-0.0019010327031222434,0.3941916016679744,-0.3942332607167672,3.6559755933580994e-09],[2.0513577916897472e-06,1.7227504903070994e-07,6.654345880509636e-06,-4.874204312075544e-10,-0.00013743471943669056,2.0232030371872774e-05,-0.9407748330828095,0.9411584043681519,4.932475940023737e-10],[-3.6439749045665826e-09,-2.7420627982216526e-11,-4.874204312075544e-10,4.682086279853358e-12,1.0734025334109285e-07,-3.9179736204996485e-08,0.00031992040011736525,-0.000320154988354687,-2.0973659109361993e-12],[-0.00036907667416555246,-2.0289721265240965e-05,-0.00013743471943669056,1.0734025334109285e-07,0.01916041294470824,0.008651884483458125,88.12194688986688,-88.17585178515559,-9.980724800711567e-08],[-9.244384424381774e-05,-0.0019010327031222434,2.0232030371872774e-05,-3.9179736204996485e-08,0.008651884483458125,6.132606585803607,157.92017905582125,-158.02251398084294,1.0384964280918472e-08],[-1.2624423188934302,0.3941916016679744,-0.9407748330828095,0.00031992040011736525,88.12194688986688,157.92017905582125,1227836.937258565,-805745.3995372029,-0.0003050099210411234],[1.2633718762812662,-0.3942332607167672,0.9411584043681519,-0.000320154988354687,-88.17585178515559,-158.02251398084294,-805745.3995372029,6962172.523789927,0.0003051650746558121],[4.188676836241142e-09,3.6559755933580994e-09,4.932475940023737e-10,-2.0973659109361993e-12,-9.980724800711567e-08,1.0384964280918472e-08,-0.0003050099210411234,0.0003051650746558121,5.122370498068564e-12]],"covariance_parameters":["alpha1_sig","alpha2_sig","c1_bkg","mean_sig","n1_sig","n2_sig","nbkg","nsig","sigma_sig"],"edm":0.844278920055524,"extended":true,"minNll":-121421924.34458701,"nFloatingParameters":9,"status":3},"record_version":1,"source":{"contents_fnv1a64":"009374f8f326ecc2","entries":8986530.0,"file":"/Users/scannito/alice/Analysis/DataFile/pp/DeltaY/Data/AnalysisResults_136_2026_PtMod_23luglio.root","histogram":"k0s-reduced-cand-producer/k0sReducedCandidates/h3K0sCandidatesMass","integral":6578757.0,"mult_bin":0,"nbins":200,"projection_name":"k0s_0_2","pt_bin":2,"xmax":0.550000011920929,"xmin":0.44999998807907104},"window_used":{"max":0.515,"min":0.475},"yields":{"background":179561.44419252846,"background_error":487.3264669847331,"purity":0.9715725034316431,"purity_error":7.82179340655048e-05,"signal":6136909.08147456,"signal_error":2567.2972733023666}}},
"parameter_points": [{"name":"default_values","parameters":[{"err":0.0025795971193459666,"name":"c1_bkg","value":-0.6321125625505132},{"name":"mass","value":0.5},{"err":2.1638128729073447e-06,"name":"mean_sig","value":0.4968660047309407},{"err":2.263265170440905e-06,"name":"sigma_sig","value":0.0035531795371073613},{"err":0.003089799870235277,"name":"alpha1_sig","value":1.220027163602068},{"err":0.1384207211552928,"name":"n1_sig","value":8.392445365677563},{"err":0.0031284412693339814,"name":"alpha2_sig","value":1.6908374104031507},{"err":2.4759192548526627,"name":"n2_sig","value":55.6754519057073},{"err":1108.0780156796973,"name":"nbkg","value":422214.9843038203},{"err":2638.5929007413797,"name":"nsig","value":6156334.05883258}]}]
}
```
phi cell (mult 0, pT 2; Voigtian with constant `width_sig` + BkgSourav2, status 3, edm 0.023; 4834 bytes), file `SC2/prov/example-phi-0-2.json`:
```
{
"distributions": [{"coefficients":["nsig","nbkg"],"extended":true,"name":"model","summands":["Voigtian_sig","BkgSourav2_bkg"],"type":"mixture_dist"},{"expression":"1.0 + c1_bkg*mass + c2_bkg*mass*mass + c3_bkg*sqrt(mass - 0.987354)","name":"BkgSourav2_bkg","type":"generic_dist"},{"mean":"mean_sig","name":"Voigtian_sig","sigma":"sigma_sig","type":"voigtian_dist","width":"width_sig","x":"mass"}],
"domains": [{"axes":[{"max":2.0,"min":-0.8,"name":"c1_bkg"},{"max":1.0,"min":0.0,"name":"c2_bkg"},{"max":20.0,"min":0.0,"name":"c3_bkg"},{"max":1.1010000240802764,"min":0.9944999986886979,"name":"mass"},{"max":1.03,"min":1.01,"name":"mean_sig"},{"max":1000000000.0,"min":0.0,"name":"nbkg"},{"max":1000000000.0,"min":0.0,"name":"nsig"},{"max":0.01,"min":0.0001,"name":"sigma_sig"}],"name":"default_domain","type":"product_domain"},{"axes":[{"max":1.08,"min":1.06,"name":"mass"}],"name":"sidebandRegion","type":"product_domain"},{"axes":[{"max":1.05,"min":1.0,"name":"mass"}],"name":"signalRegion","type":"product_domain"}],
"metadata": {"hs3_version":"0.2","packages":[{"name":"ROOT","version":"6.36.10"}]},
"misc": {"ROOT_internal":{"attributes":{"c1_bkg":{"dict":{"origName":"c1_bkg"}},"c2_bkg":{"dict":{"origName":"c2_bkg"}},"c3_bkg":{"dict":{"origName":"c3_bkg"}},"mass":{"dict":{"origName":"mass"}},"width_sig":{"tags":["Constant"]}}},"dynamic_roofitter":{"config":{"bkg_model":"BkgSourav2","calculate_purity":false,"observable":"mass","observable_max":1.1,"observable_min":0.995,"sideband":{"from_fit":false,"max":1.08,"min":1.06},"sig_model":"Voigtian","snap_to_bin":false,"start_values":[{"max":2.0,"min":-0.8,"name":"c1_bkg","value":-0.5},{"max":1.0,"min":0.0,"name":"c2_bkg","value":0.1},{"max":20.0,"min":0.0,"name":"c3_bkg","value":2.0},{"max":1.03,"min":1.01,"name":"mean_sig","value":1.019},{"max":1000000000.0,"min":0.0,"name":"nbkg","value":1000.0},{"max":1000000000.0,"min":0.0,"name":"nsig","value":1000.0},{"max":0.01,"min":0.0001,"name":"sigma_sig","value":0.001},{"const":true,"name":"width_sig","value":0.00426}],"window":{"max":1.05,"min":1.0,"mode":"fixed"}},"fit":{"covQual":3,"covariance":[[1.919756969028045e-11,-3.4133137771697354e-17,-6.553308912831621e-14,4.331294142953965e-15,3.6748593175767416e-06,-3.6751751483963763e-06,-2.8579924663075e-14],[-3.4133137771697354e-17,1.1381736653521081e-11,-4.882317670332538e-14,3.4173087543231654e-15,2.736239513219803e-06,-2.7365732881428486e-06,-2.1245103293817128e-14],[-6.553308912831621e-14,-4.882317670332538e-14,4.4518411380116594e-08,1.5197357761917277e-13,0.00015299958650194317,-0.00015302123888432886,-1.195421883181999e-12],[4.331294142953965e-15,3.4173087543231654e-15,1.5197357761917277e-13,4.5944603798135005e-11,-0.00020512208265922893,0.00020524723337583364,7.727062411508642e-12],[3.6748593175767416e-06,2.736239513219803e-06,0.00015299958650194317,-0.00020512208265922893,2183604.426162365,-312292.26227926475,-0.0024626841701104515],[-3.6751751483963763e-06,-2.7365732881428486e-06,-0.00015302123888432886,0.00020524723337583364,-312292.26227926475,664238.291338282,0.00246364054362295],[-2.8579924663075e-14,-2.1245103293817128e-14,-1.195421883181999e-12,7.727062411508642e-12,-0.0024626841701104515,0.00246364054362295,2.6411638607765456e-10]],"covariance_parameters":["c1_bkg","c2_bkg","c3_bkg","mean_sig","nbkg","nsig","sigma_sig"],"edm":0.022900302063619767,"extended":true,"minNll":-35461380.7297008,"nFloatingParameters":7,"status":3},"record_version":1,"source":{"contents_fnv1a64":"5ab72899d0f3667f","entries":5907919.0,"file":"/Users/scannito/alice/Analysis/DataFile/pp/DeltaY/Data/AnalysisResults_136_2026_PtMod_23luglio.root","histogram":"phi-strange-correlation/phiStrangenessCorrelation/phi/h3PhiData","integral":5907919.0,"mult_bin":0,"nbins":200,"projection_name":"phi_0_2","pt_bin":2,"xmax":1.2000000476837158,"xmin":0.8999999761581421},"window_used":{"max":1.05,"min":1.0},"yields":{"background":716516.7007430951,"background_error":565.796770479422,"background_in_sideband":458509.0,"background_in_sideband_error":677.1329263889033,"background_ratio":1.5627102210493036,"background_ratio_error":0.002617027830516902,"signal":343981.25374448276,"signal_error":796.7716517793783}}},
"parameter_points": [{"name":"default_values","parameters":[{"err":5.271746281898526e-05,"name":"c1_bkg","value":-0.7999999089606766},{"err":5.740363375264512e-05,"name":"c2_bkg","value":4.956793481225574e-08},{"err":0.0038682113026116127,"name":"c3_bkg","value":19.99999712298972},{"name":"mass","value":1.0475},{"err":6.778244370453379e-06,"name":"mean_sig","value":1.0195109380787841},{"err":1.6251515703066734e-05,"name":"sigma_sig","value":0.0006371479082465517},{"const":true,"name":"width_sig","value":0.00426},{"err":1477.7024143208982,"name":"nbkg","value":1871340.750781547},{"err":815.0081539093517,"name":"nsig","value":351815.77210707805}]}]
}
```
Note in the phi record: `ROOT_internal.attributes` holds a `Constant` tag for `width_sig`, `"const": true` marks it in `parameter_points`, and the expression of the generic pdf carries the threshold as the literal `0.987354` with the variable names substituted for the `@i` indices.

**Round trip over 1000 real cells** [verified, `prov-main.cpp`, output `out-prov-main.txt`]: k0s 340 + xi 260 + pi_tpc 360 + the first 40 phi cells, each fitted by the project's own `DynamicRooFitter` (hand-written model, private members opened only for the test), the record written to a file, imported into a fresh workspace, and compared: every variable's value, minimum, maximum, constant flag (all `==`) and every free parameter's error are identical in 1000 of 1000 cells; the extended NLL (`createNLL(RooDataHist(h), Extended(true))`) of the imported model at the imported values equals the original's exactly in 1000 of 1000 (largest difference 0), and equals the fit's `minNll` (largest difference 0). So the record reproduces the fitted model, values, errors and the NLL; it does not reproduce the covariance inside RooFit (it is in the side block as numbers, `covariance` with `covariance_parameters` giving the order).

| variant | mean size | total, 1000 cells | `Record()` per cell |
|---|---|---|---|
| with covariance, data identified (default) | 5523 bytes (max 5996) | 5.52 MB | 0.125 ms |
| without covariance | 3482 bytes | 3.48 MB | 0.107 ms |
| with covariance and the 200 bin contents embedded | 6696 bytes | 6.70 MB | 0.148 ms |
| without covariance, with data embedded | 4655 bytes | 4.66 MB | 0.129 ms |

Writing the 1000 files with `std::ofstream` took 0.76 ms per file (filesystem dependent; a `TObjString` in the task's output file or one JSON-lines file would avoid 1000 files, not measured). For scale one fit is 3.5 ms (these cells) and `ExtractYields` 0.73 ms, so the record adds about 4 % of the fit time compiled; interpreted (Cling, `cling-prov.C`, `out-cling-prov.txt`) the record takes 0.78 ms (k0s) and 0.93 ms (phi) and the import gives the identical `nsig` and error (k0s 6156334.0588325802 +- 2638.5929007413797, phi 351815.77210707805 +- 815.00815390935168). Machine load 1.5-4 from other jobs while measuring; the time of the verification step (import + NLL, 2.2 ms) is not part of the cost of writing.

**Is it worth adding?** [inferred from the above] Yes as an opt-in, with these limits. What it gives: for each cell the status, edm, covQual and minNll, which no output of the project records today (the notes found status 3 in most K0S cells and nothing reads it), the fitted and start parameters with errors and limits, the windows actually used, the yields, and a hash of the input, in a file that can be re-imported into RooFit (the model is rebuilt with its fitted values and errors, verified) and read by any JSON tool. What it costs: 5.5 KB and 0.13 ms per cell, a dependency on `libRooFitHS3` (experimental) and on exporters for `RooChebychev` and `RooVoigtian` (65 lines, 5.2; from ROOT 6.38 `RooChebychev` is exported natively, 10.4, `RooVoigtian` still needs the pair), and the #23221 recipe. If the owner only wants the diagnostic record, the side block alone (no HS3, no libRooFitHS3, about 40 lines with the project's rapidjson) holds almost all of its value (the side block is 1.5-1.6 KB without the covariance and 2.8-3.4 KB with it, in the two example records; a plain table of the fitted parameters with errors and limits, about 0.9 KB, would make it a standalone record of about 2.5 KB [inferred from the size of the HS3 parts of those records, 1.9-2.0 KB]); the HS3 part adds re-importability of the fitted model and the free recording of the named windows.
**Where it would hook in, without touching project code for the test:** a new public method `std::string ProvenanceRecord(const Prov::Source&) const` of `DynamicRooFitter`, reading its own `model`, `obs`, `fitResult`, `config`, `h1Data`, and the yields and window of the last `ExtractYieldsAndPurity` (these two would need to be kept as members), called by the task right after `ExtractYieldsAndPurity` where `PhiFitTask.h:129` and `PurityTask.h:327` call `fitter.DrawFit(cFit)`. The task supplies `Prov::Source` because only it knows the input file (`input_data_file`), the histogram path (`hist_name`/`base_path_data`) and the bin indices. The prototype harness used `#define private public` only to reach those members from outside.

### 10.2 Models not covered before: RooGenericPdf backgrounds and the phi Voigtian in a workspace, and the #23221 recipe

[verified, `prov/phi-models.cpp`, output `out-phi-models.txt`] Real phi cell (0,2), Voigtian (width fixed) with each of BkgSourav1, BkgSourav2 and BkgMattia (BkgMattia needs `n_bkg`, `c1..c3`: made-up start values, so its fit is not physics; identity is what is tested), built through the factory prototype, fitted, the fitter's workspace exported and imported:

| background | fit status | export | NLL original = imported | window signal yield original = imported |
|---|---|---|---|---|
| BkgSourav1 | 0 | 1655 bytes, 3 distributions | -35409092.998688563704, diff 0 | 294025.5962756749 both, equal to `ExtractYields` S |
| BkgSourav2 | 3 | 1799 bytes | -35461380.729700796306, diff 0 | 343981.2537444828 both |
| BkgMattia | 3 | 2009 bytes | -32965989.706624224782, diff 0 | 1282914.0143055317 both |

The expressions come out with names in place of `@i` and the threshold as `0.98735399999999995` (BkgSourav1: `1.0+c1_bkg*mass+c2_bkg*sqrt(mass-0.98735399999999995)`; the exporter strips the spaces). The window yield was computed on both workspaces with `createIntegral(obs, NormSet, Range("signalRegion"))` times `nsig`, i.e. the project's formula, and agrees to the last digit, so the numerical integral of the generic pdf and of the Voigtian (no analytic integral) is the same after import.

**#23221 (empty export while an integral object is alive), pinned down** [verified]: it does NOT occur when the integral is of a component pdf (`sigPdf->createIntegral(...)`, which is what `ExtractYieldsAndPurity` does): export of the fitter's workspace with such an integral alive gave the complete 1655/1799/2009 bytes with 3 distributions. It DOES occur when the integral is of the top-level pdf: `model->createIntegral(...)` alive gave 355/395/433 bytes with 0 distributions (and `exportJSONtoString` returned normally), because the exporter takes as top-level objects those without clients and the integral becomes the only client of `model`. Recipe, which the `Provenance.h` code implements: (1) export from a temporary workspace into which the model was copied with `RooWorkspace::import(model)` (the copy has no integral clients: complete export, 1655/1799/2009 bytes, verified with the top-pdf integral alive), or export before creating integrals or after destroying them (also verified: complete once the integral is destroyed); (2) always check that the document has a non-empty `distributions` array and fail otherwise. The project's `ExtractYieldsAndPurity` only integrates `sigPdf` and `bkgPdf`, so it is safe today; a future change that integrates `model` would not be.

### 10.3 `DrawFit` on the replacement workspaces, and the full `DynamicRooFitterC` in Cling

[verified, `prov/draw.cpp`, output `out-draw.txt`] The project's `DrawFit(TCanvas*)` (single pad, as in `PurityTask.h:327`) and `DrawFit(RatioCanvas)` (with pulls) were run in batch mode for the original `DynamicRooFitter`, the factory prototype (B) and the HS3 prototype (C) on five real cells (k0s (0,2) and (3,4), phi (0,2), pi_tpc (3,10), pi_tof (2,8)); the canvases were saved as PNG and every `TGraph` drawn into the pads (the data `RooHist`, the model, signal and background `RooCurve`s, and the pull histogram) was compared point by point: **all curves identical (462-592 points per single-pad canvas, 544-792 per ratio canvas), and the PNG files byte-identical (same md5) for A, B and C in all 10 canvases.** One thing had to be fixed first: the first C run produced different PNGs because HS3 has no variable title, so the observable's axis label was its name (`mass`) instead of the configured title; setting `obs->SetTitle(cfg.obs.title)` after the workspace is made (one line, now in the prototype, as `FactoryModel` already did) gave identical files. `RooFit` ends up with the same legend names because the pdf pointers are the same objects.
[verified, `proto/cling-c.C`, output `out-cling-c.txt`] The full `DynamicRooFitterC` (HS3 route, including `Hs3Model.h` and `hs3-pair.h`, all interpreted) in `root -l -b -q` on k0s (0,2), phi (0,2), pi_tpc (3,10) and pi_tof (2,8): S, its error and purity equal to 17 digits to the interpreted original in all four (e.g. pi_tof S = 482261282.22059757 +- 22193.629429113778); first construction 3.1-5.0 ms.

### 10.4 What changes in ROOT 6.38 and 6.40 for the HS3 route [doc and source of the tags; nothing was run]

Primary files fetched from the tags `v6-36-10`, `v6-38-06`, `v6-40-04` of `root-project/root` (`SC2/dl/`).
- **Release notes** (`README/ReleaseNotes/v638/index.md`, `v640/index.md`) [doc]: the only HS3/JSON lines are bug-list entries (6.38: #18697 "HS3/RooFit issue with ROOT_STANDARD_LIBRARY_PACKAGE usage"; #19768 "JSON output and infinity"); no announcement of an HS3 format, API or feature change in either version. The `RooRealVar::setVal` exception of 6.38 is in note 08.
- **Format version** [source]: `hs3VersionTag = "0.2"` in `RooJSONFactoryWSTool.cxx` of 6.36.10, 6.38.06 and 6.40.04 (the standard is at 0.2.9; the preprint's appendix shows 0.2 and 0.1.90). So a file written by 6.36.10 declares the same version as one from 6.40, but the *content* conventions differ (next points) and the importer still only checks that the field exists.
- **RooChebychev** [source]: `RooFitHS3_wsexportkeys.cxx` and `RooFitHS3_wsfactoryexpressions.cxx` of 6.38.06 and 6.40.04 contain `"RooChebychev": {"type": "chebychev_dist", "proxies": {"x": "x", "coefList": "coefficients"}}` and the matching factory expression, exactly the registration of 5.1; 6.36.10 has none. So from 6.38 the project would not need to register Chebychev (and registering it again with the same key is, by [inferred] reading, harmless but should be guarded). `RooVoigtian` appears in none of the three tags: the Voigtian pair (or proxy keys) is needed in every version.
- **Importer/exporter API** [source]: `JSONIO.h` of 6.38.06 is the same as 6.36.10 (templates `registerImporter<T>(key, topPriority)` and `registerExporter<T>(TClass*, topPriority)`), 6.40.04 adds overloads of `registerExporter` taking a string key; `Importer::importArg`, `Exporter::key()`/`exportObject` unchanged, so `hs3-pair.h` should compile unchanged [inferred, not compiled]. The built-in importers/exporters of `JSONFactories_RooFitCore.cxx` were rewritten in 6.40 as free functions (`importAddPdf`, `exportExponential`, ...) and the set of types grew (`decay_dist`, `density_function_dist`, `fft_convolution_dist`, `rate_extended_dist`, `extend_pdf`, `generic`, `binvolume`, `spline`, ...); 6.38 added the export keys of `RooChebychev` and `RooEffProd` (`efficiency_product_pdf_dist`) and dropped the `ParamHistFunc` key, and the export-key file is identical in 6.38.06 and 6.40.04; the types used here keep their names (`mixture_dist`, `gaussian_dist`, `crystalball_doublesided_dist` from the export keys, `exponential_dist`, `generic_dist`, `product_dist`, `polynomial_dist`).
- **Silent acceptance** [source, 6.40.04]: the `mixture_dist` importer (`importAddPdf`) still does not read `extended`: with coefficients it builds `RooAddPdf(name, summands, coefficients)`, without coefficients `RooAddPdf(name, summands)` (extended from the components), so defects v11/v13/v16 of 4e remain silent in 6.40; the exporter writes `extended` from `extendMode()`. A `setVal` before the range is set in `configureVariable` (value first, then `domains.writeVariable`) acts on a variable that still has an infinite range [inferred from the code order], so the 6.38 `setVal` exception probably does not turn defect v09 (value outside domain) into an error on import; not tested.
- **For the provenance record of 10.1**: nothing in 6.38/6.40 removes what it uses; the `err` key of a parameter point is still read by `configureVariable` in both tags [source].

### 10.5 The remaining standard chapters and the full preprint [doc; only what bears on the decision]

- Chapter 1 (introduction): v0.2.9; "still in development and can still undergo minor and major changes"; no official validators ("have not been finalized"); says nothing about how unknown keys are treated or what a conforming implementation must do on errors. Nothing that changes the recommendation.
- Chapter 2.1.2 (composite distributions): `mixture_dist` has `summands`, `coefficients` and an optional `extended` boolean; fraction form is `extended: false` (sum about one, or one fewer coefficient with the last computed as 1 minus the sum); extended form: the coefficient sum is the Poisson rate; the coefficient count may equal the number of summands or be one fewer, in which case `extended` must be false. `product_dist` has `factors` only. This is the rule the validator of 9.2 implements, and it shows RooFit's importer ignoring `extended` is a deviation from the standard, not a convention. Consistent with the recommendation.
- Chapter 2.4 (likelihoods): maps `distributions` to `data`; defines the likelihood structure only, nothing on fit results, best-fit values or uncertainties. Chapter 2.7 (analyses): `likelihood`, `parameters_of_interest`, `domain`, `init` (name of a `parameter_points` entry as initial value), `prior`; no field for fit results or fit settings; `parameter_points` is described there only as the source of starting values. So **the standard has no place for errors, covariance, status or edm**: the side block in `misc` is the right place, and `err` in a parameter point is a RooFit convention, not standard.
- Preprint arXiv:2606.01760 (HTML full text read) [doc]: ROOT's HS3 library "arrived in ROOT 6.36" and is called "the reference implementation for HS3 semantics"; "the vast majority of standard RooAbsPdf, RooAbsReal, and RooAbsArg subclasses are supported" and "for most analyses, round-trip compatibility is achieved" (CMS custom classes do not yet have representations, which matches finding `RooChebychev` and `RooVoigtian` missing here); semantic versioning "separating structural changes, backward-compatible extensions, and deprecated constructs" and "governance ... expected to be developed"; `parameter_points` may hold "known minima or other special points" (so a best-fit point can be stored) and `misc` "recommended fit ranges" and "optimizer configuration" (non-normative); uncertainties and covariance are not addressed; pyhs3 has tests "to ensure that pyHS3 interprets HS3 documents consistently with ROOT"; "shared validation and compliance test suites" are only a possible future mechanism; planned: vector-valued and discrete parameters, advanced domain types, extended unbinned support. [inferred] Nothing here contradicts the recommendation: the format is moving towards stability and a semantic-versioning promise exists, but there is still no validator, and the record-of-a-fit use is outside what the standard defines.

## 11. Fourth pass (15 October 2026): where to keep the provenance records, and the ModelConfig / RooSimultaneous round trip

Scratch: `SC2 = /Users/scannito/alice/Analysis/RooFitKnowledge/scratch/study0102` (`prov/prov-store.cpp`, run in `prov/store/`; `hs3/simul.cpp`, outputs `out-simul.txt`, `out-simul-setbins.txt`). Build with `SC2/build.sh <file.cpp>`.

### 11.1 TObjString in a ROOT file or one JSON-lines file
[verified, `prov-store.cpp`] 600 records (k0s 340 + xi 260 real cells, fitted by the project's own `DynamicRooFitter`, record of section 10.1 with covariance) written in two ways: one `TObjString` per cell, key `k0s_m0_p2`, in a ROOT file (`TObjString::Write(key)`, default compression); and one line per cell `{"cell":"k0s_m0_p2","record":{...}}` in a `.jsonl` file (the record has no newline, checked). The data file is only read; the stores were written in scratch.

| | ROOT file (`TObjString` per cell) | JSON lines |
|---|---|---|
| size, 600 records | 1.43 MB (compressed by ROOT) | 3.22 MB (0.59 MB if gzipped afterwards) |
| write 600 records (noisy, relative only) | 22 ms; 33 ms with `TFile::Flush()` after every record | 2.5 ms; 3.2 ms with a flush after every line |
| read one cell, fresh process | 19 ms, almost all of it opening the file (`TFile::Open` + `Get<TObjString>(key)`) | 0.1 ms for the first cell, 0.7 ms for the last (linear scan for the `"cell"` prefix) |
| text read back | identical to the record (checked for the first and the last cell) | identical |
| process killed (`abort()`) after 300 of 600 records, nothing closed | at reopening `Warning in <TFile::Init>: file crash.root probably not closed, trying to recover` and `successfully recovered 300 keys`: **all 300 records readable**, with or without a flush per record | without a flush per line: 300 lines, the last one **truncated** (3601 of about 5300 characters, does not parse), 299 usable; with `std::endl` per line: 300 complete |

`abort()` leaves what the process already handed to the operating system on disk; a power cut or a full disk could lose more in both formats [inferred].
**How to read back one cell.** ROOT (C++ or a Cling macro): `std::unique_ptr<TFile> f(TFile::Open("prov.root")); std::string rec = f->Get<TObjString>("k0s_m0_p2")->GetString().Data();` then `RooFit::Detail::JSONTree::create(rec)`, or `RooJSONFactoryWSTool{ws}.importJSONfromString(rec)` to get the fitted model back. JSON lines, without ROOT: `grep '"cell":"k0s_m0_p2"' prov.jsonl`, or in Python `next(json.loads(l) for l in open("prov.jsonl") if '"cell":"k0s_m0_p2"' in l)["record"]["misc"]["dynamic_roofitter"]["fit"]["status"]`; a table of all cells is `pandas.json_normalize([json.loads(l) for l in open("prov.jsonl")])`.
**Which is easier for the owner** [inferred from the above]: JSON lines. It is one plain-text file per task run, written with `std::endl` (or `flush()`) after each record so that a crash costs at most the record being written; it can be read by `grep`, `jq`, Python or a spreadsheet without ROOT, and the status of every fit is one `jq` line away. A `TObjString` key inside the task's existing output ROOT file is the alternative if everything must stay in one file: it is smaller (2.3 times) and survives a crash through ROOT's recovery, but needs ROOT to read, the recovery prints warnings, and the text is not visible to the usual text tools. Either way the store should go next to the task's own output (the purity or phi-fit output directory chosen by the task), not into `DataFile/`, and this study wrote only to scratch.

### 11.2 ModelConfig / RooSimultaneous: one round trip, and why it does not matter here
[verified, `hs3/simul.cpp`, outputs `out-simul.txt`, `out-simul-setbins.txt`] Two real K0S cells (mult 0 and 1, pT 2) as the two channels of a `RooSimultaneous` (category `cell`), sharing the DSCB shape, each with its own Chebychev and yields; combined `RooDataHist` from the two 200-bin histograms (`Index(cell)`, `Import(map)`); a `RooStats::ModelConfig` with POI `nsig0` imported into the workspace. Export: 4502 bytes with all the top-level blocks `analyses`, `data`, `distributions`, `domains`, `likelihoods`, `metadata`, `misc`, `parameter_points`; `likelihoods` = `sim_obsData` with distributions `model0 model1` and data `obsData_m0 obsData_m1`; `analyses` = `sim_obsData` with POI `nsig0`. Import: the `ModelConfig` comes back (pdf `sim`, POI `nsig0`, observables `mass`); the model is exact (NLL of the imported model on data rebuilt from the histograms equals the original, difference 0). **But the data do not survive**: the exporter writes each channel with the observable's binning, 100 bins, while the dataset had 200 bins per channel (`obsData_m0`: `nbins 100`, 100 contents); the import gives a weighted `RooDataSet` of 200 entries (same total 29832364), the NLL at the start values differs by 3.2e4 and the refit gives `nsig0` 6164974.4 instead of 6157389.1 (+0.12 %), minNll -559652459.45 instead of -559702289.56. With `mass->setBins(200)` before building the dataset the export has 200 bins per channel and the round trip is exact (NLL difference 0, the refit identical to the last digit). This is the same "observable keeps 100 bins" effect as in notes 04 and 05; for a single `RooDataHist` (note 07's export) the 200 bins were kept [verified there], so the rebinning is specific to the combined dataset with a category [inferred from the two cases].
**Relevance** [inferred]: none for the project as it is. Each cell is fitted on its own, there is no simultaneous fit and no ModelConfig, and the HS3 `likelihoods`/`analyses` blocks are produced by RooFit only for a `RooSimultaneous` with a `ModelConfig` (section 2). It would matter if the owner ever fitted several cells together with a shared signal shape (the test shows it works, given `setBins` to the histogram's bin count), or wanted HS3 files that pyhf-like tools read as complete statistical models. The provenance record of section 10 exports no data (it identifies them), so the rebinning cannot affect it; if the contents are embedded they are taken from the TH1 directly.

## NOT DONE
- Compiling `hs3-pair.h` against ROOT 6.38/6.40 (not installed here; section 10.4 is reading of the primary files).
- The ROOT-internal HistFactory importers and exporters (not relevant to these fits).
- A schema validator for the whole of HS3 (none exists; the validator of 9.2 covers the subset used here).
- Putting the provenance method into `DynamicRooFitter` and the store into the tasks: the owner's decision; the prototype and the measurements are in sections 10.1 and 11.1.

## Resume here
Nothing essential remains. Rebuild any program with `SC2/build.sh <file.cpp>` and run it after `source SC2/../rooenv.sh`. Main evidence: `cd SC2/proto; ./cmpC k0s k0s-reduced-cand-producer/k0sReducedCandidates/h3K0sCandidatesMass` (sections as in note 01; env `NSIGMA`, `SNAP`, `NOCACHE`, `SIG`, `BKG`, `EXTRA`); `cd SC2/hs3; ./test-pair; ./test-validate SC2/../json-hs3/good-model.json SC2/../json-hs3/variants/*.json; ./simul` (and `SETBINS=1 ./simul`); `root -l -b -q -e '.L cling-hs3.C' -e 'cling_hs3(2)'`; `DYLD_LIBRARY_PATH=$ROOTSYS/lib python3 pyroot-hs3.py`; `cd SC2/prov; ./prov-main` (env `NOCOV`, `EMBED`, `NOFILES`), `./phi-models`, `cd draw-out; LABELS=1 ../draw`, `cd store; ../prov-store make 600; ../prov-store write; ../prov-store read k0s_m0_p2; ../prov-store crash 300 0|1; ../prov-store count crash.root|crash.jsonl`. First-pass scratch files are in `SC2/../json-hs3/`.

## Sources
- ROOT source (local): `roofit/hs3/{src,inc,test}`, `roofit/jsoninterface/{src,inc}`, `roofit/doc/developers/roofit_hs3.md`, `README/ReleaseNotes/v626/index.md:398-408` (introduction of RooFitHS3: "not feature complete"), `tutorials/roofit/roofit/rf515_hfJSON.{py,json}`. No HS3-specific entry in the v6.36 release notes besides the RooCrystalBall stability fix (`v636/index.md:208-210`), and issue entries about HS3 round trips in v632/v634. [source]
- HS3 standard: https://hep-statistics-serialization-standard.github.io/ (v0.2.9, chapters 2.1.1, 2.1.2, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.9, 3.1), repository https://github.com/hep-statistics-serialization-standard/hep-statistics-serialization-standard
- ROOT doc page for the HS3 developer guide: https://root.cern/doc/v636/group__roofit__dev__docs__hs3.html (found by search, not fetched).
- Primary files of the tags v6-36-10, v6-38-06, v6-40-04 (release notes `README/ReleaseNotes/v638/index.md`, `v640/index.md`; `roofit/hs3/src/*.cxx`, `roofit/hs3/inc/RooFitHS3/JSONIO.h`), fetched from raw.githubusercontent.com into `SC2/dl/` (section 10.4).
- HS3 standard chapters 1, 2.1.2, 2.4, 2.7, 2.8, 3.2 and the full text of arXiv:2606.01760 (HTML) (sections 9.4 and 10.5).
