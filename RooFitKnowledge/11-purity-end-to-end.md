# 11 - The purity step end to end: current DoFit() against the recommended procedure (14-15 October 2026)

Two sessions plus two final passes (sections 7, 8-9). Session 1 (14 Oct) built the copy, reproduced the archive and ran the new procedure. Its xRooFit step did not match the study09 harness.
Session 2 (15 Oct) found the cause, fixed it in the copy and reran everything. It then compared every cell with note 09, ran the correlation step on both purity outputs and searched O2Physics for the map consumer.
All numbers below come from session 2. Session-1 files are kept with the suffix `-session1`.
Provenance tags: [source] code read; [verified] run here (the script, log or output is named); [inferred] my conclusion. Environment: ROOT 6.36.10 (ALICE build), macOS arm64, compiled binary built with CMake, environment from `scratch/rooenv.sh`, no alienv.
Scratch dir: `S=/Users/scannito/alice/Analysis/RooFitKnowledge/scratch/study11/`. Nothing in the project was touched.
`git status --porcelain` was identical before and after: `status-before-s2.txt` / `status-after-s2.txt`. The sha of every `Codes/*.h` is unchanged (`codes-sha-start.txt`). No timing conclusions are drawn: other jobs ran at the same time.

---------------------------------------------------------------------------------------------------------------------------------

## CONCLUSIONS OF THE LAST PASS (15 October 2026 night; sections 8-9; scratch `S3=/Users/scannito/alice/Analysis/RooFitKnowledge/scratch/study-last-11/`)

For the project owner. These supersede item 3 below where they disagree.

1. **Random starts through the real purity step (note 12's rule: 2 starts per cell, 8 in Xi pT 1.2-2.0)** [verified, section 8]. On top of the recommended procedure they move the analysis purities by > 0.001 in 89 of 737 cells, > 0.01 in 9, never > 0.05 (max 0.040). Against the lowest NLL ever found per cell (study09, including its 15-start search) the wrong minima drop from 180 to **55** analysis cells (493 to 121 source). They do not finish the job: **Xi 2026 pT 2.0-3.0 is also multimodal** (outside the rule's 1.2-2.0), and even 8 starts there plus 4 in pi_tpc (34 wrong left) leave 6 Xi cells 0.010-0.015 away from the best-fit purity. Flagged cells rise from 38 to 80, because the deeper pi_tpc minima have `nbkg` at 0 and covQual < 3: there the flag means "the error is unusable", not "the value is wrong". Cost 2.5× the recommended procedure (53 s vs 21 s for both files; noisy).
2. **Downstream, efficiency on, against the project's purities** [verified, section 8.3]: measured K0S trend up to 1.16% (rec: 1.07%), measured Ξ and π trends ≤ 0.54% and 0.26%, measured ratios ≤ 1.13%. The starts add at most 0.2-0.4% to any measured quantity.
3. **The pion-extrapolation "amplifier" is mostly a NaN error** [verified, section 9.2]. The recommended procedure writes NaN purity errors in 11 analysis cells (base: 0), mostly pi_tpc with P ≈ 1 and `nbkg` at its limit; the spectrum multiplication turns them into NaN bin errors in 63 of the 180 pion spectra, and neither ROOT's fit (`HFitInterface.cxx:66`) nor `SpectrumExtrapolator::Integrate` drops a NaN error. With those errors repaired, the extrapolated pion yield moves by at most **0.62%** (efficiency on; was 3.35%) and 2.39% (off; was 9.4%). **Fix for the owner:** give the purity step a finite error or a hard flag when the propagated error is NaN, and make the consumers test `!(err > 0)` [inferred].
4. **Stabilising the Levy-Tsallis fit** [verified, section 9.3]: with efficiency on it converges and `n` stays off its limits, but χ²/ndf is 12-204. Widening the fit range to [0.2, 1.5] keeps the purity sensitivity ≤ 0.9% even with the NaN point; [0.2, 3.0] with a 2% error floor brings it to ≤ 0.5%. But the choice of range moves the pion yield itself by -1.2% to -6.1% (median), up to -8.6%: **the extrapolation model is a 1-6% systematic, larger than any purity effect** on the extrapolated yield and ratios. Real systematic errors are not in the inputs (`CorrelationTaskBase.h:1001`).

## FINAL CONCLUSIONS (final pass, 15 October 2026 evening; section 7; scratch `S2=/Users/scannito/alice/Analysis/RooFitKnowledge/scratch/study-final-12-11/`)

1. **Project bug, confirmed and fixed in a scratch copy only.** `Codes/CorrelationTaskBase.h:578` reads every efficiency histogram with `RootIO::GetUniqueOrThrow<TH1F>`, but the current `Corrections/*_Corrections.root` written by MCTask hold TH1D. With efficiency on, the unmodified code stops at the first task: `Missing object 'h1PhiEfficiency_multIntegrated' in '../DataFile/pp/DeltaY/Corrections/2024_Corrections.root', or it is not a TH1F` [verified, `log-corr-orig.txt`]. Reading the object as `TH1` and converting a TH1D to TH1F bin by bin (22-line diff, `corr/fix.diff`) makes the whole Data chain run with "No warnings" [verified]. The owner should make reader and writer agree (read `TH1`, or have MCTask write TH1F) [inferred].
2. **Downstream with efficiency ON, recommended procedure vs baseline purities** [verified, `res-cmpfinal-effon.txt`]. The spectra change exactly as before (P_new/P_old; K0S max 20.6%). The final quantities move less than with efficiency off:

   | quantity | efficiency off (section 5.1) max | efficiency on max | efficiency on p90 |
   |---|---|---|---|
   | measured K0S trend | 1.9% | 1.07% | 0.81% |
   | extrapolated pion yield | 9.4% | **3.35%** | 0.49% |
   | K0S/π extrapolated ratio | 10.2% | **3.07%** | 1.0% |
   | Ξ/π extrapolated ratio | 10.7% | **3.23%** | 0.49% |

3. **Why the pion extrapolation amplifies purity changes** [verified numbers, `res-extrap.txt`; mechanism inferred; **superseded by the last pass, item 3 above: the main cause is a NaN bin error**]. The Levy-Tsallis fit of the pion spectrum uses 6 points in [0.2, 1.0] GeV/c for 3 parameters, and the model does not describe them: χ²/ndf = 3.3e5/3 in the worst efficiency-off case, 12-125/3 with efficiency (the 8 spectra that move most). The `n` direction is almost flat, so a change of 2.7e-4 in the fitted points moves `n` from 6.27 to 20.0 (its upper limit) and `T` from 0.092 to 0.139. The yield outside the measured range follows: the extrapolation below 0.2 GeV/c, which is **about 25% of the pion yield**, moves by 23%, and the part above 3 GeV/c (1.5-4% of the yield) by 99%. Together they give the -8.2%. Over the spectra whose total moves by more than 1%, the median share of the change carried by the two extrapolated pieces is 100% (efficiency off) and 96% (on). With efficiency the spectrum shape is closer to Levy-Tsallis and the jumps are smaller (`n` 3.7 -> 3.3 for a 0.5% change of the points), hence 3.4% instead of 9.4%. This is a property of the extrapolation fit, not of the purity fit: any per-mille change of the low-pT pion points (purity, efficiency, binning) can move the extrapolated pion yield by percent.

---------------------------------------------------------------------------------------------------------------------------------

## SUMMARY (sessions 1-2)

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
   **Superseded by the final pass (section 7):** with efficiency on (scratch fix of the TH1F bug) the maxima are 3.35% (extrapolated pion yield) and 3.1-3.2% (extrapolated ratios).
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
  This happens only if `apply_purity` is true. It is false in `correlation_base_settings` (`globalConfigBasepp.json`), and no configuration sets it true since the older `globalConfigDatapp_OnlineEffonly.json`, the only one that did, was deleted (22 October 2026).
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
  - **Corrected in section 7.3:** the dominant term is the extrapolation BELOW 0.2 GeV/c (about 25% of the pion yield), not the high-pT tail; and the cause is an ill-conditioned fit (χ²/ndf up to 1e5, `n` jumping to its limit), measured.
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
2. **The current Corrections files cannot be consumed by CorrelationTask** [verified; the cause is pinned down in section 7.1: `CorrelationTaskBase.h:578` `GetUniqueOrThrow<TH1F>` against TH1D in the files; a scratch fix makes the chain run]. `Corrections/2024_Corrections.root` (written 14 Oct 15:50) holds TH1D under `MoreFullDifferential/AccEff/MultBin` and has no `SigLoss` directory. CorrelationTask throws: `Missing object 'h1PhiEfficiency_multIntegrated' in '../DataFile/pp/DeltaY/Corrections/2024_Corrections.root', or it is not a TH1F`. MCTask now writes TH1D while `LoadCorrections` requires TH1F. The message comes from the first `runcorr` attempt, with efficiency on; that log was overwritten by the efficiency-off rerun. The Data chain with efficiency cannot run until the two agree.
3. **The xRooFit fit cache** (section 4.1): anyone adopting note 09's escalation inside PurityTask must clear `gDirectory` around `minimize()`. Otherwise later cells silently get earlier cells' fits and the cache is written into the output files. This comes from ROOT, not from the project code, but the project's practice of keeping an output file as `gDirectory` while fitting makes it bite.
4. **Nothing reads the fit status, covQual or EDM** in PurityTask/DynamicRooFitter. The baseline summary says "No warnings" while 63k RooFit lines are logged and most fits are status 3 [verified].
5. **The purity map has no consumer in O2Physics**, although PurityTask's comment says O2 applies it per candidate. pi_tof maps and spectra are written but `purity_sources` never reads pi_tof [verified search / source].
6. **Pion extrapolation is unstable**: per-mille changes of the input points move the extrapolated pion yield and the K0S/pi and Xi/pi extrapolated ratios by up to ~10% with efficiency off and ~3% with efficiency on. Cause measured in section 7.3: 6 points, 3 parameters, χ²/ndf ≫ 1, a flat `n` direction that reaches its limit, and a low-pT extrapolation worth 25% of the yield [verified]. **Corrected in section 9.2:** most of it was a NaN purity error (new procedure, `nbkg` at its limit) propagated into a spectrum bin error that the fit and the integration both keep; repaired, the sensitivity is 0.6% (efficiency on) and 2.4% (off).
7. **0/0 = NaN purity** for empty pi_tof cells, with `TGaxis::PaintAxis: wmin == wmax` errors on stderr when they are drawn (`err-base.txt`) [verified].
8. **The project's CMakeLists does not link `RooFitXRooFit`**; the escalation needs it (and the library loaded in Cling) [verified].
9. **All paths are cwd-relative** (`runAnalysis.sh`, the JSON, `GitCommit("../.git")`). A run from another directory resolves elsewhere without error. This is how the scratch redirect works, but it is also a hazard [source].
10. **Moving the scratch directory lost the symlinks and the ROOT outputs** (the move apparently skipped them); they were recreated and rerun in session 2. This is not a project problem.

## 7. Final pass: the downstream run with efficiency on (scratch `S2=/Users/scannito/alice/Analysis/RooFitKnowledge/scratch/study-final-12-11/`)

`git status --porcelain` of the project before and after: the only differences are other researchers' notes in `RooFitKnowledge/` (`git-status-start.txt`, `git-status-end.txt`); the sha of `Codes/*.h` is unchanged.

### 7.1 The TH1F/TH1D bug [verified]

- **Where:** `Codes/CorrelationTaskBase.h:578`, inside the `fetchHist` lambda of `LoadCorrections`: `std::unique_ptr<TH1F> h = RootIO::GetUniqueOrThrow<TH1F>(dir, name, "CorrelationTaskBase::LoadCorrections");`. `GetUniqueOrThrow<T>` refuses an object of another class with the same message as a missing one [source `RootIOHelpers.h:53`].
- **Reproduced** with the project's unmodified code (study11's `base` build, which compiles the project's `Codes/`), efficiency on, in `run-corr-orig/`: exit 1, `[FATAL] CorrelationTaskBase::LoadCorrections: Missing object 'h1PhiEfficiency_multIntegrated' in '../DataFile/pp/DeltaY/Corrections/2024_Corrections.root', or it is not a TH1F` (`log-corr-orig.txt`).
- **Scratch fix** (`corr/Codes/CorrelationTaskBase.h`; every other header is a symlink to the project; diff in `corr/fix.diff`): read the object as `TH1`; if it is a TH1F use it, otherwise build a TH1F with the same (variable or fixed) binning and copy contents and errors of all bins including under- and overflow. The rest of `LoadCorrections` (the binning check, `combineHists`) is unchanged.
- **Result:** both runs complete with exit 0 and `[SUMMARY] No warnings.` (`log-corr-base.txt`, `log-corr-new.txt`). The signal-loss and event-loss objects needed by the no-phi tasks were found in the `*_WTrigger_Corrections.root` files; no path was missing.
- **The efficiency is really applied:** measured pion, K0S and Xi integrals, efficiency on / off (`res-effcheck.txt`): OfflineEff files ×5.6-6.9 (median; range 1.7-18.7); OnlineEff_NoPhi ×0.99 (0.68-1.0, signal and event loss only); OnlineEff identical (`apply_efficiency` is false for the online task in the project config) to 6e-11.

### 7.2 New procedure against baseline, efficiency on [verified, `runcorr.sh`, `dumpfinal.C`, `cmpfinal.py` -> `res-cmpfinal-effon.txt`]

Setup as in section 5.1 except: `corrBasepp.json` = the project's `globalConfigBasepp.json` with only `apply_purity: true` (so efficiency and all corrections are as in the project); binary built from `corr/`; inputs copied into `inputs-corr/` (ProjData and Corrections from the project, purity outputs copied from study11 `run-base` and `run-new`).

Relative change new/base - 1 per bin over the six output files and three dy windows:

| quantity | bins | median | p90 | max | n>1% |
|---|---|---|---|---|---|
| spectrum K0S / Xi / Pi | 1800 / 1260 / 1800 | 1.4e-4 / 6.3e-5 / 6.9e-5 | 2.7% / 1.6e-4 / 0.29% | 20.6% / 2.2% / 4.2% | 513 / 36 / 9 |
| measured trend K0S / Xi / Pi | 180 each | 0.36% / 8.9e-5 / 3.2e-4 | 0.81% / 0.24% / 0.12% | 1.07% / 0.43% / 0.15% | 9 / 0 / 0 |
| extrapolated yield K0S / Xi / Pi | 180 each | 0.35% / 9.3e-5 / 9.1e-4 | 0.82% / 0.33% / 0.49% | 1.06% / 0.79% / **3.35%** | 9 / 0 / 10 |
| ratio K0S/π measured / extrapolated | 180 / 180 | 0.34% / 0.51% | 0.88% / 1.0% | 1.05% / **3.07%** | 9 / 20 |
| ratio Ξ/π measured / extrapolated | 180 / 180 | 4.4e-4 / 9.5e-4 | 0.22% / 0.49% | 0.35% / **3.23%** | 0 / 10 |

- The spectra change exactly as with efficiency off: the purity correction is a bin-by-bin factor, independent of efficiency.
- Largest final-quantity changes: the 2026 pion extrapolated yield in multiplicity bin 0 (+3.35% offline, +2.3% online), and the K0S/π extrapolated ratio in the same bin (-3.1%).
- With efficiency on, the measured K0S trend moves by at most 1.07% and the extrapolated ratios by at most about 3%. These are the numbers for the decision: the recommended purity procedure moves the K0S/π and Ξ/π results by up to 1% (measured) and 3% (extrapolated) **if** `apply_purity` is turned on; with the project's default (`apply_purity: false`) they do not move.

### 7.3 Why the pion extrapolation amplifies [verified numbers: `dumpextrap.C` -> `extrap-eff{off,on}-{base,new}.tsv`, `anaextrap.py` -> `res-extrap.txt`]

`dumpextrap.C` reads, for every `*_extended` spectrum, the fitted Levy-Tsallis TF1 attached to it (parameters, χ², ndf), the measured integral, and the integrals of the TF1 below the first and above the last measured edge.

| case | spectrum | d total | d measured | d low (0-0.2) | d high (3-10) | n base -> new (error) | T base -> new | max d of the 6 fitted points | χ²/ndf base |
|---|---|---|---|---|---|---|---|---|---|
| eff off | OfflineEff 2026, dy 0.5, mult 2 | -8.2% | -1.1e-4 | -23% | -99% | 6.27 -> **20.0** (0.02) | 0.092 -> 0.139 | 2.7e-4 | 331778/3 |
| eff off | OfflineEff 2026, dy 1.0, mult 4 | +7.7% | -8.9e-4 | +21% | +785% | 7.30 -> 4.60 (0.12) | 0.093 -> 0.066 | 4.7e-3 | 17805/3 |
| eff on | OfflineEff 2026, dy 0.5, mult 0 | +3.2% | +1.1e-3 | +4.1% | +56% | 3.71 -> 3.32 (0.03) | 0.107 -> 0.098 | 5.3e-3 | 125/3 |
| eff on | OnlineEff 2026, dy 0.5, mult 0 | +2.2% | +1.1e-3 | +3.0% | +37% | 3.73 -> 3.45 (0.03) | 0.106 -> 0.099 | 5.3e-3 | 56/3 |

- **Where the yield comes from:** the extrapolation below 0.2 GeV/c is a median 26% (efficiency off) and 24% (on) of the pion yield; above 3 GeV/c it is 1.5% and 2.3% (max 4%). For the spectra whose total moves by more than 1% (21 off, 10 on), the median share of the change carried by the two extrapolated pieces is 100% (off) and 96% (on); the measured points contribute the rest.
- **Why they move:** 6 points (0.2-1.0 GeV/c) and 3 parameters (n, T, norm), with χ²/ndf from 12 to 3e5 in the spectra listed above. The quoted parameter errors (0.02 on n) are meaningless at such χ². The (n, T) valley is flat, so a change of 3e-4 to 5e-3 in the points moves n by 0.3 to 14 units, up to its limit 20, and with it the shape below 0.2 and above 3 GeV/c.
- **Efficiency reduces it:** the efficiency-corrected spectrum is closer to the Levy-Tsallis shape (χ²/ndf 12-125 instead of up to 3e5), so the same purity changes move n less and the total by 3.4% at most.
- **Not the purity fit:** the measured pion trend moves by at most 0.15-0.2%; any other per-mille change of the low-pT pion points (efficiency, binning, a different fit range) would have the same leverage [inferred].
- Section 5.1's explanation (the high-pT tail from a fit that ignores 1-3 GeV/c) was only part of it: the high tail moves by up to 99%, but it is 1.5-4% of the yield; the low-pT extrapolation dominates.
- Done in section 9: the fit-range scan, `n` fixed from a wider fit and an error floor; and the main cause found (NaN errors, section 9.2).

### 7.4 Files (all in `S2=/Users/scannito/alice/Analysis/RooFitKnowledge/scratch/study-final-12-11/`)

- `corr/`: `CMakeLists.txt` (from study11 `base`), `Codes/` (symlinks to the project + modified `CorrelationTaskBase.h`, original in `CorrelationTaskBase.h.orig`), `fix.diff`, `build/` (built with `cmake -B build -S . -DCMAKE_BUILD_TYPE=Release && cmake --build build`; logs `corr-cmake.log`, `corr-build.log`).
- `run-corr-{base,new,orig}/`: run directories (JSON copies, `DataFile/pp/DeltaY/{Corrections,ProjData,Purities}` symlinks into `inputs-corr/`, outputs in `FinalResults/`). `orig` = the unmodified code, efficiency on (fails).
- `runcorr.sh <base|new>`; logs `log-corr-*.txt`, `err-corr-*.txt`.
- `dumpfinal.C`, `cmpfinal.py` (copied from study11) -> `final-{base,new}.tsv`, `res-cmpfinal-effon.txt`.
- `dumpextrap.C`, `anaextrap.py` -> `extrap-*.tsv`, `res-extrap.txt`; `res-effcheck.txt`.

## 8. Last pass: the purity step with random starts (scratch `S3=/Users/scannito/alice/Analysis/RooFitKnowledge/scratch/study-last-11/`)

`git status --porcelain` of the project before and after: `git-status-start.txt`, `git-status-end.txt`; sha of `Codes/*.h`: `codes-sha-start.txt`, `codes-sha-end.txt`. Times are wall times with other researchers' jobs running: relative and noisy only.

### 8.1 The copy [verified]

- `S3/ms/`: study11 `new/` copied (CMakeLists with `RooFitXRooFit`, `Codes/` = symlinks to the project). Two files are real copies:
  - `DynamicRooFitter.h` = study11's `DoFitImproved` (offset fit + refit, accept on covQual 3 and EDM < 0.01, otherwise reset and xRooFit inside `TDirectory::TContext noFitCache{nullptr}`, keep the lower NLL) **plus k random starts after it**, as in note 12 section 6.1: shapes uniform in the inner 90% of their range, nsig+nbkg = N (in-range counts, `dataHist.sumEntries()`) split at a uniform fraction in [0.02, 0.98], each start an `Offset(true)` fit + one `Offset(true)` refit, kept only if its no-offset NLL is lower by more than 0.01. Seed fixed per cell (histogram name + counts). Env: `PHI_DOFIT_K` (all cells), `PHI_DOFIT_KXI` (Xi cells overlapping pT 1.2 to `PHI_DOFIT_XIPTMAX`, default 2.0), `PHI_DOFIT_KPI` (pi_tpc). The diag TSV gains `K`, `nllRec`, `startWon`, `nStartsBetter`, `ptLo`, `ptHi`.
  - `PurityTask.h`: two added lines before `DoFit()` hand the cell's pT edges to the fitter (`DynamicRooFitter::studyPtLo/Hi`), so the Xi rule can be applied. Nothing else changed.
- `runpurity.sh <tag> <K> <KXI> [XIPTMAX] [KPI]` runs the purity step in `run-<tag>/`; `dump.C` (study11) dumps it.
- **Check:** with K=0 (`run-rec0`) the output equals study11's `dump-new.tsv` in every cell (max |dP| = 0, `cmp.py`) [verified].
- Runs: `ms` = **note 12's rule** (K=2 everywhere, K=8 in Xi pT 1.2-2.0); `ms2` = a variant after reading `ms` (K=2, K=8 in Xi pT 1.2-4.2, K=4 in pi_tpc).

### 8.2 Purity, cell by cell [verified, `cmpms.py` -> `res-cmpms.txt`, `res-cmpms2.txt`]

| comparison | analysis cells \|dP\| >0.001 / >0.01 / >0.05 (of 737) | max | source cells (of 2627) >0.001 / >0.01 / >0.05 | max |
|---|---|---|---|---|
| rec - base (= section 4.3) | 151 / 71 / 8 | 0.92 | 410 / 129 / 24 | 0.50 |
| **ms - rec** (what the starts add) | **89 / 9 / 0** | 0.040 | 225 / 8 / 0 | 0.049 |
| ms - base | 173 / 72 / 8 | 0.92 | 505 / 135 / 24 | 0.50 |
| ms2 - rec | 102 / 11 / 0 | 0.040 | 256 / 18 / 0 | 0.049 |
| ms2 - base | 180 / 74 / 8 | 0.92 | 528 / 147 / 24 | 0.50 |

- **ms - rec by particle (analysis, >0.001 / >0.01):** k0s 16 / 5 (max 0.019), xi 32 / 2 (0.014), pi_tpc 38 / 0 (0.005), pi_tof 3 / 2 (0.040).
- **A random start wins in 169 of 740 analysis cells and 498 of 2640 source cells** (ms). Largest gains of NLL: pi_tpc up to 1.7e7 (cells of 1e9-8e9 counts; P 0.9999 -> 1.0000, `nbkg` to its limit 0), K0S 2026 pT 0.8-2.0 up to 1.2e6 (P +0.010 to +0.019; in K0S mult 5-6 pT 3 the start returns to the project's own value, 0.9448), Xi 2026 pT 1.2-3.0 50-540 (P +0.002 to +0.014).
- **Against the lowest NLL ever found for the cell** (study09, all non-relaxed variants including its 15-start search; "wrong" = more than 0.5 above it) [`cmp09ms.py` -> `res-cmp09ms.txt`, `res-cmp09ms2.txt`]:

| set | cells | rec wrong | ms wrong | ms2 wrong | rec \|P-P_best\|>0.001 / >0.01 | ms | ms2 |
|---|---|---|---|---|---|---|---|
| analysis | 740 | 180 | **55** | **34** | 123 / 20 | 43 / 12 | 34 / 9 |
| source | 2640 | 493 | 121 | 81 | 335 / 27 | 135 / 19 | 112 / 14 |

  By particle (analysis, rec -> ms -> ms2 wrong): k0s 33 -> 10 -> 10; xi 46 -> 20 -> 14; pi_tpc 98 -> 24 -> 9; pi_tof 3 -> 1 -> 1.
  - The starts remove about 70% of the real-data wrong minima (80% with ms2). What is left: **Xi 2026 pT 2.0-3.0** (analysis bins 3-4), which note 12's rule gives K=2 and which even K=8 does not fix (P still 0.010-0.015 away from study09's best in 6 cells, ms2); pi_tpc cells of 1e9 counts (dNLL up to 4.5e6, P unchanged at 1e-4); K0S 2026 mult 3/7 pT 1.6-2.0 (dNLL 200-400).
  - The rule "K=8 only in Xi pT 1.2-2.0" is too narrow for the real 2026 Xi data [verified]: the multimodality extends to 3.0 GeV/c. In 1-8 cells the run even goes below study09's lowest (new minima).
  - pi_tof 2024 mult 0 pT 0 (127 counts) stays 0.11 away from study09's best; it is a 0/low-count cell, not a minimisation problem.
- **Flags go up, not down** [verified, `res-newflags.txt`]: flagged analysis cells 38 (rec) -> 80 (ms) -> 82 (ms2). All 48 newly flagged cells are cells where a start won; 45 of them end with covQual < 3, 37 are pi_tpc (the new minimum has `nbkg` at its limit 0, where HESSE is not positive definite). 6 cells (k0s) leave the flagged list. So with starts the flag means "check the error", not "the value is wrong".
- **Cost** (wall time of the whole purity step, noisy): rec 21 s, ms 53 s (2.5×), ms2 77 s (3.6×); the summed per-cell fit time in the diag files: 17.6 s, 49.1 s, 72.9 s.

### 8.3 Downstream with efficiency on [verified, `runcorr.sh`, `dumpfinal.C`, `cmpfinal.py` -> `res-final-*.txt`]

Same setup as section 7.2 (the scratch-fixed binary of `S2/corr/`, run read-only; JSON identical to `S2/run-corr-new/`; Corrections and ProjData copied into `S3/inputs-corr/`). Both runs: exit 0, "No warnings". Check: `cmpfinal.py` on S2's base/new dumps reproduces `res-cmpfinal-effon.txt` byte for byte.

Max |X/X_base - 1| over all bins of the six output files (p90 in brackets):

| quantity | rec vs base (section 7.2) | **ms vs base** | ms vs rec | ms2 vs base |
|---|---|---|---|---|
| spectrum K0S / Xi / Pi | 20.6% / 2.2% / 4.2% | 20.6% / 2.2% / 4.3% | 2.3% / 1.5% / 0.5% | 20.6% / 3.1% / 4.3% |
| measured trend K0S / Xi / Pi | 1.07% / 0.43% / 0.15% | **1.16%** (0.92%) / 0.54% / 0.26% | 0.22% / 0.38% / 0.15% | 1.16% / 0.65% / 0.26% |
| extrapolated yield Pi | 3.35% (0.49%) | **4.26%** (0.60%) | 1.33% | 1.21% (0.65%) |
| ratio K0S/π measured / extrapolated | 1.05% / 3.07% | 1.13% / **3.91%** | 0.21% / 1.30% | 1.13% / 1.09% |
| ratio Ξ/π measured / extrapolated | 0.35% / 3.23% | 0.46% / **3.88%** | 0.24% / 1.23% | 0.56% / 0.96% |

- The measured quantities move as expected: the starts add at most 0.2-0.4% to them.
- The extrapolated pion yield and the extrapolated ratios jump around: 3.35% (rec), 4.26% (ms), 1.21% (ms2), each time in the 2026 multiplicity bin 0-1 spectra, although the measured pion trend moves by at most 0.26%. This is the extrapolation amplifier of section 7.3, not a property of the purity procedure: see section 9.

## 9. Last pass: the pion extrapolation, diagnosed and stabilised (scratch `S3`; findings for the owner, no project change)

### 9.1 Method [verified, `extrapstudy.cpp` -> `ex-<run>[-nanfix].tsv`, `anaex.py` -> `res-extrap-stab.txt`, `res-extrap-effoff.txt`]

- `extrapstudy <tag> <FinalResults dir> <out>` reads every `Extract1D/Pi/Spectra/h1SpectrumPhiPi_dy*_multBin*` (180 spectra per run: 6 files × 3 dy × 10 multiplicity bins) and refits it with the project's own model (`ExtrapolationModelFactory::CreateModel`, the `Pi` block of `extrapConfig.json`, option `"0QI"`, up to 10 trials as `SpectrumExtrapolator::FitOrWarn`). Yield = measured integral (bins skipped only if error <= 0, as `SpectrumExtrapolator::Integrate`, `SpectrumExtrapolator.h:463`) + TF1 integral over [0, 0.2] + over [3, 10].
- **Checks:** on the base run the refit equals the TF1 stored by the project within |dn| 3e-4, |dT| 5e-6 (180 spectra), and on the rec run too (|dn| 3e-4); the yield equals the project's `h1MultTrendExtrap_Pi` within 1e-5 relative in all 180 bins.
- Runs compared: S2 `run-corr-base` / `run-corr-new` (efficiency on, base / rec purities), S3 `run-corr-ms`, `run-corr-ms2`; and study11 `run-corr-base` / `run-corr-new` (efficiency off). All read-only.
- **Systematic errors are not in the inputs:** the chain calls no `SetSystematicSpectrum` (`CorrelationTaskBase.h:1001`, "nothing in this chain produces per-bin systematic uncertainties yet") [source]. A 2% uncorrelated error added in quadrature to every point is used as a proxy (variants `E2`, `E2R30`) [inferred proxy, not the analysis's real systematics].

### 9.2 The amplifier is mostly a NaN error, not the fit [verified]

- **The new purity procedure writes NaN purity errors.** Analysis cells with a finite purity and a NaN error: base 0, rec **11**, ms 7, ms2 4 (`dump-*.tsv`). In rec: pi_tpc 2024 mult 1 pT 0.4-0.6, mult 4 pT 0.4-0.5, mult 5 pT 0.6-0.8; pi_tpc 2026 mult 0 pT 0.8-1.0, mult 2 pT 0.5-0.6, mult 7 pT 2.0-3.0, mult 9 pT 0.5-0.6; K0S 2024 mult 6 pT 4.2-6.4, K0S 2026 mult 9 pT 1.2-2.0. The pi_tpc ones have P = 0.99998-1.0000: the chosen fit has `nbkg` at its limit 0 and `getPropagatedError` (`DynamicRooFitter.h:146`) returns NaN [verified values; cause inferred from the limit].
- `h1Spectrum->Multiply(hPurity)` turns them into NaN bin errors: **63 of the 180 pion spectra** (rec), 45 (ms), 18 (ms2) have one (`nNaNerr` column).
- **Nobody stops a NaN error:** ROOT's fit keeps a point whose error is NaN (`HFitInterface.cxx:66` tests `error <= 0`, false for NaN) [source], and so does `SpectrumExtrapolator::Integrate` (same `<= 0.` test). The NaN point stays in the 6-point fit (ndf stays 3) and changes it [verified]; how Minuit then weighs it was not read.
- **Replacing each NaN error by the base run's error of the same bin (`-nanfix`) removes most of the amplification:**

| run | max \|Y/Y_base - 1\| of the extrapolated pion yield, as run | with NaN errors repaired | spectra > 1% (as run / repaired) |
|---|---|---|---|
| rec, efficiency on | 3.35% | **0.62%** | 10 / 0 |
| ms, efficiency on | 4.26% | 1.21% | 9 / 9 |
| ms2, efficiency on | 1.21% | 1.21% | 9 / 9 |
| rec, efficiency off (section 5.1) | 9.40% | **2.39%** | 21 / 2 |

  - The section 7.3 case (efficiency off, OfflineEff 2026 dy 0.5 mult 2): with the NaN error, n 6.27 -> 20.0 (limit) and the yield -9.37%; with it repaired, n 6.27 -> 6.71 and -2.39%.
  - The worst efficiency-on case (OfflineEff 2026 dy 0.5 mult 0, one NaN bin at 0.8-1.0 GeV/c): n 3.71 -> 3.32 and +3.35% as run; n 3.71 -> 3.65 and +0.62% repaired.
  - The 1.21% left in ms/ms2 is a genuine purity change (the starts move pi_tpc points by up to 0.5%, section 8.2); the measured pion integral moves by at most 0.15% (rec) and 0.26% (ms).
- So section 7.3's reading ("a flat `n` direction amplifies per-mille changes") is only the smaller part: the per-mille purity changes give 0.6% (efficiency on) to 2.4% (off); the rest came from a NaN error entering the fit.

### 9.3 The fit itself, and three stabilisations [verified, `res-extrap-stab.txt`, `res-extrap-effoff.txt`]

Efficiency on, 180 base spectra. "Shift" = yield of the variant / yield of the project's fit - 1 (base run). "Sensitivity" = max |Y_run/Y_base - 1| with that variant.

| variant | fit | χ²/ndf median / max | n median [min, max] | shift median [min, max] | sensitivity rec: as run / repaired | ms: as run / repaired |
|---|---|---|---|---|---|---|
| project | [0.2, 1.0], n free in [2, 20] | 12.6 / 204 | 4.20 [3.63, 7.04] | 0 | 3.35% / 0.62% | 4.26% / 1.21% |
| R15 | [0.2, 1.5] | 19.9 / 492 | 4.61 | -1.2% [-2.4, +0.7] | 0.78% / 0.46% | 0.94% / 0.91% |
| R20 | [0.2, 2.0] | 34.9 / 943 | 4.91 | -2.1% [-3.7, +0.9] | 1.13% / 0.45% | 1.14% / 0.82% |
| R30 | [0.2, 3.0] (all points) | 163 / 4230 | 5.72 | -3.8% [-5.9, +0.3] | 0.62% / 0.71% | 0.90% / 0.73% |
| nFixR30 | [0.2, 1.0], n fixed to the R30 value of the same spectrum | 93 / 2560 | (5.72) | -3.5% [-5.6, +0.5] | 0.86% / 0.63% | 1.17% / 0.71% |
| E2 | [0.2, 1.0], 2% error floor | 0.24 / 1.8 | 4.36 | -0.4% [-1.6, +0.5] | 3.95% / 0.47% | 4.77% / 0.96% |
| E2R30 | [0.2, 3.0], 2% error floor | 8.7 / 12.4 | 6.29 | -6.1% [-8.6, 0.0] | 0.59% / 0.49% | 0.51% / 0.51% |

- **Diagnosis.** With efficiency on, the fit converges (status 0 in all 1260 fits) and `n` is never at a limit; the χ²/ndf of 12-204 says the statistical errors (0.3-1%) are far below the model's mismatch. With efficiency off (study11) χ²/ndf reaches 2.5e5 and `n` sits at 20 in 7 spectra (`res-extrap-effoff.txt`). The low-pT extrapolation is 23-26% of the yield in every variant.
- **Every wider fit range removes the NaN leverage** (one bad point among 7-10 instead of 6): sensitivity ≤ 1.1% as run, ≤ 0.9% repaired. Fixing `n` from the wide fit does the same. The 2% floor alone makes χ²/ndf ≈ 0.2-0.7 but does not protect against the NaN point (3.95% as run).
- **The model choice moves the pion yield by more than any purity effect:** -1.2% (R15) to -6.1% (E2R30) median, up to -8.6%. The model does not describe 0.2-3.0 GeV/c (χ²/ndf 163 median with stat errors, 8.7 even with a 2% floor), so the range choice is a 1-6% systematic on the extrapolated pion yield and on the K0S/π and Ξ/π extrapolated ratios [verified shifts; "systematic" inferred].
- Efficiency off, repaired: project 2.39%, R15 3.45%, R30 2.68%, nFixR30 3.23%, E2 0.89%; E2R30 has one spectrum at 56% in both columns (a fit outlier, not investigated; efficiency-off spectra are not used by the analysis).
- **For the owner [inferred]:**
  1. Stop NaN errors at the source and at the consumer: in the purity step, a cell whose propagated error is NaN must be flagged and given a finite error (for example from a refit with `nbkg` released or from HESSE); in `CorrelationTaskBase`/`SpectrumExtrapolator`, test `!(err > 0)` instead of `err <= 0.` and refuse NaN bin errors. This alone brings the purity sensitivity of the extrapolated pion yield from 3.4% to 0.6% (efficiency on).
  2. Fit range: [0.2, 1.5] is the cheapest stabilisation (sensitivity ≤ 0.9%, shift -1.2%); [0.2, 3.0] with an error floor is the most robust (≤ 0.5%) but shifts the yield by -6% and the model is then visibly wrong. Which range is right is a physics choice; the spread between them (1-6%) belongs in the systematic uncertainty.

---------------------------------------------------------------------------------------------------------------------------------

## Resume here

**FINAL** (last pass, 15 October 2026 night). Done: sections 1-6 (sessions 1-2), section 7 (efficiency on, TH1F/TH1D bug), section 8 (random starts through the full purity step and downstream), section 9 (pion extrapolation diagnosed and stabilised in a scratch refit).

The former NOT DONE item "Xi archive mismatch" is **solved** (note 09 section 9: commit 3e2878a changed the Xi observable range from [1.2, 1.4] to [1.29, 1.36]; with the old range all 660 Xi cells reproduce the archive).

How to rerun the last pass (all in `S3=/Users/scannito/alice/Analysis/RooFitKnowledge/scratch/study-last-11/`):
- `bash build.sh` (purity binary in `ms/build`, `extrapstudy`).
- Purity: `./runpurity.sh <tag> <K> <KXI> [XIPTMAX] [KPI]` (`rec0` = 0 0, `ms` = 2 8, `ms2` = 2 8 4.2 4; about 20-80 s each), then `root -l -b -q 'dump.C("run-<tag>/DataFile/pp/DeltaY/Purities","dump-<tag>.tsv")'`, `python3 cmpms.py dump-<tag>.tsv diag-<tag>.tsv`, `python3 cmp09ms.py diag-<tag>.tsv dump-<tag>.tsv`.
- Downstream: `./runcorr.sh <ms|ms2>` (needs S2's `corr/build` binary), `dumpfinal.C` -> `final-<tag>.tsv`, `python3 cmpfinal.py <A.tsv> <B.tsv>`.
- Extrapolation: `./extrapstudy <tag> <FinalResults dir> ex-<tag>.tsv` (prefix `NANFIX=<base FinalResults dir>` to repair NaN errors), `python3 anaex.py`.

Impossible here:
- The real per-point systematic errors of the pion spectra in the extrapolation fit: the chain produces none (`CorrelationTaskBase.h:1001`); a 2% floor was used as a proxy (section 9.3).

NOT DONE, possible but outside this pass (none of them changes a conclusion above):
1. `IntegrateBins` through the full purity step (note 12 section 6.3): not run. It costs 32-75 times the recommended procedure per cell and its errors are unreliable at high statistics (note 12), so it belongs to a targeted polish of a few high-statistics cells rather than to this end-to-end comparison; budget, not impossibility.
2. How Minuit weighs a NaN-error point (the effect is measured, the code path was not read), and the one efficiency-off `E2R30` outlier (56%): not investigated; neither is on the analysis path once NaN errors are stopped.
3. A larger random-start rule for Xi 2026 pT 2.0-3.0 (K=8 still leaves 6 analysis cells 0.010-0.015 away from study09's best purity): would need more than 8 starts or a different start distribution; not tried.
