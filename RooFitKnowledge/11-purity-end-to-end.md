# 11 - The purity step end to end: current DoFit() against the recommended procedure (14-15 October 2026)

Two sessions. Session 1 (14 Oct) built the copy, reproduced the archive and ran the new procedure. Its xRooFit step did not match the study09 harness.
Session 2 (15 Oct) found the cause, fixed it in the copy and reran everything. It then compared every cell with note 09, ran the correlation step on both purity outputs and searched O2Physics for the map consumer.
All numbers below come from session 2. Session-1 files are kept with the suffix `-session1`.
Provenance tags: [source] code read; [verified] run here (the script, log or output is named); [inferred] my conclusion. Environment: ROOT 6.36.10 (ALICE build), macOS arm64, compiled binary built with CMake, environment from `scratch/rooenv.sh`, no alienv.
Scratch dir: `S=/Users/scannito/alice/Analysis/RooFitKnowledge/scratch/study11/`. Nothing in the project was touched.
`git status --porcelain` was identical before and after: `status-before-s2.txt` / `status-after-s2.txt`. The sha of every `Codes/*.h` is unchanged (`codes-sha-start.txt`). No timing conclusions are drawn: other jobs ran at the same time.

---------------------------------------------------------------------------------------------------------------------------------

## SUMMARY

1. **Baseline.** The project's unmodified purity step reproduces the archived `Purities/Data_2024_*`, `Data_2026_*` files and CCDB maps **bit for bit** for K0S, pi_tpc and pi_tof. Xi differs, as already known. [verified]
2. **Why the escalation first disagreed with the harness.** `xRooFit::minimize` keeps a **fit cache in `gDirectory`**. That cache is keyed on the NLL name (`nll_model`), the parameter names and the constant parameters only, never on the data.
   Inside PurityTask, `gDirectory` is the writable output file. Every escalated cell with the same model therefore got back the cached fit of an EARLIER cell, and the cache (`nll_model/...`) was written into the purity files.
   The fix is `TDirectory::TContext noFitCache{nullptr}` around the xRooFit call. After it, all 353 escalated cells match the harness's `xrf` NLL (relative 1e-9) and the harness's choice. [verified]
   Flagged cells: **35 analysis / 85 source** (note 09 predicted 33/734 and 81; the difference is the 3 analysis cells that 09 excluded as S=0) [verified + inferred].
3. **Purity changes, new procedure against the project's.** Analysis cells: |dP|>0.001 in 151 of 737, >0.01 in 71, >0.05 in 8. The new purity equals the harness's prediction (P7) in **every** non-NaN cell (|d|<1e-6). The shifts match 09's base-to-best prediction in cell and sign: of the 119 analysis cells that move by >0.001 in both, the sign agrees in 118. [verified]
4. **Downstream.** With `apply_purity: true` in a copy, the purity correction multiplies the associated spectrum bin by bin. The spectra change exactly by P_new/P_old: K0S median 1.4e-4, p90 2.7%, max 20.6%.
   Final quantities:
   - measured K0S multiplicity trend: up to 1.9%.
   - extrapolated pion yield: up to **9.4%** from purity changes of 0.5% or less. The Levy-Tsallis extrapolation amplifies them.
   - K0S/pi and Xi/pi extrapolated ratios: up to 10%.

   This does NOT reach the project's default results: `apply_purity` is false there. Efficiency had to be switched off for this run because the current `Corrections/*.root` cannot be read by CorrelationTask (section 6). [verified]
5. **CCDB map.** No code in O2Physics reads the purity maps. That covers master at 26 Sep 2026 and the user's branches closure, corrections, update and xi. `phiStrangeCorrelation` takes only efficiency maps from CCDB. So the map shifts (k0s p90 2.3%, max 19%) have no consumer today. [verified by search]

---------------------------------------------------------------------------------------------------------------------------------

## 1. What the purity step writes and who consumes it [source: PurityTask.h, WorkflowManager.h, CorrelationTask.h, CorrelationTaskBase.h, globalConfig*pp.json]

- **Tasks.** `globalConfigPuritypp.json` runs `purity_task_2024` and `purity_task_2026` (inputs `AnalysisResults_136_2024_PtMod_5agosto.root` and `..._2026_PtMod_6agosto.root`). Particles: k0s, xi, pi_tpc, pi_tof. Each cell runs `DynamicRooFitter(...).DoFit()` and then `ExtractYieldsAndPurity()`, and only `purityAndError` is stored. The status, covQual, EDM and NLL are not stored.
- **Outputs** (`output_dir ../DataFile/pp/DeltaY/Purities/`):
  - `Data_<year>_{K0S,Xi,Pi}Purity.root` (pi_tpc and pi_tof share the Pi file), containing:
    - `<binning>/Summary/h1<key>Purity_multBin<i>`: analysis binning, from fits of the MERGED source bins.
    - `.../Summary/SourceBinning/..._sourceBinning`.
    - fit canvases, summary canvases, binning stamp, Provenance.
  - CCDB maps `Purities/MoreFullDifferential/Data_<year>_h2PurityMap<key>.root`: object `ccdb_object`, a TH2F (multiplicity, pT) at the source binning.
- **Consumer in this project:** `CorrelationTask::LoadPurities` reads `h1<key>Purity_multBin<i>` for k0s, xi and pi_tpc. pi_tof is never read.
  `CorrelationTaskBase::GenerateSpectraAndTrends` then does `h1Spectrum->Multiply(hPurity)`: after the 1/N_trig and 1/(2 dy) normalisation, before the extrapolation, the trends and the ratios.
  This happens only if `apply_purity` is true. It is false in `correlation_base_settings` (`globalConfigBasepp.json`), and only the older `globalConfigDatapp_OnlineEffonly.json` sets it true.
- **Consumer outside:** the CCDB maps are meant for O2 (comment in PurityTask: "O2 applies it to single candidates"). Section 5 shows that no O2Physics code reads them.

## 2. The scratch copy and how to rerun [verified]

- **`S/base/`**: `CMakeLists.txt` copied from the project, and `Codes` = symlink to the project's Codes. That is the working tree, so it includes the owner's current uncommitted `DynamicRooFitter.h` and `RooFitMessages.h`; a copy is kept as `DynamicRooFitter.h.project-copy`.
- **`S/new/`**: `Codes/` holds per-file symlinks to the project except `DynamicRooFitter.h`, which is a modified copy. Its `CMakeLists.txt` adds `RooFitXRooFit` to `find_package` and `target_link_libraries`.
  Build both with `cmake -B <v>/build -S <v> && cmake --build <v>/build` (Release).
- **Run directories `S/run-<tag>/`**:
  - The binary runs from `run-<tag>/Codes`, so the JSON's `../` paths resolve inside scratch and no path in the JSON needed editing.
  - `JSONConfigs/` is a copy (fitConfig.json sha `4ec80c09...`, equal to the project's).
  - `DataFile/pp/DeltaY/Data/` holds symlinks to the two input files.
  - Outputs go to `run-<tag>/DataFile/pp/DeltaY/Purities/`.
  - Tags: `base` (project code), `new` (procedure ON, with the cache fix), `newcache` (procedure ON, without the fix, as in session 1), `newoff` (new binary, procedure OFF).
- **Change in `new/Codes/DynamicRooFitter.h`**:
  - `DoFit()` keeps the project's body verbatim. If env `PHI_DOFIT_IMPROVED` is set and not "0", it calls `DoFitImproved`.
  - `DoFitImproved` steps:
    1. `fitTo(..., Offset(true))` with the project's other options.
    2. One refit from the result, again with `Offset(true)`.
    3. Accept if `covQual()==3 && edm()<0.01`.
    4. Otherwise: reset the free parameters to the config's start (`setVal`, `removeError`); run `xRooFit::createNLL(*model,&dataHist).minimize()` with BoundaryCheck 0.01 and MaxFunctionCalls 1e5, **inside `TDirectory::TContext noFitCache{nullptr}`**; keep it only if its no-offset NLL is strictly lower.
  - The NLL used for the comparison is a separate `createNLL(dataHist, Extended(true))`, evaluated at the final values.
  - `fitResult` is a clone of the chosen result and the model parameters are assigned from it, so `ExtractYieldsAndPurity` is unchanged.
  - Recording: env `PHI_DOFIT_DIAG=<file>` appends one row per cell (status, covQual and EDM of fit, refit and xRooFit; the pick; final flags; NLLs). A cell still not accepted logs one constant `Logger::Warn`.
- **Rerun the purity step**: `./runpurity.sh <base|new> <tag> <0|1>`, e.g. `./runpurity.sh new new 1` (about 15 s per run here).
  Dump with `root -l -b -q 'dump.C("run-<tag>/DataFile/pp/DeltaY/Purities","dump-<tag>.tsv")'`. Compare with:
  - `python3 cmp.py dump-archive.tsv dump-base.tsv`: archive check.
  - `python3 diag.py diag-new.tsv`: acceptance tallies.
  - `python3 xcmp.py diag-new.tsv`: escalation against the harness.
  - `python3 cmp09.py`: cell by cell against note 09; output `res-cmp09.txt`.
- **Rerun the downstream step**: `./runcorr.sh <base|new>` (section 4), then `root -l -b -q 'dumpfinal.C("run-corr-<v>/DataFile/pp/DeltaY/FinalResults","final-<v>.tsv")'` and `python3 cmpfinal.py` (output `res-cmpfinal.txt`).
- **Harness replica**: `xtest.cpp` (build `bash xbuild.sh xtest`) fits one cell six ways. It showed that, in isolation, the reset-then-xRooFit escalation reproduces the harness exactly.

## 3. Baseline against the archive [verified, `cmp.py dump-archive.tsv dump-base.tsv`]

- The unmodified DoFit, through the whole PurityTask, reproduces the 7 Aug archive EXACTLY for K0S, pi_tpc and pi_tof: every cell of the analysis spectra, the source spectra and the CCDB maps of both files. Max |dP| = 0 and max |d error| = 0, with NaN positions identical.
- Xi differs in all cells (max |dP| 0.11); not investigated here.
- The new binary with `PHI_DOFIT_IMPROVED` unset (`run-newoff`) gives output byte-identical to the baseline dump, and the same logger summary.

## 4. New procedure against the baseline

### 4.1 Why session 1's xRooFit escalation did not match the harness, and the fix [verified]

Checked one difference at a time:

- **Options:** identical to the harness (BoundaryCheck 0.01, `kXrfMaxCalls` = 1e5, the same no-offset `createNLL(Extended)` used to compare).
- **Offset refit:** reproduced the harness's `off_rst1` in all 3380 cells already in session 1.
- **Single-cell replica (`xtest.cpp`, K0S 2024 source (1,29)):** every variant gives the harness NLL -1491633.995384 (status 0, covQual 3): fresh fitter; reset after the offset fits with `removeError` or with `setError(0)`; and a fresh second fitter. So start point, step sizes, offset state and the reused fitter are NOT the cause.
- **Cause:** in the full run the same cell got -853252.
  - xRooFit's source (`roofit/xroofit/src/xRooFit.cxx`, lines 779-840 and 1429ff) caches every fit result in `gDirectory/<nll name>/`.
  - On the next `minimize` it returns a cached result if the floating parameter names and the constant-parameter values match. The data are not part of the key.
  - In PurityTask `gDirectory` is the open, writable output file. Every escalated cell after the first with the same model (and the same fixed parameters) therefore received an earlier cell's fit.
  - The study09 harness had the read-only input file as `gDirectory`: no cache could be written, hence no hit.
  - Proof: `run-newcache` (session 1 code) has 20 `nll_model` directories inside the purity files; `run-new` and `run-base` have none. Session 1's numbers are reproduced exactly by `run-newcache` (`dump`, `diag` identical).
- **Fix:** `TDirectory::TContext noFitCache{nullptr}` around the xRooFit block.
- **After the fix (`xcmp.py diag-new.tsv`):**
  - offset refit equal to the harness in 3380 of 3380 cells;
  - xRooFit NLL equal to the harness's `xrf` in 353 of 353 escalated cells;
  - choice between them equal in 353 of 353.
  - The output files have the same directory and class structure as the baseline (key listing identical for `Data_2026_PiPurity.root`).

### 4.2 Acceptance [verified, `diag.py diag-new.tsv`]

| set | non-empty cells | accepted after offset+refit | escalated | xRooFit chosen | still flagged |
|---|---|---|---|---|---|
| analysis 2024 | 370 | 338 | 32 | 26 | 12 |
| analysis 2026 | 367 | 309 | 58 | 48 | 23 |
| analysis both | 737 | 647 (87.8%) | 90 | 74 | **35** (4.7%) |
| source both | 2627 | 2380 | 247 | 191 | **85** |

Flagged analysis cells by particle: k0s 8, xi 0, pi_tpc 21, pi_tof 6. Session 1 (cache bug) had 80 analysis / 234 source.
Note 09 predicted 33 of 734 analysis cells; the harness's P7 flags, recomputed over the same 737 cells, give 35 (`res-cmp09.txt`). The 3 extra cells are those 09 excluded as S=0 at the best fit [inferred].

### 4.3 Purity, cell by cell, against the baseline and against note 09 [verified, `cmp09.py` -> `res-cmp09.txt`]

**Reproduction:** in every non-NaN cell (3364, analysis and source, both files), base purity = harness `base` P and new purity = harness P7 P (|d| < 1e-6, float32 storage).

Analysis cells:
| key | cells | run new-base \|dP\|>0.001 | >0.01 | >0.05 | max | 09 base-to-best >0.001 | >0.01 | moved in both / only in run / only in 09 | sign agrees | flagged | residual \|new-best\|>0.001 / >0.01 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| all | 737 | 151 | 71 | 8 | 0.92 | 154 | 67 | 119 / 32 / 35 | 118/119 | 35 | 77 / 9 |
| k0s | 200 | 71 | 57 | 5 | 0.147 | 69 | 55 | 64 / 7 / 5 | 63/64 | 8 | 17 / 4 |
| xi | 140 | 9 | 4 | 0 | 0.020 | 26 | 4 | 8 / 1 / 18 | 8/8 | 0 | 20 / 0 |
| pi_tpc | 200 | 57 | 1 | 0 | 0.040 | 48 | 1 | 39 / 18 / 9 | 39/39 | 21 | 28 / 0 |
| pi_tof | 197 | 14 | 9 | 3 | 0.92 | 11 | 7 | 8 / 6 / 3 | 8/8 | 6 | 12 / 5 |

Source cells (= CCDB map cells): run new-base |dP|>0.001 in 410 of 2627, >0.01 in 129, >0.05 in 24. Note 09 predicted 399 and 130. 339 cells move in both, and the sign agrees in 332.

Reading the table:
- "Only in 09" = cells where the procedure does not reach 09's best fit. This is mostly Xi (18): the multi-minimum cells of 09 section 3.7.
- "Only in run" = cells where the new fit moves although 09's best equals the base within 0.001.

Largest analysis shifts (pT bin is 0-based; d = new - base; "09 best" is note 09's best deterministic fit):
| year | key | mult | pT | base | new | d | 09 best | flagged |
|---|---|---|---|---|---|---|---|---|
| 2024 | pi_tof | 8 | 0 | 0.936 | 0.016 | -0.920 | 0.013 | no (127 counts) |
| 2026 | k0s | 7 | 7 | 0.714 | 0.862 | +0.147 | 0.862 | no |
| 2024 | pi_tof | 0 | 0 | 0.352 | 0.242 | -0.110 | 0.352 | no |
| 2024 | pi_tof | 1 | 0 | 0.015 | 0.094 | +0.080 | 0.107 | no |
| 2026 | k0s | 4 | 4 | 0.896 | 0.966 | +0.070 | 0.966 | no |
| 2026 | k0s | 5 | 4 | 0.971 | 0.905 | -0.066 | 0.905 | no |
| 2026 | k0s | 9 | 4 | 0.904 | 0.968 | +0.064 | 0.968 | yes |
| 2026 | k0s | 8 | 4 | 0.979 | 0.921 | -0.058 | 0.921 | no |

Other observations:
- K0S pT bin 4 (1.2-1.6 GeV/c, the Chebychev2 override bin) dominates the K0S shifts.
- Direction of the >0.01 analysis shifts: 56 up, 15 down. In those cells the project's fit mostly underestimates the purity.
- Purity error ratio new/base: median 1.00, but outside [0.5, 2] in 211 of 732 analysis cells. The old HESSE error was computed at an unconverged point.

### 4.4 Logger summary [verified, `log-base.txt`, `log-new.txt`, `log-newcache.txt`]

| run | framework warnings | RooFit lines (kinds) |
|---|---|---|
| base | `No warnings.` | 63222 (24) |
| new | 136: all the "cell flagged" warning added in the copy; = 38 analysis + 98 source cells, including the 16 empty pi_tof cells (3 + 13) | 73202 (18) |
| newcache (session 1) | 330 | 86082 (29) |

## 5. Downstream

### 5.1 The correlation step with the two purity outputs [verified, `runcorr.sh`, `log-corr-*.txt`, `final-*.tsv`, `cmpfinal.py` -> `res-cmpfinal.txt`]

**Setup.**
- Copies of `globalConfigDatapp.json` and `globalConfigBasepp.json` (`run-corr-<v>/JSONConfigs/corr*.json`): `active_tasks` = the six `correlation_task_*` blocks (offline, online, online no-phi, for 2024 and 2026); the `phi_fit_task` blocks were removed because they would rewrite ProjData.
- `apply_purity: true`.
- `apply_efficiency: false`, because the current Corrections files cannot be read (section 6, item 2).
- Inputs: the projection cache, trigger files and corrections were COPIED into `S/inputs-corr/` (read-only). `Purities` is a symlink to `run-base` or `run-new`. `FinalResults` is a scratch directory.
- Both runs complete with "No warnings". The base run is deterministic (rerun byte-identical).

**What this measures.** The pure effect of the purity change on every histogram the correlation step writes.

**Caveat (efficiency off) [inferred]:**
- The spectrum change is exactly the purity ratio, efficiency or not.
- The measured trends are integrals of the spectrum and change by a purity-weighted average, so they change slightly with efficiency.
- The extrapolated quantities depend on the spectrum SHAPE, which efficiency changes, so their sensitivity is indicative only.

Relative change new/base - 1 per bin, all six output files, three dy windows:
| quantity (first purity-corrected first) | bins | median | p90 | max | n>1% |
|---|---|---|---|---|---|
| spectrum K0S `h1SpectrumPhiK0S_dy*_multBin*` | 1800 | 1.4e-4 | 2.7% | 20.6% | 513 |
| spectrum Xi | 1260 | 6.3e-5 | 1.6e-4 | 2.2% | 36 |
| spectrum Pi | 1800 | 6.9e-5 | 0.29% | 4.2% | 9 |
| measured trend K0S `h1MultTrend_K0S` | 180 | 0.37% | 1.1% | 1.9% | 24 |
| measured trend Xi / Pi | 180 / 180 | 8.7e-5 / 3.2e-4 | 0.27% / 0.14% | 0.49% / 0.20% | 0 / 0 |
| extrapolated yield K0S `h1MultTrendExtrap_K0S` | 180 | 0.36% | 1.1% | 1.9% | 24 |
| extrapolated yield Xi | 180 | 9.2e-5 | 0.31% | 0.78% | 0 |
| extrapolated yield Pi | 180 | 0.13% | 2.1% | **9.4%** | 21 |
| ratio K0S/Pi measured / extrapolated | 180 / 180 | 0.44% / 0.66% | 1.2% / 1.8% | 1.9% / **10.2%** | 24 / 45 |
| ratio Xi/Pi measured / extrapolated | 180 / 180 | 4.7e-4 / 9.9e-4 | 0.22% / 2.0% | 0.45% / **10.7%** | 0 / 21 |

**Checks and observations.**
- The largest spectrum change, +20.61% (K0S 2026, mult 7, pT bin 7), equals 0.8617/0.7144 - 1: the spectrum moves exactly by the purity ratio.
- **Amplification by the pion extrapolation.** Largest case: 2026, multiplicity bin 2, -9.4%.
  - In the corresponding cells the pi_tpc purity moves by at most 0.3% (pT bins 8 and 9: -0.0031, +0.0023), and the measured pion trend by at most 0.2%.
  - Yet the extrapolated pion yield moves by up to 9.4%, and `Ratio_Extrap_Meas_Pi` moves by the same amount.
  - The Pi model in `extrapConfig.json` is Levy-Tsallis fitted on [0.2, 1.0] GeV/c over a domain [0, 10]. The yield above the last measured point (3 GeV/c) comes from a fit that ignores 1-3 GeV/c, so per-mille changes in the fitted points move the high-pT tail by percent [inferred].
  - This is a property of the extrapolation, not of the purity fit. It shows that the extrapolated pion yield is sensitive at the 10% level to per-mille input changes.

### 5.2 The CCDB purity map in O2Physics [verified by search, read-only]

- I searched `/Users/scannito/alice/O2Physics` (master, last commit 2026-09-26 `ebc24712a`, plus local and remote branches `closure`, `corrections`, `update`, `xi` with `git grep`) for `h2PurityMap`, `PurityMap`, `purity`, `ccdb_object` and `Users/s/scannito`.
- The project's tasks are `PWGLF/Tasks/Strangeness/phiStrangeCorrelation.cxx` and `PWGLF/TableProducer/Strangeness/phiStrangeCorrelator.cxx`.
  - They read from CCDB only efficiency maps (`ccdbEfficiencyPath` = `Users/s/scannito/Efficiencies/h3EffMap<particle>`).
  - They weight each candidate by 1/efficiency (`computeWeightAndError`, `weight = 1/effTot`).
  - They contain no purity code on any branch.
- The only purity in CCDB code is `phik0shortanalysis.cxx` (`ccdbPurityPath` = `Users/s/scannito/PhiPuritiesData`, a TList of phi purity functions), unrelated to these maps.
- **So the h2PurityMap files have no consumer today, and the purity shifts have no effect on what O2 produces.**
- If a per-candidate purity weight were added the same way the efficiency weight is, each candidate's weight would change by P_new/P_old of its (multiplicity, pT) cell [inferred]. Over the map cells (`res-ccdb-ratio.txt`):

| map | median | p90 | max | cells >1% | cells >5% |
|---|---|---|---|---|---|
| k0s | 1.1e-4 | 2.3% | 19% | 111 of 680 | 33 |
| pi_tpc | 8.6e-5 | 0.27% | 14% | 4 | 4 |
| xi | 5.4e-5 | 1.1e-4 | 9.3% | 4 | 1 |

  pi_tof has cells with purity near 0, where the ratio is meaningless (max 1e4).

## 6. Problems found in the project (listed, not fixed)

1. **The purity correction is off in the standard Data run** (`apply_purity: false` in `correlation_base_settings`), so the purity step changes no final number in the default chain [source].
2. **The current Corrections files cannot be consumed by CorrelationTask** [verified]. `Corrections/2024_Corrections.root` (written 14 Oct 15:50) holds TH1D under `MoreFullDifferential/AccEff/MultBin` and has no `SigLoss` directory. CorrelationTask throws: `Missing object 'h1PhiEfficiency_multIntegrated' in '../DataFile/pp/DeltaY/Corrections/2024_Corrections.root', or it is not a TH1F`. MCTask now writes TH1D while `LoadCorrections` requires TH1F. The message comes from the first `runcorr` attempt, with efficiency on; that log was overwritten by the efficiency-off rerun. The Data chain with efficiency cannot run until the two agree.
3. **The xRooFit fit cache** (section 4.1): anyone adopting note 09's escalation inside PurityTask must clear `gDirectory` around `minimize()`. Otherwise later cells silently get earlier cells' fits and the cache is written into the output files. This comes from ROOT, not from the project code, but the project's practice of keeping an output file as `gDirectory` while fitting makes it bite.
4. **Nothing reads the fit status, covQual or EDM** in PurityTask/DynamicRooFitter. The baseline summary says "No warnings" while 63k RooFit lines are logged and most fits are status 3 [verified].
5. **The purity map has no consumer in O2Physics**, although PurityTask's comment says O2 applies it per candidate. pi_tof maps and spectra are written but `purity_sources` never reads pi_tof [verified search / source].
6. **Pion extrapolation is unstable**: per-mille changes of the input points move the extrapolated pion yield and the K0S/pi and Xi/pi extrapolated ratios by up to ~10% (fit range [0.2, 1.0] extrapolated to 10 GeV/c) [verified number, inferred cause].
7. **0/0 = NaN purity** for empty pi_tof cells, with `TGaxis::PaintAxis: wmin == wmax` errors on stderr when they are drawn (`err-base.txt`) [verified].
8. **The project's CMakeLists does not link `RooFitXRooFit`**; the escalation needs it (and the library loaded in Cling) [verified].
9. **All paths are cwd-relative** (`runAnalysis.sh`, the JSON, `GitCommit("../.git")`). A run from another directory resolves elsewhere without error. This is how the scratch redirect works, but it is also a hazard [source].
10. **Moving the scratch directory lost the symlinks and the ROOT outputs** (the move apparently skipped them); they were recreated and rerun in session 2. This is not a project problem.

## Resume here

Done: everything in sections 1-6. That covers the escalation discrepancy (found, fixed, matched), the cell-by-cell comparison with note 09, the flagged count, the downstream correlation run on both outputs and the O2Physics search.

NOT DONE:
1. The downstream run WITH efficiency. It needs Corrections files readable by CorrelationTask (problem 2), or the Aug corrections.
2. The extrapolated-yield numbers with efficiency (the shape changes).
3. Why the pion extrapolation amplifies (scan the fit range, look at `extrapolatedYieldHigh`).
4. The Xi archive mismatch (another researcher).
