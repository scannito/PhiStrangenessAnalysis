# 03 - The fitting API, the minimizer, and what the fit status means

SECOND PASS COMPLETED (14 October 2026): the real-cell experiments, the status 3 verdict and number-backed recommendations are now in
the sections "Real-cell experiments" and "Recommendations" below; the full study is `09-fit-quality-and-yields.md`.
Provenance tags: [doc] Doxygen/release-note text with file:line, [source] code read, [verified] something
run here (with what was run), [inferred] a conclusion not tested.

Source tree: `~/alice/sw/SOURCES/ROOT/v6-36-10-alice2/v6-36-10-alice2/` (abbreviated `SRC/`).
RooFit core files are in `SRC/roofit/roofitcore/src/`, Minuit2 in `SRC/math/minuit2/src/`.

## Summary

1. [source] The `status` that `fitTo(...Save())` returns is NOT a verdict on the whole fit. It is the
   Minuit2Minimizer member `fStatus` as left by the last `Minimize()` call (codes 0..6), plus 100 x
   (HESSE failure code) if the HESSE step failed. HESSE does not recompute it when it succeeds
   (`Minuit2Minimizer.cxx:1223-1315`).
2. [source][verified] **Status 3 means "MIGRAD ended with EDM above 10 x its target" (`Edm is above max`).**
   It is a MIGRAD flag; the later HESSE does not clear it. The `edm()` stored in the RooFitResult is the
   EDM recomputed by HESSE, so it can be tiny or huge independently of the status. Status 0 with a large
   `edm()` also occurs (verified, k0s cell (5,10): status 0, edm 0.169).
3. [verified] The project's statuses 3 come with `MINIMIZE=-1;HESSE=3` in the status history: MIGRAD
   returned "invalid" (the `-1` is RooFit's convention, `RooMinimizer.cxx:263`), HESSE then ran, was
   valid, and covQual is usually 3 (full accurate matrix) but sometimes 2 (forced positive-definite).
4. [verified, one cell, `PrintLevel(1)`] For k0s cell (0,0), MIGRAD's log ends with "Reached machine
   accuracy limit; Edm 0.329304 is smaller than machine limit 2.22052 while 0.001 was requested" then
   "No convergence; Edm 0.329304 is above tolerance 0.01". The NLL there is -3.7e7, so the EDM cannot be
   resolved below about 2 in NLL units. That is the mechanism for status 3 in high-statistics cells.
   [verified, 09] It does NOT hold in general: on the 523 status-3 merged analysis cells 28% are stable under refit (precision limit), 52% are genuinely not converged and 20% bad; EDM below the machine limit EPS2*|NLL| (EPS2=5.9e-8) is a weak benign test (54% of those cells still have a lower NLL by >0.5).
5. [verified] Status codes seen in the experiments: 0, 1, 3, 4, 5, 302, 303, 304 (details below).
6. [source][verified] `Optimize(1)` is a no-op for the numerics in this build with the default `cpu`
   backend (`RooEvaluatorWrapper::constOptimizeTestStatistic` is empty, `RooEvaluatorWrapper.h:62`).
   `Tolerance`, `Improve`, `NormRange` are not `fitTo` options in 6.36 (verified: "unrecognized command",
   the fit still runs with the other options). `BatchMode()` is commented out of the header, so it does not
   compile in C++.
7. [verified] `Save(false)` (or omitting `Save`) makes `fitTo` return `nullptr`; `DoFit()` would
   dereference it.

## Option catalog (fitTo and createNLL, ROOT 6.36)

Where the options are defined and defaulted: Doxygen table `RooAbsPdf.cxx:821-929` (createNLL) and
`RooAbsPdf.cxx:968-1060` (fitTo); parsing and defaults `FitHelpers.cxx:230-255` (MinimizerConfig),
`425-455` (minimiser options), `592-881` (createNLL), `984-1074` (fitTo), `468-590` (minimize).
RooCmdArg constructors: `RooGlobalFunc.cxx`.

How `fitTo` routes options [source `FitHelpers.cxx:990-1008`]: these go to `createNLL`:
`ProjectedObservables, Extended, Range, RangeWithName, SumCoefRange, NumCPU, SplitRange, Constrained,
Constrain, ExternalConstraints, CloneData, GlobalObservables, GlobalObservablesSource,
GlobalObservablesTag, EvalBackend, IntegrateBins, ModularL, OffsetLikelihood`. Everything else is read by
`fitTo` itself (minimiser options). An argument not known to the command parser prints
`ERROR: unrecognized command: <name>` and is ignored [verified: Tolerance, Improve, NormRange].

### Options that build the likelihood (createNLL)

| Option | Semantics and default | Notes |
|---|---|---|
| `Extended(bool)` | add the extended term `N_exp - N_obs log N_exp`. Default is "auto" (internal value 2): extended iff the pdf can be extended [source `FitHelpers.cxx:58,257-281`]. | A RooAddPdf with yields as coefficients is "MustBeExtended". `Extended(0)` on it logs `ERROR: You used the Extended(false) option on a pdf where the fit MUST be extended!` and gives nonsense [verified: xi (5,10) gives nsig=1.2e5 instead of 6.8e5]. Omitting `Extended` is identical to `Extended(1)` for the project model [verified, same numbers to all digits]. |
| `Range(lo,hi)` / `Range("name[,name2]")` | fit only the data inside; the numeric form creates a range named "fit" on all observables. Setting a Range also sets the normalisation range [doc `RooAbsPdf.cxx:828-833`; `FitHelpers.cxx:684-699`]. | [verified] xi (5,10) with the lower 10% cut works (status 0 covQual 2, different yield). |
| `SplitRange(bool)` | separate ranges per category in a RooSimultaneous: range name `rangeName_indexState`. Default off. | Irrelevant for 1D fits. |
| `NormRange(...)` | NOT a `fitTo` option. It belongs to plotOn / createIntegral / generate. | [verified] `fitTo(..., NormRange("x"))` logs unrecognized command. |
| `SumCoefRange(name)` | range in which RooAddPdf coefficients are interpreted. | |
| `ConditionalObservables(...)` | documented in the createNLL table as "do not normalise PDF over listed observables" [doc `RooAbsPdf.cxx:825`], but the parser key is `ProjectedObservables` [source `FitHelpers.cxx:615`]. | not used by the project. |
| `Constrain(set)`, `ExternalConstraints(set)` | restrict / add constraint terms (multiplied onto the likelihood). | not used. |
| `GlobalObservables(set)`, `GlobalObservablesTag(tag)`, `GlobalObservablesSource("data"\|"model")` | which observables are normalisation observables of constraint terms; default source `data`. Other values throw `std::invalid_argument` [source `FitHelpers.cxx:710-715`]. | not used. |
| `IntegrateBins(prec)` | binned fits: integrate the pdf over each bin instead of using the density at the bin centre. `prec>0` everywhere (use 1e-2..1e-6), `prec=0` only for continuous pdfs fit to RooDataHist, `prec<0` off (default -1) [doc `RooAbsPdf.cxx:915-923`]. | [verified, 09 section 9.6, 737 analysis cells] with `obs->setBins(n)` = data bins in range (K0S 200, Xi 72, pions 100; edges equal to 2e-16): CPU ~100-200x a bin-centre fit (relative only); with the project's call it converges badly (accepted 188/734), with Offset + refit 608/734; S shift vs bin centre: Xi +0.17% median (p90 1.8%), K0S/pions <0.1% median. |
| `Offset("none"\|"initial"\|"bin")` | likelihood offsetting. Default none. `initial` subtracts the starting NLL; `bin` offsets bin by bin with a template model [doc `RooAbsPdf.cxx:904-914`]. The RooCmdArg key is `OffsetLikelihood` [source `RooGlobalFunc.cxx:736-748`]. | `FitHelpers.cxx:998-1000` forwards it to the NLL unless `ModularL` is true. Release note: issue #20083 "Offset() option is ignored by fitTo()" is in the 6.36 fixed list [doc `README/ReleaseNotes/v636/index.md:656`]. |
| `EvalBackend("cpu"\|"legacy"\|"cuda"\|"codegen"\|"codegen_no_grad")` | default `cpu` (vectorised, single thread). | [verified] `legacy` works (3-10x slower, near-identical result: k0s (3,18) edm 9.158e7 in both). `codegen`/`codegen_no_grad` throw: `An analytical integral function for class "RooCrystalBall" has not yet been implemented`. `cuda` throws: `libRooBatchCompute_CUDA` missing. |
| `BatchMode(...)` | deprecated alias of `EvalBackend` [source `RooGlobalFunc.cxx:470-484`: warns "deprecated. Please use EvalBackend()"]. | The declaration is commented out in `RooGlobalFunc.h:235-240` (installed and source), so C++ code using it does not compile [verified]. |
| `NumCPU(n, strategy)` | "Parallelize NLL calculation on num CPUs. (Currently, this setting is ignored with the cpu Backend.)" [doc `RooAbsPdf.cxx:835`] | [verified] `NumCPU(4)` gives identical results on xi (5,10). Mutually exclusive with `ModularL` (`FitHelpers.cxx:628`). |
| `ModularL(bool)` | new-style likelihood. | [verified] with `ModularL(true)` and no parallelisation it runs, with a warning, and gives 0.0000003 relative-different numbers (xi (5,10): nsig 683449.5770 vs 683449.5761). |
| `CloneData`, `Verbose` | `CloneData` deprecated and ignored [doc `RooAbsPdf.cxx:902-903`]. | |

### Options that steer the fit (fitTo only)

| Option | Semantics and default |
|---|---|
| `Minimizer("type","algo")` | default type is the ROOT default (Minuit2 here), algo "minuit" string is passed through. Documented: Minuit: migrad, simplex, minimize, migradimproved; Minuit2: migrad, simplex, minimize, scan; GSLMultiMin; GSLSimAn [doc `RooAbsPdf.cxx:976-984`]. [verified] `("Foo","migrad")` prints "Could not create the Foo minimizer. Try using the minimizer Minuit" and the status becomes -1 in the final result with a Minuit fallback fit. `("Minuit2","bar")` silently behaves as migrad. `("GSLMultiMin","bfgs2")` CRASHED the process (segmentation violation in `RooAbsMinimizerFcn::BackProp`) in this build: do not use. `("Minuit2","scan")` gives status 5 with edm 5.5e7 on a test histogram: not a usable fit. |
| `Strategy(0\|1\|2)` | Minuit strategy, default 1 [doc `RooAbsPdf.cxx:992`; `FitHelpers.cxx:557`, only applied if != 1]. Out-of-range values (-1, 7) were accepted without a message in one test [verified: both gave the identical result on xi (5,10), covQual 1, edm 2.018]. |
| `Hesse(bool)` | run HESSE after MIGRAD. Default TRUE (`hesse = 1`, `FitHelpers.cxx:239`). So `Hesse(true)` is redundant and `Hesse(false)` is the only meaningful form. |
| `InitialHesse(bool)` | HESSE before MIGRAD, default off (`FitHelpers.cxx:559-560`). |
| `Minos(bool)` / `Minos(set)` | run MINOS after HESSE, default off. Error and no fit if combined with `SumW2Error(true)` or `AsymptoticError(true)` (`FitHelpers.cxx:517-521`). |
| `Improve` | NOT an option. Use `Minimizer("Minuit","migradimproved")` [doc]. [verified] unrecognized command. |
| `Tolerance` | NOT an option in fitTo. The tolerance is set only via `RooMinimizer::setEps()` (default 1.0, `RooMinimizer.cxx:146`) or `ROOT::Math::MinimizerOptions::SetDefaultTolerance`. [verified] unrecognized command. |
| `MaxCalls(n)` | max function calls; `n<=0` keeps the default 500 x #params (`RooMinimizer.cxx:153-154`; `FitHelpers.cxx:547`). [verified] `MaxCalls(50)` and `(300)` on xi (5,10) give status 304 with covQual 0 and a message "Error when calculating Hessian". |
| `Optimize(int)` | constant-term optimisation. Doxygen says "on by default". Parsed default in `fitTo` is 2 (`FitHelpers.cxx:232`), so `m.optimizeConst(2)` is called if you pass nothing; `Optimize(1)` calls `optimizeConst(1)`; `Optimize(0)` skips it (`FitHelpers.cxx:551-552`). The only difference between 1 and 2 is the flag `doAlsoTrackingOpt = flag>1` (`RooAbsMinimizerFcn.cxx:323`). **With the cpu backend the function-side call is empty** (`RooEvaluatorWrapper.h:62`), so Optimize(0/1/2) are expected numerically identical in this build [inferred; the comparison over all cells was queued but not analysed]. The INFO lines "activating const optimization" are still printed at print level > -1 (`RooAbsMinimizerFcn.cxx:314-326`). |
| `RecoverFromUndefinedRegions(strength)` | default 10; 0 = constant penalty (ROOT < 6.24 behaviour). The value returned to Minuit for an invalid point is `maxFCN_so_far + strength * badness` (`RooAbsMinimizerFcn.cxx:278-298`). |
| `EvalErrorWall(bool)` | default true: return a large value for invalid points. If off, the invalid values are "ignored" (message `Minimized function has error status but is ignored.`). |
| `PrintEvalErrors(n)` | number of eval errors printed per likelihood evaluation: negative = none (they are only counted, cheaper), 0 = counts per component, positive = up to n details (default 10) (`RooMinimizer.cxx:837-845`, `RooAbsMinimizerFcn.cxx:247-270`). |
| `PrintLevel(l)` | -1..3, default 1; passed to Minuit2 as l+1 (`RooMinimizer.cxx:471-474`). At -1 the RooFit INFO messages from the minimiser are suppressed. [verified] `PrintLevel(1)` shows the Minuit2 iteration log. |
| `Warnings(bool)`, `Verbose(bool)`, `Timer(bool)` | informational; `Timer(true)` prints "Real time ..., CP time ..." to cout [verified]. They did not change the numerics. |
| `Save(bool)` | return a RooFitResult. Default off; without it `fitTo` returns `nullptr` [verified]. |
| `SumW2Error(bool)` | `V_corr = V C^-1 V` using a second HESSE with squared weights; runs a second HESSE (status history gains a second `HESSE=` entry [verified]); covQual becomes min(covQual, covQual_w2) (`FitHelpers.cxx:189-226,583-585`). For unweighted data it should reproduce V, but is only as good as the two HESSE matrices: [verified, k0s (3,18)] error went 178477 -> 3.9e6 because the HESSE was unstable there. If the data "appear weighted" and neither SumW2Error nor AsymptoticError is given, the "weighted data" warning is printed (`FitHelpers.cxx:502-515`). |
| `AsymptoticError(bool)` | asymptotically correct weighted errors; warns for binned data ("not designed to handle binned data") (`FitHelpers.cxx:71-175`). [verified] on k0s (8,28): error 42587 vs 954 for base, so on binned/failed cells it is not stable. |
| `PrefitDataFraction(f)` | prefit on a subset; skipped with a warning if fewer than 100 events (`FitHelpers.cxx:1030-1056`); the doc warns it may be bad for binned analyses. [verified] "Skipping prefit..." on a 72-entry RooDataHist (entries = bins). |
| `Parallelize(n)` | throws `std::logic_error` ("ROOT was not compiled with multiprocessing") in this build [verified]. `ParallelGradientOptions`, `ParallelDescentOptions`, `TimingAnalysis` likewise need multiprocessing (`RooMinimizer.cxx:90-115`). |

Legacy-only (need `EvalBackend("legacy")`): `NumCPU`, `CloneData` (ignored anyway), the legacy bin offsetting via
`enableBinOffsetting`, `Optimize` having any effect. [source `FitHelpers.cxx:807-880`, `RooEvaluatorWrapper.h:62`]

### What `fitTo` executes, step by step [source `FitHelpers.cxx:468-590`]

1. build the NLL (`createNLL`), 2. `RooMinimizer m(nll)` (default Minuit2, tolerance 1.0, errordef 0.5 from the
NLL), 3. `m.setEvalErrorWall, setRecoverFromNaNStrength(10), setPrintEvalErrors(10)`, optional MaxCalls, print level,
`optimizeConst`, strategy, 4. optional `m.hesse()` (InitialHesse), 5. `m.minimize(type, alg)` (this is MIGRAD),
6. `m.hesse()` unless `Hesse(false)`, 7. optional SumW2/Asymptotic correction, 8. optional `m.minos()`,
9. `m.save()` if `Save`, 10. `optimizeConst(0)`.

## Minimizer and status codes

### Tolerance and EDM [source]

- `RooMinimizer` sets tolerance 1.0 (`RooMinimizer.cxx:146`) and error level = `defaultErrorLevel()` of the NLL (0.5)
  (`RooMinimizer.cxx:160`). Minuit2's `ModularFunctionMinimizer::Minimize` scales: `effective_toler = tol * Up`
  = 0.5 (`ModularFunctionMinimizer.cxx:73`); `VariableMetricBuilder::Minimum` multiplies by 0.002
  (`VariableMetricBuilder.cxx:64`), so the **EDM target is 0.001** and the **"above max" threshold is 10 x target =
  0.01** (`VariableMetricBuilder.cxx:187`). [verified in the PrintLevel(1) log: "Start iterating until Edm is < 0.001",
  "Edm 0.329304 is above tolerance 0.01".]
- For strategy >= 2, or strategy 1 with `Dcovar > 0.05`, MIGRAD runs an internal HESSE and, if the EDM is still above target
  and above the machine limit `|Eps2 * Fval|`, iterates once more (`VariableMetricBuilder.cxx:134-170`). If the EDM is below the machine
  limit it stops iterating and warns "Reached machine accuracy limit" (`:166`), after which the `edm > 10*edmval` test still
  decides the status.
- Call limit default: 500 x number of parameters (`RooMinimizer.cxx:153`); the project's models have 9-10 free parameters,
  so 4500-5000 calls [verified: "max-calls 4500" for the 9-parameter k0s cell].

### How `status` is formed [source]

`Minuit2Minimizer::ExamineMinimum` (`Minuit2Minimizer.cxx:635-675`) sets `fStatus` after each `Minimize()`. Later
assignments override earlier ones in this order:

| fStatus | Condition | Meaning |
|---|---|---|
| 0 | none of the below | valid minimum, accurate covariance |
| 5 | `!HasPosDefCovar()` | covariance matrix not positive definite (normally HESSE failed, or the seed failed) |
| 1 | `HasMadePosDefCovar()` | covariance matrix was forced positive-definite (MnPosDef added to the diagonal) |
| 2 | `HesseFailed()` | HESSE not valid |
| 3 | `IsAboveMaxEdm()` | EDM above max: MIGRAD stopped with EDM > 10 x target (or EDM is NaN) |
| 4 | `HasReachedCallLimit()` | MIGRAD or its internal HESSE hit the call limit |
| 6 | invalid minimum but none of the above | "unknown failure" |

If the minimum is not valid (`IsValid()` false: invalid state, above max EDM, or call limit) the minimiser returns false, which
RooFit turns into **`-1` in the MINIMIZE history entry** (`RooMinimizer.cxx:263`). The reported `fStatus` is still the code above.

HESSE (`Minuit2Minimizer::Hesse`, `:1223-1315`): on success `fStatus` is left unchanged. On failure
(`!fState.HasCovariance()`) it adds **100 x hstatus** (`:1308`), with hstatus: 1 `HesseFailed` (second derivative zero or
call limit in HESSE), 2 `InvertFailed`, 3 covariance not positive definite, 4 no information on the minimum. MINOS adds
10 x mstatus (`:927`).

Therefore (all [source], values marked [verified] seen in runs):
- **0** valid and converged; **1** valid but covariance forced positive-definite (covQual 2); **2** HESSE invalid; **3** EDM
  above max at the end of MIGRAD; **4** call limit; **5** covariance not pos-def; **6** unknown.
- **302, 303, 304** (verified: empty histogram gives 302; strat2 on the empty histogram 303; MaxCalls(50) 304): MIGRAD status
  2/3/4 plus a failed HESSE with hstatus 3 (covariance not positive definite). The `100*` digit is the HESSE failure code.
  The project's log line "Error when calculating Hessian" accompanies these (also verified).
- **1** in the project run (a handful of cells) = covariance forced positive-definite.
- **-1** can appear as the final status only when the minimiser type could not be created ([verified] "Foo") or `hesse()`/`minos()`
  was called before MIGRAD. Note that a failed HESSE does NOT give -1: `calculateHessErrors` returns `ret |= update(ret)`, which is
  always true (`RooMinimizer.cxx:899`), so `_status` takes `fStatus` (including the +100 x hstatus).

### The RooFitResult fields [source `RooMinimizer.cxx:501-533`, `RooFitResult.h:76-86,166-169`]

- `status()` = `_status` of the LAST minimiser call (HESSE if `Hesse(true)`; with `SumW2Error` the history has an additional HESSE
  but `status()` is that of the last `exec`). `statusCodeHistory(i)` / `statusLabelHistory(i)` / `numStatusHistory()` give the sequence
  `MINIMIZE`, `HESSE`, (`HESSE`), (`MINOS`). [verified: `MINIMIZE=-1;HESSE=3`, `MINIMIZE=0;HESSE=0`, `MINIMIZE=0;HESSE=0;HESSE=0` with SumW2Error.]
- `covQual()` = `Minuit2Minimizer::CovMatrixStatus()` (`Minuit2Minimizer.cxx:1317-1342`): 3 full accurate; 2 full but forced
  positive-definite; 1 approximate only; 0 available but not pos-def; -1 not available. (RooFitResult's own printout labels 0
  "Not calculated at all", `RooFitResult.cxx:~495`.)
- `edm()` = EDM of the LAST state (after HESSE). `minNll()` = `fVal - offset`. `numInvalidNLL()` = number of function calls that
  returned an invalid NLL (counted in `applyEvalErrorHandling`, `RooAbsMinimizerFcn.cxx:278-298`); `determineStatus` prints "all
  function calls during minimization gave invalid NLL values!" if all of them did (`RooMinimizer.cxx:266-268`).
- `floatParsFinal()` values and errors, `constPars()`, `initPars()`; `correlationMatrix()`, `covarianceMatrix()`; global
  correlations print as `<none>` in the verbose printout for the project's fits [verified, k0s (0,0)].
- Detecting parameters at limits: nothing in RooFitResult does it. Check `|v - min| < eps*(max-min)` (Minuit2 limits use a sin transformation, so
  a parameter at a limit sits just inside it and its error is large). [verified approach used by the harness; e.g. `n2_sig` = 150.0 =
  its upper limit with error 22 in k0s (0,0).]

## Experiments

### What was verified (small scale)

Harness: `fitexp.cpp` (scratch copy of `DynamicRooFitter.h` with `private:` turned into `public:`), builds the project model per cell from
`FitConfigManager`, runs variants from a fresh model, and records status, status history, covQual, edm, minNll, numInvalidNLL, nsig +-,
signal-window yield +-, purity, time, parameters at limits, max |correlation|, and counts of log messages.

1. [verified] The harness `base` variant is the project's call; `manualEps1` (RooMinimizer by hand with eps 1.0, Strategy 1) reproduces it
   exactly, so the hand-built `RooMinimizer` path is a valid way to test tolerance.
2. [verified] k0s sample (cells (0,0),(1,26),(3,18),(5,10),(7,2),(8,28); `base`, `manualEps1`, Minuit, legacy, asymp, sumw2T):
   - base status: 3,3,3,0,3,3. covQual: 3,3,2,3,3,2. edm: 0.80, 0.52, 9.2e7, 0.17, 8.1, 0.074. `n2_sig` at its upper limit 150 in (0,0).
   - (3,18) (2.2e7 entries): edm 9.2e7, covQual 2, signal-yield error 178477 whereas Poisson sqrt(N) ~ 3.3e3. This cell is genuinely bad.
   - `Minimizer("Minuit","migrad")` (old Minuit) gives status 0 or 4 and in (3,18) edm 8555, yield error 3605, signal yield 1.09465e7 vs 1.09611e7 (-0.13%).
   - Signal yield (window) stable to <0.01% in the cells where covQual is 3; errors differ by up to 2x between variants.
3. [verified] xi cell (5,10) [683449 signal events]: base status 3, covQual 3, edm 0.0468, nsig 683449.576 +- 1024.08;
   `MaxCalls(50/300)` gives 304, covQual 0, no errors; `Extended(0)` nonsense; `Range` cut gives status 0 covQual 2 and yield 696330 (+1.9%).
4. [verified] Synthetic histograms (k0s configuration, 200 bins; `misc2.cpp`):
   - empty histogram: base status 302, covQual 0; strat2 303.
   - 10 entries: status 0 for all variants.
   - background only (2e5): base status 1, covQual 2 (nsig at lower limit 0.0018 +- 1017); offsetInit status 0.
   - signal only (2e5): base status 1, strat2 status 3.
   - 4e4 entries (mid): base status 3 (edm 0.0077), offsetInit status 0, strat2 status 1.
   - 2.5e6 entries (big): status 3 for base, offsetInit and strat2. strat2 gave edm 3.7e-4 yet status 3, confirming that `status` and `edm()` are independent.
   - Interpretation [inferred]: status 3 appears when |NLL| is large (many events) and the EDM cannot be resolved; `Offset("initial")` removed it on the 4e4-entry
     case but not on 2.5e6 entries.

### Real-cell experiments (done; details, tables and caveats in `09-fit-quality-and-yields.md`)
[verified] Harness `study09/study.cpp` reproduces `PurityTask`'s cells (merged analysis bins via `rebinning_pt`, and source bins; purity inputs `..2026_PtMod_6agosto.root` and `..2024_PtMod_5agosto.root`, NOT 23luglio), runs the project's `DoFit()` +
`ExtractYieldsAndPurity()` verbatim and 13 variants, all judged by the same common NLL. 3380 cell sets, 14 variant rows each. K0S, pi_tpc and pi_tof reproduce the archived `Data_20xx_*Purity.root` purities for all 2720 cells (Xi does not: 660 cells differ, cause unknown).
Results (merged analysis cells, 734 valid; project's call vs best of 11 deterministic fits):
- yield in the window off by >0.1% / >1% / >5% in 29% / 12% / 4% of cells (K0S 35% / 31% / 8%); |dS|/S median 1e-4, p90 1.9%, p99 25%; purity |dP|>0.001 in 21%, >0.01 in 9% (K0S 28%); propagated error off by a factor >2 in 24% (S) / 31% (P) of cells. Source cells: 20% / 6.9% / 3.3%.
- status 3 in 71% of analysis cells (K0S 94%, Xi 51%, pi_tpc 97.5%, pi_tof 36%); classes: precision limit 28%, not converged 52%, bad 20%. Status 0 is mostly right (5% of status-0 cells off by >1%). Base EDM of status-3 cells: median 4.95, p90 3.7e9.
- `Optimize(0|1|2)`, omitted `Optimize`, `Hesse(true)`, `PrintEvalErrors(-1)`, `Warnings(false)`, `Strategy(1)`: bit-identical to the project's call in all 1674 non-empty cells of the 2026 file (this closes the inferred item 6 above).
- `Strategy(2)` is worse than the project's call (dS>1% in 158 vs 90 cells), `Offset(true)` alone does not help the yield on merged cells (103 vs 90), `RecoverFromUndefinedRegions(0|100)` changes nothing. What helps: restarts. `Offset(true)` + one refit from the result: dS>1% in 22 cells (3.0%); plus escalation to xRooFit when covQual!=3 or edm>=0.01: 10 cells (1.4%), 33 cells (4.5%) stay flagged; CPU x1.6 / x2.
- Independent of convergence: `nsig` max 1e8 clips the K0S yield in 18 analysis cells of the 2026 file (S off by a median 22%, up to 78%; P by up to 0.06); a 15-start search finds lower minima in 10% of cells (yield differences up to 1.7%); Xi `sigma_sig` at its lower limit in 125/140 cells.
- Follow-up (09 section 9) [verified]: the Xi mismatch with the 7 Aug archive was caused by the Xi observable range change [1.2,1.4] -> [1.29,1.36] (27 Aug). With paired multi-starts, the Xi `sigma_sig >= 0.002` limit binds in 122/140 analysis cells (S -0.56% median), Xi `n >= 1` binds in 16 (relaxed n~0.3 is unphysical), DSCB `n` upper limits bind but move S <0.04%, `alpha` and pi_tpc `nbkg >= 0` never bind. Among solutions within dNLL<0.5 of a 50-start best the yield varies by <=1.6e-4: the 1-2% spreads are distinguishable higher minima, not an ambiguity. `Offset("bin")` + refit: accepted 705/734 vs 647/734 for `Offset(true)` + refit. Fixing the DSCB tails cuts MIGRAD calls 3-4x but biases S (measurement only). Purity-bias toys: note 12.
- xRooFit's `minimize()` needs its `MaxFunctionCalls` capped (default -1: its old-Minuit fallback ran >5 min on one cell).

## Log messages explained

| Message | Printed at | Meaning |
|---|---|---|
| `RooAbsMinimizerFcn: Minimized function has error status.` / `Returning maximum FCN so far (X) to force MIGRAD to back out of this region. Error log follows.` | `RooAbsMinimizerFcn.cxx:247-270` (text at 252-256), called from `applyEvalErrorHandling` (`:278-298`) when the NLL is non-finite, `numEvalErrors()>0` or `>1e30`. | At one parameter point the likelihood was invalid. With `EvalErrorWall` on (default) Minuit gets `maxFCN + RecoverFromUndefinedRegions-strength x badness` instead, so MIGRAD backs out. `numInvalidNLL()` counts these. With `EvalErrorWall(false)` the text is "...has error status but is ignored." |
| `p.d.f value is less than zero, trying to recover` | `RooNormalizedPdf.cxx:49` (compiled/cpu backend path); `RooAbsPdf.cxx:291` is the legacy variant with `(%f)`; batch kernel `ComputeFunctions.cxx:650-653`. | the (unnormalised) pdf is negative at a bin centre. The project's Chebychev2 background does this for parameters outside the region where the polynomial is positive; the value is packed into a NaN so that the minimiser gets a gradient away (`RooNaNPacker`). Seen in the project log with `Chebychev2_bkg=1.65, denominator=0.12` style lines. |
| `getLogVal() top-level p.d.f evaluates to NaN` | `RooNLLVarNew.cxx:273` (cpu backend; `RooAbsPdf.cxx:205` legacy) | the same event as above one level up: the packed NaN reaches the top-level RooAddPdf. In the project log it comes together with the "less than zero" lines in the same error block (14982 NaN vs 13688 "trying to recover" in `log_output_globalConfigPuritypp.log`) [verified by grep]. Counted in `ReduceNLLOutput.nNaNValues` (`RooBatchCompute.cxx:245-255`). |
| `sum of coefficients is zero` | `RooAddHelpers.cxx:260` (warning, "WARNING: sum of coefficients is zero 0") and `:314` (error, "sum of coefficients is zero.") in `updateCoefCache` | the yields `nsig + nbkg` of the RooAddPdf are 0, so the coefficients cannot be normalised. 10000 lines in `log_output_globalConfigPuritypp.log`, 6250 in `MCClosurepp` [verified by grep]. **Cause [verified, 09 section 9.7, `soc.cpp`]: only the EMPTY pi_tof cells (all content in under/overflow; 16 in the 2026 file) emit it, 12 times in MIGRAD and 5 in HESSE per fit, because MIGRAD drives nsig and nbkg to 0 (status 302); 0 occurrences in all 920 pi_tpc fits.** The earlier reading "start of a pi_tpc fit" came from the plot lines of the previous fit in the log. Harmless for RooFit; the cell's purity is 0/0 = NaN. |
| `a likelihood fit is requested of what appears to be weighted data...` | `FitHelpers.cxx:502-515` | the dataset has non-Poisson weights (`isNonPoissonWeighted`) and neither `SumW2Error` nor `AsymptoticError` was given. Not present in the project logs (0 hits) [verified by grep]. The project data are RooDataHist with integer contents. |
| `RooMinimizer::calculateHessErrors() Error when calculating Hessian` | `RooMinimizer.cxx:891` | `Minuit2Minimizer::Hesse()` returned false (covariance not available) and status gets +100 x hstatus. [verified] with MaxCalls(50) (304) and the empty histogram (302/303). 34 hits in the project Purity log, 0 `all function calls during minimization gave invalid NLL values` (`RooMinimizer.cxx:267`). |

## Recommendations (number-backed; see 09 sections 3.6 and 4)
1. [verified] Do not use `status()` as a quality flag: record `status % 100`, `status / 100`, `covQual()`, `edm()`, `numInvalidNLL()`, the status history per cell.
2. [verified] Change `DoFit()` to: fit with `RooFit::Offset(true)`, refit once from the result with `Offset(true)`, accept if `covQual()==3 && edm()<0.01`, otherwise run `xRooFit::createNLL(...).minimize()` (cap `MaxFunctionCalls`) and keep the lower NLL; flag the cell if still not accepted. S off by >1% drops from 12.3% to 1.4% of analysis cells; CPU x2.
   The acceptance test "edm < 10*EPS2*|NLL|" on raw-NLL fits is NOT safe (40% of accepted cells wrong).
3. [verified] `fitTo` returns `nullptr` without `Save`; guard it. `Tolerance`/`Improve`/`NormRange` are not `fitTo` options.
4. [verified] Raise the `nsig`/`nbkg` limits of the K0S and pi_tpc sections of `fitConfig.json` (cells with N > 1e8 are clipped) and revisit the Xi `sigma_sig` lower limit.
5. [verified, 09 section 9.6] `Offset(true)` and `Offset("initial")` are identical (737/737 cells); `Offset("bin")` + one refit converges better (accepted 705/734 vs 647/734; S off >1% vs the best in 17 vs 22 cells) at the same CPU: a candidate for steps (a)-(b) of recommendation 2. `Optimize` is a numeric no-op here; `Strategy(2)` and `RecoverFromUndefinedRegions` do not help.

## Pitfalls

- [verified] A `RooFitMessages::Configure()` in the process captures `std::cout.rdbuf()` at its first call; if a harness redirects `std::cout`
  to a temporary stringstream before the first `DynamicRooFitter` is constructed, RooFit afterwards writes into a dangling buffer (this produced crashes in
  the first version of the harness). Replace the RooMsgService stream `os` pointer instead (see `fitexp.cpp`, `gCap`).
- [verified] `alienv printenv` fails or returns nothing when several shells call it at once (it rebuilds `MODULES`). Use the manual environment in
  `env.sh` (below), which also needs `ROOT_DYN_PATH` and `ROOT_INCLUDE_PATH` for Vc, otherwise cling fails with "Vc.pcm not found".
- [verified] Unknown options only print an error; `Minimizer("GSLMultiMin",...)` segfaults in this build.
- [verified] `Hesse(true)` is the default, and `Strategy(1)` is the default; adding them changes nothing.
- [source] The `edm()` of the result is post-HESSE, not MIGRAD's final EDM.
- [source] Chebychev2 backgrounds with free c1, c2 in [-1,1] can go negative, producing invalid NLL points: not a problem by itself (the wall handles it), but it adds
  `numInvalidNLL` and slows MIGRAD.

## Resume here

Done: the real-cell analysis, the Optimize/Hesse/... bit identity, `Offset("initial")`/`Offset("bin")`, `IntegrateBins` on real cells, the `sum of coefficients is zero` cause, the Xi archive mismatch, the limit relaxation (all in 09 section 9). NOT DONE: `Offset("bin")` combined with the escalation and the multi-start; `IntegrateBins` on the source cells. Purity-bias toys: note 12; in-sequence escalation: note 11.
Update 15 Oct (note 09 final pass, 9.9-9.10): both NOT DONE items above are DONE: `Offset("bin")` in the escalation/multi-start changes acceptance (3x fewer escalations) but not the minima or yields reached; IntegrateBins on source cells moves S by +0.01% median (Xi +0.04%, max 1.7%).
Scratch: `scratch/study09/` (harness, TSVs, analysis scripts, `cells-master.tsv`); older material in `scratch/fitting-api/` (its partial TSVs are superseded).
