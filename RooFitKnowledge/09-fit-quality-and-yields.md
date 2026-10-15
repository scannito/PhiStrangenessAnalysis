# 09 - Does the fit-quality problem change the physics? Yields, errors and purities on the real purity cells

Written 14 October 2026 (ROOT 6.36.10, ALICE build `v6-36-10-alice4-local1`, macOS arm64). Complements `03-fitting-api.md` (status codes, options)
and `07-...xroofit.md` (restart test, xRooFit). Provenance tags: [source] code read, [verified] run here (script + output named), [inferred] my conclusion.
All numbers below come from the scripts and TSVs listed in section 8 (scratch dir `study09/`, now in `RooFitKnowledge/scratch/`). Nothing in the project was modified.

---------------------------------------------------------------------------------------------------------------------------------

## ONE-PAGE CONCLUSION FOR THE PROJECT OWNER

**Update (follow-up passes, section 9):** (i) the Xi mismatch with the archive is SOLVED: 7 Aug used the Xi observable range [1.2,1.4], commit 3e2878a (27 Aug) changed it to [1.29,1.36]; the purity is lower by a median 0.074, and with the old range all 660 Xi cells reproduce the archive exactly (9.1). (ii) Limits, with paired multi-starts (9.2): the Xi `sigma_sig >= 0.002` limit binds in 122/140 cells (the fit wants 1.5-2.0 MeV; S -0.56% median, max -2.9%; physics decision); Xi `n >= 1` must stay (relaxing gives n~0.3 and S +8%, unphysical); the K0S Chebychev2 box binds in 15 cells of pT bin 4 (S +0.7%, negative pdf in 9); pi_tof `slope <= 0` binds in 130/196 cells (S 0.1%); DSCB `n` upper limits bind but move S by <0.04%; `alpha` and pi_tpc `nbkg >= 0` do not bind at all (the fit never asks for a negative background). (iii) The 1-2% yield spread between minima is NOT a likelihood ambiguity: among solutions within dNLL<0.5 of a 50-start best the S spread is at most 1.6e-4 (0.6 sigma); the larger differences are fits stuck in distinguishable higher minima (9.4), so it is not a systematic to quote but a failure a multi-start removes; even the recommended single-chain procedure is above the 50-start best in 8/40 test cells. (iv) `Offset("bin")` + refit converges better than `Offset(true)` + refit (accepted 96% vs 88%, S off >1% in 17 vs 22 cells; 9.6). (v) Integrated bins move the Xi yield by +0.17% median (p90 1.8%) against the bin-centre likelihood, other particles <0.1% (9.6); cost ~100-200x. (vi) Fixing the DSCB tails cuts MIGRAD calls 3-4x but shifts S and worsens the NLL by thousands of units (9.5, measurement only). (vii) `sum of coefficients is zero` comes only from the 16 EMPTY pi_tof cells (both yields driven to 0), never from pi_tpc (9.7). The purity-bias toys are in note 12 and the in-sequence xRooFit escalation (which flags more cells than 3.6 predicts) in note 11.

**Coverage:** 3380 cell sets = 2 purity input files x (370 analysis + 1320 source cells), 14 variants each; statistics quote the 734 valid analysis cells and 2620 valid source cells. The results support: size and distribution of the yield/purity/error shifts on these cells, the A/B/C classification of status 3, the ranking of procedures. They do not support: a proof that the best NLL found is global, anything on the final correlation results.

**Verdict: yes, the fit-quality problem changes the physics outputs, and not marginally.** Stopping at "status 3" is benign for a minority of
cells only. On the cells that `PurityTask` really fits (merged analysis pT bins, 734 cells from the two purity input files) the project's
`DoFit()` gives a signal yield in the window that differs from the best fit found by more than 0.1% in 29% of cells, more than 1% in 12%, more
than 5% in 4% (K0S: 35% / 31% / 8%). The purity S/(S+B) differs by more than 0.001 in 21% of cells and by more than 0.01 in 9% (K0S: 28%).

**What was measured** [verified, `study.cpp`; section 1]. For every cell the project fits (4 purity particles k0s, xi, pi_tpc, pi_tof; analysis
binning = merged source bins through `rebinning_pt`, and the source binning that feeds the CCDB map; both purity input files) I ran the project's exact
`DoFit()` + `ExtractYieldsAndPurity()` and 13 alternatives, evaluated every result with the SAME NLL (same data, same model, no offset), the same
fixed signal window and the same extraction code. The reproduction of the project is exact: for K0S, pi_tpc and pi_tof all 2720 cells (both files, both
binnings) reproduce the purity and its error stored in `DataFile/.../Purities/Data_20xx_*Purity.root` (|dP| < 1e-7 after float rounding, error to 1e-6), and Xi reproduces too (660 cells) with the 7 Aug config (9.1). The Xi numbers in this note are for the current `fitConfig.json`.

**The numbers (analysis cells, project's call vs the best-NLL variant of 11 deterministic fits; 2026 file alone is worse than 2024):**

| | cells | yield S off by >0.1% | >1% | >5% | median / p90 / p99 of \|dS\|/S | \|dP\| p90 / p99 / max |
|---|---|---|---|---|---|---|
| all | 734 | 210 (28.6%) | 90 (12.3%) | 28 (3.8%) | 1.0e-4 / 1.9% / 25% | 0.63% / 4.4% / 0.92 |
| K0S | 200 | 70 (35%) | 61 (30.5%) | 16 (8.0%) | 1.6e-4 / 4.3% / 17% | 2.2% / 6.6% / 14.7% |
| Xi | 140 | 27 (19%) | 6 (4.3%) | 1 | 1.1e-4 / 0.41% / 4.3% | 0.30% / 1.6% / 2.0% |
| pi_tpc | 200 | 98 (49%) | 12 (6.0%) | 5 | 9.9e-4 / 0.58% / 12% | 0.28% / 0.50% / 4.0% |
| pi_tof | 194 | 15 (7.7%) | 11 (5.7%) | 6 | 1.0e-6 / 2e-5 / 45% | 0.005% / 2.9% / 0.92 (cells with <130 counts) |
| 2026 file only | 366 | 122 (33%) | 66 (18%) | 15 (4.1%) | 1.3e-4 / 2.9% / 25% | 1.7% / 6.0% / 14.7% |

The propagated error of S differs by more than a factor 2 in 24% of cells (pi_tpc 58%) and that of the purity in 31% (pi_tpc 78%): where HESSE ran at a
non-converged point the error is not the error of the fit. The base NLL is more than 0.5 above the best in 52% of the cells. The K0S damage is concentrated in
pT bins 4-9 (1.2-6.4 GeV/c): in 2026, 4-9 of 10 multiplicity bins per pT bin have |dS|/S > 1% (up to 55%), |dP| up to 0.147. The source-binning cells (CCDB map)
behave alike (S off by >1% in 6.9% of 2620 cells; K0S 16.6%).

**Is status 3 benign? Partly, and it cannot be told from the status** [verified, section 3.4-3.5]. Of the 523 analysis cells with status 3:
28% are a precision limit (base NLL within 0.5 of the best, yield stable to <0.05%), 52% are genuinely not converged (yield moves; median 0.1%, p90 2.8%, p99 32%, max 55%),
20% are bad cells (covQual<3 at the best fit, NaN evaluations, or nsig/nbkg at a limit). "EDM below the machine limit EPS2*|NLL|" (EPS2=5.9e-8) is a weak
benign test: 332 of the 523 pass it, yet in 54% of those a restart lowers the NLL by more than 0.5 (the yield stays within 0.1% for 89% of them). EDM above that limit (r>1, 191 cells)
means not converged: the NLL drops by >0.5 in all of them and the yield moves by more than 1% in 36%. Status 0 is mostly right (206 cells: 5% with |dS|/S>1%).

**Two further problems that are not about convergence.** (1) **Config limits clip the K0S yield**: `nsig` max is 1e8, and 19 of the 100 K0S analysis cells of the 2026
file contain more than 1e8 counts (pT bins 2-5). The best fit sits at the limit in 18 K0S cells; relaxing `nsig`/`nbkg` max to 1e11 lowers the NLL by up to 2e7 and moves the yield by
a median 22% (up to 78%) and the purity by up to 0.06 (e.g. K0S mult 8, pT 3: P 0.942 -> 0.984). Two pi_tpc cells (N ~ 1e10 vs `nsig` max 1e10) are clipped too. (2) **Multiple minima**:
a 15-start random search finds an NLL lower than every deterministic variant by more than 0.5 in 73/734 analysis cells (Xi 30, pi_tpc 32, K0S 11), with the yield different by 0.24% (median) up to 1.7% (max). Xi's
`sigma_sig` sits at its lower limit 0.002 in 125 of 140 analysis cells. These are fits stuck in a DISTINGUISHABLE higher minimum (dNLL > 5), not an ambiguity of the likelihood: within dNLL<0.5 of a 50-start best, S varies by at most 1.6e-4 (section 9.4). They are removed by a multi-start (20 offset starts reached the 50-start best in 39/40 test cells), not quoted as a systematic.

**Recommendation for `DoFit()`** [verified, section 3.6]: (a) fit with `RooFit::Offset(true)`, (b) refit once from the result with `Offset(true)`, (c) accept if `covQual()==3 && edm()<0.01`
(after the offset the EDM is resolvable and this test means something), (d) otherwise run the xRooFit ladder and keep the lower NLL, (e) record status/covQual/edm per cell and flag what is still not accepted.
Leave-one-out against the other deterministic variants this leaves S off by more than 1% in 10 of 734 cells (1.4%, against 12.3%) and by >5% in 6 (0.8%, against 3.8%); 88% of the cells are accepted
at step (c); 33 cells (4.5%) stay flagged. Cost: CPU 3.0 ms -> 4.8 ms per cell for (a)+(b) (x1.6), about 6-7 ms with the escalation (x2); a whole purity file (1690 fits) costs about 5 s now and 10 s then.
`Offset(true)` alone, `Strategy(2)`, `RecoverFromUndefinedRegions`, `Optimize`, `Hesse`, `PrintEvalErrors` do NOT fix it (section 3.3); a restart WITHOUT offset also helps (up to 10 refits, x2.5 time) but its EDM cannot be used as acceptance test.
Independently, raise the `nsig`/`nbkg` limits of the K0S (and pi_tpc) sections. Second-pass refinements [verified, 9.4, 9.6]: `Offset("bin")` instead of `Offset(true)` in (a)-(b) is accepted in 96% of cells instead of 88%; the escalation as really run in sequence flags more cells than the combined rows above (note 11: 80/737 vs 33); for the cells still flagged, a 20-start offset multi-start (median CPU about 33x the project's fit, relative only) found the lowest minimum in 39 of 40 test cells.

**Cells to look at by hand** (section 5; full list in `cells-master.tsv`): in the 2026 file, 125 of 366 analysis cells carry at least one hard flag, notably K0S (5,2)(6,2)(8,2)(9,2), (1..9,3), (2,4)(5,4)(6,4)(8,4), (5,5)
(nsig at limit), K0S pT 4-9 in general, all Xi pT 1-4 (multimodal, yield spread >0.1%), pi_tpc pT 0, 3, 4, 5, 9 (covQual<3, no variant converged) and pi_tof pT 0 (empty or <10 counts: purity NaN).
Uncertainties and what was not done: section 7.

---------------------------------------------------------------------------------------------------------------------------------

## 1. What was reproduced, and how

### 1.1 The cells that PurityTask fits [source `PurityTask.h:70-135, 292-333`, `AnalysisDataStructures.h:56-97`, `BinningUtils.h:120-143`, `FitConfigManager.h`]
- `globalConfigPuritypp.json` activates `purity_task_2024` (input `AnalysisResults_136_2024_PtMod_5agosto.root`, prefix `Data_2024_`) and `purity_task_2026`
  (`AnalysisResults_136_2026_PtMod_6agosto.root`, prefix `Data_2026_`); both inherit `purity_base_settings` from `globalConfigBasepp.json`, which lists four particles:
  k0s (`h3K0sCandidatesMass`), xi (`h3XiCandidatesMass`), pi_tpc (`h3PionTPCnSigma`), pi_tof (`h3PionTOFnSigma`) with `rebinning_pt`
  k0s `[0,0.3,0.5,0.8,1.2,1.6,2.0,2.4,3.0,4.2,6.4]`, xi `[0.8,1.2,1.6,2.0,2.4,3.0,4.2,6.5]`, pions `[0.2,0.3,0.4,0.5,0.6,0.8,1.0,1.2,1.5,2.0,3.0]`. **`23luglio` is not a purity input.**
  [verified] its K0S pT axis has an edge at 6.5 where `rebinning_pt` has 6.4, so `MapToSourceBins` would throw there (I read the axis edges; I did not run PurityTask on that file).
- Every particle has 10 multiplicity bins. Per file: analysis cells k0s 10x10, xi 10x7, pi_tpc 10x10, pi_tof 10x10 = 370; source cells 340+260+360+360 = 1320. `FitPuritySpectrum` fits BOTH sets: the analysis bin k covers source bins
  `mappedSourceBins[k]` (`ProjectionZ(name, mult+1, mult+1, first, last)`: the MERGED mass histogram), and, because `rebinning_pt` is set, the source bins again one by one (these feed the CCDB map).
- `GetConfig(key, mult, k)` is called with k = the index in the binning being fitted. The per-bin overrides of `fitConfig.json` (k0s: `mult*_pt4`, `mult*_pt5`, Chebychev2 background) therefore hit analysis pT bins 4,5
  (1.2-1.6 and 1.6-2.0 GeV/c) in the analysis fits but source bins 4,5 (0.8-0.9, 0.9-1.0) in the source fits [source + harness: both follow `k`]. Whether this is intended is a question for the owner.
- Signal windows are fixed ranges in all four sections (`integration_range` k0s [0.475,0.515], xi [1.305,1.335], pions [-3,3]), `snap_to_bin` false, so "the same window" holds by construction for every variant.

### 1.2 The harness `study09/study.cpp` [verified]
It uses the project's own headers (`DynamicRooFitter.h` with `private` made public in the scratch copy only, `FitConfigManager.h`, `BinningUtils.h`), builds each cell exactly as above (ProjectionZ on a TH3F loaded from the file,
`MapToSourceBins` from the project header), builds a FRESH `DynamicRooFitter` per variant (as the project builds one per fit), and for the project's variant calls `DoFit()` and `ExtractYieldsAndPurity()` verbatim.
Not reproduced: `ResolvePtBinning` verification, drawing, writing, the cell order (key outer instead of mult outer: checked irrelevant, results are bit-identical alone and in the full run, section 6.1) and the WorkflowManager.
**Validation against the project's own archived output** [verified, `dumppur.C`, python in section 8]: `Data_2024_*Purity.root` and `Data_2026_*Purity.root` (written 7 Aug) contain `h1<key>Purity_multBin<i>` (analysis) and `.../SourceBinning/..._sourceBinning`
(source). My base purity and purity error equal the stored ones (|dP| < 1e-7 after float32 rounding, relative error < 1e-6) in all 2720 K0S, pi_tpc and pi_tof cells of both files and both binnings (e.g. K0S 2026 (0,0): error 0.00017689096678699204 in both).
With the current config all 660 Xi cells differ (stored P 0.954, mine 0.877 for the 2024 analysis cell (0,0)); with the 7 Aug Xi config they match exactly: see section 9.1. The Xi numbers in sections 3-5 are for the current config.
Earlier-note cross-check: with the 23luglio file my harness gives status 3 in 310/340 K0S, 134/260 Xi, 338/360 pi_tpc source cells (the numbers of `BOOKMARK.md`), cell (3,18) edm 9.158e7 and error 178500, and the 30-cell table of note 07
(status 0: 1/30 project, 10/30 `Offset`, 1/30 `Strategy(2)`, 28/30 xRooFit; covQual 3: 22, 24, 20, 29): identical.

### 1.3 Variants (all start from the config's initial values; `FIT()` = project call `Optimize(1), Extended(1), Save(1), PrintLevel(-1)`)
| name | what |
|---|---|
| `base` | the project's `DoFit()` |
| `rst1` / `rstN` | `FIT()` again from the result (1 restart) / repeated until the common NLL changes by <0.01 (max 10 refits) |
| `s2` | `FIT(Strategy(2))` |
| `off` | `FIT(Offset(true))` |
| `off_rst1` / `offN` | `Offset(true)` fit, then 1 refit from the result / repeated until NLL stable (max 10) |
| `xrf`, `xrf0` | xRooFit `xRooFit::createNLL(model,&dh).minimize()` with `BoundaryCheck 0.01` / without (its status is not comparable: 4000 etc. mean the old-Minuit fallback finished; I use covQual and edm) |
| `rec0`, `rec100` | `FIT(RecoverFromUndefinedRegions(0 or 100))` |
| `ms` | multi-start: 15 random starts (nsig+nbkg = N split randomly, shape parameters uniform in the inner 90% of their range), each an `Offset(true)` fit repeated until stable (<=6 fits), keep the lowest NLL; fixed seed per cell |
| `base_rel`, `xrf_rel` | project call / xRooFit with `nsig`, `nbkg` max raised to 1e11 (only these two; starting values unchanged) |

xRooFit needed one change: its default `MaxFunctionCalls` is -1 and the old-Minuit fallback ran for >5 minutes on one pi_tpc source cell; I capped it at 1e5 calls (`kXrfMaxCalls`).
Recorded per cell and variant: status, status history, covQual, edm, own minNll, common NLL (`createNLL(dh, Extended(true))` evaluated at the final values, no offset), nInvalid, nsig, nbkg and errors, S, B, P and their propagated errors
(`getPropagatedError`), all floating parameters with value/error/limits, CPU and wall time. [verified] the common NLL equals the project's minNll for `base` to 3e-14 over all 3380 cells.

"Best" = the variant with the lowest common NLL. Two pools are used and named every time: **det** = the 11 deterministic variants (primary, because every one of them is an ordinary fit whose status/edm/covQual can be read) and
**all** = det + `ms`. "At a limit" = parameter within 1e-5 of its range of a limit (the distribution of the distance is bimodal, <1e-6 or >1e-4, `study09` check). "Converged" = covQual 3 and edm < 0.01.

---------------------------------------------------------------------------------------------------------------------------------

## 2. Population (valid = in-range content > 0; 3380 cells = 2 files x 1690)

Base `status` over the valid cells [verified, `res-det-a3.txt`]:

| set | key | valid cells | status 3 | status 0 | other |
|---|---|---|---|---|---|
| analysis | k0s | 200 | 188 (94%) | 11 | 304: 1 |
| analysis | xi | 140 | 71 (51%) | 69 | |
| analysis | pi_tpc | 200 | 195 (97.5%) | 2 | 1: 1, 304: 2 |
| analysis | pi_tof | 194 (3 empty, 3 with S=0 at the best fit) | 69 (36%) | 124 | 1: 1 |
| source | k0s | 680 | 521 (77%) | 153 | 1: 2, 303: 1, 304: 3 |
| source | xi | 520 | 162 (31%) | 358 | |
| source | pi_tpc | 720 | 677 (94%) | 28 | 1: 6, 303: 3, 304: 6 |
| source | pi_tof | 700 | 82 (12%) | 617 | 1: 1 |

Empty cells: the pi_tof cells of the lowest pT bin have all their entries in the under/overflow of the n-sigma axis (TOF not available): 3 analysis (2026: (0,0), (7,0), (8,0)) and 13 source cells have in-range content 0;
the project still fits them: status 302, S = B = 0 and purity = 0/0 = NaN [verified, `res-det-a3`, `study09/out/y2026-*.tsv`]. 14 further analysis pi_tof cells have fewer than 1000 counts (1-3 counts for most of the 2026 ones). The
`entries` column of earlier notes counts under/overflow; in-range integrals are in `cells.tsv` (analysis medians: K0S 1.1e7, Xi 3.8e5, pi_tpc 5.3e8, pi_tof 1.8e8; maxima 1.9e8, 4.2e6, 1.05e10, 2.7e9).

---------------------------------------------------------------------------------------------------------------------------------

## 3. Results

### 3.1 The project's call against the best fit: yield, purity, errors (pool det) [verified, `res-det-a3.txt`]
Analysis cells, both files (n = valid cells; "dS" = |S_base - S_best|/S_best; "dP" = |P_base - P_best|):

| group | n | dS>0.1% | >1% | >5% | dS median | p90 | p99 | max | dP>0.001 | >0.01 | >0.05 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| all | 734 | 210 | 90 | 28 | 9.8e-5 | 1.93% | 25.2% | 326* | 154 | 67 | 7 |
| k0s | 200 | 70 | 61 | 16 | 1.6e-4 | 4.3% | 17% | 54.7% | 69 | 55 | 5 |
| xi | 140 | 27 | 6 | 1 | 1.1e-4 | 0.41% | 4.3% | 6.7% | 26 | 4 | 0 |
| pi_tpc | 200 | 98 | 12 | 5 | 9.9e-4 | 0.58% | 12.2% | 53.7% | 48 | 1 | 0 |
| pi_tof | 194 | 15 | 11 | 6 | 1.0e-6 | 2.2e-5 | 45% | 326* | 11 | 7 | 2 |
| file 2026 | 366 | 122 | 66 | 15 | 1.3e-4 | 2.9% | 24.9% | 54.7% | 90 | 50 | 5 |
| file 2024 | 368 | 88 | 24 | 13 | 6.8e-5 | 0.40% | 20.1% | 326* | 64 | 17 | 2 |

\* a pi_tof 2024 cell with 127 counts (S: 88.6 vs 0.27). |dP| (absolute): all median 3.2e-5, p90 6.3e-3, p99 4.4e-2, max 0.92; K0S p90 2.2e-2 p99 6.6e-2 max 0.147; Xi p90 3.0e-3 max 2.0e-2; pi_tpc p90 2.8e-3 max 4.0e-2.
In units of the best fit's own propagated error: the project's yield is off by more than 1 sigma in 38.7% of the analysis cells and by more than 3 sigma in 32.7% (K0S 44% / 34%, pi_tpc 83% / 78%, Xi 19% / 11%, pi_tof 3% / 2.6%);
the sigma are tiny at these statistics (1e-4 to 1e-3 relative), which is why a 0.1% shift is already many sigma.
Errors, project / best: S error ratio median 1.0, p90 16 (K0S 44, pi_tpc 1000), outside [0.5, 2] in 24.4% of cells (K0S 23.5%, Xi 5.7%, pi_tpc 57.5%, pi_tof 4.6%); P error outside [0.5,2] in 30.7% (K0S 24.5%, Xi 6.4%, pi_tpc 77.5%, pi_tof 6.2%).
Base NLL above the best by >0.5: 52.3% of analysis cells (median dNLL 1.1; K0S 68.5%, pi_tpc 100%, Xi 20.7%, pi_tof 9.3%).

Source cells (2620 valid): dS>0.1% 531 (20.3%), >1% 180 (6.9%), >5% 87 (3.3%); median 6.6e-5, p90 0.35%, p99 16%; K0S 139/113/48 (20.4%/16.6%/7.1%, p90 3.6%), Xi 53/4/2, pi_tpc 311/42/21 (43%/5.8%/2.9%), pi_tof 28/21/16;
dP>0.001: 398, >0.01: 130, >0.05: 24. The maximum (dS = 2610) is a pi_tof cell with a handful of counts.

Same comparison against pool **all** (including the multi-start): analysis dS>0.1% 235 (32.0%), >1% 106 (14.4%), >5% 29 (4.0%); p90 1.96%, p99 31.8%; dP>0.001 184, >0.01 80.

Status 0 is mostly reliable: of the 206 analysis cells with base status 0, 16 (7.8%) have dS>0.1%, 11 (5.3%) >1%, 3 >5%, and in 10 the NLL is >0.5 above the best. Of the 90 analysis cells with dS>1%, 78 had status 3, 11 status 0, 1 status 304.

### 3.2 By particle and pT bin [verified, in-line script on `out/y20*-analysis.tsv`; 10 multiplicity bins per pT bin]
- **K0S** (2026): pT bins 0-3 (0-1.2 GeV/c): 0 of 10 with dS>1%, max dP 1e-4 (but see 3.8: bins 2,3 have nsig at its limit); bins 4,5,6,7,8,9 (1.2-1.6, 1.6-2.0, 2.0-2.4, 2.4-3.0, 3.0-4.2, 4.2-6.4): 8,9,9,8,8,4 cells with dS>1%; max dS 0.40, 0.031, 0.062, 0.547, 0.094, 0.036; max dP 0.070, 0.018, 0.026, 0.147, 0.029, 0.019.
  2024: bins 4,5,6 affected in 5,2,5 cells (dS up to 6.4%, dP up to 0.045), bin 7 in 2 (up to 15.5%).
- **Xi** (2026): pT bins 1-4 (1.2-3.0): 1,3,1,1 cells with dS>1% (max 6.7%, dP up to 0.020); 2024: none above 1%.
- **pi_tpc**: purities are ~0.99-1.0 so dP stays small (max 0.040 in one 2026 cell, pT bin 9); the yield moves by >1% in 2 cells of each of bins 0,1,2 in 2026 (max 12%) and by 29% in one 2024 cell (pT bin 5).
- **pi_tof**: unaffected except the lowest pT bin (empty or <1000 counts) and a few isolated cells (2026 pT 6 and 7, one cell each: dS 0.015 and 0.417).

### 3.3 The variants against the best (pool det; analysis cells, n = 734; xrf and ms rows are optimistic because they define the best) [verified, `res-det-a6.txt`]
| variant | NLL within 0.5 of best | within 0.01 | dS>0.1% | >1% | >5% | dP>0.001 | covQual3 & edm<0.01 | CPU ms (mean) |
|---|---|---|---|---|---|---|---|---|
| base (project) | 350 | 171 | 210 | 90 | 28 | 154 | 185 | 3.45 |
| rst1 | 400 | 230 | 102 | 22 | 5 | 79 | 244 | 4.72 |
| rstN (<=10 refits) | 594 | 318 | 71 | 8 | 5 | 55 | 354 | 8.71 |
| s2 | 311 | 178 | 247 | 158 | 75 | 205 | 181 | 4.04 |
| off | 415 | 299 | 212 | 103 | 47 | 174 | 339 | 3.51 |
| off_rst1 | 559 | 523 | 128 | 22 | 10 | 99 | 646 | 5.07 |
| offN | 584 | 557 | 111 | 13 | 8 | 84 | 590 | 5.91 |
| xrf | 642 | 602 | 60 | 3 | 2 | 43 | 646 | 9.89 |
| rec0 / rec100 | 347 / 348 | 170 / 171 | 207 / 208 | 87 / 89 | 26 / 29 | 152 / 153 | 185 / 186 | 3.5 |
| ms (15 starts) | 732 | 712 | 53 | 23 | 4 | 64 | 575 | 80 |
| base_rel / xrf_rel (limits relaxed) | 354 / 630 | | 231 / 101 | 95 / 26 | 39 / 17 | | | 3.6 / 10.0 |

Source cells show the same ordering (base 531/180/87 for dS>0.1%/1%/5%, rst1 196/21/10, rstN 161/9/6, off 505/216/125, off_rst1 276/33/15, offN 258/20/11, xrf 196/9/3, s2 736/386/227). Observations:
`Strategy(2)` is clearly worse than the project's call on these cells (dS>1% in 158 vs 90); `Offset(true)` alone does not help the yield on merged cells (103 vs 90) although it raises the share of status-0 fits from 27% to ~50%
(note 07 found it useful on 30 low-statistics source cells; on the merged high-statistics cells it is not enough); `RecoverFromUndefinedRegions(0/100)` changes nothing (within 3 cells of base); restarts are what moves the result;
Offset(true) + restart reaches the same quality as many plain restarts at lower cost. `Optimize(0|1|2)`, omitted `Optimize`, `Hesse(true)`, `PrintEvalErrors(-1)`, `Warnings(false)`, `Strategy(1)` are bit-identical to the project's call
in all 1674 non-empty cells of the 2026 file [verified, `study09/opt.cpp`: minNll, edm, status, every floating value and error equal; max relative difference 0].

Restarts needed [verified, `res-det-a7.txt`]: a plain restart chain (project call) needs 1 refit in 225 of 734 analysis cells, 2 in 118, 3 in 45, 4-10 in 192, and is NOT converged after 10 refits in 154 cells (21%);
with `Offset(true)` the chain needs 1 refit in 320 cells, 2 in 349, 3 in 50, 4-9 in 15 (none above 9). Source cells: plain chain 1:1350, 2:320, 3:126, 4-10:481, not converged 343 (13%); offset chain 1:1627, 2:844, 3:118, 4-8:31.

### 3.4 Classifying the status 3 cells [verified, `res-det-a5.txt`, pool det]
Definitions: **C bad** = the best fit has covQual<3, or no variant reached (covQual 3 and edm<0.01), or `nsig`/`nbkg` sits at a limit in the best fit, or the base fit logged invalid-NLL (NaN) evaluations; **A precision** = not C, base NLL within 0.5 of the best,
|dS|/S<0.1% and |dP|<0.001 (stable under refit); **B not converged** = the rest. Shape parameters at a limit (tails n, alpha; sigma; c1) are NOT counted as bad; they are tallied separately (3.8).

| set / key | status 3 cells | A precision | B not converged | C bad | hard flags (not exclusive) |
|---|---|---|---|---|---|
| analysis all | 523 | 148 (28%) | 273 (52%) | 102 (20%) | best covQual<3: 56, NaN evals in base: 34, yield param at limit: 21, no variant converged: 6 |
| k0s | 188 | 52 | 78 | 58 | NaN 34 (Chebychev2 cells), covQual<3 17, yield limit 17, none converged 1 |
| xi | 71 | 44 | 26 | 1 | covQual<3 1 |
| pi_tpc | 195 | 0 | 153 | 42 | covQual<3 38, none converged 5, yield limit 3 |
| pi_tof | 69 | 52 | 16 | 1 | yield limit 1 |
| source all | 1442 | 453 (31%) | 717 (50%) | 272 (19%) | covQual<3 156, yield limit 85, NaN 38, none converged 3 |

Yield shift of the project's result within each class (analysis, dS = |S_base-S_best|/S_best): A median 3.3e-5 (p90 1.7e-4, max 5.0e-4); B median 1.0e-3, p90 2.8%, p99 32%, max 55%; C median 1.1e-3, p90 5.7%, p99 54%, max 326.
Status 3 is therefore not a failure flag in itself: a third of the status-3 cells are stable under refit, but half move measurably and a fifth are bad for other reasons. pi_tpc has no stable status-3 cell at all (the project's
call is above the best NLL by a median 5.4e5 there: the Gaussian-background + DSCB model on 1e8-1e10 counts is far from convergence after one MIGRAD).

### 3.5 Is "EDM small relative to |NLL|" a usable benign test? [verified, `res-det-a5.txt`]
Minuit2 cannot resolve an EDM below about EPS2*|NLL| with EPS2 = 5.9e-8 (calibrated on note 03's log: machine limit 2.22 at NLL -3.7e7) [source note 03 + log]. For the status-3 analysis cells (523), ratio r = edm/(EPS2*|NLL|) of the project's fit:

| r | cells | A / B / C | dS>0.1% | dS>1% | base NLL >0.5 above best |
|---|---|---|---|---|---|
| <=1 | 332 | 148 / 135 / 49 | 38 (11%) | 10 (3%) | 179 (54%) |
| 1-10 | 21 | 0 / 14 / 7 | 13 | 1 | 21 |
| 10-1e3 | 32 | 0 / 24 / 8 | 24 | 7 | 32 |
| >1e3 | 138 | 0 / 100 / 38 | 115 | 60 | 138 |

So r<=1 is a necessary condition for a stable result (all 191 cells with r>1 had a lower NLL elsewhere) but far from sufficient for NLL optimality; for the yield alone it is a decent filter at the 1% level (3% of those cells move by >1%).
The base EDM for status 3 spans median 4.95, p90 3.7e9, p99 5.5e12, max 1.5e14 (analysis); only 21% of the status-3 cells have EDM<0.1 and 38% have EDM>100: for these the "precision limit" explanation is simply wrong, the fit stopped far from the minimum.
(Mechanism, [inferred] from the sequences of refits: with |NLL| of 1e7-1e10 Minuit2's numerical derivative step is set by the machine-precision estimate and MIGRAD stops early; each refit restarts with new step sizes and gains a few units of NLL; subtracting the starting NLL with `Offset(true)` removes the large constant and lets the EDM be resolved to 1e-6.)

### 3.6 A cheap, robust procedure [verified, `res-det-a7.txt`, `res-det-a10.txt`, `res-det-a11.txt`]
Leave-one-out evaluation: each procedure is judged against the best NLL among the OTHER deterministic variants (so it does not define its own reference); cost = CPU of the fits the procedure executes (mean per analysis cell, main run;
the clean timing run, section 6.2, gives 3.0 / 4.8 / 9.5 ms for base / off_rst1 / xrf).

| procedure | NLL within 0.5 of best-other | dS>0.1% | >1% | >5% | dP>0.001 | CPU ms | accepted (covQual3 & edm<0.01) |
|---|---|---|---|---|---|---|---|
| P0 project call | 350 / 734 | 210 | 90 | 28 | 154 | 3.45 | 185 |
| P1 base + 1 refit | 400 | 102 | 22 | 5 | 79 | 4.72 | 244 |
| P2 plain refits until NLL stable (<=10) | 594 | 73 | 9 | 5 | 56 | 8.71 | 354 |
| P3 `Offset(true)` | 415 | 213 | 104 | 48 | 176 | 3.51 | 339 |
| P4 `Offset(true)` + 1 refit | 559 | 128 | 22 | 10 | 100 | 5.07 | 646 |
| P5 `Offset(true)` refits until stable | 584 | 112 | 13 | 8 | 84 | 5.91 | 590 |
| P6 xRooFit minimize | 642 | 82 | 5 | 2 | 59 | 9.89 | 646 |
| **P7 = P4, if not accepted then xRooFit (keep lower NLL)** | 623 | 88 | **10** | **6** | 75 | **6.73** | 701 (33 stay flagged) |
| P8 = P5, then xRooFit | 650 | 78 | 7 | 5 | 70 | 8.74 | 668 (66 flagged) |

Source cells (2620): P0 531/180/87 (dS>0.1%/1%/5%), P4 276/33/15, P6 (leave-one-out) 234/11/5 (NLL<0.5: 2356), P7 248/11/3 (escalated 242, flagged 81, CPU 4.71 ms vs 2.79 base).
Acceptance test, "of the cells accepted, how many are wrong" with wrong = NLL>0.5 above the best-other OR dS>0.1% (dS>1% in brackets): base status 0 & covQual 3: 201 accepted, 14 wrong (7 with dS>1%);
`Offset` + 1 refit + (covQual 3 & edm<0.01): 646 accepted (88%), 138 wrong at the strict criterion (21%), but only 11 (1.7%) with dS>1%; xRooFit + (covQual 3 & edm<0.01): 646 accepted, 89 strict-wrong, 5 with dS>1%.
**The raw-NLL acceptance "edm < max(0.01, 10*EPS2*|NLL|)" is NOT safe**: 646 of 734 accepted after one plain refit, 256 of them wrong (40%) at the strict criterion; it accepts too much, because above ~1e7 the EDM floor is simply what Minuit reports.
Hence the order matters: the edm test is meaningful only after `Offset(true)`.
Recommended DoFit procedure: **fit with `Offset(true)`, refit once from the result with `Offset(true)`, accept if `covQual()==3 && edm()<0.01`; otherwise run xRooFit (or, without that dependency, repeat the offset refit up to 10 times until the NLL changes by <0.01) and keep the lower NLL; store status/covQual/edm/history and mark the cell if it is still not accepted.**
Time: at ~6 ms per cell the 1690 fits of one purity file cost about 10 s against 5 s now [inferred from the per-cell means; the drawing, which dominates the real run, was not measured]. Note that in the offset fits `fr->minNll()` is NOT the NLL (it is the offset-subtracted value, e.g. -0.007 where the true value is -3.7e7):
use a separately created NLL if the true value is wanted. The project does not store the NLL today.

### 3.7 Residual ambiguity after a good refit: multiple minima [verified, `res-det-a8.txt`, in-line script]
- `ms` NLL is lower than the best deterministic variant by more than 0.5 in 73/734 analysis cells (K0S 11, Xi 30, pi_tpc 32; source: 259/2620, K0S 44, Xi 118, pi_tpc 97), median gap 133 (analysis), max 9.6e6. In these cells the yield of `ms` differs by a median 0.24%
  (p90 1.3%, max 1.7%; source: median 0.19%, max 1.7%) and the purity by 0.2% (max 1.4%). Over all analysis cells S_ms and S_det differ by >0.1% in 53 cells, >1% in 23, >5% in 4; the 4 are pi_tof 2024 cells with 28-127 counts.
- The Xi solutions found by `ms` in the 12 cells of largest yield difference: `sigma_sig` at its lower limit 0.002, n1 (and n2) at the lower limit 1 (power-law tails), NLL lower by 34-590, found by only 1-2 of 15 starts; xRooFit and every Offset fit stay on the narrower-tail branch. S differs by 1-1.7%.
- [second pass, 9.4] With 50 starts on 40 cells, the solutions within dNLL<0.5 of the best differ in S by at most 1.6e-4: the differences above come from distinguishable minima, not from a flat likelihood.
- Among the deterministic variants within dNLL<0.01 of the best the yield spread is median 1.2e-6, p90 2.4e-4, p99 1.7% (analysis; 54 cells above 0.1%, 25 above 1%): the yield is well determined once two fits reach the same minimum, and badly determined in the flat-valley cells.
- The cells where fewer than a third of the 15 random starts reach the best (multimodal): 83/734 analysis (Xi 48, pi_tpc 24, K0S 10, pi_tof 1), 243/2620 source.

### 3.8 Parameters at limits, and the yield clipping [verified, `res-det-a9.txt`, in-line scripts]
- **nsig and nbkg limits** (K0S and Xi: nsig [0,1e8], nbkg [0,2.5e7]; pions nsig [0,1e10], nbkg [0,2e9]): the 2026 K0S analysis cells with in-range N > 1e8 are 19 (mult x pT bins 2-5; N up to 1.85e8); the best deterministic fit sits at `nsig_hi` in 17 of them and at `nbkg_hi` in one;
  2 pi_tpc analysis cells (pT bin 0, mult 8 and 9; N ~ 1e10) too. Relaxing both maxima to 1e11 (`xrf_rel`): NLL lower by 2.2e7 at most; in the 20 affected analysis cells |dS|/S median 21.9%, p90 53.8%, max 77.8%, |dP| median 0.016, p90 0.042, max 0.061
  (examples [verified]: K0S mult 8 pT 3: S 9.999e7 -> 1.778e8, P 0.9424 -> 0.9838; mult 5 pT 4: S 9.996e7 (best clipped) vs 1.257e8, P 0.9045 vs 0.9655; the project's own result in that cell is S 5.99e7, P 0.9708: the clipped fit AND the unconverged fit disagree by 40%).
  No source cell reaches these limits (maximum source in-range N 5.9e7 for K0S, 1.6e6 Xi) and the 2024 file is 10x smaller: the CCDB map is not affected, the analysis-binning purity of the 2026 K0S file is.
- Shape parameters at a limit in the best deterministic fit (analysis; the K0S/Xi `n` limits are [1,150], alpha [0.5,5], sigma [0.002,0.008]): K0S n2_sig_hi 41, n1_sig_hi 18, c1_bkg_lo 15; **Xi sigma_sig_lo 125 of 140 cells**; pi_tpc n2_sig_hi 171, n1_sig_hi 20, n2_sig_lo 15, sigma_bkg_hi 14;
  pi_tof slope_bkg_hi 130 (slope max 0), nbkg_lo and nsig_lo 14 each (tiny cells) (1e-5-of-range criterion, `atlimit` in `lib.py`). The effect of relaxing these is measured in 9.2.

### 3.9 Zero-integral and tiny cells
pi_tof lowest-pT cells: see section 2. In the 2024 file 8 of 10 pi_tof pT-bin-0 analysis cells have 28-158 counts, the yield and purity of which are ill-determined (dS up to 3.3e2 between variants, |dP| up to 0.92): they should be excluded or merged, whatever the fit.

---------------------------------------------------------------------------------------------------------------------------------

## 4. Answers to the four questions

(i) **How often and by how much do yield, error and purity differ between the project's call and the best variant?** Section 3.1 (pool det) and 3.3 table: median |dS|/S 1.0e-4, p90 1.9%, p99 25%, max 55% (excluding a 127-count cell); S off by >0.1% / >1% / >5% in 210 / 90 / 28 of 734 analysis cells (K0S 70/61/16);
|dP| median 3e-5, p90 0.63%, p99 4.4%; error ratio outside [0.5,2] in 24% (S) and 31% (P) of cells.
(ii) **Classification of status 3**: Section 3.4: 28% precision-limit, 52% genuinely not converged, 20% bad (analysis; source 31/50/19%).
(iii) **A simple robust procedure**: Section 3.6; Offset(true) + one refit + acceptance (covQual 3, edm<0.01) + escalation; cost x1.6 (x2 with escalation), reduces S errors above 1% from 12.3% to 1.4% of cells, leaves 4.5% flagged.
(iv) **Cells to look at by hand**: Section 5.

---------------------------------------------------------------------------------------------------------------------------------

## 5. Cells a physicist should look at by hand

Flags per cell (pool det best fit): HARD = yield parameter (nsig/nbkg) at a limit; best covQual<3; no variant reached covQual 3 and edm<0.01; yield spread >0.1% among variants within dNLL<0.5 of the best; multimodal (<=1/3 of 15 random starts reach the best);
fewer than 1000 counts. Counts: analysis 177 of 734 cells have >=1 hard flag (24%; K0S 46, Xi 50, pi_tpc 66, pi_tof 15); source 486 of 2620 (18.5%). Of the 90 analysis cells where the project's yield is off by >1%, 38 carry a hard flag (a better single fit does
not make them trustworthy) and 52 do not (they are fixed by the refit); for >0.1%: 95 with a flag, 115 without.

Analysis cells (mult, pT bin) with hard flags, 2026 file (125 of 366 cells; the real purity input):

| particle | flag | cells (mult,pT) |
|---|---|---|
| k0s | nsig/nbkg at upper limit (config clipping, 18) | (5,2)(6,2)(8,2)(9,2) (1,3)(2,3)(3,3)(4,3)(5,3)(6,3)(7,3)(8,3)(9,3) (2,4)(5,4)(6,4)(8,4) (5,5) |
| k0s | best covQual<3 (12) | (9,0)(2,1)(1,3)(4,4)(2,5)(4,5)(9,5)(4,6)(7,6)(8,6)(0,7)(1,7) |
| k0s | no variant converged / multimodal / yield spread | (9,5) / (7,1)(4,3)(0,5)(5,9) / (6,4)(3,5) |
| xi | multimodal (39) and yield spread >0.1% (30) | pT 1-4 in nearly all multiplicity bins, plus (1,6)(3,6)(4,6)(7,6); covQual<3: (9,3) |
| pi_tpc | best covQual<3 (24) | (6,0)(3,1)(4,1)(5,1)(2,2)(3,2)(2,3)(4,3)(6,3)(7,3)(8,3)(9,3)(3,4)(6,4)(8,4)(0,5)(4,5)(5,5)(7,5)(0,6)(2,6)(1,7)(1,9)(2,9) |
| pi_tpc | no variant converged (5) / yield at limit (2) / multimodal (21) | (2,3)(8,3)(9,3)(9,4)(5,9) / (8,0)(9,0) / pT 0,1,4,5,6,9 scattered |
| pi_tof | tiny (<1000) or covQual<3 | pT 0: mult 1,2,4,5,6,9 (1-3 counts) |

2024 file: 52 of 368 analysis cells flagged (K0S covQual<3: (5,2)(7,2)(9,5)(2,6)(8,7)(5,9); K0S multimodal pT 6 and others; pi_tpc covQual<3 in 14 cells; Xi pT 1 multimodal in 9 cells; pi_tof pT 0: 8 cells with <200 counts).
Master list of every cell (file, set, key, mult, pT, base status/covQual/edm, best variant, dNLL, S, P of project and best, class, hard and all flags): `study09/cells-master.tsv`.
Other things the owner should know about: (a) the override indices (1.1), (b) the archived Xi purities (7 Aug) used the observable range [1.2,1.4]; the current range [1.29,1.36] lowers P by a median 0.074 (section 9.1), (c) 0/0 purity (NaN) written for empty pi_tof cells, (d) the K0S cells with the Chebychev2 override (pT index 4 and 5, both binnings) log invalid-NLL evaluations during MINIMIZE (nInvalid>0 in 40 of 200 analysis and 39 of 680 source K0S cells; Chebychev2 with c1,c2 in [-1,1] is negative over part of its box, note 01).

---------------------------------------------------------------------------------------------------------------------------------

## 6. Rigor checks

6.1 Order independence and determinism [verified]: the base result of K0S 2026 analysis (7,7) is bit-identical (S = 7796506.224031806) in a single-cell run and in the 370-cell run; a rerun of the 370 cells (timing run) reproduced every S of base, off_rst1 and xrf bit for bit (max difference 0).
6.2 Timing [verified, `study09/timing/`]: the clean run (one process, 2026 analysis cells, two other single-threaded jobs `phisynth` and `cmp` on the 10-core machine, load average ~2.4) gave per-cell CPU (wall) means: base 3.02 (3.03), rst1 4.20, rstN 8.67, s2 3.48, off 3.33, off_rst1 4.84, offN 5.82, xrf 8.86 (9.45), ms 73 ms; medians 2.25, 3.42, 8.67, 2.73, 2.56, 4.47, 5.60, 8.33, 93.
The main runs used two processes while others were also running (load average up to 5.7): means there are 5-15% higher (base 3.45 for 2026+2024 analysis). Take the ratios, not the absolute values, as the result: off_rst1 / base = 1.6, xrf / base = 3, ms / base = 24. No timing conclusion is drawn from the follow-up runs (another job was running).
6.3 Same-data same-model comparison [verified]: the NLL of every variant is evaluated by one `createNLL(dh, Extended(true))` without offset at that variant's final parameter values, in the same fitter (so no mixing of data, model or offset); for `base` it equals `fitResult->minNll()` to 3e-14.
The windows are identical because the configuration is not changed by any variant (except nsig/nbkg limits in `*_rel`). Source and analysis cells are never pooled. The cell order within every variant is identical (one row per (cell, variant), 14 rows per cell in fixed order).
6.4 Reference quality: "best" = lowest NLL found. It is not proven to be the global minimum (3.7 shows it is not, in 10% of cells, by up to 590 NLL units), and a variant that defines the best is optimistic (hence the leave-one-out table). Where two minima are within dNLL<0.5 the yield difference
is not a fit-quality artefact but a likelihood property.
6.5 Xi archive mismatch: solved, section 9.1 (observable range change of 27 Aug).
6.6 Statistical independence: cells within a file are not independent (merged and source bins overlap; the two files share the configuration). Percentages are descriptive, not estimates with error bars. The 2024 and 2026 files give consistent but different rates (analysis dS>1%: 6.5% vs 18%), driven by the statistics (K0S bins with N>1e7).

---------------------------------------------------------------------------------------------------------------------------------

## 7. What could not be done / uncertainties
- PurityTask itself was not run by me (the spectra it writes are validated cell by cell instead: 2720 of 3380 cells bit-exact with the current config, all 3380 with the 7 Aug Xi config; another researcher runs it end to end, note 11).
- The 23luglio file was used only for the source-cell cross-check with the earlier notes (`out/y2023luglio-source.tsv`); it cannot feed K0S `rebinning_pt` (edge 6.5).
- The multi-start search is a 15-start search with an Offset fit chain (50 starts on 40 cells in 9.4); more starts or other optimisers could still find lower minima. The true global minimum is unknown, but in 9.4 the 20-start search missed the 50-start best in only 1 of 40 cells.
- The effect of the changes on the final correlation results was not propagated: `apply_purity` is false in `correlation_base_settings` of `globalConfigBasepp.json` (read, not tested downstream), so the purity enters through the CCDB map / checks; the yield S itself was compared as well because the question asked for it. I did not follow how the K0S purity enters any final number.
- Error comparison: the project's error is compared with the error of the best fit (also HESSE based). I did not run toys (note 12 does), so "the best fit's error is right" is [inferred] here; note 04 found the propagated error within ~1% of covariance toys when covQual is 3 and statistics are high.
- The `Offset` variants were compared by the common NLL, but the recorded `minNll` of those rows is offset-subtracted; only the `nllCommon` column is comparable.
- Timing: see 6.2 (shared machine, 10 cores, two foreign single-threaded jobs).
- Not tested: threads (`NumCPU`), `Minos`, Minuit (old) `migradImproved` alone, GSL. The follow-up items still open are in 9.8 and `## Resume here`.

---------------------------------------------------------------------------------------------------------------------------------

## 8. Files and how to reproduce (all in `/Users/scannito/alice/Analysis/RooFitKnowledge/scratch/study09/`)
- `study.cpp` (harness; build `bash build.sh study`; run `./study <file.root> <analysis|source> k0s,xi,pi_tpc,pi_tof out.tsv [mod:K | cell:i:k]`), `parts.h` (generated from `globalConfigBasepp.json`), `common.h`, `runall.sh`.
  Outputs `out/y2026-analysis.tsv`, `out/y2026-source.tsv`, `out/y2024-*.tsv`, `out/y2023luglio-source.tsv` (14 variant rows per cell; columns file,set,key,i,k,binlo,binhi,entries,nonzero,variant,status,hist,covQual,edm,minNll,nllCommon,nInvalid,nsig,...,S,Serr,B,Berr,P,Perr,nAtLimit,limitPars,maxAbsCorr,wallMs,cpuMs,nIter,note,pars).
  Earlier incomplete variants of the run (before `ms` and the xRooFit call cap) are in `out_v1`, `out_v2`, `out_v3` and gave the same numbers for the common variants.
- `cells.cpp` -> `cells.tsv` (in-range integral, max bin, non-zero bins per cell), `opt.cpp` (Optimize/Hesse/... bit-identity), `dumppur.C` -> `stored-purity.tsv` (the project's archived purities, read only), `insp.C` (file/axis inspection).
- Analysis (plain python, no numpy): `lib.py`, `a1.py`...`a12.py`; results `res-det-a*.txt` (pool det, primary) and `res-all-a*.txt` (pool det + ms), `cells-master.tsv`. `REF=det python3 a3.py` etc.
- Follow-up: `relax.cpp`, `rowlib.h`, `runrelax.sh`, `out_relax/`, `a12.py` (limit families); `toys.cpp`, `toycells.tsv`; `fitConfig-xi-7aug.json`, `out_xi7aug/` (Xi with the 7 Aug config, `FITCFG` env).
- Second follow-up: `msx.cpp` + `runmsx.sh` -> `out_msx/` (paired limit study, `a13.py` -> `res-a13-analysis.txt`); `runms50.sh`, `ms50cells-*.txt` -> `out_ms50/` (50 starts, per-start rows in `*-starts.tsv`; `a14.py` -> `res-a14.txt`); `fixp.cpp`, `mkfix.py` -> `fixvals.tsv`, `out_fixp/` (`a15.py` -> `res-a15.txt`); `offib.cpp` + `runoffib.sh` -> `out_offib/` (`a16.py` -> `res-a16.txt`); `soc.cpp` + `run2.sh` -> `out_soc/`.
- `timing/t-analysis-2026.tsv`, `timing/load.txt`.
- Nothing was written to `Codes/`, `JSONConfigs/`, `DataFile/`, `Logs/` or the project root; the data files were opened read-only.

---------------------------------------------------------------------------------------------------------------------------------

## 9. Follow-up passes (items requested after the first delivery)

### 9.1 The Xi mismatch with the archived 7 Aug purity files: SOLVED [verified]
- `git log` on `JSONConfigs/fitConfig.json` after 7 Aug: 3a30c12 (26 Aug, phi observable max 1.055 -> 1.1), **3e2878a (27 Aug, "Some adjustements or comparison task": xi observable `min` 1.2 -> 1.29, `max` 1.4 -> 1.36)**, 7c34e47 (12 Oct, sideband schema of phi only, no xi change). `globalConfigBasepp.json`/`Puritypp.json`: no xi window or rebinning change relevant to the archive (rebinning_pt for xi `[0.8,...,6.5]` is the same).
- Rebuilt in scratch (`scratch/study09/fitConfig-xi-7aug.json` = current file with xi observable [1.2,1.4]; the project file untouched; harness reads it through `FITCFG`): `out_xi7aug/*.tsv`. **All 660 Xi cells (2024 and 2026 files, analysis and source binning) reproduce the archived purity (|dP|<1e-7 after float rounding) and its error (<1e-6): 0 mismatches.**
- Effect of the change on the Xi purity (same code, same data): |P(current) - P(7 Aug)| over the 660 cells: median 0.074, p90 0.092, max 0.111 (current is lower: e.g. 0.877 vs 0.954). The signal window [1.305,1.335] is unchanged; the change is the fit range (72 instead of 200 bins), i.e. which sidebands constrain the background shape and normalisation. Which range is right is a physics decision for the owner; the archived Xi purity files are NOT from the current config.
- With the 7 Aug Xi config the convergence picture is milder (project's call vs best of 11 deterministic fits, valid cells): analysis 140 cells, status 3 in 29, dS>0.1%/1%/5% in 15/13/9 cells (max 17%), dP p90 0.0014 max 0.057; source 520 cells, status 3 in 99, 21/20/15 cells, max 26%. (`out_xi7aug`, in-line script.)

### 9.2 Relaxing non-yield limits with PAIRED multi-starts [verified, `msx.cpp`, `runmsx.sh`, `out_msx/`, `a13.py` -> `res-a13-analysis.txt`]
**Method (second pass, replaces the first-pass numbers, which mixed limit effects with procedure variability).** For every analysis cell of both files (734 valid): the standard limits (`std`) and one relaxed family at a time are fitted from the SAME 20 random start points (drawn once per cell inside the STANDARD ranges, fixed seed), with the SAME procedure per start (`Offset(true)` fit repeated until the common NLL changes by <0.01, at most 6 fits), lowest common NLL kept. The relaxed run gets one extra start at the std best (so it can only go lower); and the std model is refitted from the relaxed best clipped into the std ranges (`stdfrom_<family>`), the std reference being the lower of the two. **True gain = NLL(std reference) - NLL(relaxed)**; "search-only gain" counts cells where the refit from the relaxed best found a lower std minimum (i.e. the relaxed run only found a better minimum that the std model also has). Families: `sigma_lo` (sigma_sig min/10), `n_lo` (n1,n2 min 0.1), `n_hi` (max 1e4), `alpha` ([0.1,20]), `c_bkg` (c1,c2 in [-3,3]), `nbkg_neg` (nbkg min -N), `slope_hi` (pi_tof slope max +2). Same window, same extraction.

| key | family | cells | param beyond old limit | TRUE gain >0.5 (median / max gain) | in gain cells: dS/S median / p90 / max | dP median / p90 / max | relaxed values (gain cells) | physical? |
|---|---|---|---|---|---|---|---|---|
| xi | sigma_lo | 140 | 125 | **122** (775 / 3.1e4) | 0.56% / 2.3% / 2.9% (S lower: signed median -0.56%) | 0.0036 / 0.016 / 0.021 (P lower) | sigma 1.50-1.97 MeV, median 1.72 MeV | plausible (bin 1 MeV); physics decision |
| xi | n_lo | 140 | 16 | 16 (468 / 1.4e3) | 8.1% / 9.3% / 10.1% (S higher) | 0.075 / 0.085 / 0.091 | n 0.17-2.4, median 0.33 | NO: n<1 tail not normalisable, the signal tail absorbs background (pT bins 3,4) |
| xi | n_hi, alpha | 140 | 1, 0 | 0, 0 (search-only gain in 7 and 8 cells) | - | - | - | limits harmless |
| k0s | c_bkg | 200 | 15 | 15 (568 / 1.3e3) | 0.66% / 0.72% / 0.76% (S higher) | 0.0061 / 0.0066 / 0.0070 | all 15 in analysis pT bin 4 (Chebychev2 override); pdf negative somewhere in 9 of 15 | mixed: 6 of 15 positive pdf -> the [-1,1]^2 box is not the positivity condition of Chebychev2 (note 01); 9 unphysical |
| k0s | n_hi | 200 | 57 | 54 (8.6 / 8.9e3) | 7.5e-5 / 3.3e-4 / 3.4e-4 | 6e-5 / 1.9e-4 / 2.0e-4 | n up to 1e4 (median 8081) | yes (n -> infinity = Gaussian+exponential tail); harmless for S |
| k0s | n_lo, alpha | 200 | 0, 0 | 0, 0 | - | - | - | limits not binding |
| pi_tpc | n_hi | 200 | 181 | 181 (3.0e4 / 7.5e5) | 1.2e-4 / 2.0e-4 / 3.4e-4 | 3e-6 / 4.5e-5 / 1.9e-4 | n median 1e4 (runs to the new limit) | yes; harmless for S |
| pi_tpc | n_lo | 200 | 2 | 2 (4.7 / 7.4) | <1e-6 | <1e-9 | n1 or n2 0.90 | harmless |
| pi_tpc | alpha | 200 | 0 | 0 | - | - | - | not binding |
| pi_tpc | **nbkg_neg** | 200 | **0** | **0** | - | - | nbkg never goes below 0 | **the fit does not ask for a negative background**; keep nbkg >= 0 |
| pi_tof | slope_hi | 196 | 131 | 130 (2.9e5 / 2.5e6) | 0.11% / 0.55% / 0.64% | 0.0010 / 0.0041 / 0.0052 | slope +0.008 to +0.10 (median +0.068) | a background rising with n-sigma; [inferred] plausible (heavier particles at positive n-sigma_TOF); owner's choice |

What this changes with respect to the first pass (9.2 of the previous version): the pi_tpc "nbkg<0 in 7 cells" and all the "NLL drops" of families whose parameter stayed inside its limits were procedure variability; with paired starts `alpha`, `n_lo` (K0S, pi_tpc) and `nbkg_neg` show NO gain at all. The binding limits, in order of effect on S and P:
1. **Xi `sigma_sig` >= 0.002**: binding in 122/140 cells, the fit wants 1.5-2.0 MeV; S drops by a median 0.56% (max 2.9%), P by 0.004 (max 0.021). **Physics decision** (is a 1.7 MeV Xi core resolution plausible in this data? it is above the 1 MeV bin width). Evidence favours lowering the limit to ~0.001.
2. **Xi `n` >= 1**: relaxing gives n ~0.3 and S +8%: **keep n >= 1** (unphysical tail).
3. **K0S Chebychev2 box in analysis pT bin 4**: relaxing c1/c2 raises S by 0.7%; in 9 of 15 cells the solution has a negative pdf somewhere: **keep the box or, better, constrain positivity explicitly** (physics/model decision; note 01 discusses the Chebychev2 positivity region).
4. **pi_tof `slope_bkg` <= 0**: binding in 130/196 cells; S changes by 0.1% (max 0.6%), P by 0.001 (max 0.005). Owner's choice; small.
5. **DSCB `n` <= 150 (K0S) / 50 (pi_tpc)**: binding often but S moves by <0.035%: raising it is free and harmless; **`alpha` and `nbkg >= 0` are not binding**: keep.
Not done: the same paired study on the source cells (the first-pass, unpaired numbers for source cells are in `out_relax/`).

### 9.3 Purity bias with toys: moved to note 12 (another researcher). `scratch/study09/toys.cpp` and `toycells.tsv` from this note were the starting point.

### 9.4 Is the lowest minimum found? 50-start search on 40 cells [verified, `msx.cpp` (family std, 50 starts), `runms50.sh`, `out_ms50/`, `a14.py` -> `res-a14.txt`]
Cells: 40 analysis cells, 5 per particle and file at the N quantiles 5/30/50/70/95% (`ms50cells-y2026.txt`, `ms50cells-y2024.txt`). Each start: `Offset(true)` fit repeated until stable. Compared with the 50-start best:

| fit | NLL >0.5 above the 50-start best | (>5) | dS>0.1% | dS>1% | max dS/S | max dP | by particle (cells above / 10) |
|---|---|---|---|---|---|---|---|
| project's call | 20/40 | 17 | 10 | 6 | 41.7% | 0.014 | k0s 6, xi 2, pi_tpc 10, pi_tof 2 |
| recommended procedure P7 (rows combined as in 3.6) | 8/40 | 8 | 5 | 1 | 1.6% | 0.014 | k0s 1, xi 2, pi_tpc 5, pi_tof 0 |
| best of 11 deterministic variants | 4/40 | 2 | 1 | 1 | 1.6% | 0.014 | k0s 1, xi 1, pi_tpc 2, pi_tof 0 |
| 15-start multi-start of the study (`ms`) | 2/40 | 2 | 2 | 1 | 1.2% | 0.011 | xi 2 |
| 20-start multi-start of 9.2 (`std`) | 1/40 | 1 | 1 | 1 | 1.2% | 0.010 | xi 1 |

- **The ambiguity among solutions that are statistically indistinguishable (within dNLL<0.5 of the 50-start best) is negligible**: S spread (max-min)/S median 2.5e-6, p90 3.9e-5, max 1.6e-4, i.e. at most 0.58 sigma_S; P spread median 2.6e-6, p90 7.6e-4, max 0.0026. Within dNLL<2: S spread max 0.25%, P spread max 0.0026.
- So the 1-2% yield differences of 3.7 are NOT a likelihood ambiguity: they are fits stuck in a DISTINGUISHABLE higher minimum (dNLL > 5 in all the affected cells here). **It is not a systematic to quote; it is a failure that a multi-start removes.** The one exception to keep an eye on: P can move by up to ~0.003 between equal-NLL solutions (background shape under the peak), which is at the level of the statistical error of P in the large cells.
- Hard cells: only 2 of 40 cells (both Xi: 2026 (4,3) and 2024 (4,1)) have <=10% of the 50 starts reaching the best; median fraction 0.76.
- Consequence for the recommendation: the recommended single-chain procedure (P7) is still above the lowest minimum in 8/40 cells (pi_tpc 5/10), and the in-sequence escalation is worse than the combined rows suggest (note 11: 80 of 737 analysis cells stay flagged against 33 here). A 20-start offset multi-start reached the 50-start best in 39/40 cells at a median CPU of 33x the project's fit per cell (p90 56x; `out_msx` cpuMs vs the study's base, both measured with other jobs running: relative only, no timing conclusion).

### 9.5 Fixed tails and Strategy 0 (MEASUREMENT ONLY; it changes the physics, no recommendation) [verified, `fixp.cpp`, `mkfix.py` -> `fixvals.tsv`, `out_fixp/`, `a15.py` -> `res-a15.txt`]
Fits by hand with `RooMinimizer` (same steps as `fitTo`; `base_m` reproduces the project's fit exactly: max |dS|/S = 0 over 540 cells) so that `RooMinimizer::evalCounter()` gives the number of NLL evaluations. n1, n2, alpha1, alpha2 fixed to the median of the converged best fits per particle and pT region (K0S/pi_tpc analysis pT 0-3, 4-6, 7-9; Xi 0-2, 3-4, 5-6; `fixvals.tsv`; e.g. K0S region 0: n1 7.4, n2 150, alpha 1.29/1.72; pi_tpc: n2 at its limit 50 in all regions). DSCB cells only (K0S, Xi, pi_tpc; 540 analysis cells). Reference: best deterministic free fit.

| variant | MIGRAD calls median (mean) | MIGRAD+HESSE median | status 0 | covQual3 & edm<0.01 | S - S_free: median, abs p90, abs max, cells >1% | P - P_free: median, abs p90 | NLL above free best (median) |
|---|---|---|---|---|---|---|---|
| project's call (`base_m`) | 980 (1145) | 1064 | 82 | 61 | -9e-5, 2.7%, 55%, 79 | -5e-5, 0.013 | 69 |
| Strategy 0 | 754 (875) | 813 | 120 | 99 | -1.6e-4, 2.9%, 131%, 97 | -8e-5, 0.014 | 1.5e3 |
| tails fixed | 258 (456) | 292 | 317 | 308 | -2.1e-4, 1.6%, 132%, 63 | -8e-6, 0.0037 | 4.4e3 |
| tails fixed + Strategy 0 | 218 (361) | 240 | 334 | 312 | -4.8e-4, 3.0%, 133%, 82 | -1.8e-4, 0.012 | 5.4e3 |
| free, Offset + refit | 1526 (1641) | 1697 | 446 | 464 | 0, 0.32%, 97%, 14 | 0, 0.0025 | 3e-4 |
| tails fixed, Offset + refit | 378 (509) | 450 | 476 | 497 | -2.0e-4, 0.73%, 97%, 49 | -2e-6, 0.0034 | 4.2e3 |

Per particle (fixed + Offset refit vs free + Offset refit): K0S S off by >1% in 36 vs 13 cells, Xi 3 vs 1 (median S shift -0.14%, P -0.0011), pi_tpc 10 vs 0. Fixing the tails cuts the NLL evaluations by a factor 3-4 and raises the convergence rate (status 0 from 15% to 59% for the project's call), but it moves S systematically (median -0.02% to -0.14%, several % in the K0S cells of pT 4-9) and the likelihood is worse by thousands of units: the tails are not universal across multiplicity in a region. Strategy 0 alone saves ~25% of calls and is slightly worse than Strategy 1 for S.

### 9.6 Offset("initial"), Offset("bin"), IntegrateBins [verified, `offib.cpp`, `runoffib.sh`, `out_offib/`, `a16.py` -> `res-a16.txt`; CPU ratios only, no timing conclusion]
737 analysis cells (both files), each variant from a fresh fitter; reference = best deterministic bin-centre fit (pool det).
- **`Offset(true)` and `Offset("initial")` are the same thing**: identical S, status and NLL path in 737/737 cells [verified]; [inferred] `Offset(true)` selects the initial-value offsetting.
- **`Offset("bin")` is better than `Offset(true)`** with the same refit: accepted (covQual 3, edm<0.01) 705/734 vs 647/734; NLL within 0.5 of the best 579 vs 559; S off by >1% in 17 vs 22 cells (K0S 9 vs 13; pi_tpc 0 vs 0); one fit alone: accepted 579 vs 341. CPU per cell about the same as `Offset(true)` (median ratio to base 1.2 for one fit, 1.6 with the refit; relative only). Not evaluated with the xRooFit escalation or the multi-start. **Candidate replacement for steps (a)-(b) of the recommendation** [verified on these 734 cells only].
- **IntegrateBins(1e-4)** with `obs->setBins(n)` = data bins in range (K0S 200, Xi 72, pi_tpc 100, pi_tof 100; obs edges equal the histogram edges to 2e-16 in all 737 cells): with the project's call it converges badly (accepted 188/734; K0S 0/200, pi_tpc 0/200); with Offset + refit 608/734. **Bin-centre effect** = S(integrated bins) - S(bin centre), same procedure, both accepted (556 cells): median +9.9e-5, |.| p90 0.40%, max 21.6% (a pi_tof cell); per particle: **Xi median +0.17%, p90 1.8%, max 2.1% (P +0.0011, p90 0.015)**, K0S median +5e-5 (p90 0.07%), pi_tpc +8e-5 (p90 0.34%), pi_tof +1e-4. In units of sigma_S the Xi shift is median 0.58 sigma (max 26). The Xi peak (sigma 1.5-2 MeV) on 1 MeV bins is where the bin-centre approximation matters; it is a model choice for the owner. Cost: CPU ~100-200x a base fit (median per-cell ratio 120 for the project's call, 184 with Offset+refit) [relative only].

### 9.7 The cause of "sum of coefficients is zero" [verified, `soc.cpp`, `out_soc/`]
- The project's fit split into its steps (fitter construction, `createNLL`, MIGRAD, HESSE) with RooFit's messages captured per step, over all pi_tpc cells (2 files x (100 analysis + 360 source) = 920 fits): **0 messages in any step**. Probes: evaluating the NLL with nsig=nbkg=0 emits it in every cell; nsig=0 alone, nbkg=0 alone or the configured start values never.
- On pi_tof (2026 file, empty cells kept): **all and only the 16 empty cells** (3 analysis: (0,0),(7,0),(8,0); 13 source, pT bins 0-1) emit it: 12 times during MIGRAD and 5 during HESSE per fit; MIGRAD drives both yields to 0 (status 302, nsig=nbkg=0) on an empty histogram, `RooAddPdf::updateCoefCache` then finds the coefficient sum 0. In `Logs/log_output_globalConfigPuritypp.log` (7 Aug, 10000 lines) the blocks are followed by `Error when calculating Hessian` and the purity NaN with `signalYield = 0, bkgYield = 0`, and the next plot is of `Gaussian_sig` (the pi_tof model) [verified by reading the log]; the earlier attribution to pi_tpc (note 03) came from the plot lines of the PREVIOUS fit in the log.
- **Harmless for the fits that matter, but a symptom**: it marks the empty pi_tof cells whose purity is written as NaN (section 2). The fix belongs in PurityTask (skip or flag cells with no in-range content), not in RooFit.

### 9.8 Not done
- The paired limit study (9.2) on the source cells.
- Note 12 owns the toy study (purity bias); note 11 owns the in-sequence xRooFit escalation, which does not reproduce the combined-row evaluation of 3.6.
- `Offset("bin")` combined with the escalation and the multi-start; IntegrateBins on the source cells.

---------------------------------------------------------------------------------------------------------------------------------

## Resume here
Done: sections 1-6 and 9.1-9.7 (Xi archive, paired limit study on the analysis cells, 50-start check, fixed tails and Strategy 0, Offset("initial"/"bin"), IntegrateBins, `sum of coefficients is zero`). Moved: purity-bias toys -> note 12; in-sequence xRooFit escalation -> note 11.
NOT DONE, in value order: (1) the paired limit study (`./runmsx.sh <tag> <file> source 20`) on the source cells; (2) `Offset("bin")` + refit combined with the escalation / multi-start, evaluated like 3.6 and 9.4; (3) IntegrateBins on the source cells; (4) owner decisions: Xi fit range, Xi sigma lower limit, K0S Chebychev2 positivity, pi_tof slope sign, bin-centre vs integrated bins for Xi.
To rerun: `cd scratch/study09; bash build.sh <prog>` for study, relax, msx, fixp, offib, soc, toys; runners `runall.sh`, `runrelax.sh`, `runmsx.sh`, `runms50.sh`, `run2.sh` (soc/fixp), `runoffib.sh`; analysis `REF=det python3 a3.py` ... `a16.py`; `FITCFG=<json>` overrides the config; environment `source scratch/rooenv.sh` (no `alienv`).
