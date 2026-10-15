# 07 - RooWorkspace, HistFactory, RooStats, xRooFit, persistence (FINAL, 15 October 2026)

Written 14 October 2026 for ROOT 6.36.10 (ALICE build `v6-36-10-alice4-local1`, macOS arm64). Source tree used for
[source]/[doc] tags: `~/alice/sw/SOURCES/ROOT/v6-36-10-alice2/v6-36-10-alice2/` (abbreviated `SRC/`).
Tags: [doc] Doxygen/markdown text, [source] code read, [verified] something run here, [inferred] my conclusion, not tested.
The first pass was stopped before it was finished; the third pass (15 October) added sections (h) to (k) (high-statistics profile/MINOS/toys, persistence cost, O2 and RooUnfold, xRooFit/RooStats/HistFactory tools); the second pass (sections (d) to (g) under Experiments) added the HS3/JSON export test, a profile-likelihood and RooMCStudy test on a real cell, the cross-version check and the web/GitHub research. The final pass (15 October) added (l), a helper that exports one fitted cell to HS3 and checks the re-import, and closed the note: see "NOT DONE (summary)", where only impossible or moved items are listed.

## Summary

- [verified] A RooWorkspace round trip works for the project's K0S model. For one real cell (mult 3, pT 10, 26.2 M entries,
  200 bins) the workspace file is 18 KB, reloading takes 0.27 s in a fresh process, a refit from the stored `prefit`
  snapshot is bit-identical to the original (max relative parameter deviation 0, `isIdentical` true at tol 1e-12), and
  the yield/purity extraction from the stored `RooFitResult` reproduces S, B, P and their propagated errors exactly
  (max relative difference 0). Re-extracting with 3 other signal windows costs 0.22 ms for all three.
- [verified] On 30 real k0s cells, `status()` is useless as a quality flag: 29/30 fits end with status 3 (history
  `MINIMIZE=-1 HESSE=3`), 1/30 with 0. What discriminates is edm, `covQual`, parameters at their limit and a
  restart / random-restart test. The restart test showed that in 20/30 cells the project's `fitTo` result is not at the
  minimum (a restart finds a lower NLL by more than 0.5, up to 4e5).
- [verified] xRooFit (`ROOT::Experimental::XRooFit`) is usable in this build. Its `xRooNLLVar::minimize()` (strategy
  ladder with Scan, Hesse strategy sequence, NLL offsetting, optional boundary check) gave status 0 in 28/30 of the
  same cells and an NLL within 0.5 of the best of five variants in 26/30, against 1/30 and 6/30 for the project's
  `fitTo` call. Plain RooFit with `Offset(true)` alone already gave status 0 in 10/30 and NLL-best in 19/30.
  This is the most important finding of this pass. It is not confirmed on the real (merged) analysis bins.
- [verified, second pass] HS3/JSON export of the fitted workspace works and is lossless for model, data and ranges once `RooChebychev` is registered with TWO ONE-LINE JSON FILES (`loadExportKeys`, `loadFactoryExpressions`; no C++): the NLL and all 10 parameter values/ranges are identical after import; the named range `signalRegion` is exported as a domain; the `RooFitResult` (covariance, edm, status) and the errors are NOT written by the exporter (third pass, note 02 section 10.1: the importer does read an `err` entry of a parameter point, so a record can add the errors and they come back exactly; status, edm, covQual, minNll and the covariance go in a side block of our own under `misc`, verified on 1000 real cells). Pitfall reproduced (GitHub #23221): while a `createIntegral` object of the pdf is alive the export silently contains no distributions (1559 instead of 3739 bytes) and still returns true.
- [verified, second pass] On a converged real cell (k0s mult0, pT33, N=107148): the profile likelihood of `nsig` gives [-0.997, +1.03] sigma_Hesse (Hesse accurate to ~3%), MINOS fails for the lower error, and `RooMCStudy` (300 binned toys) gives nsig pull mean +0.06 and RMS 0.98; 8% of the toys end with status 3 and `RooMCStudy` silently drops them from the parameter dataset.
- [verified, third pass, (h)] On two HIGH-statistics status-3 K0S analysis cells of the 2026 file, once the fit is brought to the minimum (`Offset("bin")` refit) Hesse matches the profile likelihood to <0.3% and MINOS works; on the project's plain NLL the profile is distorted by up to 50% at 1 sigma and MINOS fails in one cell. With the project's options 199/200 and 100/100 toys end in status 3 even when started at the truth (0 with `Offset("bin")`), `RooMCStudy::fitParDataSet` then holds 1 and 0 entries, and binned toys use the observable's 100 bins instead of the data's 200 (a +6.5 sigma artefact) unless `obs->setBins(200)` is called.
- [verified, third pass, (i)-(k)] A full per-cell workspace costs 6.7 KB compressed (data 2.7 KB, fit result 2.3 KB) plus about 12 KB of StreamerInfo and header per file; O2Physics/O2 use RooFit only in standalone macros, never in tasks or CCDB; TUnfold is in this ROOT, RooUnfold only as an alidist recipe; xRooFit's `pgof()` is not usable on the project's continuous model (negative saturated difference), a Poisson deviance is.
- The cross-ROOT-version load test could not be done: ROOT 6.36.04 does not start with a manual environment (modulemap error) and `alienv` was off limits (section (f)).
- HistFactory and RooStats (limits/intervals/hypothesis tests) are not relevant to this project [inferred from the
  Doxygen index pages and class lists]. xRooFit is relevant as a source of ideas and possibly as a minimiser.
- [verified, final pass, (l)] A scratch helper (`scratch/study-final-010708/hs3/ProjectHS3.h`) registers Chebychev and Voigtian for HS3 and exports one fitted cell of the project with checks. On real k0s (0,2) and phi (0,2) it gives an identical NLL and window yield after re-import. A refit from the HS3 file is bit-identical only if the parameter errors (MINUIT's initial steps), which HS3 does not store, are restored; without them the k0s refit moved nsig by 7e-6.

## RooWorkspace

### Class facts [source]
- Doc block (`SRC/roofit/roofitcore/src/RooWorkspace.cxx`, top of file): "Persistable container for RooFit projects. A
  workspace can contain and own variables, p.d.f.s, functions and datasets. All objects that live in the workspace are
  owned by the workspace." `import()` enforces consistency and no duplicate names. Warning in the same block: reading
  large/deeply nested workspaces can crash with a too-small stack (`ulimit -s`).
- Header: `RooWorkspace.h` ends with `ClassDefOverride(RooWorkspace, 8)`; `RooFitResult.h` has `ClassDefOverride(RooFitResult,5)`.
  Members persisted: `_uuid`, `_classes` (CodeRepo, must be first), `_allOwnedNodes`, `_dataList`, `_embeddedDataList`,
  `_views`, `_snapshots`, `_genObjects`, `_studyMods`, `_namedSets`, `_eocache`. Transient (`!`): `_dir`, `_factory`.
- `import(const RooAbsArg&, ...)` CLONES the whole server tree and the workspace owns the clone (the doc comment says so).
  It imports the CURRENT values: so a model imported after the fit stores post-fit values. Variables already present
  by name are shared (the new pdf is connected to them); a pdf/function with an existing name is an ERROR ("another
  instance with same name already in the workspace and no conflict resolution protocol"). Options: `RenameConflictNodes`,
  `RenameAllNodes`, `RenameAllVariables`, `RenameVariable`, `RecycleConflictNodes`, `Silence`, `NoRecursion`.
- RETURN VALUE IS INVERTED relative to intuition: `import`, `defineSet`, `renameSet`, `removeSet` return `true` on ERROR
  (`return true ;` in every error branch). `loadSnapshot` returns `false` when the snapshot does not exist and `true`
  when loaded; `saveSnapshot` always returns `true`. Check these before relying on a return value.
- `import(RooAbsData const&)`: clones the dataset, imports its observables if not present, stores it in `_dataList`;
  duplicate dataset name is an error. Import of a `RooDataHist` is therefore a full copy (bin contents, weights).
- `import(TObject const&, replaceExisting)` and `import(TObject const&, aliasName, replaceExisting)`: clones any TObject
  into `_genObjects`; the alias form wraps it in `RooTObjWrap` (needed for objects without settable name, e.g. TVectorD).
  Retrieve with `genobj(name)` / `obj(name)`. This is how a `RooFitResult` is stored.
- `saveSnapshot(name, params, importValues)`: clones the selected workspace parameters (value, error, range, attributes
  incl. "Constant") into a `RooArgSet` named `name`; replaces a snapshot with the same name. `loadSnapshot(name)` does
  `actualParams.assign(*snap)`; `RooAbsCollection::assign` copies value, cache and the `Constant` attribute, and
  `RooRealVar::copyCache(valueOnly=false)` also copies `_error`, `_asymErrLo/Hi`. It does NOT touch ranges (min/max).
  `getSnapshot(name)` returns the stored COPY, not the live variables.
- `defineSet(name, RooArgSet, importMissing)` / `defineSet(name, "a,b,c")`, `extendSet`, `renameSet`, `removeSet`, `set(name)`.
  Redefining only warns. `argSet("a,b")` throws `std::runtime_error` for an unknown name.
- Accessors: `pdf()`, `function()`, `var()`, `cat()`, `data()`, `arg()`, `allVars()`, `allPdfs()`, `components()`; all return
  `nullptr` when missing, so null-check. `factory(expr)` / `factory()` is `RooFactoryWSTool` (string DSL); not needed here
  because the project builds objects in C++.
- `Print("v")` lists variables, pdfs, datasets, snapshots, named sets and generic objects; `"t"` prints trees.
- `writeToFile(name, recreate)` just opens a TFile and calls `Write()`. `Streamer` (RooWorkspace.cxx near line 2445)
  temporarily detaches client links from objects outside the workspace before writing and restores them after;
  on reading it runs `ioStreamerPass2` and re-points the expensive-object cache and workspace pointer.
- Transactions: `startTransaction/commitTransaction/cancelTransaction` give all-or-nothing imports.
- Custom classes: `importClassCode()` / `autoImportClassCode()` embed source of non-ROOT classes in the workspace so it
  can be read elsewhere. NOT needed for the project's models: every class used by `DynamicRooFitter` (RooCrystalBall,
  RooGaussian, RooVoigtian, RooChebychev, RooExponential, RooGenericPdf, RooAddPdf, RooProdPdf, RooRealVar) is in ROOT [source:
  `DynamicRooFitter.h` includes]. `RooGenericPdf` persists its formula string.
- Copy constructor / `Clone()` deep-copy nodes, data, snapshots, sets and generic objects.

### Persistence behaviour [verified]
- Program `write.cpp` (scratch, see Resume) built the workspace for cell (3,10) from the project's `DynamicRooFitter`
  (internals reached with `#define private public` in the scratch program only): `import(model)`, `import(RooDataHist)`,
  `import(RooFitResult, "fitResult")`, snapshots `prefit` (values from a snapshot taken before the fit, via
  `saveSnapshot(name, set, importValues=true)`) and `postfit`, named sets `obs`, `floatPars`, `poi`, plus a TObjString
  (window settings) and a TVectorD (original S,B,P and errors). `Print("v")` shows all of these.
- File: `ws_3_10.root` = 18,379 bytes. Building the workspace 16 ms; first `Write` 1.29 s (one-off class/streamer-info
  initialisation, [inferred] from the fact that the 30-workspace run below wrote everything in 0.29 s).
- Reload in a fresh process (`read.cpp`, no project code): open + `Get<RooWorkspace>` 0.27 s; `genobj("fitResult")`
  returns a valid `RooFitResult` with status/covQual/edm/statusHistory intact (nHist 2).
- 30 workspaces in one file: full ws with `RooDataHist` 218,442 B (7.3 KB per cell); ws without the data 134,239 B
  (4.5 KB); `RooFitResult` alone 79,333 B (2.6 KB). Each figure includes the file's one-off StreamerInfo, not separated.
  Linear extrapolation to 1000 cells [inferred]: about 7 MB (full), 4.5 MB (no data), 2.6 MB (fit result only).
- `Print("v")` of a reloaded `RooFitResult` shows `<none>` in the GblCorr column: the legacy global-correlation list is
  filled lazily [source: `_globalCorr` is `mutable`, filled by `fillLegacyCorrMatrix`], not lost.

### RooFitResult persistence [source] and [verified]
- `RooFitResult` doc: contains constant parameters, initial and final floating parameters with errors, correlation
  matrix and global correlation, NLL and EDM at minimum; "No references to the fitted PDF and dataset are stored".
  Members: `_status`, `_covQual`, `_numBadNLL`, `_minNLL`, `_edm`, `_constPars`, `_initPars`, `_finalPars`, `_CM` (correlation),
  `_VM` (covariance), `_statusHistory` (vector of (label, code)). Transient: `_globalCorr`, `_randomPars`, `_Lt`.
  Streamer has backward-compatible reading for class versions <= 3 (`RooFitResult::Streamer`), so old results can be read.
- `RooFit::Save()` makes `fitTo` return the result: `FitHelpers.cxx` builds it with `RooMinimizer::save` (name
  `fitresult_<pdf>_<data>`). `status()` is the status of the LAST minimiser step: in `RooMinimizer::exec` every step
  sets `_status` and appends `("MIGRAD"/"MINIMIZE"/"HESSE"/"MINOS", status)` to `_statusHistory`; `save()` stores the last.
  `RooMinimizer::determineStatus`: status is `-1` when the fitter returned false. In the project's fits the history is
  `MINIMIZE=-1 HESSE=3` [verified, 29/30 cells]: the minimisation step failed (-1) and HESSE then reports 3 (edm above max).
  `DynamicRooFitter::DoFit` returns only the final 3.
- `covQual` meaning [source: `RooFitResult::printMultiline`]: -1 externally provided, 0 not calculated, 1 approximation only,
  2 full but forced positive-definite, 3 full and accurate.
- Minuit2 status codes [source: comment in `xroofit/src/xRooFit.cxx`]: 0 OK, 1 covariance made pos-def, 2 Hesse invalid, 3 edm above
  max, 4 call limit, 5 other failure.
- `isIdentical(other, tol, tolCorr, verbose)` compares NLL, const/initial/final parameters (relative tol; errors at 1e-3)
  and correlations (absolute tolCorr) [source]; `isIdenticalNoCov` skips correlations. Note that it also compares the
  INITIAL parameters, so a warm-start refit never matches a cold one [verified below].
- `randomizePars()` returns the floating parameters perturbed with the covariance (square-root method), reusing one
  internal list; `correlationMatrix()`, `covarianceMatrix()`, `reducedCovarianceMatrix`, `conditionalCovarianceMatrix`,
  `createHessePdf`, `correlationHist`, `plotOn` (2-D ellipse) exist [source: RooFitResult.cxx function list].
- The extraction in `DynamicRooFitter::ExtractYieldsAndPurity` uses `getPropagatedError(*fitResult, RooArgSet(*obs))`; the
  reloaded workspace objects work with the stored result because parameters are matched by name [verified: exact
  reproduction of S, B, P and errors].

### Schema evolution and other ROOT versions
- [source] The workspace relies on ROOT's normal streamer-info schema evolution; `RooWorkspace::Streamer` has a "pass-2"
  hook for evolution (`ioStreamerPass2`). `RooFitResult` has explicit version handling. Release notes v6.30-v6.36 list
  RooFit class removals/renames that would break reading a workspace containing them (`RooPower` -> `RooPowerSum`,
  `RooExpPoly` -> `RooLegacyExpPoly`, removal of `RooMomentMorphND`; v632 notes) but none for the classes this project uses.
  [doc: `SRC/README/ReleaseNotes/v632/index.md`]
- Reading a 6.36.10 workspace with ROOT 6.36.04 and the reverse: NOT POSSIBLE here (6.36.04 does not start without the alienv module environment, see (f)); the claim "loads in a different ROOT version" remains untested. A relevant community report (#17163, RooCrystalBall streamer warnings across versions) is in (f).
- (Done in the third pass, section (j).) ALICE/O2 considerations beyond: the O2Physics environment (`alienv printenv O2Physics/latest-master-o2`)
  provides the same ROOT 6.36.10 used here [verified: `ROOTSYS`], and the fits ran against it.
- Text alternative [doc: `SRC/roofit/doc/developers/roofit_hs3.md`, first 80 lines; verified: headers
  `RooFitHS3/RooJSONFactoryWSTool.h` and `libRooFitHS3` exist in the install]: RooFit HS3 exports/imports a RooWorkspace
  to JSON; the doc marks the library "still at an experimental stage" and says feature completeness will probably never be
  reached. The export/import test was done in the second pass: see (d) (RooCrystalBall, RooAddPdf, RooDataHist survive; RooChebychev after a 2-line proxy registration).

### Practical notes
- One workspace per cell (names must be unique inside a workspace, so one workspace cannot hold two independent cells with
  identical parameter names without renaming) [inferred from the import rules]. In one file use keys `ws_<mult>_<pt>` [verified:
  30 workspaces in one file, no conflict].
- `DynamicRooFitter` builds the model with fixed names (`model`, `nsig`, `nbkg`, `mass` or the configured observable name,
  `DSCB_sig`, `Chebychev1_bkg`, ...) [source: `DynamicRooFitter.h`]; `read.cpp` hard-codes `mass`, `DSCB_sig`, `Chebychev1_bkg`.
  A real implementation would store the sig/bkg pdf names (generic object or named set) rather than guess.

## Surrounding tools (what each is for; relevance)

| Tool | What it is for | Relevant here? |
|---|---|---|
| HistFactory (`SRC/roofit/histfactory`) | Builds a RooWorkspace from histogram templates (samples per channel, normalisation and shape systematics via nuisance parameters with +-1 sigma histograms), XML-configured (`hist2workspace`), a standard-form extended pdf per channel. [doc: `histfactory/doc/index.md`, `README`] | No. The project has analytic signal/background shapes, no templates, no nuisance-parameter systematics. [inferred] v634 note: `ExportOnly` now default, fit it yourself as in `hf001`. [doc: v634 release notes] |
| RooStats (`SRC/roofit/roostats`) | Statistical tests on top of RooFit: `ModelConfig`, `ProfileLikelihoodCalculator`, `FeldmanCousins`, `HypoTestInverter`, `AsymptoticCalculator`, Bayesian/MCMC calculators, `SPlot`, number-counting utilities (class list read from `roostats/inc/RooStats`). [doc: `roostats/doc/index.md`] | Not for limits/intervals/tests. Two curiosities: `RooStats::SPlot` (sWeights from an extended unbinned fit; needs per-event data, not a binned cell) and `ProfileLikelihoodCalculator` (profile intervals), both only [inferred] relevant. |
| `ModelConfig` (`RooFit/ModelConfig.h`, doc in `roofitcore/src/ModelConfig.cxx`) | Tells RooStats tools which parameters are POI, nuisance, observables, global observables; stored in the workspace. [doc] | Optional: could mark `nsig` as POI (`poi` named set does the same job here). |
| xRooFit (`SRC/roofit/xroofit`, namespace `ROOT::Experimental::XRooFit`) | High-level API (`xRooNode` browsable tree over a workspace; `xRooNLLVar`; `xRooHypoSpace` for hypothesis tests and limits; `xRooFit::minimize` with a strategy ladder). [doc: `xroofit/doc/index.md`, headers] Built into this ROOT (`libRooFitXRooFit`, version c9465df 2024-11-28 [verified: `xRooFit::GetVersion()`]). | Partly. See below. Its limits/hypothesis-test code: no. Its minimiser and fit-status handling: yes. |
| roofitmore (`SRC/roofit/roofitmore`) | Components that need mathmore: `RooHypatia2`, `RooLegendre`, `RooNonCentralChiSquare`, `RooSpHarmonic` (headers listed). [doc: `roofitmore/doc/index.md`] | `RooHypatia2` is a possible alternative to the double-sided Crystal Ball, but RooCrystalBall improvements for large N are in v636 [doc: v636 notes]. Not evaluated. |
| `RooFit::Experimental` | In `RooGlobalFunc.h`: `ParallelGradientOptions`, `ParallelDescentOptions` (multiprocess minimisation); also the codegen/AD classes (`RooFuncWrapper`, `CodegenContext`; doc `doc/developers/roofit_ad.md`, summarised in note 08 section 5.4). | No for this project: the project already found `Parallelize` throws in this build and `NumCPU` does nothing [source: comments in `DoFit`]. |
| RooUnfold-like (closed in (j): TUnfold is in this ROOT, RooUnfold only as an alidist recipe) | Nothing found in `SRC/roofit` (class lists for roofitcore, roostats, histfactory, roofitmore, xroofit). `libUnfold`/TUnfold appear in the module list of ROOT (not RooFit) [verified: "Failed to load module Unfold" message list]. | Closed in (j): TUnfold is part of this ROOT; RooUnfold is not installed (only an alidist recipe). No unfolding is needed by the purity fits. |

### xRooFit in detail [source] and [verified]
- `xRooFit::createNLL(pdf, data*, cmdargs)`, `xRooNLLVar::minimize(fitConfig)` returns a `RooFitResult`. Default fit config
  [source: `xRooFit.cxx` default fit config block]: Minuit2, errordef 0.5, Hesse after fit, Minos on parameters with attribute "minos", strategy -1
  (start at the front of the sequence), `StrategySequence` `"0s01s12s2s3m"` (strategies 0,1,2,3; `s` = Scan; `m` = Minuit/migradImproved fallback),
  `HesseStrategySequence` `"23"`, `OptimizeConst` 2, `BoundaryCheck` 0 (off; e.g. 0.01 = flag post-fit values within 1% of a limit), NLL option
  `OffsetLikelihood` on. The loop retries up to 4 times, doubles max function calls when status is 4, and warns on every non-zero status.
- Status handling [source]: after Hesse, status 0 with `covQual` 2 becomes status 1, other `covQual != 3` becomes status 2.
  With `BoundaryCheck`, `statusHistory` gets `("BOUNDCHK", code)` and the status is increased by 900 (a parameter within the given fraction
  of its limit and no "physical" range) or 9000 (value +- error beyond the range). It also refines a parameter sitting at a limit onto the limit if the NLL
  there is lower. It prints "Warning: post-Hesse edm greater than allowed by tolerance" when the edm is above tolerance after Hesse.
- Diagnostics available on `xRooNLLVar::xRooFitResult` [source: `xRooNLLVar.h`]: `impact(poi, np, up, prefit, approx)`, `ranknp(...)`
  (nuisance parameters ranked by impact on the POI), `conditionalError(poi, nps)`, `cfit`, `ifit`. For hypothesis points `xRooHypoPoint::status()` is
  a bitmask of the fit statuses, with `allowedStatusCodes = {0}`. These need a POI and nuisance parameters with constraints; the project has none,
  so impacts and pulls of nuisance parameters do not apply as such. [inferred]
- Run on the real K0S cell (3,10) [verified, `xrf.cpp`]: workspace loaded from the file written above, wrapped as `xRooNode(*ws)`
  (`Print()` lists datasets, pdfs, fits, objects, sets, snapshots), `nll = w["pdfs/model"]->nll(w["datasets/dataHist"])`,
  `nll.fitConfigOptions()->SetValue("BoundaryCheck", 0.01)`, `nll.minimize()`. Result: status 0, covQual 3, edm 1.66e-6, 0 invalid NLL, 45 ms,
  history `Minuit2Migrad0=3 Minuit2Scan0=5 Minuit2Migrad0=3 Minuit2Migrad1=3 Minuit2Scan1=5 Minuit2Migrad1=3 Minuit2Migrad2=0 BOUNDCHK=0`
  (i.e. strategies 0 and 1 failed, 2 succeeded). Versus plain `fitTo`: status 3, edm 0.121. The two minima differ: n1_sig 1.0000 (its lower limit) vs 1.5008,
  nsig 2.2544e7 vs 2.2542e7 (about 0.3 sigma, from the printed values). So the "fit result" is not unique at this precision; both are
  "within errors" for the yield but only one is converged. xRooFit prints "Inferring initial errors of 9 parameters" when parameters have no error.
- Caveats [inferred]: the API is declared experimental by ROOT [doc: xroofit index.md]; memory/ownership is shared_ptr based and different from the
  project's `DynamicRooFitter` style; the `xRooNode` tree is a large dependency for what is needed.

## Native diagnostics and an audit recipe

Everything below is built from stock RooFit and was run in `batch.cpp` (function `DoAudit`) [verified]. Per cell it costs ~18 ms median
(5 random restarts included) against a median fit time of 5 ms.

1. From `RooFitResult`: `status()`, `covQual()`, `edm()`, `numInvalidNLL()`, `numStatusHistory()` with `statusLabelHistory(i)` / `statusCodeHistory(i)`
   (print as `MINIMIZE=-1 HESSE=3`).
2. Parameters at a limit: for each `floatParsFinal()` `RooRealVar`, `min(val-min, max-val)/(max-min) < 1e-3` = at limit; `val -+ err` outside
   `[min,max]` = near limit (the same two tests xRooFit's boundary check uses).
3. Correlation structure: `correlationMatrix()`; global correlation `sqrt(1 - 1/(C^-1)_ii)`; smallest eigenvalue of C (`TMatrixDSymEigen`).
   Flags used: max global correlation > 0.99, min eigenvalue < 0.01.
4. Goodness of fit on the binned data: `model->createHistogram("e", obs, Binning(nb, lo, hi))` normalised to `model->expectedEvents(obs)`;
   Baker-Cousins deviance 2*sum(mu - n + n ln(n/mu)), Pearson chi2, pulls (n-mu)/sqrt(mu), number of |pull| > 3. Cross-checked against
   `RooPlot::chiSquare` (frame with `dataHist.plotOn` + `model->plotOn`): e.g. 61.69 vs dev/ndf 60.89 for cell (0,4); 4120 vs 4112 for cell (1,12).
5. Restart test: `fitTo` again from the final values; report max parameter shift in units of the original error and the `nsig` shift in sigma.
   Rationale [inferred, consistent with data]: the edm is the estimated vertical NLL distance to the minimum, so the worst-case shift is
   about sqrt(2*edm) sigma; cells with edm 0.4-0.8 restarted with nsig shifts of 0.03-0.2 sigma.
6. Multi-start test: `parameters->assign(fr.randomizePars())` then `fitTo`, 5 times; report min/max of `NLL_new - NLL_orig` and the max `nsig` shift.
   A negative value below -0.5 means a better minimum exists. (Starting values from `randomizePars` can leave the allowed region; the minimiser
   then prints RooFit evaluation-error logs. They are harmless but loud. [verified: log noise])
7. `createProfile` (profile likelihood of `nsig`) and `RooMCStudy` toys were run in the second pass on one converged real cell: see (e). The profile is a robust way to get an asymmetric interval when MINOS fails; the toys give calibrated pulls but silently omit non-converged fits.

Flag rules and what they caught, on 30 real k0s cells (mult 0..9 x source pT bins 4, 12, 24; default `fitConfig.json`; all 30 had >1000 entries) [verified]:

| criterion | cells flagged (of 30) |
|---|---|
| `status != 0` | 29 |
| `covQual < 3` | 8 |
| `numInvalidNLL > 0` | 9 (nine of the ten pT-4 cells, which use the `Chebychev2` override; 1 to 44 invalid evaluations) |
| `edm > 0.1` / `> 1` / `> 100` | 27 / 17 / 15 |
| parameter at limit | 5 (`c1_bkg` in (0,12),(7,12),(9,12); `alpha2_sig` in (1,24); `n2_sig` in (9,24)) |
| value +- error beyond range | 12 |
| max global corr > 0.99 or min eigenvalue < 0.01 | 9 |
| deviance/ndf | 5.6 to 5056; 29/30 above 10 |
| restart `nsig` shift > 0.1 / > 0.5 sigma | 22 / 17 |
| random restart finds NLL lower by > 0.5 | 20 |

Taking "bad" = restart `nsig` shift >= 0.5 sigma or a random restart finds NLL lower by > 0.5 (20 cells), "good" = the other 10:
`status != 0` flags 20/20 bad but also 9/10 good; `edm > 1` flags 17/20 bad and 0/10 good; `covQual < 3` 8/20, 0/10; at-limit 3/20, 2/10;
`edm > 1 or at-limit or covQual < 3` flags 19/20 bad and 2/10 good. One counter-example: cell (1,24) has edm 0.019 and covQual 3 yet a restart
moved `nsig` by 65 sigma and lowered the NLL by 2.2e4, with `alpha2_sig` at its limit, so edm alone is not sufficient.
Caveats: (a) these are source-bin cells, not the merged analysis bins, and the pT-4/5 overrides in `fitConfig.json` are keyed by the analysis index,
as the project notes themselves warn [source: DESIGN_NOTES.md section 3]; (b) the deviance/ndf of 5 to 5000 shows the shapes do not describe
10^6-10^7-entry cells, so chi2/ndf must not be a pass/fail criterion at this statistics; (c) "good/bad" is defined from my own restart tests, so
the table measures agreement among diagnostics, not truth.

## Experiments

### (a) Workspace round trip: done, see "Persistence behaviour". Cold refit from `prefit`: 26.6 ms, status 3, covQual 3, edm 0.121181, NLL -473250930.276 (stored identical); `isIdentical` true at tol 1e-12, 1e-9, 1e-6, 1e-4.
Warm refit from the `postfit` snapshot: 1.3 ms, status 3, edm 0.0664; NOT identical (c1_bkg relative shift 2.0e-4, n2_sig 2.2e-4, nsig 1.2e-6; `isIdentical(1e-6)`
false, and it also reports initial parameters differ). So "reproduce" holds from the same starting point, not from the fitted point, because the fit was
not fully converged (status 3). Stored objects: model (post-fit values), `RooDataHist`, `RooFitResult`, `prefit` and `postfit` snapshots, three named sets, TObjString, TVectorD.

### (b) Audit over 30 cells: done, see table above. Per-cell lines are in `batch.log` (scratch).

### (c) xRooFit summary: done for one fit, plus a comparison over 30 cells (`variants.cpp`), common NLL = `model->createNLL(dh, Extended(true))->getVal()` evaluated at each variant's final values,
all variants started from the same prefit values [verified]:

| variant | status 0 | covQual 3 | NLL within 0.5 of best | nsig within 0.1 sigma of best | median edm | total time (30 cells) |
|---|---|---|---|---|---|---|
| V0 project default (`Optimize(1), Extended, Save`) | 1/30 | 22 | 6 | 3 | 173 | 0.25 s |
| V1 + `Offset(true)` | 10/30 | 24 | 19 | 12 | 0.10 | 0.20 s |
| V2 + `Strategy(2)` | 1/30 | 20 | 6 | 1 | 2.3 | 0.29 s |
| V3 + `Offset(true)` + `Strategy(2)` | 6/30 | 16 | 13 | 8 | 9522 | 0.26 s |
| V4 xRooFit `createNLL(...).minimize()` with `BoundaryCheck 0.01` | 28/30 | 29 | 26 | 26 | 7.9e-6 | 0.70 s |

"best" = lowest NLL among the five variants for that cell (so it is not a proven global minimum). V4 failures: cell (7,24) status -1, cell (8,12) status 9000 (parameter
at a boundary), cells (2,4) and (5,24) converged but 4.7 and 9.3 above the best NLL. Timing is a few ms per fit in all cases. The warning "post-Hesse edm greater than allowed by tolerance"
was printed for a few cells. [source for the offset advice: comment in `xRooFit.cxx`: "if fits are failing because of edm over max, it can be a good idea to activate the Offset option when building nll"]

### (d) HS3 / JSON export of the fitted workspace [verified, this pass; program `study060708/hs3.cpp`, files `hs3-*.json`]
Input: the real-cell workspace written in the first pass (k0s mult 3, pT 10, `ws_3_10.root`: `model` = RooAddPdf(nsig * DSCB_sig + nbkg * Chebychev1_bkg), `dataHist` (200 bins), snapshots `prefit` and `postfit`, `RooFitResult`, `TObjString`, `TVectorD`, named sets, the observable `mass` with its named range `signalRegion` [0.475,0.515]). Tool: `RooJSONFactoryWSTool(ws).exportJSON(file)` / `importJSON(file)` (libRooFitHS3, nlohmann backend).
1. Export as it comes: `RooChebychev` has no export keys. ROOT prints a long explanation to stderr (`unable to export class 'RooChebychev' - no export keys available!`, three possible reasons, a pointer to the HS3 developer page), writes the distribution with NO `type` (an empty stub, `Chebychev1_bkg:?`), and `exportJSON` still returns true (file 3516 bytes). The import of this file would fail (note 02).
2. Proxy-based registration WITHOUT C++ (the mechanism of `roofit_hs3.md`, summarised in note 08 section 5.3), two one-line JSON files and two calls: `chebkeys.json` = `{"RooChebychev": {"type": "chebychev_dist", "proxies": {"x": "x", "coefList": "coefficients"}}}`, `chebexpr.json` = `{"chebychev_dist": {"class": "RooChebychev", "arguments": ["x", "coefficients"]}}`, then `RooFit::JSONIO::loadExportKeys("chebkeys.json")` and `RooFit::JSONIO::loadFactoryExpressions("chebexpr.json")`. (`coefList` is the proxy name of `RooChebychev::_coefList`, RooChebychev.cxx:48; `chebychev_dist` with `x` and `coefficients` is the name in the HS3 standard, note 02.) Export: 3739 bytes, valid, with `Chebychev1_bkg` as `{"coefficients": ["c1_bkg"], "type": "chebychev_dist", "x": "mass"}`.
3. Content of the file [verified by reading the JSON]: `distributions` = `model` as `mixture_dist` (`summands` [DSCB_sig, Chebychev1_bkg], `coefficients` [nsig, nbkg], `extended: true`), `DSCB_sig` as `crystalball_doublesided_dist` (`alpha_L`, `alpha_R`, `m`, `m0`, `n_L`, `n_R`, `sigma_L`, `sigma_R`: the project's single `sigma_sig` is written as both `sigma_L` and `sigma_R`), the Chebychev; `data` = the histogram as `binned` data (200 bins, edges 0.45 to 0.55, `contents`); `domains` = the parameter ranges as `default_domain` (all 10 variables with min and max) AND the named range `signalRegion` as a second `product_domain` (mass 0.475 to 0.515), so the signal window survives; `parameter_points` = THREE entries `default_values`, `prefit`, `postfit` (name and value only, no errors, no constant flag); `metadata` = `hs3_version 0.2` and the ROOT version.
   Not in the JSON: the `RooFitResult` (covariance matrix, edm, status, minNll), the named sets, the generic objects (`TObjString`, `TVectorD`), the parameter errors, the constant flags, and the choice of evaluating the pdf at bin centres or integrated over bins (GitHub #22598, open, note 08 section 9.5). Size: 3.7 KB JSON (4.2 KB when reserialised) against 18.4 KB for the ROOT workspace file.
4. Import into an empty `RooWorkspace`: `importJSON` succeeded; the new workspace contains 3 pdfs, 10 variables, 1 dataset and the three parameter snapshots; the NLL of the imported model on the imported data at the imported values is -473250930.276474, identical to the NLL of the original workspace at its stored values (same 15 digits); all 10 variables have identical value, min, max and constant flag (0 differences). A refit of the imported model (`fitTo`, extended): status 3, covQual 3, minNll -473250930.315883 (the original fit's -473250930.276 was 0.04 above, i.e. the same not-quite-converged behaviour). So the HS3 round trip of the project's K0S model is lossless for the model, data and ranges (with the two custom registration files for Chebychev).
5. Pitfall found and reproduced (GitHub #23221, opened 2026-09-01, note 08 section 9.9): while a `createIntegral` object of the pdf is alive the exporter finds no top-level pdf and writes a document without `distributions` and `domains` (1559 bytes, `parameter_points` empty) yet returns true; with the pdf freshly loaded, after `getVal(normSet)`, or after the integral object is destroyed the export is complete (3739 bytes). Export BEFORE building integrals or after destroying them, and check that the file contains `distributions`. [third pass, note 02 section 10.2, verified on the real phi cell] The trigger is narrower: only a live `createIntegral` of the TOP-level pdf empties the export (355-433 bytes, 0 distributions); an integral of a component pdf, which is what the project's `ExtractYieldsAndPurity` creates, does not. Copying the model into a temporary workspace with `RooWorkspace::import(model)` and exporting that gives the complete document even with the top-pdf integral alive.
6. What it means for the project [inferred]: an HS3 file can carry the model, data, ranges and the pre/post-fit values of one cell (~4 KB) and can be read by other frameworks, but not the fit quality (no covariance/edm/status) nor the extraction settings; for archiving a fit the ROOT workspace with the stored `RooFitResult` (section Persistence) remains the complete record, and the JSON is a model description. One JSON file per cell would be about 4 MB for 960 cells [inferred from 4 KB each].

### (e) Profile likelihood and RooMCStudy on one real cell [verified, this pass; program `study060708/prof.cpp`, output `prof-0-33.out`]
Cell: real k0s, mult 0, pT index 33 (N = 107148 entries, 200 bins, DSCB + Chebychev), chosen because its default `DoFit()` ends with status 0 (covQual 3, edm 2.3e-3, nsig = 91869.6 +- 455.5 Hesse). Statistics are modest so the toys are fast.
- `createProfile` of `nsig` (minimising the NLL over the 8 other floating parameters at each point): 2 x (profile NLL - minimum), taking the value at the Hesse optimum as zero, at nsig = nsig0 + k * sigma_Hesse: k=-3 8.09, -2 3.74, -1 1.01, 0 0.00, +1 0.95, +2 3.66, +3 7.13 (parabola would give 9, 4, 1, 0, 1, 4, 9). So the profile interval is [-0.997, +1.03] sigma_Hesse, i.e. about [-454, +467] events, against Hesse +-455.5: the Hesse error is accurate to about 3% here and the profile is slightly asymmetric. The profile minimum lies 0.009 in NLL below the NLL where MIGRAD+HESSE stopped in the same program (edm 2.3e-3), consistent with an incompletely converged minimum.
- MINOS on `nsig` via `RooMinimizer::minos` returned -1 (failed): "MnMinos could not find Lower Value for Parameter nsig" (the constrained re-minimisations inside MINOS did not produce a crossing of the 0.5 level; they start from EDM 0.013 and 0.034), the upper error was found, +466.4 (vs about +467 from the profile, Hesse 458.6). `fitTo(..., Minos(RooArgSet(nsig)))` reported the same symmetric numbers with status -1. So MINOS is unreliable on this model with the project's options; the `createProfile` curve is the more robust way to get an asymmetric interval [verified for this one cell].
- `RooMCStudy` (`Binned(true)`, `Extended(true)`, `FitOptions(Save(true), PrintLevel(-1))`, 300 toys generated from the fitted model): 276 of 300 toys have status 0 and enter `fitParDataSet` (23 toys status 3, 1 toy status >= 5; covQual 3 for 291, 2 for 8, 0 for 1). On the 276: nsig pull mean +0.059 (statistical error on the mean 0.059), pull RMS 0.983: the Hesse error is calibrated for this cell and the fit is unbiased at this statistics. The mean toy error of nsig is 504 against 455.5 for the data fit, 10% larger, not explained [the toys' shape parameters float freely and fluctuate; not investigated]. Note that `RooMCStudy` silently keeps only the status-0 fits in `fitParDataSet`, so 8% of the toys are missing from the pull and a status-3-heavy study would be biased by that selection (the status of every toy is available through `RooMCStudy::fitResult(k)` because `Save` was passed) [verified].
- Runtime: the whole program (profile, MINOS attempt, 300 toys) took about 1 s of wall time.
- The same on high-statistics status-3 cells: done in the third pass, section (h) (the toys there are indeed almost all status 3 with the project's options).

### (f) Cross-ROOT-version load test: NOT POSSIBLE here [verified]
Installed ROOT builds: `~/alice/sw/osx_arm64/ROOT/{v6-36-04-alice9-local1,-local2,-local3, v6-36-10-alice4-local1}` (plus symlinks `latest*`); no Homebrew ROOT, no PyROOT outside the alice stack. With the manual environment (`ROOTSYS`, `PATH`, `DYLD_LIBRARY_PATH` set by hand, no `alienv`), `root -l -b -q` of 6.36.04 (local1, local2 and local3) fails at start-up: `std_darwin.modulemap:458:10: error: header 'locale.h' not found` (local2, local3; local1 fails with `__type_traits/add_lvalue_reference.h not found`), and a compiled program linked against 6.36.04 (`study060708/rdv.cpp`) fails in the same way at cling initialisation, before opening any file. The brief states that 6.36.04 from the alice stack does not start on this SDK because of a modulemap error, which is what I saw; whether a full `alienv` module environment would change that was not tried because `alienv` was off limits (concurrent calls collide) [inferred: the first pass found that a manual environment is also insufficient for 6.36.10 itself]. Therefore the claim "a 6.36.10 workspace loads in another ROOT version" remains untested. Evidence from the community instead [doc]: GitHub #17163 (opened 2024-12-02, closed 2025-01-24): a workspace written with 6.26.08 and read with 6.32.02 printed `TStreamerInfo` checksum and layout warnings for `RooCrystalBall` (members `alphaR_`, `nR_` changed from `RooTemplateProxy` to `unique_ptr<RooTemplateProxy>`), the reporter suspected a (slightly) wrong CDF, the developer answered that the class layout had not changed since 6.24 and asked for a reproducer; the item is in the 6.36.00 notes. HS3 JSON is the version-independent route (the `hs3_version` field is recorded; ROOT-specific JSON features are not guaranteed across versions).

### (g) Web research for this note [doc, GitHub via `gh` and forum]
Issues that touch workspaces and HS3, with dates (details in note 08 section 9.9 and 4b): #23221 empty HS3 export while an integral object is alive (reproduced above); #22497 round-trip import of a large exported workspace aborts with `DependencyMissingError` (2026-06-05, closed next day, master); #22598 evaluation semantics of binned data against a continuous pdf are not recoverable from the HS3 file (open); #21371 `RooFormulaVar` `@N` indices shift after serialisation when a parameter is unused (opened 2026-02-24, closed 2026-07-25), relevant to `RooGenericPdf` backgrounds only if their formulas use `@N`; #17291 `RooFormulaArgStreamer` parameter ordering (6.34.04, fixed in 6.36.00); #19770 inverted return values of `RooWorkspace::import` (documented, unchanged in 6.38/6.40; `writeToFile` fixed); #20904 `RooWorkspace::data` returns null with 52 or more datasets (6.38 only; [verified] absent in 6.36.10 with up to 300 datasets, `study060708/wsn.cpp`). The xRooFit tutorials were not read beyond the first pass.

### (h) Profile likelihood, MINOS and RooMCStudy on HIGH-statistics status-3 cells [verified, third pass; program `scratch/study060708/prof3.cpp`, outputs `prof3-2-7-*.out`, `prof3-1-8-*.out`]
Cells: K0S analysis bins of the 2026 file `AnalysisResults_136_2026_PtMod_6agosto.root` (the real purity input), pT bins merged exactly as `PurityTask` does (`BinningUtils::MapToSourceBins`, analysis edges 0, 0.3, 0.5, 0.8, 1.2, 1.6, 2.0, 2.4, 3.0, 4.2, 6.4), configuration `GetConfig("k0s", mult, ptIndex)`. Both are flagged in note 09 (`cells-master.tsv`, class B "not converged"): (mult 2, pT 7) with 3.54e7 entries, project status 3, edm 5.1, NLL 4.98 above the best minimum; (mult 1, pT 8) with 2.32e7 entries, status 3, edm 3.7e4, NLL 9.7e4 above the best minimum. Steps: (A) the project's `DoFit()`; (B) two `fitTo(..., Offset("bin"))` refits from there (the note-09 recipe) give the best point; (C) `createProfile(nsig)` on the plain NLL (as the project's) and on an `Offset("bin")` NLL, values 2 x (profile - profile at the best point) at nsig_best + k x sigma_Hesse; (D) `RooMinimizer::minos(nsig)` on both NLLs; (E) `RooMCStudy` with `Binned(true)`, `Extended(true)`, toys generated from the best fit, fitted (i) with the project's options and (ii) with `Offset("bin")`, (iii) with `Offset("bin")` + `IntegrateBins(1e-6)`.

| | (2,7) | (1,8) |
|---|---|---|
| project `DoFit` | status 3, covQual 3, edm 5.1, nsig 24 231 246 +- 7031 | status 3, covQual 3, edm 3.7e4, nsig 15 825 531 +- 4390 |
| Offset("bin") refit x2 | status 0, edm 6.5e-8, nsig 24 243 094 +- 7032; NLL 4.98 lower | status 0, edm 2.3e-5, nsig 16 400 372 +- 5968; NLL 97 327 lower |
| profile, plain NLL, k = -2, -1, +1, +2 | 5.82, 1.36, 1.46, 5.67 | 5.93, 1.01, 1.50, 5.77 |
| profile, Offset("bin") NLL | 4.00, 0.999, 0.997, 3.98 | 4.01, 1.004, 0.998, 3.98 |
| MINOS, plain NLL | FAILED (returned -1; errors = Hesse) | +5980 / -5962 (Hesse 5965) |
| MINOS, Offset("bin") NLL | +7044 / -7034 (Hesse 7037) | +5975 / -5967 (Hesse 5963) |

Reading [verified for these two cells]: (1) at the true minimum the Hesse error of nsig matches the profile likelihood to better than 0.3% (with the offset NLL the profile is a parabola: 0.999, 0.997, 4.00, 3.98 against 1, 1, 4, 4) and MINOS agrees (asymmetry 0.1%). (2) On the PLAIN NLL (the project's) the profile is distorted by up to 50% at 1 sigma (1.36 and 1.46 in (2,7); 1.01 and 1.50 in (1,8)) (each profile point is itself a minimisation that stops early, as the main fit does) and MINOS fails in one cell: the profile/MINOS tools inherit the round-off problem of note 08 section 9.1, and work once the NLL is offset. (3) The project's own result in (1,8) is 3.6% below the best nsig (5.75e5 events = 96 sigma) and its Hesse error 26% too small: in a non-converged cell neither the value nor the error can be used, which no profile or MINOS run fixes unless the fit is first brought to the minimum.

RooMCStudy (toys start from the generating values, so they test the minimiser near the truth, not the project's start values):
| toys | fit options | status counts | nsig pull, all toys | pull, status 0 only | `fitParDataSet` entries |
|---|---|---|---|---|---|
| (2,7), 200 | project (no offset) | 3: 199, 0: 1 | mean -1.32 +- 0.08, RMS 1.20 | (1 toy) | 1 |
| (2,7), 200 | Offset("bin") | 0: 200 | mean -1.23 +- 0.07, RMS 1.01 | same | 200 |
| (2,7), 200 | Offset("bin") + IntegrateBins | 0: 190, 1: 8, 3: 2 | mean +3.0 +- 2.2, RMS 31.7 | mean +0.16 +- 0.07, RMS 1.00 | 190 |
| (1,8), 100 | project | 3: 100 | mean -0.53 +- 0.11, RMS 1.08 | (none) | 0 |
| (1,8), 100 | Offset("bin") | 0: 100 | mean -0.21 +- 0.09, RMS 0.90 | same | 100 |
Findings: (a) with the project's options virtually EVERY toy ends with status 3 at this statistics even when started at the truth (199/200 and 100/100), while `Offset("bin")` gives status 0 in all of them: the status-3 rate is a property of the plain-NLL minimisation at |NLL| ~ 1e8, not of the data. (b) `RooMCStudy::fitParDataSet()` keeps only status-0 fits, so with the project's options it contains 1 and 0 entries: anyone reading pulls from `fitParDataSet` gets nothing (or one toy). The pulls must be taken from `fitResult(k)` (needs `FitOptions(Save(true))`). (c) The convergence noise inflates the pull RMS (1.20 against 1.01 with the offset) but shifts the mean little when starting at the truth. (d) The remaining -1.2 sigma (2,7) and -0.2 sigma (1,8) bias of the converged fits is the bin-centre evaluation (the toys are generated by sampling, the fits evaluate at bin centres); with `IntegrateBins` the status-0 pulls are +0.16 +- 0.07, RMS 1.00. (e) Dropping non-zero-status toys matters when they are outliers: with IntegrateBins the 10 non-zero-status toys have pulls up to O(100) and change the mean from +0.16 to +3.0. (f) PITFALL found on the way: `RooMCStudy` with `Binned(true)` generates histograms with the OBSERVABLE's binning, which is 100 bins in the project (note 04) while the data have 200: with 100-bin toys the converged-fit pull mean was +6.5 sigma (20 toys, RMS 1.0), a pure bin-width artefact. Call `obs->setBins(nbins_of_data)` before building the `RooMCStudy`.

### (i) Per-cell workspace persistence cost [verified, third pass; `scratch/study060708/wsio.cpp`, files `scratch/study060708/wsio/ws{A,B,C}.root`; sizes exact, times relative only]
30 K0S cells of the 23luglio file (mult 0-9 x pT index 4, 12, 24), each fitted with `DoFit()`, stored as A) a full workspace (model at post-fit values, `RooDataHist` 200 bins, `RooFitResult`, `prefit`/`postfit` snapshots), B) the `RooFitResult` alone, C) the workspace without the data.
| file | size | keys | on disk per cell (compressed, incl. key header) | uncompressed per cell | compression | StreamerInfo record |
|---|---|---|---|---|---|---|
| A full workspaces | 214 326 B | 30 workspaces + 1 `TProcessID` (155 B) | 6.5-7.0 KB (mean 6.73 KB) | 32.5-35.0 KB | 0.20 | 10 471 B, 49 classes |
| B RooFitResult only | 79 350 B | 30 + 1 | 2.33 KB | 9.9 KB | 0.23 | 6 113 B, 28 classes |
| C workspace without data | 130 964 B | 30 + 1 | 4.0 KB | 26 KB | 0.15 | 8 572 B, 41 classes |
- The data (200-bin `RooDataHist`) cost 2.7 KB per cell compressed (A - C), the fit result 2.3 KB, the model and snapshots about 1.7 KB. Cells with Chebychev2 (pT index 4) are 2.5 KB bigger uncompressed. The StreamerInfo (about 10 KB) and the file header/key list (about 2 KB) are paid once per FILE, which is why the single-cell file of (a) was 18 KB. So 960 cells in one file would be about 6.5 MB with data, 3.9 MB without data, 2.3 MB for fit results only [inferred by scaling].
- Reading back all 31 objects (open + `ReadObj` + `delete`, one fresh process each): full workspaces 0.19-0.22 s for 30 (about 6-7 ms each; 0.17 s without data), fit results 0.016 s for 30 (0.5 ms each), file open about 0.04 s. Relative: a workspace costs about 12 times a bare `RooFitResult` to read back. Timing is NOT a conclusion here (shared machine); the coordinator's quiet-machine pass owns absolute numbers.

### (j) O2/ALICE-specific considerations; TUnfold/RooUnfold [source, read-only search of `/Users/scannito/alice/O2Physics` and `/Users/scannito/alice/O2`; verified file listings]
- O2Physics compiled code does NOT use RooFit: no `CMakeLists.txt` of O2Physics or O2 links RooFit, and the only files that include a `Roo*` header are standalone macros: `PWGHF/D2H/Macros/HFInvMassFitter.{h,cxx}` (built separately with its own `CMakeLists_HFInvMassFitter.txt`, `find_package(ROOT REQUIRED COMPONENTS RooFit RooFitCore)`, executable `runMassFitter`) and `PWGDQ/Macros/fit_library/*Pdf.{h,cxx}` (custom `RooAbsPdf` classes) plus `PWGDQ/Macros/DQFitter.py` (PyROOT, `RooWorkspace.factory`). In O2 no RooFit include was found. So RooFit is a post-processing tool in ALICE Run 3, as in this project; there is no O2 rule that forbids it, but nothing in the O2 framework (tasks, DPL, AOD tables) can read or write RooFit objects [inferred from the absence of any use].
- CCDB: no RooFit object is uploaded to or read from CCDB anywhere in O2Physics; the CCDB uploads found in macros store histograms/calibration objects (e.g. `PWGDQ/Macros/dqFlowAccWeights.C` uploads acceptance weights with `o2::ccdb::CcdbApi`). A `RooWorkspace` in CCDB would need the RooFit dictionaries in every consumer (O2Physics tasks do not link RooFit) [inferred]. For this project's outputs (purity histograms consumed by later steps) the conclusion is: keep the outputs as plain ROOT histograms/TGraphs as now; RooFit objects (workspaces, fit results, HS3 JSON) are an archive for the analyst, not something to feed back into O2 [inferred].
- How the PWGHF fitter does it (for comparison, [source] `HFInvMassFitter.cxx:200-356`): one `RooWorkspace` per fit holding all candidate pdfs (`fillWorkspace`), named ranges for sidebands and signal, `fitTo(..., Range("full"), Save())` or `chi2FitTo`, sideband pre-fit with `chi2FitTo(..., DataError(RooAbsData::SumW2))`, the signal yield from `createIntegral(*mass, NormSet(*mass), Range("signal"))`, and it records `status()`, `covQual()`, `edm()`, `minNll()` and the global correlation of the signal yield for every fit. Starting values are randomised within ranges (`randomizeInitialParameter`). No `Offset`, `Strategy` or `IntegrateBins` is used there either.
- RooUnfold/TUnfold: this ROOT has TUnfold (`root-config --features` lists `unfold`; `libUnfold.so`, headers `TUnfold.h`, `TUnfoldDensity.h`, `TUnfoldBinning.h`, `TUnfoldIterativeEM.h`) [verified]. RooUnfold is NOT part of ROOT and is not built in the local alice stack (no `RooUnfold` under `~/alice/sw/osx_arm64`), but alidist has a recipe `roounfold.sh` (package RooUnfold, tag `V02-00-01-alice6`, source https://github.com/alisw/RooUnfold, requires ROOT and boost) used by the `aliphysics` and `focal` recipes, not by O2Physics [source]. Neither is relevant to fitting the purity [inferred]; this closes the "RooUnfold-like" row of the tools table.

### (k) HistFactory, RooStats and xRooFit tools beyond the headings: what bears on the project [source/doc read in SRC; one verified test]
- xRooFit has no tutorials in `tutorials/` in 6.36 (only `roofit/xroofit/test/xroofit_python.py`); the API is documented in its headers. `xRooFit::createNLLOptions()` defaults [source, xRooFit.cxx:474-484]: `Offset()` (= Offset(true)) and `Optimize(0)`. `xRooFit::defaultFitConfig()` [xRooFit.cxx:496-535]: Minuit2, `ErrorDef 0.5`, HESSE on, MINOS for parameters carrying the attribute `minos`, `StrategySequence "0s01s12s2s3m"` (strategy 0, then a Scan "s" and strategy 0 again, then 1, scan, 1, 2, scan, 2, 3, and finally old Minuit "migradImproved" ("m")), `HesseStrategySequence "23"`, `BoundaryCheck 0` (set e.g. 0.01 to warn when a fitted value is within 1% of a limit), `LogSize 0`, `OptimizeConst 2`; options for `xRooFit::createNLL` include `xRooFit::Tolerance(v)`, `StrategySequence("...")`, `MaxIterations(n)`, `ReuseNLL(bool)`. In `minimize()` a status other than 0 moves on to the next letter of the sequence, up to 4 tries [source xRooFit.cxx:1026-1050]. This is the ladder that reached status 0 in 28/30 cells in (c) and is the escalation step of note 09.
- xRooFit goodness of fit [source xRooNLLVar.cxx:873-930]: `pgof() = TMath::Prob(2(NLL - saturatedVal()), ndof())` with `ndof() = data()->numEntries() - floating parameters`, and `mainTermPgof()`; the code warns that the NLL must be created with an explicit `Binned(...)` option for the saturated term to be consistent. [verified, `scratch/study060708/gof.cpp`] On the project's model (continuous `RooAddPdf` on a `RooDataHist`) it is NOT usable: for three real K0S cells xRooFit's minimisation converged (status 0) but 2 x (NLL - saturated) came out as -1.6e6, -3.6e8 and -5.2e6 and `pgof()` = 0, because the project's NLL is the density-at-bin-centre form, not a binned Poisson term. A hand-made Poisson deviance at the same point gives 259.5 (N=1.07e5), 48 440 (N=2.4e7) and 372 (N=3.4e5) for 200 bins - 9 parameters = 191 d.o.f., i.e. deviance/ndf 1.36, 254 and 1.95 (Pearson chi2 within 1% of the deviance). This is the statistic the RooFit developer now recommends (GitHub #21697: "what we want is the Poisson deviance", `RooPlot::chiSquare` to be deprecated; note 08 section 9.6). Note the huge deviance/ndf for the 2.4e7-entry cell: at that statistics the DSCB + Chebychev1 model does not describe the data within statistical errors (note 05 territory; not followed up here).
- RooStats tutorials [doc, headers and code]: `rs301_splot` (sPlot: unfold a control-variable distribution with weights from a discriminating-variable fit; relevant only if a background-subtracted distribution of another variable were wanted), `StandardProfileLikelihoodDemo` (the `ProfileLikelihoodCalculator` "based on Wilks's theorem", about 10 lines with a `ModelConfig`: the same profile as (e)/(h) with interval bookkeeping), the HypoTest/limit demos (not relevant). HistFactory `hf001_example` (run in note 08): template models with `RooRealSumPdf` and the `BinnedLikelihood` attribute, which integrate per bin (note 06), the opposite of the project's continuous shapes; not relevant unless templates replaced the analytic shapes.

### (l) A helper for one cell's HS3 export: register Chebychev and Voigtian, export, re-import, check [verified, final pass; `scratch/study-final-010708/hs3/ProjectHS3.h`, test `hs3cell.cpp`, outputs `out-hs3cell.txt`, `out-hs3cell-steps.txt`, files `k0s-0-2.json`, `phi-0-2.json`]
The helper is kept in scratch (it is not part of the project). `ProjectHS3.h` (about 90 lines, header-only) has four functions:
- `RegisterTypes()` registers `RooChebychev` (`chebychev_dist`) and `RooVoigtian` (`voigtian_dist`) through `loadExportKeys` / `loadFactoryExpressions`, from in-memory strings (the proxy route of note 02, 5.1). It runs once per process (`std::call_once`).
- `ExportCell(model, data)` copies the model and the data into a private `RooWorkspace`, saves the current parameter values as the snapshot `postfit`, and calls `exportJSONtoString()`. It then checks the two silent failures that `exportJSON` reports as success: the document must contain `distributions` and the top-level model, and it must re-import into a probe workspace without an exception. A typeless stub of an unregistered class fails that last check. `ExportCellToFile` writes the string to a file.
- `ImportCell(file)` imports into a new workspace and loads `postfit`.

Test: `hs3cell <section> <h3path> <mult> <pt> <out.json>` fits one real cell with the project's `DynamicRooFitter` (`DoFit` and `ExtractYieldsAndPurity`, through a copy of `ws-topic/common.h`). It then keeps a `createIntegral` of the top-level model alive on purpose (the #23221 trigger), exports, re-imports and compares. Data: `AnalysisResults_136_2026_PtMod_23luglio.root`.

| cell | model | fit (project options) | JSON | NLL original / imported | parameters (value, range, const) | window S original / imported |
|---|---|---|---|---|---|---|
| k0s (0,2), 8.99e6 entries | RooCrystalBall + RooChebychev | status 3, covQual 3 | 3284 B | -121421924.3445870131 / identical (diff 0) | 9 compared, 0 differ | 6136909.081475 / identical |
| phi (0,2), 5.91e6 entries | RooVoigtian + RooGenericPdf (BkgSourav2) | status 3, covQual 3 | 2770 B | -35461380.7297007963 / identical (diff 0) | 8 compared, 0 differ | 343981.253744 / identical |

- The live top-level integral did not empty the export, because the model is copied into a private workspace first (the remedy of note 02, 10.2). The named range `signalRegion` survives ([0.475,0.515] and [1,1.05]).
- The observable title is lost: `M(#pi^{+}#pi^{-}) (GeV/c^{2})` becomes `mass`, as in note 02, 10.3. A reader must set it again from the config.
- **A refit from the re-imported model is not bit-identical unless the parameter errors are restored** [verified]. HS3 stores no errors, so all 9 (k0s) or 8 (phi) parameters come back with different errors. RooFit uses each error as MINUIT's initial step size, so the same refit (`fitTo` with the project's options, from the same post-fit values) takes a different path. On k0s it ended at minNll -121421924.9442 (original) against -121421924.9639 (imported), with nsig 6156490.7 against 6156532.4: 42 events (7e-6 relative) on a status-3 fit. With `STEPS=1` the program copies the errors over first, and both refits become identical to the last digit on both cells (k0s minNll -121421924.9442267418, nsig 6156490.707616; phi -35461380.7297007963, nsig 351815.772107). So the HS3 file reproduces the model's value exactly, but it does not reproduce a refit. Anyone who wants to repeat a fit from an HS3 file must also store the errors (or the `RooFitResult`, in the ROOT workspace) [verified; the dependence on step sizes is the reason, inferred from it vanishing when only the errors are copied].
- Cost: one export with its re-import check is a few ms for a cell [inferred from the run time of the whole program, which includes three fits; not timed separately; noisy machine].

## Recommendations (recommendations only)

- (third pass) For error checks on high-statistics cells build the NLL with `Offset("bin")`, bring the fit to status 0 first, then compare Hesse with `createProfile`/MINOS (section (h)); for toys set the observable binning to the data's and read every toy's status from `fitResult(k)`. Keep the project's purity outputs as plain ROOT objects; RooFit workspaces/JSON are an analyst archive only (section (j)).

- (second pass) For a JSON description of fitted cells use HS3 with a 2-file registration of `RooChebychev` (section (d)); export before creating integral objects (GitHub #23221) and check that `distributions` is non-empty; keep the `RooFitResult` in the ROOT workspace, it is not in the JSON. For an asymmetric or non-parabolic error use `createProfile` rather than MINOS (section (e)). When running toys with `RooMCStudy` read `fitResult(k)->status()` for every toy: the parameter dataset contains only status-0 fits.

1. RooWorkspace persistence is feasible and cheap, but the model need not be stored. [inferred from the measurements] The model is rebuilt from the JSON
   configuration in 0.1 ms (`t_build` 1e-4 s) and a fit takes ~5 ms, so what is expensive to recompute and not reproducible is the RESULT of a particular
   fit. Store per cell: the `RooFitResult` (2.6 KB), the post-fit and pre-fit parameter snapshots (so a refit starts from a defined point), the configuration needed
   for extraction (windows, sideband, role names of sig/bkg pdfs) and the histogram (or `RooDataHist`) that was fitted. Full workspace (with model and data) is
   7.3 KB per cell, about 7 MB per 1000 fits, enough to inspect fits interactively without the project code. [verified sizes, extrapolation inferred]
2. Decoupling extraction from the fit pays: re-extraction with a different integration window costs ~0.05 ms (0.22 ms for 3 windows) against 5-20 ms for a fit,
   and reproduces S, B, P and errors exactly from the stored `RooFitResult` [verified]. Use the same `getPropagatedError(fr, obs)` call.
3. Make the stored `statusHistory`, `covQual`, `edm`, `numInvalidNLL` and the audit flags part of the persisted record, since downstream code can only exclude a
   bad bin if it can see why it is bad. Do not rely on `status()`: it reads 3 for almost every fit.
4. Before building the quality monitoring, re-run the variant comparison on the REAL analysis bins and the real overrides: the measured gap between the
   project's `fitTo` call and an `Offset(true)` or xRooFit-ladder fit (1/30 vs 10/30 vs 28/30 converged) is large enough that the fit settings, not only the monitoring, are
   the issue. Cheapest candidate: add `RooFit::Offset(true)` to the existing call; heavier candidate: use `xRooFit::createNLL(...).minimize()` as the minimiser. Both
   are untested on the analysis bins. Hand this to the researcher on fit status.
5. If a quality flag is wanted, the tests that agreed with each other here were: edm > 1, `covQual < 3`, any parameter within 0.1% of a limit, and a one-shot restart
   from the result with `nsig` shift in sigma. Thresholds above come from 30 cells of the default config and should be recalibrated on the real bins.
6. Keep HistFactory and RooStats out of scope. Consider xRooFit only as a minimiser; do not adopt `xRooNode` for persistence (the plain RooWorkspace is enough).

## Pitfalls

- (third pass) `RooMCStudy` with `Binned(true)` generates with the observable's binning (100 bins in the project), not the data's (200): call `obs->setBins(nbins)` first, or the pulls carry a bin-width artefact (+6.5 sigma seen). Its `fitParDataSet()` contains only status-0 fits (1 of 200 toys with the project's options at 3.5e7 entries).
- (third pass) A profile likelihood or MINOS on the project's plain NLL at |NLL|~1e8 is distorted by the same round-off as the fit; use an `Offset("bin")` NLL for both.

- `RooWorkspace::import`, `defineSet` and friends return `true` on ERROR. `loadSnapshot` returns `false` when the snapshot is missing. [source]
- `import(RooAbsArg)` clones and imports CURRENT values; import the model after the fit if post-fit values are wanted, or save snapshots explicitly. A second import of an object with
  the same name fails. [source]
- `loadSnapshot` restores value, error and `Constant` attribute, not ranges. [source]
- `RooFitResult::isIdentical` also compares INITIAL parameters and the correlation matrix, so a warm refit never compares identical to the original. [source + verified]
- `status()` is the last step's code (HESSE); the failed minimisation (-1) appears only in `statusHistory`. [source + verified]
- Randomised restarts can step outside the valid region and print many evaluation-error blocks; set `RooMsgService` levels or `PrintEvalErrors(-1)` in an audit run. [verified: log noise]
- `alienv printenv O2Physics/latest-master-o2` was flaky during this pass: it returned `test 0 = 1;` or an empty string intermittently (six or seven retries, each a few seconds apart, were needed once).
  [verified] Probable cause: concurrent `alienv` calls by the other researchers [inferred, not tested]. Saving its output once to a file (`alien-env.sh`) and sourcing that avoided the problem.
  Setting only `ROOTSYS`, `PATH` and `DYLD_LIBRARY_PATH` by hand was NOT enough: the program crashed in `TStreamerInfo::CompareContent` while opening the data file, after many
  "Failed to load module" lines. [verified]
- Compile notes [verified]: `#define private public` for a scratch program needs every standard and project header included first (libc++ `<ranges>` breaks otherwise).
  `RooFit::Import` needs a non-const `TH1&`. `RooFit::OffsetLikelihood` does not exist in 6.36.10; use `RooFit::Offset(true)` (`RooGlobalFunc.h` lines 324-330).
  xRooFit needs `-lRooFitXRooFit -lRooStats -lHistFactory`.

## NOT DONE (summary)

This note is FINAL (final pass, 15 October 2026). Nothing is left NOT DONE. What follows is either impossible here or belongs to another note:
- NOT POSSIBLE here: the cross-version load test, i.e. reading a 6.36.10 workspace in another ROOT version and the reverse (section (f)). The only other installed ROOT, 6.36.04 (alice9 local1, local2, local3), does not start on this macOS SDK: cling stops at a modulemap error (`locale.h` or `__type_traits/add_lvalue_reference.h not found`) before any file is opened, both in `root` and in a compiled program. ROOT 6.38 and 6.40 are not installed, and `alienv` is off limits (parallel calls collide). The community evidence (#17163) is in (f); HS3 JSON is the version-independent route ((d), (l)).
- NOT POSSIBLE here: absolute read/write timings of the workspaces (section (i) gives relative numbers). Other jobs share the machine, so the briefs forbid conclusions from timings; this needs a quiet machine.
- MOVED to note 09: a systematic evaluation of the xRooFit variants on the real merged analysis bins (the fit-quality researcher owns it).
- MOVED to note 12: the toy study of (h) with the project's START values from the config instead of the truth (the bias of the project's fit including its start-value failures).
- Done: the HS3 export helper (l). Done in the third pass: (h) to (k). Done in note 02, sections 10.1-10.2: the HS3 export of the `RooGenericPdf` backgrounds and of the phi Voigtian, and the provenance record.

## Resume here

Final. Experiments (a) to (l) are in this note; the items that remain are impossible here or moved (see the list above). To rerun the helper: `cd scratch/study-final-010708/hs3; ../build.sh $PWD/hs3cell.cpp; source ../../rooenv.sh; ./hs3cell k0s k0s-reduced-cand-producer/k0sReducedCandidates/h3K0sCandidatesMass 0 2 k0s-0-2.json`. For phi, use `phi phi-strange-correlation/phiStrangenessCorrelation/phi/h3PhiData 0 2 phi-0-2.json`. Set `STEPS=1` to copy the parameter errors before the refit comparison. If a histogram path is not found, the program prints `MISSING histogram` and exits with code 2: ask the owner for the current O2Physics path, and do not guess it.
If a working 6.36.04, 6.38 or 6.40 environment is provided later, the cross-version test is to write `ws-topic/write.cpp` output in one version and read it with `ws-topic/read.cpp` in the other, one `alienv` call at a time.

Scratch files: `/Users/scannito/alice/Analysis/RooFitKnowledge/scratch/ws-topic/` (first pass: `common.h`, `write.cpp`, `read.cpp`, `batch.cpp`, `xrf.cpp`, `variants.cpp`, `build.sh`, `env.sh`), `scratch/study060708/` (second and third passes: `hs3.cpp`, `hs3b.cpp`, `chebkeys.json`, `chebexpr.json`, `hs3-*.json`, `prof.cpp`, `prof-0-33.out`, `prof3.cpp`, `prof3-*.out`, `wsio.cpp` + `wsio/`, `gof.cpp`, `rdv.cpp`, `wsn.cpp`), `scratch/study-final-010708/hs3/` (final pass: `ProjectHS3.h`, `hs3cell.cpp`, `common.h`, outputs). Build: `source scratch/rooenv.sh` then the clang++ line of the brief with `-I/Users/scannito/alice/Analysis/Codes` (add `-lRooFitXRooFit -lRooStats -lHistFactory` for `prof3`, `gof`; `-lRooFitHS3 -lRooFitJSONInterface` for the HS3 programs; `scratch/study-final-010708/build.sh` has all of them). No project file was modified.
