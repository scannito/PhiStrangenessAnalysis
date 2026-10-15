# RooFit evaluation backends, codegen, multiprocessing, performance (ROOT 6.36.10, ALICE build)

STATUS: second pass done 14 October 2026 (codegen with gradient, Clad, thread-safety test, numeric-integrator node, pi_tpc profile, bin-centre check, bounds scan). The unfinished items left are in "Resume here".
Provenance tags: [doc] documentation/comment in the source tree, [source] code read, [verified] something I ran, [inferred] my reasoning.
Source tree: `~/alice/sw/SOURCES/ROOT/v6-36-10-alice2/v6-36-10-alice2/` (called SRC below). Install: `~/alice/sw/osx_arm64/ROOT/v6-36-10-alice4-local1` (called ROOTSYS).
Caveat: the source tree is "alice2", the installed tag is "alice4" (`$ROOTSYS/etc/gitinfo.txt`: tags/v6-36-10-alice4). I did not diff them. [verified: gitinfo]

## Summary

- The default evaluation backend is `cpu` (Evaluator + RooBatchCompute + RooNLLVarNew). `Optimize(1)` is a no-op with it; `NumCPU` is ignored with it. [source] [doc]
- Parallel fits (`Parallelize`, `ParallelGradientOptions`, ...) throw here because the ROOT option `roofit_multiprocess` is OFF in this build (default OFF; alidist `root.sh` does not set it); neither RooFitZMQ nor RooFitMultiProcess libraries are installed. [source] [verified: library listing, earlier runtime throw reported by caller]
- `EvalBackend("codegen")` and `codegen_no_grad` both FAIL on the project's DSCB models (and `RooVoigtian` is not translated either): `RooCrystalBall` has no codegen translation and no codegen analytical integral; verified for both backends on a real cell. Clad DOES load in this build and the AD gradient is correct on supported models (Gauss/CBShape + Exp/Chebychev), but costs ~3 s for the first fit and ~20 ms per later fit for a 0.3 ms cpu fit. [verified]
- Threads: fitting different cells in different `std::thread`s of one process CRASHES or HANGS (2 threads: 6/6 crashed plus one hang; 4 threads: 6/6 crashed, also with `ROOT::EnableThreadSafety()`); no run produced a wrong number; one worker thread alone, or a mutex around each fit, is bit-identical to serial. Cause: unlocked global state (`RooLinkedList::_pool`). Use processes, not threads. [verified]
- The numeric-integrator INFO message does not occur for the real K0S, Xi and pion models at all (all analytic); it comes from the phi RooVoigtian signal and RooGenericPdf background only. [verified]
- Bin-centre versus integrated bins on Asimov data of the real K0S fits: bias of nsig at most 0.08% relative (-2.55 sigma at nsig=2.3e7, <0.5 sigma elsewhere), removed by `IntegrateBins`. [verified]
- Many parameters sit on their bounds (Xi sigma_sig on its lower bound in 237/260 cells; pi_tpc nbkg=0 in 165/360 cells) and status 3 becomes more frequent with statistics (k0s: 39% at 1e5-3e5 entries, 83% at 3e5-1e6, 100% above 1e6). [verified]
- (first-pass wording, superseded by the first bullet above) `EvalBackend("codegen_no_grad")` FAILS on the project's model: `RooCrystalBall` (the DSCB signal) has no codegen translation and no codegen analytical integral. `codegen` (with gradient) goes through the same translation step, so it must fail the same way. I did not run it. [verified for codegen_no_grad]
- Profile of `DoFit` on real data: about 98% of the time is the Minuit2 MIGRAD loop (about 1000 NLL evaluations of about 5 us each for k0s). Everything else (RooDataHist import, createNLL, RooMinimizer construction, save, teardown) is about 0.1 ms per fit, under 2%. HESSE is about 7%. [verified]
- Nothing in the backend or parallelism choice is worth optimising for about 1000 fits (about 2 to 7 s in total). The one real observation is about fit quality, not speed: almost every real k0s fit ends with Minuit status 3 ("Edm is above max") and some with EDM of order 1e10. See Profile and Pitfalls. [verified] [inferred]
- Note on timings: other researchers were running jobs on the same machine at the same time, so absolute times carry noise (my first-fit numbers were 17 to 40 ms, versus about 120 ms measured earlier by the caller).

## Architecture (cpu backend)

Flow of `RooAbsPdf::fitTo` in 6.36. [source: SRC/roofit/roofitcore/src/FitHelpers.cxx]
1. `FitHelpers::createNLL` parses the command arguments (RooCmdConfig). `EvalBackend` default = `RooFit::EvalBackend::defaultValue()` (cpu). (FitHelpers.cxx:620)
2. If the backend is not `Legacy` (FitHelpers.cxx:735 ff.):
   - `pdf.compileForNormSet(normSet, ctx)` clones the pdf and wraps it with a `RooNormalizedPdf` (pdf / normalisation integral) for the observables in the data (`RooAbsPdf::compileForNormSet`, RooAbsPdf.cxx:2786). The user's original pdf is not touched.
   - `createNLLNew` builds a `RooNLLVarNew` inside a `RooAddition` (FitHelpers.cxx:371-419). `fixAddCoefNormalization` is applied to the observables in the data.
   - The result is wrapped in a `RooEvaluatorWrapper`, which owns a `RooFit::Evaluator` (FitHelpers.cxx:797; RooEvaluatorWrapper.cxx:34).
3. `RooEvaluatorWrapper::setData` turns the RooDataHist into flat arrays (observable values and weights) with `BatchModeDataHelpers::getDataSpans`. Zero-weight entries are skipped unless the pdf has the attribute `BinnedLikelihoodActive` (RooEvaluatorWrapper.cxx:107-135). [source]
4. `Evaluator` (SRC/roofit/roofitcore/src/RooFit/Evaluator.cxx):
   - it sorts the computation graph topologically (`getSortedComputationGraph`) and stores one `NodeInfo` per node (servers, clients, buffer, dirty flag, output size);
   - it evaluates each node through `RooAbsReal::doEval(EvalContext&)`; nodes that are not scalar are put in `ADirty` mode, scalar nodes keep their original mode (Evaluator.cxx:~275-300);
   - the arrays go through `RooBatchCompute` (vectorised; the CPU library is compiled for several instruction sets and picked at runtime), and the NLL is reduced with Kahan sums. [doc: SRC/roofit/doc/developers/batchcompute.md] [source]
   - One Evaluator is created per `fitTo` and destroyed at its end. (Evaluator.cxx class comment) [doc]
5. Classes that have no `doEval` override fall back to `RooAbsReal::doEval`, which loops the scalar `evaluate()` over the batch (RooAbsReal.cxx:4303 ff.). So "unsupported by cpu" means "evaluated through the slower scalar loop", not an error. [source] [inferred for the performance consequence]
6. `RooMinimizer` (default type Minuit2/Migrad, `ROOT::Math::MinimizerOptions::DefaultMinimizerType()`) minimises the wrapper: `minimize`, `hesse`, `save` (FitHelpers.cxx:~551-590). [source] [verified: program printed "Minuit2 / Migrad"]
   - Minuit2 became the ROOT default in 6.30 (SRC/README/ReleaseNotes/v630/index.md:184). [doc]
   - 6.36 notes: Minuit2 now caches the trigonometric transformations used for bounded parameters (SRC/README/ReleaseNotes/v636/index.md:306-312). The project's parameters are all bounded, so it benefits. [doc]

### RooNLLVarNew and the binned data case
- `RooNLLVarNew::doEval`: if `_binnedL` it calls `doEvalBinnedL`, which computes mu = pdf value times bin width and sums `MathFuncs::nll` (Poisson) per bin. Otherwise it uses `RooBatchCompute::reduceNLL` on the per-entry probabilities, weighted, plus the extended term. (RooNLLVarNew.cxx:~215-285) [source]
- The attribute `BinnedLikelihoodActive` is set only by `RooRealSumPdf` (RooRealSumPdf.cxx:747, the HistFactory-style pdf), `RooAbsOptTestStatistic`/`RooNLLVar` (legacy) and `TestStatistics::RooBinnedL`. Not by RooAddPdf. [source]
- Consequence [inferred from the above]: for the project's `RooAddPdf` and `RooDataHist`, the cpu backend treats each bin as a weighted event. The pdf is evaluated at the bin centre and weighted by the bin content (an unbinned-style likelihood on weighted bin centres). It is not an integrated per-bin Poisson likelihood unless `RooFit::IntegrateBins(prec)` is given (`RooBinSamplingPdf`, FitHelpers.cxx createNLLNew). [verified in this pass, section Bin-centre versus integrated-bin evaluation] the difference to an integrated-bin fit exists but is at most 0.08% on nsig for the K0S fits.
- Normalisation sets: with cpu/codegen the pdf is normalised by an explicit `RooNormalizedPdf` node = pdf / integral over the data observables; integrals are `RooRealIntegral` nodes. The integral nodes are scalar and cached while the parameters they depend on do not change. [source] [inferred for caching]

### `Optimize`
- Legacy path: `nll->constOptimizeTestStatistic(Activate, optConst>1)` caches constant sub-expressions (FitHelpers.cxx:869-871). [source]
- cpu path: the function returns before that code, and `RooEvaluatorWrapper::constOptimizeTestStatistic` is an empty override (RooEvaluatorWrapper.h:62). So `Optimize(1)` only affects the `cloneData` default (FitHelpers.cxx:~681) and prints a message at INFO level. [source]
- `Optimize(1)` therefore does nothing useful for the project. Removing it would not change results. [inferred] I did not time a run without it (timing handed to the coordinator's quiet-machine pass).

### The message "RooRealIntegral::init(...) using numeric integrator"
- It is printed by `cxcoutI(NumIntegration)` in RooRealIntegral.cxx:702, i.e. INFO level, extra topic NumIntegration. [source]
- It means that some integral had no (complete) analytic form for the integrated variable and a numeric integrator was created. The project's `RooFitMessages::Configure` sets `setGlobalKillBelow(WARNING)`, so it should not appear now. [source: RooFitMessages.h]
- [verified, this pass] Which node triggers it (program `study060708/numint.cpp`: builds the project's `DynamicRooFitter` for a configuration, enables INFO output AFTER construction, prints stage markers around `DoFit()` and `ExtractYieldsAndPurity()`, outputs `ni-*.out`):
  | model | real or synthetic | messages in DoFit | messages in ExtractYieldsAndPurity |
  |---|---|---|---|
  | k0s DSCB + Chebychev (cell 2,12) | real | 0 | 0 |
  | xi DSCB + Chebychev1 (cell 3,10) | real | 0 | 0 |
  | pi_tpc DSCB + Gaussian (cell 3,10) | real | 0 | 0 |
  | phi Voigtian + BkgSourav2 | synthetic histogram over the configured observable range | 2: `BkgSourav2_bkg_Int[mass]` and `Voigtian_sig_Int[mass]` | 4: `Voigtian_sig_Int[mass|signalRegion]_Norm[mass]` and `BkgSourav2_bkg_Int[mass|signalRegion]_Norm[mass]`, each twice |
  So for the three spectra in the real data file (K0S, Xi, pion) NO numeric integration is created at all, in the fit or in the yield extraction: RooCrystalBall, RooChebychev and RooGaussian all have analytic integrals (also over the named signal range). The numeric integrators belong to the phi models only: the RooVoigtian signal and the RooGenericPdf background, once per `fitTo` (each is rebuilt for every new NLL) and again for each range integral in the extraction. Counts in the project logs (4200 and 840, note 08 section 8.7) are consistent with this. Message text: `RooRealIntegral::init(Voigtian_sig_Int[mass|signalRegion]_Norm[mass]) using numeric integrator RooIntegrator1D to calculate Int(mass)`. The integrator is RooIntegrator1D with the global default precision (see note 08, tutorial rf901). The phi fit itself ran (synthetic, status 0).
  The signal-window integral `createIntegral(..., Range("signalRegion"))` of the DSCB is analytic, so the yield and purity numbers for K0S/Xi/pion carry no numeric-integration error (precision of the analytic CB integrals checked in 'Numerical checks', note 08 section 9.3).

## Backends available here

Build facts. [verified: `root-config --features` printed ... clad ... roofit ...; `ls $ROOTSYS/lib`]
- `root-config --features` contains `clad`, `roofit`, `imt`, `root7`, `runtime_cxxmodules`, `tmva-cpu`; it does not contain `cuda` or any multiprocess item.
- Present libraries: libRooFitCore, libRooFit, libRooFitMore, libRooFitCodegen, libRooBatchCompute (+ `_GENERIC`; the other per-ISA libraries are loaded at runtime, not checked), libRooFitHS3, libRooFitJSONInterface, libRooFitXRooFit, libRooStats, libHistFactory.
- Absent: libRooFitZMQ, libRooFitMultiProcess, any CUDA RooBatchCompute library. `libMultiProc` is present, but it is the unrelated TProcessExecutor library.
- Headers: `$ROOTSYS/include/RooFit/TestStatistics/` is present (modular likelihoods, serial part only). There are no `RooFit/MultiProcess` headers.
- `$ROOTSYS/include/Math/CladDerivator.h` exists; no separate clad shared library is installed under `$ROOTSYS`, yet Clad works (verified in this pass, section Codegen and AD: gradient generation times are printed and the fits converge); the differentiator is part of the Cling that ships with this build.
- Legacy backend: `roofit_legacy_eval_backend` is forced ON in `SRC/roofit/CMakeLists.txt:7`, and `libRooFitXRooFit` is present. The caller measured it working (slow). [source]

| EvalBackend | what it does | in this build |
|---|---|---|
| `cpu` (default) | Evaluator + RooBatchCompute, single thread; NumCPU ignored. Documented agreement with legacy: NLL better than 1e-12 relative, parameters usually better than 1e-6. [doc: RooAbsPdf.cxx:~850-857] | yes, used |
| `legacy` | old RooNLLVar, one entry at a time; supports NumCPU via RooRealMPFE (fork). Doc: "you might need more than 20 threads to maybe see about 10% performance gain over the default cpu-backend". [doc] | yes (caller measured 340 ms/fit with NumCPU(2)) |
| `cuda` | same kernels compiled for GPU [doc] | no (no CUDA library, not built). Selecting it makes the Evaluator throw "RooBatchCompute CUDA could not be loaded" (Evaluator.cxx ctor) [source] |
| `codegen` | experimental: generates C++ for the NLL, JIT-compiles it with Cling, builds the gradient with Clad (`RooFuncWrapper::createGradient`). All nodes must support codegen. [doc] [source: FitHelpers.cxx:782-795] | selectable; FAILS on the DSCB model with the same exception as `codegen_no_grad` [verified]; works on Gaussian, CBShape, Chebychev, Exponential models (see Codegen and AD) |
| `codegen_no_grad` | same without the Clad gradient (numeric derivatives) [doc] | selectable; FAILS on the DSCB model [verified] |

## Multiprocessing

- Options: `Parallelize(n)`, `ParallelGradientOptions(enable, orderStrategy, chainFactor)`, `ParallelDescentOptions(enable, splitStrategy, numSplits)`, `TimingAnalysis(bool)`, `ModularL`. [doc: RooAbsPdf.cxx:1041-1057]
- `Parallelize(n >= 1)` or `-1` implies `ModularL(true)` and the RooFit::MultiProcess gradient job; `0` disables. [doc: RooAbsPdf.cxx:1041-1045]
- Why it throws here: RooMinimizer.cxx:~110-115 is inside `#ifdef ROOFIT_MULTIPROCESS`; otherwise `throw std::logic_error("Parallel minimization requested, but ROOT was not compiled with multiprocessing enabled, please recompile with -Droofit_multiprocess=ON for parallel evaluation")`. `ROOFIT_MULTIPROCESS` is defined by roofitcore/CMakeLists.txt:473-474 only `if(roofit_multiprocess)`. [source]
- The CMake option: `ROOT_BUILD_OPTION(roofit_multiprocess OFF ...)` (cmake/modules/RootBuildOptions.cmake:160); it is ON by default only in the "all" configuration (line 249). [source]
- alidist `~/alice/alidist/root.sh` lists its -D flags (lines ~145-201): `-Droofit=ON` and no `roofit_multiprocess`. It also passes `-DCMAKE_IGNORE_PATH=/opt/homebrew/include`. [source]
- Therefore RooFitZMQ and RooFitMultiProcess were not built. [verified: absent from lib/]
- What building it would take [source + inferred, NOT tried]:
  - ROOT configured with `-Droofit_multiprocess=ON`;
  - ZeroMQ >= 4.3.5 with the draft API (`zmq_ppoll`) and cppzmq. `SearchInstalledSoftware.cmake:1852-1915` falls back to `builtin_zeromq`/`builtin_cppzmq` if they are not found (the builtin sources may need network access);
  - the nlohmann_json dependency of RooFitMultiProcess (roofit/multiprocess/CMakeLists.txt);
  - not supported on Windows.
  - Homebrew has `zeromq` installed (`/opt/homebrew/opt/zeromq`), but alidist hides `/opt/homebrew/include`. Whether its version/drafts suffice was not checked.
  - This would mean editing alidist `root.sh` and rebuilding ROOT, which was out of scope.
- Would it help the project? [inferred] MultiProcess parallelises ONE fit across worker processes (ZMQ messaging, gradient per parameter). It needs a modular likelihood. For 100-200 bins and about 10 parameters at about 5 us per NLL evaluation, inter-process messaging costs far more than the work it splits. It is not suited to many small independent fits. The documentation says gradient parallelism pays when there are "at least as many [parameters] as there are cores" and for expensive likelihoods (v626/index.md:~228, [doc]).
- Running independent fits in separate processes (for example one process per file, or per group of cells) needs no RooFit support. This is the natural way to use several cores. [inferred]

## Codegen and AD

- [doc: SRC/roofit/doc/developers/roofit_ad.md, summarised in note 08 section 5.4] Path: a `CodegenContext` visits the computation graph via `codegenImpl(RooAbsArg&, CodegenContext&)`; `RooFuncWrapper` JIT-compiles the generated function with Cling (`gInterpreter->ProcessLine`, `#pragma cling optimize(2)`: RooFuncWrapper.cxx:~110-120 [source]); Clad generates the gradient, which is also compiled. The document says the coverage of RooFit classes is incomplete ("not yet achieved full coverage") and that classes relying on numerical integration cannot support AD "so far".
- Which classes have a `codegenImpl` in this tree [source: `grep -oE "^void codegenImpl\(Roo[A-Za-z0-9]+" roofit/codegen/src/CodegenImpl.cxx`]: RooAddPdf, RooMultiVarGaussian, RooAddition, RooBernstein, RooBifurGauss, RooCBShape, RooChebychev, RooConstVar, RooConstraintSum, RooGamma, RooFormulaVar, RooEffProd, RooEfficiency, RooExponential, RooExtendPdf, RooGaussian, RooGenericPdf, RooHistFunc, RooHistPdf, RooLandau, RooLognormal, RooParamHistFunc, RooPoisson, RooPolyVar, RooPolynomial, RooProduct, RooRatio, RooRealIntegral, RooRealSumFunc, RooRealSumPdf, RooRealVar, RooRecursiveFraction, RooUniform, RooWrapperPdf (and RooStats helpers); analytic-integral translations (`codegenIntegralImpl`) for RooBernstein, RooBifurGauss, RooCBShape, RooChebychev, RooEfficiency, RooExponential, RooGamma, RooGaussian, RooHistFunc, RooHistPdf, RooLandau, RooLognormal, RooMultiVarGaussian, RooPoisson, RooPolyVar, RooPolynomial, RooRealSumPdf, RooUniform. NOT in either list: `RooCrystalBall` (the project's DSCB), `RooVoigtian`, `RooBreitWigner`, `RooArgusBG`, `RooKeysPdf`.
- [verified, earlier pass] On cell (2,12) of the real k0s data (DSCB + Chebychev), `fitTo(..., EvalBackend("codegen_no_grad"))` printed `[#0] ERROR:Minimization -- Translate function for class "RooCrystalBall" has not yet been implemented.`, `... An analytical integral function for class "RooCrystalBall" has not yet been implemented.` and then threw `std::runtime_error`; no fallback to cpu.
- [verified, this pass] `EvalBackend("codegen")` (WITH Clad gradient) on the same real cell (`backends/codegen1 k0s ... 2 12 codegen 2`): the same two ERROR lines and the same `std::runtime_error: An analytical integral function for class "RooCrystalBall" has not yet been implemented.` So every project model with `sig_model: DSCB` (k0s, xi, pi_tpc) is excluded from both codegen backends.
- [verified, this pass] CLAD LOADS in this build. `root-config --features` contains `clad`; there is no separate `libclad` under `$ROOTSYS` (`etc/cling/plugins/lib/` only holds cmake files, `etc/cling/plugins/include/clad` the headers), yet a Gaussian + Exponential (and a Gaussian + Chebychev1) extended binned fit with `EvalBackend("codegen")` ran and printed `Function JIT time: 3.38 ms`, `Gradient generation time: 8.88 ms`, `Gradient IR to machine code time: 8.12 ms` (program `study060708/cg.cpp`, 100 bins, 5e4 events from toy data). So the Clad differentiator is built into this Cling.
- [verified, this pass] Correctness of the codegen backends on that supported model (Gauss + Exp, same start values, all 4 backends, `PrintLevel(-1)`, status 0 and covQual 3 in every case): minNll cpu -402184.83563509, legacy -402184.83563509, codegen_no_grad -402184.83563509, codegen (Clad gradient) -402184.83563447 (difference 6e-7, relative 1.5e-12); nsig 20013.8677 (cpu, legacy, no_grad) versus 20013.9013 (codegen), i.e. 0.03 events when sigma(nsig) is about 150; m 4.996766 versus 4.996769. Gauss + Chebychev1: cpu -397789.57450, codegen -397789.57451, nsig 14650.567 versus 14650.557. So the AD gradient gives an equally valid minimum, differing at the 1e-4 sigma level.
- [verified, this pass] Single-sided Crystal Ball (`RooCBShape`, which HAS codegen) + Exponential, 2 fits from the same start: codegen gave status 0, edm 3.4e-4 and 4.0e-4, minNll -402185.4929 both times; cpu gave status 0 (edm 3.3e-3, minNll -402185.474) and then status 3 (edm 0.063, minNll -402185.517). With an analytic gradient the EDM was 10-100 times smaller on this (one, synthetic) example, so the numeric-gradient noise IS a plausible contributor to the project's status 3 [inferred from one toy; not shown on the real cells because the DSCB cannot be code-generated].
- [verified, this pass] `RooVoigtian` (project's phi signal): `EvalBackend("codegen")` prints `ERROR:Minimization -- Translate function for class "RooVoigtian" has not yet been implemented.` twice but does NOT throw, and the fit continues and returns a minimum that differs from the cpu one (minNll -402184.90 versus -402184.95, nsig 20142 versus 20094 on the first fit of a free-width Voigtian toy; the toy itself is ill-conditioned: the cpu fit gave status 1 then 3). Whatever the codegen path does for the untranslated node, the result is not the cpu result; treat codegen output with a Voigtian as untrustworthy. The cause was not investigated [inferred: the node is dropped or replaced by something other than the Voigtian].
- Cost [verified, noisy machine, orders of magnitude only]: the FIRST `fitTo` with a codegen backend in a process took 2.5 to 3.2 s (cling start-up, JIT; cpu: 25-45 ms for the first fit), and EVERY later `fitTo` regenerates and recompiles the code (the INFO lines `Function JIT time`, `Gradient generation time` are printed for every fit): about 3 ms per fit for `codegen_no_grad` and about 21-26 ms per fit for `codegen` against 0.2-0.3 ms (cpu, legacy 0.5 ms) on this small model. The generated function depends on the dataset and is not reused between calls. No timing conclusions beyond this order of magnitude are drawn.
- Conclusion for the project [inferred from the above]: codegen (AD) is not usable for the DSCB models; for supported models it is correct but costs more per fit than the whole cpu fit. It could only matter as a scientific tool (analytic gradient, tighter EDM) if a DSCB codegen translation were written (`codegenImpl(RooCrystalBall&)` plus `codegenIntegralImpl`, following roofit_ad.md steps), which is a development task, not an option.

## Profile of a DoFit

Method: [verified] my program `prof1.cpp` (path under Resume here) replicates `fitTo` (= createNLL + RooMinimizer: `setPrintLevel(-1)`, `optimizeConst(1)`, `minimize("", "minuit")`, `hesse()`, `save()`) with `std::chrono` timers around each stage. It also times the project's own `DoFit()` on the same cell. Real data, all cells with integral >= 1000. Per cell: 1 discarded warm-up then 3 (k0s: 3 in the final run, 5 in an earlier run) repetitions with a fresh NLL each time and start values reset from the config. Number of NLL evaluations is taken from `RooMinimizer::evalCounter()` after MIGRAD and after HESSE.

Results (mean over cells, ms per fit):

| spectrum | cells | dataHist | createNLL | RooMinimizer ctor | MIGRAD (NLL calls) | HESSE (NLL calls) | save | teardown | sum | DoFit() (project) |
|---|---|---|---|---|---|---|---|---|---|---|
| k0s (200 bins, N up to 4e7) | 340 | 0.013 | 0.045 | 0.009 | 5.14 (about 1018) | 0.38 (about 78) | 0.008 | 0.023 | 5.61 | 5.83 (median 4.95, p90 8.2, max 29) |
| k0s, earlier 5-rep run | 340 | 0.018 | 0.060 | 0.012 | 6.26 (about 1034) | 0.47 (about 78) | 0.012 | 0.033 | 6.86 | 7.05 (median 6.5, p90 9.4, max 27) |
| xi (200 bins) | 260 | 0.008 | 0.037 | 0.008 | 1.66 (about 722) | 0.16 (about 74) | 0.007 | 0.018 | 1.90 | 1.93 (median 1.79, p90 2.9, max 4.8) |
| pi_tpc (100 bins, N up to 1.7e9) | 360 | 0.007 | 0.033 | 0.008 | 3.42 (about 1417) | 0.22 (about 94) | 0.006 | 0.018 | 3.71 | 3.87 (median 3.63, p90 5.41, max 16.6) |

- `harness real0` (the caller's `real.cpp`, rebuilt) on k0s: 340 cells fitted, total DoFit 2458 ms, mean 7.23 ms, 0 throws. [verified]
- Per NLL evaluation: k0s about 5 us (5.1 ms / 1018 calls), xi about 2.3 us. [verified by division; no separate loop of `getVal`]
- Setup + result creation + teardown (dataHist, createNLL, ctor, save, teardown) is about 0.1 ms/fit, under 2% for k0s/xi. The "about 2.2 ms steady state" quoted by the caller matches the xi/pion-like cells; k0s is about 5 to 7 ms because of the larger count of function calls (more parameters at bounds, N of order 1e7).
- First fit of the process in my runs: 17 to 40 ms (noisy, machine shared). The caller measured about 120 ms. Not investigated.
- Whole-run totals for about 1000 fits: roughly 2 to 7 s depending on the spectrum. Honest answer: there is nothing worth optimising on the backend side. Even a hypothetical 3x faster NLL would save a few seconds in a run.
- Where an optimisation could matter, if ever: the number of MIGRAD NLL calls (about 1000 for 9 to 10 parameters; numeric gradient = 2 evaluations per parameter per iteration). Options that reduce it are better start values, fewer free shape parameters (for instance fixing `n1`/`n2`/`alpha`), or Strategy 0. These change the fit and need physics review; I did not test any of them. NOT DONE.

### Fit-quality observations from the same runs [verified; interpretation is [inferred]]
- On k0s almost every cell ends with `status() == 3` (Minuit2: "Edm is above max", Minuit2Minimizer.cxx:651-653). On cell (2,12) the final `edm` was 2.06e10, `covQual` 2, with alpha1_sig error 2.84 and tiny errors on others. Also seen on xi (several cells) and pi_tpc (nearly all; one cell status 303 after 5001 calls).
- During MIGRAD RooFit prints (at WARNING level, so not suppressed by the project's filter) "p.d.f value is less than zero, trying to recover" for `Chebychev2_bkg`, "top-level p.d.f evaluates to NaN" and "Minimized function has error status. Returning maximum FCN so far". So the background polynomial goes negative and the pdf goes NaN while exploring.
- The NLL is about -5e8 in magnitude for the high-statistics cells, so relative floating-point precision limits (about 1e-16 x 5e8 = 5e-8) the achievable gradient accuracy of numeric derivatives. This is a plausible reason why EDM cannot be reached. Not tested.
- Whether these status-3 fits matter physically is outside this topic. The caller's harness already records the status.

## Threads

Test [verified, this pass]: program `study060708/thr.cpp` (runner `runthr.sh`). It takes N real k0s cells (mult index 3, pT index 4 onwards, histograms projected serially in the main thread and then used by the threads), and each `std::thread` builds its OWN `DynamicRooFitter` (own `RooRealVar`s, pdfs, `RooDataHist`), calls `DoFit()` and `ExtractYieldsAndPurity()` and stores status, signal, error and minNll. Reference: the same cells fitted serially in the main thread (`thr-serial.txt`). Results are compared as 12-digit text.

| configuration | outcome |
|---|---|
| 1 worker `std::thread` (not the main thread), 1 cell | 3/3 runs identical to serial |
| 2 threads x 1 cell | 6/6 runs CRASHED (SIGTRAP or abort); one earlier run HUNG: after about 14 minutes of wall time (27 minutes of CPU time, 196% CPU) it was still running with both threads busy and the main thread in `join`, and I killed it |
| 4 threads x 3 cells | 3/3 runs crashed (`*** Break *** segmentation violation`, then SIGTRAP) |
| 4 threads x 3 cells with `ROOT::EnableThreadSafety()` called first | 3/3 runs crashed (2 SIGSEGV, 1 SIGTRAP) |
| 4 threads x 3 cells, a global `std::mutex` around each whole fit | 3/3 runs identical to serial (so a worker thread other than main is not the problem, concurrency is) |

What exactly happens: crash or hang, never a silently wrong number (no run completed with a result different from serial). Where: the stack of the crashes and the `sample` of the hung process (`study060708/hang-sample.txt`) put both threads inside `RooDataHist::RooDataHist(...)` -> `RooCmdConfig::process(RooLinkedList const&)` / `process(RooCmdArg const&)` -> `RooLinkedList::Add`, i.e. the command-argument parsing of the first RooFit call in `DoFit` (`RooDataHist dataHist("dataHist","Data",*obs,RooFit::Import(*h1Data))`). In the hang both threads loop forever in `RooCmdConfig::process`, which is consistent with a corrupted linked list (a cycle) [inferred]. Cause [source]: `RooLinkedList` allocates its elements from a process-wide static pool, `RooLinkedList::Pool* RooLinkedList::_pool` (RooLinkedList.cxx:112-135 class, :259-280 creation and `acquire()`), with no locking; further process-wide state that is also shared: `RooMsgService::instance()` (the project redirects its streams in `RooFitMessages::Configure`), the `RooNameReg` registry, `RooTrace`, `gInterpreter`/`gDirectory` and ROOT's histogram registration. `ROOT::EnableThreadSafety()` does not help because the problem is RooFit's own global state, not ROOT's I/O locking.
The only explicit statement in the tree is for class generation: `RooClassFactory.cxx:125` "Making a new compiled class is not thread safe". The 6.36 release notes and `RooAbsPdf.cxx` say nothing about concurrent fits; the new `NumCPU` multithreading in master (note 08 section 9.8) was written so that "no RooAbsArg state is ever touched concurrently", which is the developers' own acknowledgement that RooFit objects are not thread-safe. [doc]
Consequence [verified + inferred]: do not run fits of different cells in different threads of one process. Separate processes (one per file or per group of cells, each with its own `RooMsgService`) remain the safe parallelisation (note: the 960-cell run takes 2 to 7 s serially, so none is needed).

## Bin-centre versus integrated-bin evaluation (numeric check) [verified, this pass]
Background [source, above]: with the cpu backend and a `RooAddPdf` on a `RooDataHist`, each bin is a weighted event at the bin centre (note 05 verified that the NLL equals the extended unbinned NLL at bin centres, to 0); `IntegrateBins(prec)` wraps the pdf in `RooBinSamplingPdf` and integrates it over each bin of the OBSERVABLE's binning (which has 100 bins by default for the project's 200-bin histograms, note 04), so `obs->setBins(nbins)` must be called first.
Design of the test (program `study060708/asimov.cpp`): for a real k0s cell take the project's own default fit as the truth, build an Asimov histogram of EXACT per-bin integrals of the fitted model (via `createIntegral(..., Range)` per bin, scaled), and fit it twice from the same start, (a) as the project does (bin-centre) and (b) with `obs->setBins(200)` and `IntegrateBins(1e-6)`. Everything with `Offset("bin")` so that all 20 fits converge (status 0 in all of them) and the shift of nsig is not hidden by non-convergence. Pull = (fitted - true nsig) / error.
| cell | true nsig | bin-centre: nsig - true (error), pull | integrated: nsig - true (error), pull |
|---|---|---|---|
| 3,10 | 2.254e7 | -1.71e4 (6.7e3), -2.55 | +94 (8.5e3), +0.01 |
| 5,8 | 3.942e7 | -1.02e4 (6.6e3), -1.53 | -61 (6.9e3), -0.01 |
| 7,12 | 1.700e7 | +1243 (7.7e3), +0.16 | -117 (7.2e3), -0.02 |
| 9,18 | 9.29e6 | -1563 (3.7e3), -0.42 | -17 (3.4e3), -0.01 |
| 0,2 | 6.16e6 | -1016 (2.6e3), -0.39 | +2 (2.6e3), 0.00 |
At 1% of these statistics (same shapes): bin-centre pulls -0.26, -0.14, +0.02, 0.00, -0.03; integrated pulls <= 0.03 in absolute value (true nsig 2.3e5, 3.9e5, 1.7e5, 9.3e4, 6.2e4).
So the bin-centre approximation biases nsig by a relative -7.6e-4 (cell 3,10), -2.6e-4, +7e-5, -1.7e-4, -1.6e-4: at most 0.08%, only visible as a 1.5 to 2.5 sigma shift in the highest-statistics cells, below 0.5 sigma elsewhere; `IntegrateBins` removes it. The size is as expected for sigma/bin width of about 8 [inferred]. No purity bias was computed (the bias on nsig is mostly a shape effect; the purity depends also on the background) [not done].
Real data, same comparison without the Offset (default fit, then the same fit with integrated bins): the signal yields differ by up to 9% (cells 7,15 and 7,12; `bincentre.cpp`), with status 3 in both fits except cell 7,15 (status 0 for the bin-centre fit, 3 for the integrated one), but these differences are NOT a bin-centre effect (the Asimov result above shows 0.1%): they show that two fits of one real cell that differ only in fine numerical details end at different points when status is 3. The NLL values of the two variants are not comparable (the integrated one is a different function). This is the symptom analysed by the fit-quality researcher, not studied here.

## Parameters at their bounds and status 3 versus statistics (real cells) [verified, this pass]
Program `study060708/bounds.cpp` (default project fit on every cell with at least 1000 entries; floating parameters within 1e-4 of the range of a bound counted) and `cellscan.cpp`. The status counts reproduce the earlier numbers: k0s 310/340, xi 134/260, pi_tpc 338/360 with status 3.
| spectrum | parameter (floating in n cells) | at lower bound | at upper bound |
|---|---|---|---|
| k0s (340) | c1_bkg | 7 | 0 |
|  | n1_sig / n2_sig [1,150] | 2 / 6 | 0 / 3 |
|  | alpha2_sig [0.5,5] | 0 | 1 |
|  | everything else (mean, sigma, alpha1, nsig, nbkg) | 0 | 0 |
| xi (260) | sigma_sig [0.002,0.008] | 237 | 0 |
|  | nbkg / nsig | 6 / 4 | 0 |
|  | n2_sig | 0 | 2 |
| pi_tpc (360) | nbkg [0,2e9] | 165 | 0 |
|  | mean_bkg | 95 | 0 |
|  | sigma_bkg | 1 | 77 |
|  | n1_sig [1,50] / n2_sig | 4 / 50 | 21 / 79 |
|  | alpha1_sig | 8 | 0 |
|  | sigma_sig | 50 | 0 |
Reading [inferred]: for Xi the signal width sits on its LOWER bound (0.002) in 237 of 260 cells, for pi_tpc the background normalisation is zero in 165 of 360 cells and the n parameters of the tails are at a bound in about 150 cells; for k0s the shape parameters are mostly free. Parameters at a bound make the Minuit2 covariance and EDM unreliable by themselves (the transformation to internal variables has zero derivative there), independently of the NLL round-off discussed in note 08 section 9.1. These are model/bound questions for the analyst; no value was changed here. The full list per cell is in `bounds-k0s.tsv`, `bounds-xi.tsv`, `bounds-pi_tpc.tsv` (columns i, j, parameter, side, value, status).
Fraction of status 3 versus statistics for k0s cells with N between 1e5 and 2e6 entries (78 cells, `cellscan.txt`): N in [1e5,3e5) 7/18 status 3 (median final edm of those 0.045); [3e5,1e6) 35/42 (0.074); [1e6,2e6) 18/18 (0.24); no status-0 cell above 1e6 entries. This is the dependence on |NLL| expected from the machine-limit explanation (note 08 section 9.1, Moneta: limit = 1e-8 x |NLL|), but it is a correlation and not a proof [inferred].

## Recommendations (recommendations only; all [inferred] from the evidence above)

0. (second pass) Do not run fits in threads; do not use codegen with DSCB or Voigtian; if the bin-centre effect (<= 0.08% on nsig, only visible above ~1e7 entries) ever matters use `obs->setBins(nbins)` + `IntegrateBins(1e-4..1e-6)` together; check the parameters at bounds (section Parameters at their bounds) before attributing status 3 only to round-off.
1. Keep the default `cpu` backend. Do not add `NumCPU`, `Parallelize`, `EvalBackend("legacy")` or `codegen*`: the first is ignored, the second throws, the third is 50-100x slower, and `codegen*` fails for DSCB.
2. `Optimize(1)` can be dropped without effect on the cpu backend (it is a no-op there); keeping it is harmless. It is the only option in `DoFit()` that I can show is inert, but I did not benchmark its removal.
3. Do not spend effort on speed: about 98% of a DoFit is MIGRAD, which is about 1000 cheap NLL evaluations. A total of 2 to 7 s per run is small.
4. If wall time ever matters, parallelise at process level over files or cells (no RooFit changes needed), not inside RooFit.
5. Look at the status 3 / EDM problem separately: it is a convergence/quality question rather than a speed question. Candidate checks: the start values and bounds of the shape parameters (`alpha`, `n`), the background polynomial going negative, and `RooFit::IntegrateBins` versus bin-centre evaluation (the cpu backend evaluates the pdf at the bin centre).

## Pitfalls

- Never fit in several threads of one process (crash or hang, section Threads). `ROOT::EnableThreadSafety()` does not change this.
- `EvalBackend("codegen")` with a `RooVoigtian` prints an ERROR but still returns a (different) result instead of throwing.

- With `codegen*` the failure arrives as an exception after seconds of start-up, not as a clean fallback.
- `RooFitMessages::Configure()` silences only below WARNING. The RooFit WARNING blocks above still go to stdout (it is only counted by the tee).
- `NumCPU` is silently ignored on the default backend (RooAbsPdf.cxx:835), so adding it only gives a false sense of parallelism.
- With the cpu backend RooDataHist bins are treated as weighted points evaluated at the bin centre unless `IntegrateBins` is given. The fitted yields are therefore not an integrated-bin Poisson fit. Not numerically compared.
- Running `alienv printenv` concurrently from several agents races: the module tree `~/alice/sw/MODULES/osx_arm64` was being deleted and rebuilt, so `alienv` failed with "Unable to locate a modulefile for 'BASE/1.0'" for minutes. Workaround used: a retry loop (once per second) and caching the output in `scratch/backends/env.sh`, then `eval "$(cat env.sh)"`. Setting `ROOTSYS`/`PATH` by hand is not enough: ROOT then fails with "module file .../BUILD/.../Vc.pcm not found" and `$ROOT_DYN_PATH` errors.

## Resume here
Third pass (15 October 2026): no new runs for this note by instruction; timing items moved to the coordinator (item 4).
Done in the second pass: codegen with gradient and Clad, thread-safety test, numeric-integrator node, pi_tpc profile, Asimov bin-centre check, bounds scan and status-versus-statistics scan.
Remaining (all optional):
1. Why codegen with `RooVoigtian` silently returns a different minimum (read `CodegenContext` handling of an untranslated node); whether the exact Clad gradient lowers the EDM on a DSCB-like model (would need a `codegenImpl(RooCrystalBall&)`; compare with `RooCBShape`, which already has one: the one-example result above is encouraging).
2. Purity (not only nsig) bias from bin-centre evaluation; the Asimov test was done for nsig only.
3. Test fixing shape parameters or Strategy 0 on the number of MIGRAD calls (changes physics, needs approval).
4. TIMINGS: handed to the coordinator's quiet-machine timing pass (15 October 2026 instruction). Every time in this note (profile table, first-fit cost, codegen JIT/Clad costs, per-NLL-call cost, the `Optimize(1)` removal not timed (line on `Optimize`), the workspace read-back times of note 07 (i)) was measured while other jobs ran and is indicative only; the coordinator's numbers supersede them. Correctness results (codegen, threads, bin-centre, bounds) are not affected.
5. The xi `sigma_sig` lower bound (237/260) and the pi_tpc bounds are for the analyst to look at; they were reported, not changed.
Scratch files under `/Users/scannito/alice/Analysis/RooFitKnowledge/scratch/study060708/`: `cg.cpp`/`cg2.cpp` (synthetic backend/codegen tests, outputs `cg-*.out`), `thr.cpp`, `runthr.sh`, `thr-serial.txt`, `hang-sample.txt` (thread tests), `numint.cpp`, `ni-*.out`, `asimov.cpp`, `bincentre.cpp`, `bounds.cpp` + `bounds-*.tsv`, `cellscan.cpp` + `cellscan.txt`, `cbtest.cpp`, `prof-pitpc.txt`. Earlier first-pass files remain in `scratch/backends/` (`prof1`, `codegen1`, `real0`).
