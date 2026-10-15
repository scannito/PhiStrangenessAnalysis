# 12 - Bias and coverage of the signal yield S and the purity P = S/(S+B), measured with toys (15 October 2026)

This note completes note 09, section 9.3: the toy study there was only smoke-tested.
Environment: ROOT 6.36.10 (ALICE build), macOS arm64, `scratch/rooenv.sh`, no alienv. Compiled with `clang++ -std=c++20 -O2` and `root-config`, linking `RooFitXRooFit`, as in `study09/build.sh`.
Provenance tags: [source] code read; [verified] run here (the script and output file are named); [inferred] my conclusion.
Scratch dir: `S=/Users/scannito/alice/Analysis/RooFitKnowledge/scratch/study12/`. Nothing in the project or in `study09/` was modified. The data files were opened read-only.
Other jobs ran at the same time, so no timing conclusions are drawn. CPU numbers appear only as counts of fits.

---------------------------------------------------------------------------------------------------------------------------------

## SUMMARY

**What was done** [verified, `toys12.cpp`, `out/full-*.tsv`]. I chose 12 real analysis cells: K0S, Xi, pi_tpc and pi_tof at low and high statistics, plus 4 cells flagged in note 09:

- K0S pT 3.0-4.2 not converged;
- Xi multimodal;
- pi_tof with 134 counts;
- the K0S cell of the 09 smoke test.

The truth for each cell is the lowest-NLL fit of study09. I generated 200 binned Poisson toys per cell. The expected count per bin is the model integrated over the bin, not the bin-centre value. I fitted every toy three ways:

- (a) the project's `DoFit()`, verbatim;
- (b) the recommended procedure;
- (c) a 9-start search (one start at the truth, 8 random).

S, B and P and their errors come from the project's `ExtractYieldsAndPurity()` in the fixed window. The truth S and P are the window integrals of the true components.

**1. Yes, the project's call biases P, and in the bad cells the bias is far larger than the error.** Bias in units of σ_stat, the real spread of P over the toys:

| cell | effect of the project's call | wrong-minimum toys |
|---|---|---|
| pi_tpc pT 2.0-3.0, 2024 | P 0.99695 instead of 0.99995 in every toy: −3.0e-3, −630σ | 100% |
| K0S 2026 pT 3.0-4.2 (flagged) | P mean −0.026, median −0.015 | 99% |
| K0S 2026 pT 4.2-6.4 (09 smoke cell) | P −0.0145, −63σ | 96% |
| Xi multimodal | P −0.0046, −17σ | 99.5% |
| Xi high statistics | mean P −0.004, from a tail of bad toys | 46% |
| K0S high statistics | wrong minimum in nearly every toy, but dP only −2.8e-5 (−1.4σ); coverage 0.35 | 97% |

In the three clean low-statistics cells (K0S 3.8e5 counts, Xi 1.1e4, pi_tof 5.8e6) the project's call is unbiased within 0.15σ.

**2. The recommended procedure removes the bias where the project's call merely stopped early. It does not help when a deep wrong minimum is reachable from the config start.** It fixes:

- pi_tpc low: +0.8σ, the bin-centre residual;
- K0S high: −0.4σ;
- most of the K0S pT 3.0-4.2 cell: median −0.8σ, but 11.5% of toys still sit in a wrong minimum with dP −0.0155.

It fails in two cells:

| cell | wrong-minimum toys | P bias | coverage |
|---|---|---|---|
| K0S 09 smoke cell | 73% | −0.0144 per wrong toy, mean −0.011 | 0.17 |
| Xi multimodal | 91% | −0.0029, −11σ | 0.06 |

**Its acceptance test cannot see these wrong minima:** 534 of the 545 wrong-minimum toys of the recommended procedure pass `covQual==3 && edm<0.01` with status 0 (`res-acceptance.txt`). They are genuine local minima, up to 3e4 in NLL above the best. Random starts reach the best minimum in ≥96.5% of toys in every cell (K0S smoke cell: 8.9 of 9 starts), so a few extra random starts would find them [verified for the 9-start search, inferred for fewer starts].

The project's own wrong minima are almost all flagged by their own fit result: 34 of 1504 pass the acceptance test. The project, however, never reads the status.

**3. A second, smaller bias remains after the minimum is right: the bin-centre approximation of the fit.** Asimov data (expected counts, no fluctuation) fitted from the truth reproduce it exactly. `IntegrateBins(1e-6)` removes it in all 11 regular cells (`out/asimov-ib.tsv`):

| cell | S, relative | P, absolute | coverage of P (S) |
|---|---|---|---|
| Xi high | | +6.5e-4 (+2.7σ) | 0.03-0.06 |
| pi_tof high | −1.1e-4 (−5σ) | −3.3e-6 (−1.7σ) | 0.27 (0.00) |
| pi_tpc high | −1.3e-4 (−9.5σ) | +6.5e-9 (negligible in absolute terms) | |
| K0S cells | | ≤1e-4 (≤0.6σ) | |

Physically small (|dP| ≤ 7e-4), but large against the tiny statistical errors of the high-statistics cells.

**4. The propagated errors are trustworthy when the fit is at the right minimum.** Median error/σ_stat is 0.96-1.12 and the 1σ coverage is 0.57-0.78 (expected 0.683 ± 0.033 with 200 toys) in every cell without wrong minima or bin-centre bias. Coverage collapses only through bias: wrong minima (coverage 0.01-0.17) or the bin-centre approximation (0.00-0.45). It is not a wrong error size.

Two exceptions:

- the project's call in pi_tpc: errors 14-17 times σ_stat, and still coverage ≈0;
- the final fit of the multi-start in K0S high: covQual<3 in 72% of toys, error 1.36σ, coverage 0.81. Its errors should not be used.

**5. Problematic regimes:**

- K0S 2026 pT 3.0-6.4: a deep wrong minimum, with a DSCB tail parameter (n2_sig or alpha2_sig) at its limit, is reached from the config start;
- Xi pT 1.2-2.0: multimodal, with `sigma_sig` at its limit;
- pi_tpc with the project's call: stops at a point that shifts P by −0.003;
- all high-statistics cells: bin-centre bias of several σ;
- pi_tof pT 0.2-0.3 (≈100 counts): P = 0.38 ± 0.14, biased by +0.02 to +0.09 and with coverage 0.56-0.65, and the three procedures disagree by >0.01 in 29-35% of toys. A statistical regime, not usable as a purity.

**Recommendation** [inferred from the above]:

1. Adopt the recommended procedure (it fixes the commonest failure).
2. Add 2-4 random starts in the flagged families (K0S pT ≥ 3, Xi pT 1.2-2.0) and keep the lowest NLL; random starts reach the best in ≥96.5% of toys.
3. Consider `IntegrateBins` for the high-statistics cells, with `obs->setBins(n in-range bins)` as note 00 warns.

**Caveat:** toys are perfect-model data. In the K0S 09 smoke cell the real-data fit from the config start is within 0.6 in NLL of the best (study09), yet 73-96% of the toys fall into the wrong basin. The toys show that the hazard exists for this model and start. They do not prove that the real-data fit of every such cell is wrong.

---------------------------------------------------------------------------------------------------------------------------------

## 1. Method

### 1.1 Cells [verified, `mkcells.py` -> `cells12.tsv`; truth = lowest common NLL of the study09 variants, `*_rel` excluded]

| label | file | particle (mult, pT index) | pT (GeV/c) | expected N in range | S truth | P truth | truth at a limit | project vs best on real data (dNLL, study09) |
|---|---|---|---|---|---|---|---|---|
| k0s-low | 2024 | K0S (9, 9) | 4.2-6.4 | 3.83e5 | 3.140e5 | 0.92855 | - | 1.24 |
| k0s-high | 2026 | K0S (1, 2) | 0.5-0.8 | 8.50e7 | 8.026e7 | 0.97683 | - | 3.77 |
| k0s-flag-pt8 | 2026 | K0S (2, 8) | 3.0-4.2 | 2.28e7 | 1.611e7 | 0.86105 | - | 2.6e6 |
| k0s-smoke09 | 2026 | K0S (7, 9) | 4.2-6.4 | 4.52e6 | 3.603e6 | 0.91399 | - | 0.62 |
| xi-low | 2024 | Xi (9, 6) | 4.2-6.5 | 1.13e4 | 9087 | 0.91341 | - | 0.008 |
| xi-high | 2026 | Xi (1, 1) | 1.2-1.6 | 2.77e6 | 2.097e6 | 0.87858 | sigma_sig (lower) | 0.18 |
| xi-flag-multimodal | 2026 | Xi (5, 2) | 1.6-2.0 | 3.56e6 | 2.814e6 | 0.89917 | sigma_sig (lower) | 9236 |
| pitpc-low | 2024 | pi_tpc (0, 9) | 2.0-3.0 | 1.08e7 | 1.048e7 | 0.999952 | - | 1666 |
| pitpc-high | 2026 | pi_tpc (6, 1) | 0.3-0.4 | 5.74e9 | 5.615e9 | 0.9999998 | n2_sig (upper, 50) | 4.5e7 |
| pitof-low | 2024 | pi_tof (0, 1) | 0.3-0.4 | 5.85e6 | 5.457e6 | 0.98049 | - | 7e-4 |
| pitof-high | 2026 | pi_tof (2, 4) | 0.6-0.8 | 1.78e9 | 1.679e9 | 0.98585 | slope_bkg (upper, 0) | 0.014 |
| pitof-tiny | 2024 | pi_tof (9, 0) | 0.2-0.3 | 134 | 13.6 | 0.381 | mean_sig (lower) | 3e-4 |

- Files: `AnalysisResults_136_2024_PtMod_5agosto.root` and `AnalysisResults_136_2026_PtMod_6agosto.root`, i.e. the purity inputs of `globalConfigPuritypp.json`. Config: the current `JSONConfigs/fitConfig.json`.
- The cell histogram is built exactly as `PurityTask::FitPuritySpectrum` builds it: `ProjectionZ` over the merged source bins from `MapToSourceBins`, and `GetConfig(key, mult, k)` [source `PurityTask.h:300-315`].
- "Low" and "high" are the extremes of each particle's analysis cells. K0S cells with N > 1e8 were avoided because their yield is clipped by `nsig` max (note 09, 3.8). pi_tof has no cells between 134 and 5.8e6 counts, so both pitof-low and pitof-tiny are included.
- The 12th cell, `k0s-smoke09`, is the one where note 09 saw a wrong minimum in 1 of 3 toys.

### 1.2 Truth and toy generation [verified, `toys12.cpp`, `out/full-*.tsv.truth`]

- **Truth parameters.** The parameters of the best study09 fit are loaded into a fresh `DynamicRooFitter`. The common NLL on the real data equals the stored study09 value in all 12 cells (difference ≤ 2e-8; pi_tof high 7.6e-6 on an NLL of 3.3e10). An `Offset(true)` refit from the truth, repeated until the NLL is stable, lowered it in no cell (`refined=0`), so every truth is a local minimum.
- **Fit range.** It is the observable range after `RooDataHist` import. K0S, pi_tpc and pi_tof keep the config range. Xi is silently widened to the bin edges, [1.289, 1.361] (note 05); the truth uses that range, like the fit.
- **Expected counts.** For each in-range bin b: mu_b = nsig · ∫_b sigPdf + nbkg · ∫_b bkgPdf. Each pdf is normalised over the fit range, and each integral is an analytic `createIntegral` with a named range at the bin edges. There is no bin-centre approximation in the truth.
  - Σ_b ∫_b pdf = 1 to 1e-15 for both components in every cell.
  - Σ mu_b equals the real in-range count to < 1 count (the extended-ML property), e.g. 383270.75 against 383270.
- **Toys.** The full histogram is cloned and reset. Each in-range bin is set to Poisson(mu_b) (`TRandom3`, fixed seed per cell) with error √n. The bins outside the range stay at 0, which is irrelevant because the import drops them.
- **Total count.** The total expected count is the cell's (nsig+nbkg); the toy total fluctuates as a Poisson variable.
- **Truth S, B, P.** S_t = nsig · ∫_window sigPdf and B_t = nbkg · ∫_window bkgPdf over the fixed `integration_range` (K0S [0.475, 0.515], Xi [1.305, 1.335], pions [-3, 3]); P_t = S_t/(S_t+B_t). This is the same expression `ExtractYieldsAndPurity` evaluates [source `DynamicRooFitter.h:114-142`].
- **Asimov data.** Toy index −1 holds the expected counts mu_b themselves, non-integer, fitted the same way. RooFit warns "weighted data" for these fits only: the warning count is the same with 0 toys as with 200 (`out/check0-*.stdout`). It does not change the fitted values, and the Asimov errors are not used.

### 1.3 Procedures (each on a fresh `DynamicRooFitter`, as the project builds one per fit)

- **project**: `F.DoFit()` verbatim (`fitTo(Optimize(1), Extended(1), Save(1), PrintLevel(-1))` from the config start), then `ExtractYieldsAndPurity()`.
- **recommended**: `RecommendedProc` of `study09/rowlib.h`, unchanged. An `Offset(true)` fit and one `Offset(true)` refit; accept if `covQual==3 && edm<0.01`; otherwise run xRooFit `minimize()` (calls capped at 1e5) and keep the lower common NLL.
- **multistart** (reference): 9 starts, each an `Offset(true)` fit repeated until the common NLL changes by < 0.01 (at most 6 fits); the lowest NLL is kept.
  - Start 0 is the truth.
  - Starts 1-8 are random, as in study09 `ms`: nsig+nbkg = N split at a uniform fraction, shape parameters uniform in the inner 90% of their range.
- **Extraction.** In every case `ExtractYieldsAndPurity()` of the project, with the `RooFitResult` of the chosen fit: propagated errors through `getPropagatedError`.
- **Common NLL.** `createNLL(dh, Extended(true))`, no offset, evaluated at the final parameters. The NLL at the truth parameters on the same toy is recorded too (`dNllTruth`).

### 1.4 Definitions

- **bias**: mean (and median) of (fit − truth), over the 200 toys.
- **σ_stat**: the standard deviation over the toys of the multistart value, i.e. the real spread of the best estimator. Biases are quoted relative (S) or absolute (P), and in units of σ_stat.
- **pull**: (fit − truth)/(propagated error of that toy).
  - pull width: the plain standard deviation, plus a robust width (1.4826 × MAD), because a few toys with absurd errors dominate the plain one.
  - coverage: the fraction of toys with |fit − truth| < propagated error. The expectation is 0.683, and the binomial uncertainty with 200 toys is ±0.033.
- **wrong minimum**: common NLL more than 0.5 above the best of the three procedures on the same toy.
- **at a limit**: within 1e-6·N of a yield limit, or within 1e-5 of the range width for shape parameters. This replaces the 1e-3·range criterion of `study.cpp`, which flags small yields such as nsig = 9087 against a max of 1e8.
- **accepted**: `covQual==3 && edm<0.01`.

### 1.5 Checks [verified]

- **Reproducibility.** The whole run was made twice: v1 in `out/v1/`, and v2 with the `dNllTruth` column added. All 6633 common rows are identical in S, P, the errors, the NLL and the status (fixed seeds).
- **The reference is sound.** In every toy of every cell the best of the three procedures has an NLL at or below the NLL at the truth parameters: never above truth+0.5 (`res-truthnll.txt`: the 'best-of-3 > truth+0.5' fraction is 0.000 in all 12 main and 9 control runs).
- **The multistart does not rely on the truth start.** The number of starts within 0.5 of its best is ≥2, so at least one random start reached it, in 100% of toys for the K0S, pi_tof high and pi_tpc high cells, 99.5% for xi-low and pi_tpc low, 96.5% for Xi multimodal and 94.5% for pi_tof tiny (`res-nnear.txt`). The mean number of the 9 starts reaching the best is 8.2-9.0 for K0S and 3.9 for Xi multimodal.
- No exceptions were thrown in 7200 + 3 × 12 fits.

---------------------------------------------------------------------------------------------------------------------------------

## 2. Results (200 toys per cell; tables from `sum.py` -> `res-summary.txt`, flags from `ana.py` -> `res-full.txt`)

### 2.1 Purity P

proj = project's call, rec = recommended procedure, ms = multistart.

| cell | P truth | σ_stat | error/σ_stat (proj / rec / ms) | mean bias/σ_stat | median bias/σ_stat | Asimov bias/σ_stat | coverage | robust pull width | wrong min (proj / rec) |
|---|---|---|---|---|---|---|---|---|---|
| k0s-low | 0.92855 | 7.1e-4 | 0.96 / 1.00 / 1.00 | −0.12 / +0.14 / +0.16 | −0.01 / +0.23 / +0.23 | +0.15 | 0.66 / 0.75 / 0.78 | 1.05 / 0.87 / 0.87 | 0.10 / 0.00 |
| k0s-high | 0.97683 | 2.0e-5 | 0.96 / 0.96 / 1.36 | −1.39 / −0.37 / −0.37 | −1.45 / −0.45 / −0.44 | −0.40 | 0.35 / 0.66 / 0.81 | 1.13 / 0.89 / 0.67 | 0.97 / 0.00 |
| k0s-flag-pt8 | 0.86105 | 1.0e-4 | 1.12 / 1.12 / 1.25 | −261 / −18.0 / −0.61 | −144 / −0.76 / −0.51 | −0.62 | 0.06 / 0.59 / 0.77 | 8.76 / 1.09 / 0.67 | 0.99 / 0.12 |
| k0s-smoke09 | 0.91399 | 2.1e-4 | 0.75 / 0.75 / 0.98 | −63.0 / −49.6 / −0.16 | −67.9 / −67.5 / −0.17 | −0.22 | 0.04 / 0.17 / 0.70 | 1.52 / 3.43 / 0.94 | 0.96 / 0.73 |
| xi-low | 0.91341 | 2.9e-3 | 1.06 / 1.06 / 1.06 | +0.07 / +0.07 / +0.08 | +0.08 / +0.09 / +0.09 | −0.08 | 0.76 / 0.76 / 0.76 | 0.86 / 0.87 / 0.86 | 0.08 / 0.08 |
| xi-high | 0.87858 | 2.4e-4 | 1.10 / 1.11 / 1.10 | −17.1 / +2.73 / +2.64 | +1.22 / +2.76 / +2.59 | +2.69 | 0.09 / 0.06 / 0.03 | 2.84 / 1.34 / 0.86 | 0.46 / 0.28 |
| xi-flag-multimodal | 0.89917 | 2.7e-4 | 0.80 / 0.81 / 1.08 | −17.2 / −10.8 / +0.07 | −12.9 / −11.6 / +0.12 | +0.23 | 0.01 / 0.06 / 0.71 | 2.52 / 1.09 / 0.96 | 0.99 / 0.91 |
| pitpc-low | 0.999952 | 4.8e-6 | 16.9 / 1.08 / 1.10 | −633 / +0.76 / +0.76 | −636 / +0.76 / +0.77 | +0.84 | 0.00 / 0.57 / 0.61 | 51.0 / 0.92 / 0.85 | 1.00 / 0.00 |
| pitpc-high | 0.9999998 | 5.6e-10 | 14.2 / 1.01 / 1.28 | −6.8e5 / −3.5e4 / +11.7 | −1.2e4 / +11.5 / +11.7 | +11.8 | 0.07 / 0.00 / 0.01 | 48.4 / 1.19 / 5.82 | 1.00 / 0.18 |
| pitof-low | 0.98049 | 4.2e-5 | 0.99 / 0.99 / 0.99 | −0.07 / −0.04 / −0.04 | −0.10 / −0.07 / −0.07 | −0.03 | 0.69 / 0.70 / 0.69 | 0.96 / 0.94 / 0.94 | 0.00 / 0.00 |
| pitof-high | 0.98585 | 1.9e-6 | 1.09 / 1.09 / 1.08 | −1.89 / −1.74 / −1.73 | −1.92 / −1.75 / −1.74 | −1.71 | 0.21 / 0.28 / 0.27 | 1.00 / 0.98 / 1.00 | 0.79 / 0.12 |
| pitof-tiny | 0.381 | 0.14 | 0.87 / 1.00 / 0.89 | +0.63 / +0.23 / +0.17 | +0.38 / +0.30 / +0.24 | −0.00 | 0.56 / 0.64 / 0.65 | 1.16 / 1.04 / 1.05 | 0.17 / 0.31 |

Absolute bias of P, mean / median of P_fit − P_true:

| cell | project | recommended | multistart |
|---|---|---|---|
| k0s-low | −8.9e-5 / −6.4e-6 | +1.0e-4 / +1.6e-4 | +1.1e-4 / +1.6e-4 |
| k0s-high | −2.8e-5 / −2.9e-5 | −7.5e-6 / −9.0e-6 | −7.5e-6 / −8.9e-6 |
| k0s-flag-pt8 | **−2.6e-2 / −1.5e-2** | −1.8e-3 / −7.7e-5 | −6.1e-5 / −5.2e-5 |
| k0s-smoke09 | **−1.5e-2 / −1.6e-2** | **−1.1e-2 / −1.6e-2** | −3.7e-5 / −3.9e-5 |
| xi-low | +2.0e-4 / +2.3e-4 | +2.1e-4 / +2.7e-4 | +2.3e-4 / +2.7e-4 |
| xi-high | **−4.1e-3** / +3.0e-4 | +6.6e-4 / +6.7e-4 | +6.4e-4 / +6.3e-4 |
| xi-flag-multimodal | **−4.6e-3 / −3.4e-3** | **−2.9e-3 / −3.1e-3** | +1.9e-5 / +3.2e-5 |
| pitpc-low | **−3.0e-3 / −3.0e-3** | +3.6e-6 / +3.6e-6 | +3.6e-6 / +3.7e-6 |
| pitpc-high | −3.8e-4 / −6.8e-6 | −1.9e-5 / +6.4e-9 | +6.5e-9 / +6.5e-9 |
| pitof-low | −2.9e-6 / −4.3e-6 | −1.6e-6 / −2.7e-6 | −1.5e-6 / −2.7e-6 |
| pitof-high | −3.6e-6 / −3.7e-6 | −3.3e-6 / −3.3e-6 | −3.3e-6 / −3.3e-6 |
| pitof-tiny | +8.9e-2 / +5.3e-2 | +3.3e-2 / +4.2e-2 | +2.4e-2 / +3.5e-2 |

### 2.2 Signal yield S

| cell | S truth | σ_stat (relative) | mean bias/σ_stat (proj / rec / ms) | median bias/σ_stat | Asimov/σ_stat | coverage | mean dS/S (proj / rec / ms) |
|---|---|---|---|---|---|---|---|
| k0s-low | 3.140e5 | 2.0e-3 | +0.00 / +0.10 / +0.10 | −0.02 / +0.07 / +0.08 | +0.04 | 0.70 / 0.70 / 0.70 | +2.7e-6 / +1.9e-4 / +2.1e-4 |
| k0s-high | 8.026e7 | 1.3e-4 | −0.52 / −0.13 / −0.13 | −0.55 / −0.06 / −0.06 | −0.07 | 0.50 / 0.59 / 0.66 | −6.9e-5 / −1.8e-5 / −1.8e-5 |
| k0s-flag-pt8 | 1.611e7 | 2.7e-4 | −181 / −12.9 / −0.26 | −102 / −0.31 / −0.28 | −0.24 | 0.23 / 0.62 / 0.76 | −4.9e-2 / −3.5e-3 / −7.2e-5 |
| k0s-smoke09 | 3.603e6 | 5.5e-4 | −38.7 / −29.9 / −0.14 | −41.0 / −40.5 / −0.19 | −0.14 | 0.07 / 0.17 / 0.68 | −2.1e-2 / −1.7e-2 / −7.5e-5 |
| xi-low | 9087 | 1.1e-2 | +0.05 / +0.05 / +0.06 | −0.02 / −0.02 / −0.02 | −0.03 | 0.72 / 0.72 / 0.74 | +5.7e-4 / +5.9e-4 / +6.2e-4 |
| xi-high | 2.097e6 | 7.6e-4 | −11.8 / +0.90 / +1.21 | −0.11 / +1.22 / +1.14 | +1.34 | 0.35 / 0.33 / 0.45 | −9.0e-3 / +6.9e-4 / +9.3e-4 |
| xi-flag-multimodal | 2.814e6 | 7.1e-4 | −10.0 / −4.86 / +0.19 | −6.33 / −5.13 / +0.20 | +0.20 | 0.12 / 0.07 / 0.69 | −7.1e-3 / −3.5e-3 / +1.3e-4 |
| pitpc-low | 1.048e7 | 2.9e-4 | −10.0 / −0.19 / −0.19 | −10.1 / −0.17 / −0.17 | −0.19 | 0.02 / 0.68 / 0.68 | −2.9e-3 / −5.6e-5 / −5.6e-5 |
| pitpc-high | 5.615e9 | 1.3e-5 | +147 / +30.1 / −9.48 | −8.99 / −9.24 / −9.52 | −9.47 | 0.18 / 0.00 / 0.01 | +2.0e-3 / +4.0e-4 / −1.3e-4 |
| pitof-low | 5.457e6 | 4.3e-4 | −0.20 / −0.19 / −0.19 | −0.30 / −0.28 / −0.28 | −0.15 | 0.68 / 0.69 / 0.69 | −8.5e-5 / −8.2e-5 / −8.2e-5 |
| pitof-high | 1.679e9 | 2.3e-5 | −4.98 / −4.97 / −4.97 | −5.02 / −5.02 / −5.02 | −5.00 | 0.00 / 0.00 / 0.00 | −1.1e-4 / −1.1e-4 / −1.1e-4 |
| pitof-tiny | 13.6 | 0.48 | +0.52 / +0.27 / +0.23 | +0.20 / +0.23 / +0.21 | −0.00 | 0.61 / 0.66 / 0.68 | +0.25 / +0.13 / +0.11 |

The error/σ_stat of S is 0.88-1.12 for every procedure and cell (`res-summary.txt`). For pi_tpc the project's S error is fine; its P error is not (2.1).

### 2.3 Fit flags (fractions of the 200 toys) [verified, `res-full.txt`]

| cell | status≠0 (proj / rec / ms) | covQual<3 | at a limit | wrong min | accepted | rec escalated to xRooFit |
|---|---|---|---|---|---|---|
| k0s-low | 0.86 / 0 / 0 | 0.005 / 0.075 / 0.14 | 0 / 0 / 0 | 0.105 / 0 / 0 | 0.045 / 0.93 / 0.86 | 0.17 |
| k0s-high | 1.00 / 0.37 / 0.77 | 0 / 0.11 / 0.72 | 0 | 0.975 / 0 / 0 | 0 / 0.89 / 0.28 | 0.415 |
| k0s-flag-pt8 | 0.95 / 0.09 / 0.60 | 0.55 / 0.005 / 0.33 | 0.005 / 0.005 / 0 | 0.99 / 0.115 / 0 | 0 / 0.995 / 0.67 | 0.13 |
| k0s-smoke09 | 0.995 / 0.015 / 0.21 | 0.055 / 0 / 0.085 | 0.005 / 0.135 / 0 | 0.965 / 0.73 / 0 | 0.005 / 1.00 / 0.915 | 0.005 |
| xi-low | 0.05 / 0 / 0 | 0 | 0.155 / 0.15 / 0.16 | 0.08 / 0.08 / 0.005 | 0.98 / 1.00 / 1.00 | 0 |
| xi-high | 1.00 / 0.01 / 0 | 0.30 / 0 / 0 | 0.02 / 0.095 / 0 | 0.455 / 0.275 / 0 | 0 / 0.985 / 1.00 | 0.045 |
| xi-flag-multimodal | 1.00 / 0 / 0 | 0.21 / 0 / 0 | 0.03 / 0.01 / 0 | 0.995 / 0.91 / 0 | 0 / 1.00 / 1.00 | 0 |
| pitpc-low | 1.00 / 0.175 / 0.395 | 0.495 / 0.01 / 0.225 | 0.255 / 0 / 0 | 1.00 / 0 / 0 | 0 / 0.99 / 0.775 | 0.07 |
| pitpc-high | 1.00 / 0.075 / 0.63 | 0.715 / 0.03 / 0.59 | 0.11 / 0.99 / 1.00 | 1.00 / 0.185 / 0 | 0 / 0.94 / 0.40 | 0.50 |
| pitof-low | 0 / 0 / 0 | 0 | 0 | 0 | 1.00 / 1.00 / 1.00 | 0 |
| pitof-high | 0.95 / 0.25 / 0.065 | 0 / 0.005 / 0.005 | 0.94 / 0.26 / 0.075 | 0.785 / 0.12 / 0 | 0.055 / 0.99 / 0.975 | 0.105 |
| pitof-tiny | 0.115 / 0.005 / 0.01 | 0.08 / 0 / 0.005 | 0.515 / 0.785 / 0.785 | 0.17 / 0.31 / 0.14 | 0.87 / 0.985 / 0.99 | 0.015 |

- **Status.** Status 3 is almost the only non-zero status of the project's call. The share of status 3 grows with the statistics, as in note 09: 86% at 3.8e5 K0S counts and 100% at 8.5e7.
- **"At a limit" largely reflects the truth.**
  - pi_tpc high: the true `n2_sig` is at 50, so 99-100% of the rec and ms fits are at the limit.
  - pi_tof high: the true slope is at 0. The project's fits sit at the limit in 94% of toys, the better fits in 7.5-26%.
  - pi_tof tiny: `mean_sig` is at −1.
  - Xi: the true `sigma_sig` is at 0.002, but the fits are flagged only 0-16% of the time with the 1e-5 criterion. They stay just above the limit.
- **"Wrong min" of the multistart** is non-zero only where (a) or (b) found a lower NLL: xi-low 1 toy, pi_tof tiny 28 toys. These are flat likelihoods.

### 2.4 How the wrong minima bias P [verified, `res-full.txt` section "wrong-minimum toys", `res-acceptance.txt`]

| cell | procedure | wrong toys | median dNLL | mean dP of wrong toys | mean dP of right toys | accepted / status 0 among the wrong |
|---|---|---|---|---|---|---|
| k0s-low | project | 21 | 1.1 | −1.1e-3 (−1.5σ) | +3.0e-5 | 0 / 0 |
| k0s-high | project | 195 | 1.9 | −2.8e-5 | −1.5e-5 | 0 / 0 |
| k0s-flag-pt8 | project | 198 | 4.8e4 | −2.7e-2 | −1.9e-4 | 0 / 8 |
| k0s-flag-pt8 | recommended | 23 | 1.2e5 | −1.5e-2 | −6.0e-5 | 23 / 20 |
| k0s-smoke09 | project | 193 | 3.1e4 | −1.4e-2 | −1.8e-4 | 1 / 1 |
| k0s-smoke09 | recommended | 146 | 3.1e4 | −1.4e-2 | +2.9e-5 | **146 / 146** |
| xi-low | both | 16 | 2.0 | +2.4e-3 (+0.8σ) | +1e-5 to +2e-5 | 16 / 16 |
| xi-high | project | 91 | 4.5e3 | −9.8e-3 | +5.5e-4 | 0 / 0 |
| xi-high | recommended | 55 | 263 | +6.9e-4 | +6.5e-4 | 52 / 53 |
| xi-flag-multimodal | project | 199 | 550 | −4.6e-3 | −7.0e-5 (1 toy) | 0 / 0 |
| xi-flag-multimodal | recommended | 182 | 525 | −3.2e-3 | +1.0e-4 | **182 / 182** |
| pitpc-low | project | 200 | 564 | −3.0e-3 | - | 0 / 0 |
| pitpc-high | project | 200 | 1.3e6 | −3.8e-4 | - | 0 / 0 |
| pitpc-high | recommended | 37 | 1.7e7 | −1.1e-4 | +6.6e-9 | 32 / 33 |
| pitof-high | project | 157 | 1.9 | −3.8e-6 | −2.9e-6 | 8 / 7 |
| pitof-high | recommended | 24 | 1.4 | −3.4e-6 | −3.3e-6 | 24 / 22 |
| pitof-tiny | project / recommended | 34 / 62 | 87 / 2.6 | +0.35 / −0.006 | +0.036 / +0.050 | 9 / 59 |

**Totals over the 12 cells (2400 toys):**

- project: 1504 wrong-minimum toys, of which 34 are "accepted" and 42 have status 0 with covQual 3;
- recommended: 545 wrong, of which 534 are accepted and 533 have status 0 with covQual 3.

**Interpretation** [inferred]:

- **Project.** Its failures are mostly "stopped before the minimum" (status 3, large EDM). Its own `RooFitResult` flags them, and the recommended refit fixes them.
- **Recommended.** Its failures are converged fits in another basin: K0S pT 3.0-6.4 and Xi pT 1.2-2.0. No convergence diagnostic can see them; only a comparison of NLLs between starts can.
  - The direction is systematic: P too low by 0.003 (Xi multimodal) to 0.015 (K0S), i.e. 11σ to 70σ.
  - In the Asimov fits the K0S wrong basin has a DSCB tail parameter at its limit: `n2_sig_hi` (recommended) or `alpha2_sig_hi` (bin-centre control), `out/full-k0s-smoke09.tsv`, `out/centre-k0s-smoke09.tsv`.
- **The 09 smoke test is now explained.** Its "wrong minimum" in 1 of 3 toys (P 0.8995 vs 0.914) is this K0S basin. It occurs in 73% of toys for the recommended procedure and in 96% for the project's call, with dP −0.0143.
- **Real data versus toys.** On the real data of the same cell the project's call is within 0.62 in NLL of the best (study09). The real distribution apparently guides the config start to the right basin and the perfect-model toys do not, so the toy rate is not a real-data failure rate for that cell.
  - For k0s-flag-pt8 the real-data fit is also wrong: the study09 dNLL is 2.6e6, and S is off by 9.4%.

### 2.5 The bin-centre bias [verified, `out/asimov-ib.tsv`, `out/asimov-ib-k0s-smoke09.tsv`, `res-centre.txt`]

The binned NLL that RooFit builds for a `RooDataHist` evaluates the pdf at the bin centre [source note 04; `DoFit` has no `IntegrateBins`]. With the integrated truth this gives an estimator bias that does not shrink with N, while σ_stat does. Three controls:

**1. Asimov fits started at the truth**, Offset refits until stable, with and without `IntegrateBins(1e-6)` (`obs->setBins(n in-range bins)`). Columns: S/St−1 and P−Pt.

| cell | bin centre | IntegrateBins |
|---|---|---|
| k0s-low | +8.4e-5 / +1.1e-4 | −2e-9 / −3e-11 |
| k0s-high | −9.4e-6 / −8.0e-6 | +8.8e-7 / +8.5e-7 |
| k0s-flag-pt8 | −6.4e-5 / −6.2e-5 | −7.9e-8 / −4.2e-8 |
| k0s-smoke09 | −7.8e-5 / −4.7e-5 | +1.1e-6 / +1.3e-6 |
| xi-low | −2.8e-4 / −2.2e-4 | −2.9e-7 / −1.1e-7 |
| xi-high | +1.0e-3 / +6.6e-4 | +3.8e-6 / +2.7e-6 |
| xi-flag-multimodal | +1.4e-4 / +6.1e-5 | +6.6e-7 / +4.6e-7 |
| pitpc-low | −5.6e-5 / +4.0e-6 | +8.8e-7 / +1.6e-8 |
| pitpc-high | −1.3e-4 / +6.6e-9 | +3.9e-8 / +1.4e-11 (status 304, covQual 0: HESSE failed, values fine) |
| pitof-low | −6.2e-5 / −1.2e-6 | −2e-14 / 1e-15 |
| pitof-high | −1.1e-4 / −3.3e-6 | −3.7e-6 / −1.8e-6 |
| pitof-tiny | +0.128 / +0.059 | +0.128 / +0.059 (flat likelihood: the truth is not the Asimov minimum) |

**2. The bin-centre Asimov bias equals the toy bias of the multistart** in every regular cell (the "Asimov/σ_stat" columns of 2.1 and 2.2). The residual bias of a correctly minimised fit is therefore this effect, not a statistical or boundary effect.

**3. Control toys generated with bin-centre expectations** (`TRUTHMODE=centre`, 100 toys, `res-centre.txt`):

- the pi_tof high S bias disappears: −5.0σ → +0.06σ;
- the K0S high P bias goes from −0.37σ to +0.25σ;
- the Xi high bias stays (P +4σ).

For Xi, a narrow peak (σ 2 MeV, at its limit, in 1 MeV bins), the midpoint sum of the pdf differs from its integral by an amount the fit can exploit: the best NLL on the bin-centre Asimov is 101 below the truth NLL. This is the same bin-centre approximation in a different form, and `IntegrateBins` removes it (Asimov P bias 2.7e-6). [inferred from the controls]

### 2.6 Coverage of the propagated error [verified, `res-summary.txt`]

- **Unbiased fits.** Where the fit is at the right minimum and the bin-centre bias is ≪ σ_stat (k0s-low, xi-low, pitof-low; rec in k0s-high and pitpc-low; ms in k0s-flag-pt8, k0s-smoke09 and xi-flag-multimodal), coverage is:
  - S: 0.66-0.76 (k0s-high rec 0.59);
  - P: 0.57-0.78;
  - error/σ_stat: 0.96-1.12;
  - robust pull width: 0.85-1.05.

  `getPropagatedError` is therefore a correct 1σ error when covQual is 3, which agrees with note 04's bootstrap check (within about 10%). The coverages of 0.75-0.78 in k0s-low (rec, ms) and 0.57-0.61 in pi_tpc low come with error/σ_stat 1.00-1.10. They are the 2-3σ binomial edge, or slightly non-Gaussian distributions, not a wrong error.
- **The project's call in pi_tpc.** The P error is 17× (low) and 14× (high) σ_stat, with plain pull widths of 50: HESSE at an unconverged point (covQual 2 in 50-72% of toys). The error is huge, and the coverage is still ≈0 because the bias is larger.
- **The multistart's own final fit.** It often has covQual<3 (K0S high 72%, pi_tpc high 59%). Its P error is 1.25-1.36 σ_stat there, giving coverage 0.81. Errors should come from a fit that ends with covQual 3, which is one more argument for the recommended acceptance test plus a refit after the multi-start.
- **pitof-tiny.** Coverage 0.56-0.68 with a bias of 0.2-0.6σ. The likelihood is flat, and `mean_sig` sits at −1 in 52-79% of toys.

---------------------------------------------------------------------------------------------------------------------------------

## 3. Answers

1. **Is P biased with the project's call, and by how much relative to its error?**
   - **Clean cells: no.** K0S low, Xi low and pi_tof low: |bias| ≤ 0.15σ.
   - **Every cell where note 09 saw the project's call stop early: yes, by many σ.**

     | cell | bias of P | wrong-minimum toys |
     |---|---|---|
     | pi_tpc pT 2.0-3.0 | −0.0030 (−630σ) | all |
     | K0S 2026 pT 3.0-6.4 | −0.015 to −0.026 (−60σ to −260σ) | 96-99% |
     | Xi multimodal | −0.0046 (−17σ) | 99.5% |
     | Xi high | mean −0.004 from 46% bad toys | 46% |
     | K0S high (σ small) | −1.4σ, coverage 0.35 | |

   - The project's error does not cover any of these (coverage 0.00-0.35, except k0s-low 0.66).
2. **Does the recommended procedure remove it?**
   - **Yes, where the project's call merely stopped early:** pi_tpc low and high, K0S high, most toys of K0S pT 3.0-4.2.
   - **No, where a deep wrong basin is reachable from the config start:**

     | cell | wrong-minimum toys | P bias |
     |---|---|---|
     | K0S 2026 pT 4.2-6.4 (09 smoke cell) | 73% | −0.011 mean, −68σ median |
     | K0S pT 3.0-4.2 | 11.5% | mean −0.0018 |
     | Xi multimodal | 91% | −0.0029, −11σ |

     All these toys pass its acceptance test. Random starts find the right minimum, so a few extra starts in these families are needed.
   - **What remains after a correct minimisation** is the bin-centre bias: Xi high P +6.5e-4 (+2.7σ), pi_tof high P −3.3e-6 (−1.7σ) and S −5σ, pi_tpc high S −9.5σ. `IntegrateBins` removes it.
3. **Are the propagated errors trustworthy?** Yes in size, when the fit ends at the right minimum with covQual 3: error/σ_stat 0.96-1.12, coverage 0.57-0.78 for 200 toys. No coverage survives a wrong minimum or a bias of several σ. The project's pi_tpc errors (14-17× too large at unconverged points) are meaningless.
4. **Problematic cells and regimes:**
   - K0S 2026 pT 3.0-6.4: a deep wrong basin, a DSCB tail at its limit;
   - Xi pT 1.2-2.0: multimodal, `sigma_sig` at its limit, and the bin-centre bias of a narrow peak;
   - pi_tpc with the project's call;
   - every high-statistics cell, for the bin-centre bias;
   - pi_tof pT 0.2-0.3 (≈100 counts): a statistical regime, P biased by 0.2-0.6σ, the three procedures disagree by >0.01 in 29-35% of toys.

   Clean: K0S at 4e5 counts in 2024, Xi at 1e4, pi_tof at 6e6, and pi_tof high P apart from the bin-centre effect (−3e-6).

---------------------------------------------------------------------------------------------------------------------------------

## 4. Limits of this study

- **Cell count.** There are 12 cells, chosen by hand: one cell per regime. The rates (for example 73% wrong minima) are specific to the cell and its truth. They are not rates for the family.
- **Perfect-model toys.** Real data deviate from the model. In the K0S 09 smoke cell the toys fail far more often than the real-data fit did.
- **Truths at a limit.** The truths of Xi high and Xi multimodal (`sigma_sig`), pi_tpc high (`n2_sig`), pi_tof high (`slope_bkg`) and pi_tof tiny (`mean_sig`) are at a parameter limit. This makes the estimator non-regular, but the bin-centre controls show that the bias seen comes from the bin centre, not the limit.
- **Multistart cost.** It is about 10× the fits of the recommended procedure: 9 starts × 1-6 fits. A cheaper version (recommended + 2-4 random starts) was NOT tested directly. Its success rate is inferred from the per-start reach rate (3.9/9 starts for Xi multimodal, ≥8/9 for K0S).
- **`IntegrateBins`** was tested on Asimov data only, not on the toys, and not for its effect on convergence or CPU. pi_tpc high gave HESSE status 304 with it.
- **Statistics.** 200 toys per cell (100 in the controls). The coverage uncertainty is ±0.033 (±0.047 in the controls).
- **Not studied:** the source-binning cells (CCDB map), the 2024 K0S cells with nsig clipping (none exist), the effect on the correlation results.

---------------------------------------------------------------------------------------------------------------------------------

## 5. Files and how to rerun (all in `S=/Users/scannito/alice/Analysis/RooFitKnowledge/scratch/study12/`)

- `toys12.cpp`: the program. It includes the project's `DynamicRooFitter.h` (with `private` made public, as in study09), and copies of `common.h`, `parts.h` and `rowlib.h` from study09, unchanged.
  - Build: `bash build.sh toys12`.
  - Run: `./toys12 cells12.tsv <label|all> <nToys> <out.tsv> [nRandomStarts=8]`.
  - Environment modes: `TRUTHMODE=centre` (bin-centre truth); `ASIMOVIB=1` (only the Asimov fits with and without `IntegrateBins`).
  - Outputs: `<out>.tsv`, one row per toy and procedure, toy −1 = Asimov; columns status, covQual, edm, nll, dNllBest, S, Serr, B, Berr, P, Perr, nsig, nAtLim, limPars, escalated, nNear, cpuMs, St, Bt, Pt, dNllTruth. Also `<out>.tsv.truth`: fit range, bins, NLL reproduction, Σ integrals, truth values.
  - Binaries: `toys12` (main runs), `toys12b` (centre runs) and `toys12ib` (Asimov `IntegrateBins`) were built from successive versions of the same source. Each mode is gated by its environment variable, and the current source reproduces all of them.
- `mkcells.py` → `cells12.tsv`: truth = the lowest common NLL in `study09/out/y20*-analysis.tsv`.
- Run scripts: `run.sh <nToys> <nStarts> <labels...>` (main, two processes) and `runc.sh <nToys> <labels...>` (centre control). The log is `out/run.log`.
- Outputs:
  - `out/full-<label>.tsv`: main, 200 toys, 8 random starts;
  - `out/v1/`: the first run, identical;
  - `out/centre-<label>.tsv`: control, 100 toys;
  - `out/asimov-ib*.tsv`;
  - `out/check0-*`: Asimov-only runs for the warning count;
  - `out/smoke.tsv`: 2-toy smoke test.
- Analysis (plain python):
  - `ana.py [glob]` → `res-full.txt` (main) and `res-centre.txt` (control); `res-*-v1.txt` are the same from v1;
  - `sum.py` → `res-summary.txt`, the tables of section 2;
  - in-line scripts → `res-acceptance.txt` (section 2.4), `res-nnear.txt` and `res-truthnll.txt` (section 1.5).

---------------------------------------------------------------------------------------------------------------------------------

## Resume here

Done: everything requested (12 cells × 200 toys × 3 procedures, bias, pulls, coverage, flags, the wrong minima and their effect on P), plus the bin-centre controls.

Next steps, NOT DONE, in value order:

1. Test "recommended + k random starts" (k = 2, 4) as the production procedure on the same toys. Add a mode to `toys12.cpp` that logs every start's NLL, so that the k-start success can be read directly.
2. Run the K0S 2026 pT 3.0-6.4 and Xi pT 1.2-2.0 families with 50 toys per cell, to get family rates instead of single-cell rates.
3. Fit the toys, not only Asimov, with `IntegrateBins` (`obs->setBins` = in-range bins), and check convergence and HESSE (status 304 seen on pi_tpc high).
4. Find out why the config start leads to the K0S tail-at-limit basin in toys but not on the real data of the smoke cell (compare the two likelihood surfaces along n2/alpha2).
