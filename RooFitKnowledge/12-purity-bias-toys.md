# 12 - Bias and coverage of the signal yield S and the purity P = S/(S+B), measured with toys (15 October 2026)

This note completes note 09, section 9.3: the toy study there was only smoke-tested.
Environment: ROOT 6.36.10 (ALICE build), macOS arm64, `scratch/rooenv.sh`, no alienv. Compiled with `clang++ -std=c++20 -O2` and `root-config`, linking `RooFitXRooFit`, as in `study09/build.sh`.
Provenance tags: [source] code read; [verified] run here (the script and output file are named); [inferred] my conclusion.
Scratch dir: `S=/Users/scannito/alice/Analysis/RooFitKnowledge/scratch/study12/`. Nothing in the project or in `study09/` was modified. The data files were opened read-only.
Other jobs ran at the same time, so no timing conclusions are drawn. CPU numbers appear only as counts of fits.

---------------------------------------------------------------------------------------------------------------------------------

## FINAL CONCLUSIONS (last pass, 15 October 2026; sections 6 and 7; scratch `S2=.../scratch/study-final-12-11/`, `S3=/Users/scannito/alice/Analysis/RooFitKnowledge/scratch/study-last-12/`)

For the project owner. Toys are perfect-model binned Poisson toys from the best real-data fit of each cell; 200 toys per cell. "rec" = the recommended procedure of note 09 (Offset fit + refit + acceptance test + xRooFit). "Wrong" = a converged fit more than 0.5 in NLL above the best minimum found on that toy: the acceptance test does not see these (section 2.4).

1. **The wrong minima are a family problem, not a few bad cells** (section 7.1, all 40 cells of the two flagged families, 8000 toys) [verified, `res-fam-*.txt`]:

   | family (2026) | rec: wrong toys | worst cell | P shift of a wrong toy | rec + 2 random starts | rec + 8 / 10 |
   |---|---|---|---|---|---|
   | K0S pT 3.0-4.2 | 5.2% (9 of 10 cells affected) | 14% | -0.013 to -0.016 (cell median bias up to -85σ) | 0.05% | 0 (k=4) |
   | K0S pT 4.2-6.4 | 38.8% (6 cells above 20%) | 84% | same | 0.05% | 0 (k=4) |
   | Xi pT 1.2-1.6 | 25.2% | 44.5% | mostly +1e-4 to +1.4e-3 | 2.7% | 0 / 0 |
   | Xi pT 1.6-2.0 | 78.1% (every cell ≥ 58%) | 100% | -4σ to -11σ mean bias | 34.1% | 3.3% / 1.4% |

   The 09 smoke cell (73%) and the Xi multimodal cell (91%) are typical of their families. In Xi pT 1.6-2.0 a random start reaches the best minimum in only 33-36% of toys in every cell, so the miss rate falls as 0.65^k.
2. **A start with the yields scaled to the histogram removes most of it at no cost, except in Xi** (sections 6.2, 7.4). Setting nsig, nbkg = N_in-range × the config fraction (2/3), config shapes, then rec:
   - K0S family: 2 wrong toys in 4000 (rec: 880); K0S smoke 73% -> 0, pi_tpc high 19% -> 0, pi_tof high 12% -> 0, Xi pT 1.2-1.6 25% -> 0.6%. It is also cheaper than the config start (0.54-0.92 of the CPU of rec).
   - Harmful for Xi pT 1.6-2.0 (99.8% wrong) and Xi at 1e4 counts (8% -> 36.5%).
   - With 2 random starts after it, no wrong minimum is left in any K0S, pi_tpc or pi_tof cell tested (26 distinct cells, 5200 toys), apart from the statistical pi_tof tiny cell (≈100 counts).
3. **Xi pT 1.6-2.0 needs a start from the neighbouring cells or about 10 random starts** (section 7.2). `sigma_sig` started at its limit helps little (78% -> 57%), yield scaling makes it worse (100%). A start taken from the median best fit of the other multiplicity classes of the same pT bin gives 0 wrong in 19 of 20 Xi cells and 26% in the 20th (mult 0, pT 1.6-2.0); with 8 random starts after it, 2 toys in 4000. Without it, 10 random starts leave 1.4% (worst cell 3%) at about 11 times the CPU of rec [verified, `res-famnb-xi.txt`, `res-fam-xi-pt2.txt`].
4. **Why the toys fail more than the real data** (section 6.2): the K0S wrong basin is a local minimum of the perfect-model likelihood but not of the real-data likelihood (the real data differ from the model by χ² 2884 / 200 bins). The toy rates are therefore upper limits for the real-data rate where the model is imperfect. They do not make the hazard zero: the real-data fit of K0S pT 3.0-4.2 (study09) is wrong.
5. **`IntegrateBins`** (sections 6.3, 7.3):
   - It removes the bin-centre bias on toys (Xi high P +2.6σ -> +0.06σ, pi_tpc high P +11.6σ -> +0.04σ, pi_tof high S -5σ -> -0.13σ). After the right minimum this bias is what remains family-wide: Xi pT 1.2-1.6 +1.5σ to +2.9σ, K0S up to -0.8σ (section 7.1).
   - Its precision is not the knob: 1e-4, 1e-5 and 1e-6 give the same results (within 0.12σ), the same failure rates and nearly the same CPU.
   - The convergence failures (status ≠ 0 in 77-89% at N ≥ 1e7) come from the **refit**: a single IB fit started at the bin-centre minimum has status 0 in 91-92% of toys, the right error (0.93-1.14 σ) and the same value within 0.16σ (K0S), at 14-30 times the CPU of rec.
   - The bin-centre fit's error covers the IB value correctly (P coverage 0.645-0.73 in 10 of 12 cells; the exceptions are explained in 7.3).
6. **Recommended procedure, final form** [inferred from 1-5; every step was tested on toys, the whole chain was not run end to end]:
   1. start from the config shapes with yields = N × the config fraction (K0S, pi_tpc, pi_tof), or from the fit of a neighbouring cell (Xi);
   2. recommended procedure (Offset fit + refit, acceptance test, xRooFit escalation with `gDirectory` null);
   3. 2 random starts in every cell, 8 in Xi pT 1.2-2.0; keep the lowest NLL;
   4. in high-statistics cells where the bin-centre bias matters (Xi, pi_tof, pi_tpc), one `IntegrateBins(1e-4)` fit from that minimum, with `obs->setBins(n in range)`; quote its error if status 0 and covQual 3, else the bin-centre error.

   Cost per cell: 2-3 times the recommended procedure (about 9 in Xi pT 1.2-2.0), plus 14-30 times for the IB fit where used (noisy CPU, other jobs ran).

---------------------------------------------------------------------------------------------------------------------------------

## SUMMARY (first pass, sections 1-5)

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
2. Add random starts and keep the lowest NLL. Tested in sections 6 and 7; the final form (yield-scaled or neighbour start + 2 random starts, 8 in Xi pT 1.2-2.0) is in the FINAL CONCLUSIONS at the top.
3. Use `IntegrateBins` only as a final polish in the high-statistics cells, with `obs->setBins(n in-range bins)`; tested on toys in section 6.3.

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
- **Real data versus toys.** On the real data of the same cell the project's call is within 0.62 in NLL of the best (study09). The real distribution apparently guides the config start to the right basin and the perfect-model toys do not, so the toy rate is not a real-data failure rate for that cell. (Explained in section 6.2: the wrong basin does not exist in the real-data likelihood.)
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

- **Cell count.** There are 12 cells, chosen by hand: one cell per regime. The rates (for example 73% wrong minima) are specific to the cell and its truth. The family rates for K0S 2026 pT 3.0-6.4 and Xi 2026 pT 1.2-2.0 (40 cells) are in section 7.1.
- **Perfect-model toys.** Real data deviate from the model. In the K0S 09 smoke cell the toys fail far more often than the real-data fit did.
- **Truths at a limit.** The truths of Xi high and Xi multimodal (`sigma_sig`), pi_tpc high (`n2_sig`), pi_tof high (`slope_bkg`) and pi_tof tiny (`mean_sig`) are at a parameter limit. This makes the estimator non-regular, but the bin-centre controls show that the bias seen comes from the bin centre, not the limit.
- **Multistart cost.** The 9-start reference is about 10× the fits of the recommended procedure. The cheaper "recommended + k random starts" was tested in section 6.1.
- **`IntegrateBins`** was tested on Asimov data in sections 1-5 and on the toys in section 6.3.
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

## 6. Final pass: random starts, the K0S basin, `IntegrateBins` on toys (scratch `S2=/Users/scannito/alice/Analysis/RooFitKnowledge/scratch/study-final-12-11/`)

Program `final12.cpp` (build `bash build.sh final12`), run with `run.sh 200 4 <labels>` (env `IBCFG=1`), analysed with `anams.py` -> `res-ms.txt`. It regenerates **the same toys** as `toys12.cpp` (same seed, same Poisson loop): its recommended-procedure NLL equals study12's in every toy (max difference 5e-4, 0.05 for pi_tof high and 0.5 for pi_tpc high on NLLs of 1e8-1e11) [verified, `res-ms.txt` header lines]. The project and 9-start rows are read from `study12/out/full-*.tsv`. `TDirectory::TContext{nullptr}` is set for the whole program (note 11's xRooFit cache).

### 6.1 Recommended procedure + k random starts [verified, `res-ms.txt`]

Per toy: the recommended procedure, then 4 random starts drawn as in the 9-start reference (yield fraction uniform, shapes uniform in the inner 90% of their range), each an `Offset(true)` fit + one `Offset(true)` refit. "rec+k" keeps the lowest NLL among the recommended result and the first k starts; a start replaces the choice only if its NLL is lower by more than 0.01. "wrong" = NLL more than 0.5 above the best of all procedures on that toy. Bias in units of σ_stat (as in section 1.4). CPU relative to the recommended procedure, noisy.

| cell | procedure | wrong | P bias/σ mean / median | P coverage | S coverage | covQual<3 | fits per cell | CPU rel. |
|---|---|---|---|---|---|---|---|---|
| k0s-smoke09 | project | 0.965 | -63.2 / -68.1 | 0.04 | 0.07 | 0.055 | 1 | 0.7 |
| | rec | 0.730 | -49.7 / -67.7 | 0.17 | 0.17 | 0 | 2.0 | 1 |
| | rec+1 | 0.035 | -2.30 / -0.21 | 0.65 | 0.65 | 0.05 | 4.0 | 2.1 |
| | rec+2 | **0** | -0.17 / -0.18 | 0.67 | 0.68 | 0.045 | 6.0 | 3.3 |
| | rec+4 | 0 | -0.17 / -0.18 | 0.67 | 0.68 | 0.045 | 10.0 | 5.7 |
| | 9 starts | 0 | -0.16 / -0.17 | 0.70 | 0.68 | 0.085 | | 10.1 |
| k0s-flag-pt8 | rec | 0.115 | -18.1 / -0.76 | 0.59 | 0.62 | 0.005 | 2.1 | 1 |
| | rec+1 | **0** | -0.61 / -0.52 | 0.68 | 0.72 | 0.01 | 4.1 | 1.7 |
| xi-flag-multimodal | rec | 0.910 | -10.8 / -11.6 | 0.06 | 0.07 | 0 | 2.0 | 1 |
| | rec+1 | 0.605 | -7.03 / -11.0 | 0.26 | 0.28 | 0 | 4.0 | 2.1 |
| | rec+2 | 0.380 | -4.40 / -1.07 | 0.41 | 0.42 | 0 | 6.0 | 3.1 |
| | rec+4 | 0.135 | -1.50 / -0.13 | 0.61 | 0.59 | 0 | 10.0 | 5.2 |
| | 9 starts | 0 | +0.07 / +0.12 | 0.71 | 0.69 | 0 | | 8.7 |
| xi-high | rec | 0.275 | +2.74 / +2.76 | 0.06 | 0.33 | 0 | 2.0 | 1 |
| | rec+2 | 0.020 | +2.70 / +2.64 | 0.03 | 0.44 | 0 | 6.0 | 2.9 |
| | rec+4 | 0 | +2.63 / +2.59 | 0.03 | 0.45 | 0 | 10.0 | 4.9 |
| pitpc-high | rec | 0.190 | -3.5e4 / +11.5 | 0.00 | 0.00 | 0.03 | 2.5 | 1 |
| | rec+2 | 0.010 | -2.1e3 / +11.6 | 0.00 | 0.01 | 0.09 | 6.5 | 2.2 |
| | rec+4 | **0** | +11.7 / +11.7 | 0.01 | 0.01 | 0.095 | 10.5 | 3.3 |
| pitof-high | rec | 0.120 | -1.74 / -1.76 | 0.28 | 0.00 | 0.005 | 2.1 | 1 |
| | rec+2 | 0.005 | -1.73 / -1.76 | 0.28 | 0.00 | 0.01 | 6.1 | 2.3 |
| xi-low | rec / rec+2 | 0.080 / 0.015 | +0.07 / +0.08 | 0.76 | 0.72 / 0.73 | 0 | 2.0 / 6.0 | 1 / 3.5 |
| pitof-tiny | rec / rec+4 / 9 starts | 0.315 / 0.130 / 0.145 | +0.23 / +0.18 / +0.17 | 0.64 / 0.66 / 0.65 | 0.66 / 0.70 / 0.68 | 0 | 2.0 / 10.0 | 1 / 4.3 |

- **k0s-low, k0s-high, pitpc-low, pitof-low:** the recommended procedure is already at the best minimum in all 200 toys; the starts change nothing (P bias identical to 0.01σ). They cost 2.6-3.4× (k=2) and 4.2-5.8× (k=4) the CPU.
- **Which start finds the best** (`res-ms.txt`, "first start reaching the best"): K0S smoke cell 139 toys at start 1, 7 at start 2; K0S pT 3.0-4.2 all at start 1; Xi multimodal 61 / 45 / 28 / 21 at starts 1-4 and none in 27 toys. Mean number of the 4 starts reaching the best: 3.8-3.9 (K0S), 3.0 (pi_tpc high), 2.7 (Xi high), 1.4 (Xi multimodal).
- **Xi multimodal** needs about 8 starts. Measured directly with a separate run of 8 starts (`NOIB=1 ./final12 cells12.tsv xi-flag-multimodal 200 k8/out/ms-xi-flag-multimodal.tsv 8`, `anams-k8.py` -> `res-ms-k8.txt`; its starts are drawn afresh, so its k=4 differs from the table by chance): wrong minima 37% (k=2), 16.5% (k=4), 5% (k=6), **2% (k=8)**; P bias -4.3σ, -1.9σ, -0.52σ, **-0.16σ** (median +0.06σ); P coverage 0.43, 0.59, 0.67, 0.69; 18 fits per cell and 9.4× the CPU of the recommended procedure at k=8. In 4 of 200 toys none of the 8 starts reaches the best. This matches a per-start reach of about 0.35 (miss rate ≈ 0.65^k).
- **Errors after a random start wins:** covQual<3 stays ≤ 0.11 (the 9-start reference's final fit had up to 0.72), and the P error/σ_stat stays 0.90-1.12, except pi_tpc high where the chosen random-start fits give P errors 4× too small (P error ~1e-10; physically irrelevant).
- **Cost** [verified relative, noisy]: fits per cell 2.0-2.5 (rec) -> 6.0-6.5 (k=2) -> 10.0-10.5 (k=4); CPU 2.2-3.5× (k=2) and 3.3-6.0× (k=4) the recommended procedure, which itself costs about 2× the project's call (note 09). Per cell this is a few ms [inferred from the per-toy CPU of 5-12 ms for rec].
- **Is it worth it?** Yes [inferred]. With k=2 the wrong minima of the K0S pT 3.0-6.4 family (P -0.014, 50-70σ) and of pi_tpc high disappear on these toys, at about 3× the CPU of a step that takes seconds per file. Xi pT 1.2-2.0 needs about 8 starts. Clean cells are unaffected, so applying k=2 to every cell is the simplest rule.

### 6.2 Why the K0S toys fall into the wrong basin and the real data do not [verified, `MODE=basin`, `out/basin-k0s-smoke09.tsv`, `res-basin.txt`]

Cell k0s-smoke09 (K0S 2026, mult 7, pT 4.2-6.4, N = 4.5e6). On each histogram the reference is the iterated `Offset` fit from the truth; dNLL is relative to it. "wrong start" = the parameters of the project's fit on the Asimov (the wrong basin).

| histogram | project from config | rec from config | rec, config shapes, yields = N × truth fractions | Offset fits from the wrong-basin start |
|---|---|---|---|---|
| real data (α=0) | 0.62 | 3e-6 | 3e-4 | **9e-5 (rolls to the best)** |
| 0.9 real + 0.1 Asimov | 5.6e5 (stopped) | 4e-4 | -2e-4 | -3e-5 |
| 0.75 / 0.25 | 183 | -1e-4 | -3e-5 | -3e-4 |
| 0.5 / 0.5 | 0.47 | 4e-4 | -1e-5 | 3e-5 |
| 0.25 / 0.75 | 0.58 | -5e-5 | -1e-5 | **3.0e4 (stays)** |
| 0.1 / 0.9 | 7696 | 4e-5 | -5e-6 | 3.07e4, `alpha2_sig` at 5 |
| Asimov (α=1) | 3.1e4 | **3.1e4, `n2_sig` at 150** | 2.5e-5 | 3.1e4, `alpha2_sig` at 5 |

- **The wrong basin is a feature of the perfect-model likelihood.** It is a local minimum for α ≥ 0.75 and does not exist at α ≤ 0.5: started inside it, the fit on the real data or on a half-real mixture returns to the best minimum.
- **The real data deviate from the model.** χ² of the real data against the expected counts of the best fit: 2884 for 200 bins; in every 10th bin (the only ones printed) pulls of -7.5 and +9.2 at 0.500 and 0.505 GeV/c², -3.8 and -4.6 at 0.510 and 0.515, +4.9 at 0.525. The mismatch at the peak and in the right tail removes the secondary minimum in which the right tail is switched off (`n2_sig` or `alpha2_sig` at its upper limit).
- **Direct test with toys:** adding the real-data residual (real - Asimov, clipped at 0) to each of 50 toys, the recommended procedure from the config start falls into a wrong minimum in 4% of toys instead of 60%. (The project's call is still wrong in 78% of these toys, mostly by stopping early.)
- **The start contributes:** the config yields (nsig 1e5 + nbkg 5e4) are 30 times below N. With the same config shapes and only the yields set to N times the true fractions, the Asimov fit reaches the right minimum (rec dNLL 2.5e-5; project 0.54, its usual early stop). The path from a start with yields far too small runs through the region where the tail parameters go to their limits [inferred from the end points; the trajectory was not recorded].
- **Not binning, range or generation:** toys, Asimov and real data have the same 200 bins in the same range, the bins outside the range are dropped at import, and the bin-centre toys of section 2.5 show the same basin (`alpha2_sig` at its limit).
- **Consequence** [inferred]: the toy rates of section 2 overstate the real-data rate wherever the data deviate from the model, as in K0S. They remain the right test of the procedure, and the real-data fit can still be wrong (K0S pT 3.0-4.2, study09 dNLL 2.6e6). A start with yields scaled to the histogram's in-range count removes this path: tested on 40 + 12 cells in 7.4 (it works for K0S, pi_tpc, pi_tof and Xi pT 1.2-1.6, not for Xi pT 1.6-2.0).

### 6.3 `IntegrateBins` on toys: bias, coverage, convergence [verified, `res-ms.txt`]

"ib": from the best of rec+4, `F.obs->setBins(n in-range bins)`, then `fitTo(..., Offset(true), IntegrateBins(1e-6))` twice. "ibcfg": the same from the config start. The NLL used for "wrong" is the bin-centre one, so for ibcfg a toy is called wrong when |P(ibcfg) - P(ib)| > 3σ_stat.

| cell | P bias/σ rec+4 -> ib | P coverage rec+4 -> ib | S bias/σ rec+4 -> ib | S coverage | ib status≠0 | ib covQual<3 | ib P error/σ | ibcfg wrong | CPU ib / rec |
|---|---|---|---|---|---|---|---|---|---|
| k0s-low | +0.15 -> +0.08 | 0.76 -> 0.74 | +0.10 -> +0.08 | 0.70 -> 0.70 | 0 | 0.38 | 1.05 | 0 | 64 |
| k0s-high | -0.37 -> +0.06 | 0.66 -> 0.79 | -0.14 -> -0.06 | 0.59 -> 0.68 | 0.88 | 0.80 | 1.37 | 0.05 | 47 |
| k0s-flag-pt8 | -0.61 -> -0.01 | 0.68 -> 0.80 | -0.27 -> -0.04 | 0.72 -> 0.76 | 0.78 | 0.47 | 1.36 | 0.095 | 42 |
| k0s-smoke09 | -0.17 -> +0.08 | 0.67 -> 0.70 | -0.14 -> +0.01 | 0.68 -> 0.68 | 0.34 | 0.19 | 1.05 | 0.285 | 71 |
| xi-low | +0.08 -> +0.19 | 0.76 -> 0.77 | +0.06 -> +0.09 | 0.73 -> 0.78 | 0.01 | 0 | 1.14 | 0 | 59 |
| xi-high | +2.63 -> **+0.07** | 0.03 -> **0.71** | +1.21 -> -0.07 | 0.45 -> 0.69 | 0.055 | 0.06 | 1.14 | 0.025 | 50 |
| xi-flag-multimodal | -1.50 -> -0.06 | 0.61 -> 0.73 | -0.53 -> +0.05 | 0.59 -> 0.70 | 0.185 | 0.095 | 1.18 | 0.015 | 60 |
| pitpc-low | +0.76 -> -0.09 | 0.57 -> 0.72 | -0.19 -> -0.00 | 0.68 -> 0.71 | 0.42 | 0.165 | 1.21 | 0 | 43 |
| pitpc-high | +11.7 -> **+0.05** | 0.01 -> **0.76** | -9.5 -> -0.01 | 0.01 -> 0.73 | 0.765 | 0.74 | 1.49 | 0.355 | 32 |
| pitof-low | -0.04 -> -0.01 | 0.70 -> 0.70 | -0.19 -> -0.05 | 0.69 -> 0.71 | 0 | 0 | 1.00 | 0 | 75 |
| pitof-high | -1.73 -> -0.94 | 0.28 -> 0.53 | -4.98 -> **-0.13** | 0.00 -> **0.72** | 0.075 | 0.02 | 1.09 | 0 | 34 |
| pitof-tiny | +0.18 -> +0.22 | 0.66 -> 0.67 | +0.24 -> +0.26 | 0.70 -> 0.67 | 0.03 | 0.01 | 0.96 | 0 | 65 |

(xi-flag-multimodal "rec+4" still contains 13.5% wrong minima; the ib row starts from them too, and its mean P(ib) - P(rec+4) is +1.45σ, median -0.08σ.)

- **Bias:** `IntegrateBins` removes the bin-centre bias on fluctuating toys exactly as on Asimov. Remaining: pi_tof high P -0.94σ (coverage 0.53). Its true `slope_bkg` is at the limit 0 and its S is unbiased, so this is the boundary of the background slope, not the bin centre [inferred].
- **Convergence:** the IB polish ends with status ≠ 0 in 77-88% of toys for k0s-high, k0s-flag-pt8 and pitpc-high (34% k0s-smoke09, 42% pitpc-low). Status histories in `res-ms.txt`: K0S mostly `MINIMIZE=1,HESSE=1`; pi_tpc high `MINIMIZE=-1,HESSE=3` in 112 toys and **`MINIMIZE=-1,HESSE=304` in 7** (1 in pi_tpc low). The first IB fit of the pair converges more often (status 0 in 84.5-100% of toys); the refit fails. My reading at the time: the numerical bin integrals (relative precision 1e-6) make the NLL noisy at the level the EDM test needs once N ≥ 1e7. **Refuted in 7.3:** 1e-4 and 1e-5 fail just as often; the failures come from the refit, and a single IB fit converges in 91-92% of toys.
- **Errors:** where IB fails to converge, its P error is 1.36-1.49 σ_stat (coverage 0.76-0.80). In the low-statistics cells it is 1.00-1.21.
- **Basins:** IB from the config start does not avoid the wrong minima (28.5% K0S smoke cell, 35.5% pi_tpc high, 9.5% K0S pT 3.0-4.2). It has to come after the multi-start, not instead of it.
- **Cost:** the IB polish (2 fits) costs 32-75 times the recommended procedure (CPU, noisy); from the config start 73-162 times.

### 6.4 Files (all in `S2=/Users/scannito/alice/Analysis/RooFitKnowledge/scratch/study-final-12-11/`)

- `final12.cpp` (+ `common.h`, `parts.h`, `rowlib.h`, `cells12.tsv`, `build.sh` copied from study12, unchanged).
- `run.sh <nToys> <K> <labels...>` -> `out/ms-<label>.tsv` (one row per toy and stage: rec, start1-4, ib, ibcfg; columns status, covQual, edm, bin-centre NLL, S, P, errors, nFits, cpuMs, at-limit parameters, status history). Log `out/run.log`.
- `anams.py [labels]` -> `res-ms.txt` (sections 6.1 and 6.3).
- `MODE=basin ./final12 cells12.tsv k0s-smoke09 50 out/basin-k0s-smoke09.tsv` -> `out/basin-k0s-smoke09.tsv`; the in-line tabulation -> `res-basin.txt` (section 6.2).
- `out/smoke.tsv`: 3-toy smoke test. `k8/out/ms-xi-flag-multimodal.tsv`, `anams-k8.py` -> `res-ms-k8.txt`: Xi multimodal with 8 starts, no IB.

---------------------------------------------------------------------------------------------------------------------------------

## 7. Last pass: family rates, smarter starts, `IntegrateBins` precision (scratch `S3=/Users/scannito/alice/Analysis/RooFitKnowledge/scratch/study-last-12/`)

Program `last12.cpp` (built with `bash build.sh last12`; `OUT=last12b bash build.sh last12` for the binary with the neighbour start of 7.2). It reuses `Prepare`, `RandomStart` and the toy loop of `final12.cpp` unchanged, so for a cell of `cells12.tsv` it regenerates **the same toys and the same random starts**: on the K0S smoke cell, toys 0-2, its `rec` and `start1-4` NLL and P equal those of `study-final-12-11/out/ms-k0s-smoke09.tsv` to all printed digits [verified, `out/smoke-fam.tsv`]. `TDirectory::TContext{nullptr}` for the whole program (note 11).

### 7.1 Family rates: all K0S 2026 pT 3.0-6.4 and all Xi 2026 pT 1.2-2.0 cells [verified, `famcells.py` -> `fam-k0s.tsv`, `fam-xi.tsv`; `out/fam-k0s.tsv`, `out/fam-xi.tsv`; `famana.py` -> `res-fam-*.txt`]

- **Cells.** All 10 multiplicity classes × 2 pT bins for each family: K0S (mult 0-9, pT index 8 = 3.0-4.2 and 9 = 4.2-6.4 GeV/c), Xi (mult 0-9, pT index 1 = 1.2-1.6 and 2 = 1.6-2.0 GeV/c), 40 cells. Truth = the lowest-NLL study09 fit, chosen exactly as `study12/mkcells.py` does (`famcells.log`). Histogram entries 3e6-3.4e7 (K0S), 1.0e6-4.3e6 (Xi). Every Xi truth has `sigma_sig` at its lower limit 0.002; no K0S truth is at a limit.
- **Toys: 200 per cell** (the run was cheap enough, no reduction needed), 8000 toys in all. The cells K0S (2,8), K0S (7,9), Xi (1,1), Xi (5,2) are the 12-cell study's k0s-flag-pt8, k0s-smoke09, xi-high and xi-flag-multimodal, with the same toys: their `rec` rates reproduce section 2 (0.115, 0.730, 0.275, 0.910).
- **Stages per toy** (each on a fresh `DynamicRooFitter`): `rec` (the recommended procedure, `rowlib.h`); `ys` (the recommended procedure from the config shapes with yields N_toy × the config fraction 2/3); for Xi `siglo` (config start, `sigma_sig` = lower limit + 1e-3 × range) and `yssiglo` (both); `truth` (iterated `Offset` fits from the truth, a reference only); K random starts (K=4 for K0S, K=10 for Xi), each an `Offset(true)` fit + one refit.
- **Definitions** as in 6.1: "X+k" = stage X followed by the first k random starts, keeping the lowest NLL (a start replaces only if lower by > 0.01); wrong = NLL > best of all stages + 0.5; σ_stat = spread over the toys of P of the best fit; coverage with the fit's own error. The reference is solid: the truth-start fit is at the best in 100% of the 8000 toys, and no stage ever beat it by more than 0.5 (`truth-start fit not at best: 0.000` in every cell).

**Pooled rates** (the "cells" column counts the cells with a rate > 0 / > 5% / > 20%; the bias is the mean over cells of the cell's mean P bias in σ_stat; CPU relative to `rec`, noisy):

| family, pT | procedure | wrong toys (rate) | cells >0 / >5% / >20% | worst cell | mean P bias/σ | fits per toy | CPU rel. |
|---|---|---|---|---|---|---|---|
| K0S 3.0-4.2 | rec | 104 / 2000 (5.2%) | 9 / 3 / 0 | 14.0% (mult 1) | -7.7 | 2.1 | 1 |
| | rec+1 | 3 (0.15%) | 2 / 0 / 0 | 1.0% | -0.63 | 4.1 | 1.7 |
| | **rec+2** | 1 (0.05%) | 1 / 0 / 0 | 0.5% | -0.55 | 6.1 | 2.3 |
| | rec+4 | 0 | 0 | 0 | -0.52 | 10.1 | 3.7 |
| | **ys** | **0** | 0 | 0 | -0.52 | 2.2 | **0.54** |
| K0S 4.2-6.4 | rec | 776 / 2000 (38.8%) | 8 / 6 / 6 | 84.0% (mult 2) | -29.1 | 2.0 | 1 |
| | rec+1 | 31 (1.55%) | 6 / 0 / 0 | 4.5% | -1.26 | 4.0 | 2.0 |
| | **rec+2** | 1 (0.05%) | 1 / 0 / 0 | 0.5% | -0.32 | 6.0 | 2.9 |
| | rec+4 | 0 | 0 | 0 | -0.28 | 10.0 | 4.8 |
| | **ys** | 2 (0.1%) | 1 / 0 / 0 | 1.0% (mult 0, dP +3.9e-4) | -0.28 | 2.1 | **0.68** |
| | ys+2 | 0 | 0 | 0 | -0.28 | 6.1 | 2.6 |
| Xi 1.2-1.6 | rec | 503 / 2000 (25.2%) | 10 / 10 / 5 | 44.5% (mult 8) | +2.13 | 2.0 | 1 |
| | rec+2 | 54 (2.7%) | 10 / 1 / 0 | 5.5% | +2.48 | 6.0 | 3.0 |
| | rec+4 | 7 (0.35%) | 5 / 0 / 0 | 1.5% | +2.44 | 10.0 | 4.9 |
| | rec+8 | 0 | 0 | 0 | +2.43 | 18.0 | 8.9 |
| | ys | 12 (0.6%) | 4 / 0 / 0 | 4.5% (mult 0) | +2.41 | 2.1 | 0.85 |
| | ys+2 | 3 (0.15%) | 1 / 0 / 0 | 1.5% | +2.44 | 6.1 | 2.8 |
| Xi 1.6-2.0 | rec | 1562 / 2000 (78.1%) | 10 / 10 / 10 | 100% (mult 0) | -6.27 | 2.0 | 1 |
| | rec+2 | 681 (34.1%) | 10 / 10 / 10 | 45.0% | -2.62 | 6.0 | 3.1 |
| | rec+4 | 292 (14.6%) | 10 / 10 / 0 | 19.5% | -0.94 | 10.0 | 5.2 |
| | **rec+8** | 66 (3.3%) | 10 / 1 / 0 | 5.5% (mult 7) | +0.03 | 18.0 | 9.3 |
| | rec+10 | 28 (1.4%) | 10 / 0 / 0 | 3.0% | +0.20 | 22.0 | 11.4 |
| | ys | 1996 (99.8%) | 10 / 10 / 10 | 100% | -8.46 | 2.1 | 0.94 |

Per cell (wrong-minimum rate, and in brackets the cell's mean P bias in σ_stat), K0S [verified, `res-fam-k0s.txt`, table by `mktab.py` -> `tab-fam-k0s.md`]:

| cell | entries | rec | rec+1 | rec+2 | ys | rec&ys |
|---|---|---|---|---|---|---|
| k0s-m0-pt8 | 1e+07 | 0.005 (-0.62σ) | 0.000 (-0.24σ) | 0.000 (-0.24σ) | 0.000 (-0.25σ) | 0.000 (-0.24σ) |
| k0s-m1-pt8 | 3.1e+07 | 0.140 (-21.42σ) | 0.010 (-1.46σ) | 0.000 (-0.59σ) | 0.000 (-0.59σ) | 0.000 (-0.59σ) |
| k0s-m2-pt8 | 3e+07 | 0.115 (-18.07σ) | 0.000 (-0.61σ) | 0.000 (-0.61σ) | 0.000 (-0.61σ) | 0.000 (-0.61σ) |
| k0s-m3-pt8 | 2.5e+07 | 0.040 (-5.91σ) | 0.000 (-0.53σ) | 0.000 (-0.53σ) | 0.000 (-0.54σ) | 0.000 (-0.53σ) |
| k0s-m4-pt8 | 2.1e+07 | 0.125 (-15.64σ) | 0.005 (-0.80σ) | 0.005 (-0.81σ) | 0.000 (-0.54σ) | 0.000 (-0.54σ) |
| k0s-m5-pt8 | 3.4e+07 | 0.005 (-1.07σ) | 0.000 (-0.32σ) | 0.000 (-0.32σ) | 0.000 (-0.32σ) | 0.000 (-0.32σ) |
| k0s-m6-pt8 | 2.6e+07 | 0.045 (-7.04σ) | 0.000 (-0.68σ) | 0.000 (-0.68σ) | 0.000 (-0.69σ) | 0.000 (-0.68σ) |
| k0s-m7-pt8 | 1.9e+07 | 0.010 (-2.02σ) | 0.000 (-0.80σ) | 0.000 (-0.80σ) | 0.000 (-0.80σ) | 0.000 (-0.80σ) |
| k0s-m8-pt8 | 2.5e+07 | 0.035 (-5.22σ) | 0.000 (-0.48σ) | 0.000 (-0.48σ) | 0.000 (-0.48σ) | 0.000 (-0.48σ) |
| k0s-m9-pt8 | 1.6e+07 | 0.000 (-0.39σ) | 0.000 (-0.39σ) | 0.000 (-0.39σ) | 0.000 (-0.39σ) | 0.000 (-0.39σ) |
| k0s-m0-pt9 | 3e+06 | 0.485 (-25.52σ) | 0.020 (-1.30σ) | 0.000 (-0.26σ) | 0.010 (-0.24σ) | 0.005 (-0.25σ) |
| k0s-m1-pt9 | 9e+06 | 0.005 (-0.39σ) | 0.000 (+0.04σ) | 0.000 (+0.04σ) | 0.000 (+0.04σ) | 0.000 (+0.04σ) |
| k0s-m2-pt9 | 8.8e+06 | 0.840 (-67.57σ) | 0.045 (-3.31σ) | 0.005 (-1.14σ) | 0.000 (-0.74σ) | 0.000 (-0.74σ) |
| k0s-m3-pt9 | 7.2e+06 | 0.445 (-32.71σ) | 0.005 (-0.36σ) | 0.000 (-0.05σ) | 0.000 (-0.05σ) | 0.000 (-0.05σ) |
| k0s-m4-pt9 | 6.1e+06 | 0.000 (-0.15σ) | 0.000 (-0.15σ) | 0.000 (-0.15σ) | 0.000 (-0.16σ) | 0.000 (-0.15σ) |
| k0s-m5-pt9 | 9.7e+06 | 0.805 (-65.08σ) | 0.030 (-2.30σ) | 0.000 (-0.32σ) | 0.000 (-0.32σ) | 0.000 (-0.32σ) |
| k0s-m6-pt9 | 7.3e+06 | 0.555 (-48.08σ) | 0.020 (-2.33σ) | 0.000 (-0.51σ) | 0.000 (-0.50σ) | 0.000 (-0.50σ) |
| k0s-m7-pt9 | 5.5e+06 | 0.730 (-49.67σ) | 0.035 (-2.30σ) | 0.000 (-0.17σ) | 0.000 (-0.16σ) | 0.000 (-0.16σ) |
| k0s-m8-pt9 | 7.1e+06 | 0.015 (-1.69σ) | 0.000 (-0.52σ) | 0.000 (-0.52σ) | 0.000 (-0.52σ) | 0.000 (-0.52σ) |
| k0s-m9-pt9 | 4.3e+06 | 0.000 (-0.10σ) | 0.000 (-0.10σ) | 0.000 (-0.10σ) | 0.000 (-0.10σ) | 0.000 (-0.10σ) |

Xi [verified, `res-fam-xi.txt`, `tab-fam-xi.md`]:

| cell | entries | rec | rec+2 | rec+4 | rec+8 | ys | siglo | yssiglo | truth |
|---|---|---|---|---|---|---|---|---|---|
| xi-m0-pt1 | 1e+06 | 0.090 (+0.83σ) | 0.030 (+1.36σ) | 0.015 (+1.43σ) | 0.000 (+1.45σ) | 0.045 (+1.12σ) | 0.095 (+0.79σ) | 0.065 (+1.06σ) | 0.000 (+1.46σ) |
| xi-m1-pt1 | 3.4e+06 | 0.275 (+2.74σ) | 0.010 (+2.66σ) | 0.000 (+2.63σ) | 0.000 (+2.63σ) | 0.000 (+2.64σ) | 0.180 (+3.61σ) | 0.005 (+2.66σ) | 0.000 (+2.65σ) |
| xi-m2-pt1 | 3.4e+06 | 0.405 (+0.40σ) | 0.030 (+2.67σ) | 0.000 (+2.61σ) | 0.000 (+2.61σ) | 0.000 (+2.62σ) | 0.085 (+3.00σ) | 0.005 (+2.64σ) | 0.000 (+2.63σ) |
| xi-m3-pt1 | 2.9e+06 | 0.135 (+2.23σ) | 0.025 (+2.24σ) | 0.000 (+2.16σ) | 0.000 (+2.16σ) | 0.000 (+2.17σ) | 0.065 (+2.40σ) | 0.010 (+2.21σ) | 0.000 (+2.18σ) |
| xi-m4-pt1 | 2.6e+06 | 0.145 (+2.51σ) | 0.030 (+2.23σ) | 0.000 (+2.18σ) | 0.000 (+2.18σ) | 0.000 (+2.19σ) | 0.010 (+2.21σ) | 0.015 (+2.24σ) | 0.000 (+2.20σ) |
| xi-m5-pt1 | 4.3e+06 | 0.410 (+2.40σ) | 0.040 (+2.88σ) | 0.005 (+2.79σ) | 0.000 (+2.77σ) | 0.005 (+2.80σ) | 0.495 (+1.83σ) | 0.000 (+2.78σ) | 0.000 (+2.78σ) |
| xi-m6-pt1 | 3.4e+06 | 0.340 (+1.87σ) | 0.055 (+2.58σ) | 0.005 (+2.58σ) | 0.000 (+2.56σ) | 0.005 (+2.58σ) | 0.065 (+2.88σ) | 0.000 (+2.57σ) | 0.000 (+2.57σ) |
| xi-m7-pt1 | 2.7e+06 | 0.155 (+2.38σ) | 0.020 (+2.21σ) | 0.000 (+2.16σ) | 0.000 (+2.16σ) | 0.000 (+2.16σ) | 0.015 (+2.20σ) | 0.005 (+2.18σ) | 0.000 (+2.17σ) |
| xi-m8-pt1 | 3.8e+06 | 0.445 (+2.77σ) | 0.025 (+3.03σ) | 0.005 (+2.94σ) | 0.000 (+2.92σ) | 0.000 (+2.93σ) | 0.320 (+2.77σ) | 0.000 (+2.93σ) | 0.000 (+2.93σ) |
| xi-m9-pt1 | 2.9e+06 | 0.115 (+3.20σ) | 0.005 (+2.91σ) | 0.005 (+2.91σ) | 0.000 (+2.89σ) | 0.005 (+2.93σ) | 0.005 (+2.91σ) | 0.000 (+2.91σ) | 0.000 (+2.91σ) |
| xi-m0-pt2 | 1.1e+06 | 1.000 (-4.45σ) | 0.415 (-1.68σ) | 0.160 (-0.44σ) | 0.020 (+0.23σ) | 0.990 (-4.40σ) | 0.350 (-1.32σ) | 1.000 (-4.44σ) | 0.000 (+0.31σ) |
| xi-m1-pt2 | 3.5e+06 | 0.625 (-4.96σ) | 0.235 (-2.15σ) | 0.110 (-0.82σ) | 0.040 (-0.05σ) | 1.000 (-10.39σ) | 0.320 (-3.07σ) | 1.000 (-10.32σ) | 0.000 (+0.38σ) |
| xi-m2-pt2 | 3.5e+06 | 0.645 (-4.60σ) | 0.265 (-1.64σ) | 0.125 (-0.35σ) | 0.025 (+0.50σ) | 0.990 (-8.26σ) | 0.480 (-3.65σ) | 1.000 (-8.51σ) | 0.000 (+0.71σ) |
| xi-m3-pt2 | 2.9e+06 | 0.580 (-4.01σ) | 0.285 (-1.97σ) | 0.080 (-0.33σ) | 0.020 (+0.15σ) | 1.000 (-7.72σ) | 0.525 (-3.94σ) | 1.000 (-7.72σ) | 0.000 (+0.31σ) |
| xi-m4-pt2 | 2.5e+06 | 0.820 (-7.31σ) | 0.390 (-3.52σ) | 0.150 (-1.32σ) | 0.030 (-0.20σ) | 1.000 (-9.13σ) | 0.700 (-6.31σ) | 1.000 (-9.12σ) | 0.000 (+0.06σ) |
| xi-m5-pt2 | 4.1e+06 | 0.910 (-10.83σ) | 0.390 (-4.50σ) | 0.180 (-2.02σ) | 0.035 (-0.33σ) | 1.000 (-11.67σ) | 0.495 (-5.44σ) | 1.000 (-11.66σ) | 0.000 (+0.07σ) |
| xi-m6-pt2 | 3.2e+06 | 0.870 (-8.51σ) | 0.310 (-2.98σ) | 0.125 (-1.15σ) | 0.015 (-0.09σ) | 1.000 (-9.84σ) | 0.545 (-5.40σ) | 1.000 (-9.83σ) | 0.000 (+0.06σ) |
| xi-m7-pt2 | 2.5e+06 | 0.840 (-6.94σ) | 0.450 (-3.49σ) | 0.195 (-1.33σ) | 0.055 (-0.17σ) | 1.000 (-8.08σ) | 0.770 (-6.15σ) | 1.000 (-8.07σ) | 0.000 (+0.30σ) |
| xi-m8-pt2 | 3.3e+06 | 0.810 (-6.38σ) | 0.320 (-2.43σ) | 0.160 (-1.08σ) | 0.045 (-0.12σ) | 1.000 (-8.17σ) | 0.635 (-3.82σ) | 1.000 (-8.16σ) | 0.000 (+0.26σ) |
| xi-m9-pt2 | 2.3e+06 | 0.710 (-4.77σ) | 0.345 (-1.84σ) | 0.175 (-0.53σ) | 0.045 (+0.42σ) | 1.000 (-6.98σ) | 0.915 (-6.31σ) | 1.000 (-6.98σ) | 0.000 (+0.75σ) |

- **K0S pT 3.0-6.4 as a family.** The recommended procedure falls into the wrong basin in 22% of all 4000 toys: 5.2% at pT 3.0-4.2 (worst cell 14%) and 38.8% at pT 4.2-6.4 (6 of 10 cells above 20%, worst 84%). The wrong toys all have the same signature, P too low by 0.013-0.016 (`mean dP of wrong toys` in `res-fam-k0s.txt`). The 09 smoke cell (73%) is typical of pT 4.2-6.4, not an outlier. Two random starts reduce the family rate to 2 toys in 4000 (0.05%), four to 0.
- **Xi pT 1.2-2.0 is two regimes.**
  - pT 1.2-1.6: wrong minima in 25% of toys, but they are shallow in P: the mean dP of the wrong toys is +1e-4 to +1.4e-3 in 8 cells, -7.8e-4 in mult 2 and -3.0e-3 in mult 0 (`res-fam-xi.txt`), so the mean P bias of `rec` (+2.1σ) is close to that of the best fit (+2.4σ, the bin-centre bias). Two starts leave 2.7%, four 0.35%.
  - pT 1.6-2.0: wrong minima in 78% (every cell ≥ 58%, mult 0 100%), P bias -4σ to -11σ. Each random start reaches the best in 33-36% of toys in every cell (`mean fraction of random starts reaching the best` in `res-fam-xi.txt`; pT 1.2-1.6: 64-72%), so the miss rate after k starts is about 0.65^k: 3.3% at k=8 (max cell 5.5%), 1.4% at k=10 (max 3%). The multimodal cell of section 6.1 (2% at k=8) is typical.
- **Residual bias at the right minimum** is the bin-centre bias of section 2.5, family-wide: mean P bias of the `truth`-start fit per cell: K0S -0.80σ to +0.04σ, Xi pT 1.2-1.6 +1.46σ to +2.93σ (P coverage 0.03-0.33), Xi pT 1.6-2.0 +0.06σ to +0.75σ (coverage 0.57-0.77). Only `IntegrateBins` removes it (section 6.3, 7.3).

### 7.2 Smarter starts for Xi [verified, same runs; neighbour start: `mknb.py` -> `nb-start.tsv`, `out/nb-*.tsv`, `res-nb-*.txt`]

Every start below is followed by the recommended procedure (fit + refit + acceptance test + xRooFit), on the same 4000 Xi toys as 7.1. Rates of wrong minima, pooled per pT bin (`res-fam-xi-pt1.txt`, `res-fam-xi-pt2.txt`, `res-nb-xi.txt`, `res-famnb-xi.txt`); CPU per toy relative to `rec`, noisy:

| start | Xi pT 1.2-1.6 | Xi pT 1.6-2.0 | worst cell | CPU rel. |
|---|---|---|---|---|
| config (`rec`) | 25.2% | 78.1% | 100% (mult 0, pT 1.6-2.0) | 1 |
| `siglo`: `sigma_sig` at its lower limit (+1e-3 of the range), where every Xi truth sits | 13.4% | 57.4% | 91.5% (mult 9, pT 1.6-2.0) | 0.90-0.98 |
| `ys`: yields = N × 2/3, config shapes | 0.6% | **99.8%** | 100% | 0.85-0.94 |
| `yssiglo`: both | 1.1% | **100%** | 100% | 0.64-0.68 |
| `rec` and `yssiglo`, keep the lower NLL | 0.7% | 78.1% | 100% | 1.6-1.7 |
| 2 / 8 / 10 random starts after `rec` | 2.7% / 0 / 0 | 34.1% / 3.3% / 1.4% | 5.5% / 5.5% / 3% | 3.0 / 9.3 / 11.4 |
| **`nb`: neighbour-informed start** | **0** | **2.6%** (all in mult 0) | 26% (mult 0, pT 1.6-2.0) | 0.47-0.48 |
| **`nb` + 8 random starts** | 0 | 0.1% (2 toys, mult 0) | 1% | ~8.8 |

- **The parameter-wise "smart" starts do not work for Xi pT 1.6-2.0.** Starting `sigma_sig` at its limit helps a little (78% -> 57%); scaling the yields to the histogram makes it worse (99.8-100%: the yield-scaled start leads deterministically into the wrong basin there). At pT 1.2-1.6 the yield-scaled start alone is as good as 4 random starts (0.6%) at no extra cost. Keeping the better of the config and the yield-scaled start does not help at pT 1.6-2.0.
- **The neighbour-informed start works.** `nb` = shapes set to the median of the truths (the study09 best fits) of the **other 9** multiplicity classes of the same pT bin (leave-one-out, `mknb.py` -> `nb-start.tsv`), yields = N × their median signal fraction. It finds the best minimum in every toy of 19 of the 20 Xi cells and in 74% of toys of Xi mult 0, pT 1.6-2.0 (the cell where even the config start fails in 100% of toys); followed by 8 random starts it fails in 2 of 4000 toys. On K0S it is as good as the yield-scaled start (1 wrong toy in 4000, `res-nb-k0s.txt`). It costs about half the CPU of `rec`, because it starts near the minimum.
- **Caveat** [inferred]: the Xi shapes are almost the same in all multiplicity classes (e.g. `alpha2_sig` 2.52-2.53, `n2_sig` 1.24-1.31 at pT 1.6-2.0), which is why the neighbours' median is a good start. In production it would be the fit of a neighbouring cell (or of the multiplicity-integrated histogram) done first; that fit must itself be right, so it should be the multi-start one. The toys are generated from the cell's own truth, so they test the start, not how well neighbouring cells agree on real data; this was not tested on the real data.
- **Answer to item 2:** for Xi pT 1.6-2.0 no single cheap start replaces the random starts; a start taken from the neighbouring cells comes closest (2.6% wrong, 0 in 9 of 10 cells, at half the cost of one fit), and with 8 random starts after it the family rate is 0.05%. Without such a start, 10 random starts give 1.4% (worst cell 3%) at about 11 times the CPU of the recommended procedure.

### 7.3 `IntegrateBins` precision at high statistics, and the error to quote with the IB value [verified, `out/ib-<label>.tsv`, `ibcov.py` -> `res-ib.txt`, `res-ibcov-final.txt`]

Same toys and random starts as 6.3 (200 per cell, K=4). From the lowest bin-centre NLL among `rec` and the 4 starts ("bc"), `obs->setBins(n in range)` and `Offset(true)` + `IntegrateBins(p)` fit + refit, for p = 1e-6 (`ib6`, the setting of 6.3), 1e-5 (`ib5`) and 1e-4 (`ib4`), each from the same bc point. Six cells: the three where 6.3 saw the failures, plus pi_tpc low, Xi high and pi_tof high for the bias. `ib6` reproduces 6.3 (same status fractions and P bias). σ_IB = spread of the IB value over the toys. CPU relative to `rec`, noisy.

| cell | status≠0 ib6 / ib5 / ib4 | covQual<3 ib6 / ib4 | P bias/σ_IB ib6 / ib4 | max \|P(ib4) - P(ib6)\| / σ_IB | P coverage: own IB error / bc error | bc P error/σ_IB | CPU ib6 / ib4 |
|---|---|---|---|---|---|---|---|
| k0s-high | 0.88 / 0.825 / 0.825 | 0.80 / 0.78 | +0.05 / +0.05 | 0.11 | 0.79 / 0.665 | 0.92 | 47 / 45 |
| k0s-flag-pt8 | 0.775 / 0.805 / 0.805 | 0.465 / 0.465 | -0.01 / -0.02 | 0.09 | 0.795 / 0.695 | 1.05 | 44 / 40 |
| pitpc-low | 0.42 / 0.36 / 0.41 | 0.165 / 0.20 | -0.07 / -0.07 | 0.04 | 0.72 / 0.675 | 0.92 | 42 / 38 |
| pitpc-high | 0.765 / 0.765 / 0.765 | 0.74 / 0.74 | +0.04 / +0.04 | 0 | 0.755 / 0.66 | 0.22 (unstable, see below) | 32 / 33 |
| xi-high | 0.055 / 0.075 / 0.055 | 0.06 / 0.035 | +0.06 / +0.09 | 0.12 | 0.715 / 0.72 | 1.06 | 50 / 43 |
| pitof-high | 0.075 / 0.075 / 0.075 | 0.02 / 0.02 | -0.94 / -0.94 | 0 | 0.525 / 0.525 | 1.09 | 35 / 35 |

(`res-ib.txt`; status histories: K0S and pi_tpc low `MINIMIZE=1,HESSE=1`; pi_tpc high `MINIMIZE=-1,HESSE=3` in 112 and `HESSE=304` in 7 toys at every precision.)

- **A looser precision does not remove the status failures.** With 1e-4 or 1e-5 the failure rate is the same (K0S 0.78-0.83, pi_tpc high 0.765). In the two K0S cells 1e-4 and 1e-5 give identical fits, in pi_tpc high and pi_tof high all three settings do, and in pi_tpc low and Xi high they differ by ≤ 0.12 σ_IB: the bin integrator evidently meets 1e-4 in its first step there (`RooBinSamplingPdf`'s own default precision is 1e-4 [source `RooBinSamplingPdf.h:129`]). The bias is removed equally well (P(1e-4) within 0.12 σ_IB of P(1e-6)), and the CPU barely changes (32-47 -> 33-45 times `rec`). The guess of 6.3 (integration noise at 1e-6) is wrong.
- **The failures come from the refit, not from `IntegrateBins` as such.** Logging the first IB fit too (`IBSINGLE=1`, 100 toys, 1e-6, `out/ibs-*.tsv`, `ibcov.py` -> `res-ibs.txt`):

  | cell | status≠0 single / fit+refit | covQual<3 single / fit+refit | own P error/σ_IB single / fit+refit | P coverage, own error, single / fit+refit | P bias/σ_IB single / fit+refit | max \|P(single) - P(refit)\| / σ_IB | CPU single / fit+refit |
  |---|---|---|---|---|---|---|---|
  | k0s-high | 0.08 / 0.89 | 0.11 / 0.83 | 0.93 / 1.49 | 0.66 / 0.83 | +0.08 / +0.08 | 0.16 | 30 / 50 |
  | k0s-flag-pt8 | 0.08 / 0.79 | 0.18 / 0.42 | 1.14 / 1.24 | 0.79 / 0.82 | -0.03 / -0.03 | 0.07 | 27 / 43 |
  | pitpc-high | 0.09 / 0.77 | 0.27 / 0.76 | 1.11 / 1.34 | 0.80 / 0.78 | -0.15 / -0.04 | 1.28 | 14 / 32 |

  One IB fit started at the bin-centre minimum converges (status 0 in 91-92% of toys, covQual 3 in 73-89%), gives the same P as the fit+refit within 0.16σ in K0S, and its own error is right (0.93-1.14 σ_IB). The refit started at that converged point fails in 77-89% of toys with `MINIMIZE=1` (covariance forced positive definite). [inferred: MINUIT restarts from the tiny step sizes left by the previous HESSE, and the numerical second derivatives of the integrated NLL are then too noisy; not tested further.] In pi_tpc high the single fit is less complete (P up to 1.3σ_IB from the refit, mean -0.15σ), irrelevant in absolute terms (σ_IB(P) = 6e-10).
- **Coverage of the bin-centre error around the IB value** (item 3 of the old list). Pairing each toy's IB value with the propagated error of its bin-centre fit (`ibcov.py` on the 6.3 runs -> `res-ibcov-final.txt`, all 12 cells; and `res-ib.txt`): P coverage 0.645-0.73 and bc P error/σ_IB 0.85-1.11 in every cell except
  - pi_tof high: 0.525, from the -0.94σ boundary bias of the IB value itself (its own error gives the same 0.525);
  - pi_tpc high: the bin-centre P error is unstable (median 0.22 σ_IB over 200 toys, 1.09 over the first 100): the random-start fits that win there report P errors several times too small (6.1). S is fine (bc S error/σ_IB 1.00, S coverage 0.73). P is 1 - 2e-7 there, so this has no physical weight;
  - Xi multimodal: rec+4 still has 13.5% wrong minima, which inflate the bc spread; the bc error/σ_IB is 0.99 and the coverage 0.625.

  The S coverage with the bc error is 0.59-0.76. So the bin-centre error is a valid 1σ error for the IB value (as expected: `IntegrateBins` moves the centre by a bias of ≤ 12σ in pi_tpc high, ≤ 5σ in S of pi_tof high, but hardly changes the curvature).
- **Answer to item 3:** the precision is not the knob. Use **one** `IntegrateBins` fit (1e-4 is enough and no slower) from the bin-centre multi-start minimum, and quote either its own error when it ends with status 0 and covQual 3, or the bin-centre error, which covers the IB value correctly. Cost: 14-30 times the recommended procedure per cell for the single fit.

### 7.4 The yield-scaled start on more cells [verified]

`ys` = the recommended procedure started from the config shapes with yields N_toy × the config fraction (2/3 for every particle). Tested on the 40 family cells (7.1) and on the 12 cells of section 1 with their 200 toys (`out/ys12.tsv`, K=4; `famana.py --s12` -> `res-ys12.txt`, which also uses study12's 9-start fit in the reference). Wrong-minimum rates (CPU relative to `rec`, noisy):

| cell | rec | ys | rec+2 | ys+2 | rec and ys, keep the lower | CPU ys |
|---|---|---|---|---|---|---|
| k0s-low | 0 | 0.02 | 0 | 0 | 0 | 0.76 |
| k0s-high | 0 | 0 | 0 | 0 | 0 | 0.85 |
| k0s-flag-pt8 | 0.115 | **0** | 0 | 0 | 0 | 0.58 |
| k0s-smoke09 | 0.73 | **0** | 0 | 0 | 0 | 0.78 |
| xi-low | 0.08 | **0.365** | 0.015 | 0.04 | 0.06 | 0.90 |
| xi-high | 0.275 | **0** | 0.02 | 0 | 0 | 0.74 |
| xi-flag-multimodal | 0.91 | **1.00** | 0.38 | 0.425 | 0.91 | 0.71 |
| pitpc-low | 0 | 0.005 | 0 | 0 | 0 | 0.81 |
| pitpc-high | 0.19 | **0** | 0.01 | 0 | 0 | 0.72 |
| pitof-low | 0 | 0 | 0 | 0 | 0 | 0.92 |
| pitof-high | 0.12 | **0** | 0.005 | 0 | 0 | 0.80 |
| pitof-tiny | 0.315 | 0.385 | 0.17 | 0.21 | 0.21 | 0.88 |
| K0S family, 20 cells (7.1) | 0.22 | 0.0005 | 0.0005 | 0 | 0.00025 | 0.54-0.68 |
| Xi family pT 1.2-1.6 / 1.6-2.0 | 0.25 / 0.78 | 0.006 / 0.998 | 0.027 / 0.34 | 0.0015 / 0.44 | 0.005 / 0.78 | 0.85-0.94 |

- **Where the wrong minima come from a start far below the data (K0S, pi_tpc high, pi_tof high, Xi pT 1.2-1.6), the yield-scaled start removes them** and is cheaper than the config start (0.54-0.85 of the CPU). The mechanism of 6.2 (config yields nsig+nbkg = 1.5e5, i.e. 20-230 times below N in the K0S family and 1e4-4e4 times in pi_tof high and pi_tpc high; xi-low is the only tested cell where N, 1.1e4, is below them) is general for these families.
- **It is harmful for Xi pT 1.6-2.0 and Xi low statistics** (xi-low 8% -> 36.5%, shallow minima there: +0.41σ mean P bias instead of +0.07σ), and does nothing for the flat pi_tof tiny cell. Random starts after it restore the rate of 2 random starts.
- **Answer to item 4** [inferred from the above]: replace the config's absolute yields by N × the config fraction for K0S, pi_tpc and pi_tof; keep the config start (or better, the neighbour start of 7.2) for Xi. With 2 random starts after it ("ys+2"), no wrong minimum is left in any non-Xi cell except pi_tof tiny (a statistical regime), at 1.9-3.2 times the CPU of the recommended procedure.

### 7.5 Files (all in `S3=/Users/scannito/alice/Analysis/RooFitKnowledge/scratch/study-last-12/`)

- `last12.cpp` (+ `last12-head.inc` = the unchanged first part of `final12.cpp`; `common.h`, `parts.h`, `rowlib.h`, `cells12.tsv`, `build.sh` copied from study-final-12-11, `build.sh` now takes `OUT=<binary>`). Binaries: `last12` (family and IB-precision runs), `last12b` (+ neighbour start, `NBSTART`, `NBONLY`), `last12c` (+ `IBSINGLE`); the current source builds all three behaviours, each gated by its environment variable.
  - Usage: `./last12 <cells.tsv> <label|all> <nToys> <out.tsv> [K]`; `MODE=fam` (default) or `MODE=ib` with `IBPREC=1e-6,1e-5,1e-4`.
  - Output: one row per toy and stage (columns as in 6.4, plus the stages `ys`, `siglo`, `yssiglo`, `nb`, `truth`, `ib6`, `ib5`, `ib4`, `ib6a`).
- `famcells.py` -> `fam-k0s.tsv`, `fam-xi.tsv` (truths, log `famcells.log`); `mknb.py` -> `nb-start.tsv`.
- Runs: `run.sh <cells> <label|all> <nToys> <K> <out>` (log `out/run.log`), `runib.sh <labels>` (7.3), `chain.sh` (7.2 neighbour start, then 7.4), `chain2.sh` (7.3 single IB fit).
  - `out/fam-k0s.tsv` (200 toys, K=4), `out/fam-xi.tsv` (200 toys, K=10); `out/sub-*.tsv` are the per-pT subsets of them.
  - `out/nb-k0s.tsv`, `out/nb-xi.tsv` (200 toys, K=0, `NBSTART=nb-start.tsv NBONLY=1`).
  - `out/ib-<label>.tsv` (200 toys, K=4, `MODE=ib`), `out/ibs-<label>.tsv` (100 toys, `IBSINGLE=1`, precision 1e-6).
  - `out/ys12.tsv` (the 12 cells of `cells12.tsv`, 200 toys, K=4).
  - `out/smoke-fam.tsv`: 3-toy reproduction check.
- Analysis (plain python): `famana.py <tsv> <k list> [--s12]` -> `res-fam-k0s.txt`, `res-fam-xi.txt`, `res-fam-<fam>-<pt>.txt`, `res-nb-*.txt`, `res-ys12.txt`; `mktab.py` -> `tab-fam-*.md`; `ibcov.py <tsv...>` -> `res-ib.txt`, `res-ibcov-final.txt` (the latter on `study-final-12-11/out/ms-*.tsv`).
- `insert7.py` puts this section into the note from `sec7.md` and `sec7*.md`.
- `git-status-start.txt`, `git-status-end.txt`: the project's `git status --porcelain` before and after.

---------------------------------------------------------------------------------------------------------------------------------

## Resume here

**FINAL.** Done: sections 1-5 (first pass), section 6 (final pass: recommended + k random starts on the same 2400 toys, the mechanism of the K0S wrong basin, `IntegrateBins` on the toys) and section 7 (last pass), which closes the four items left open by section 6:
1. family rates over all 20 K0S 2026 pT 3.0-6.4 and all 20 Xi 2026 pT 1.2-2.0 cells, 200 toys per cell (no reduction of the toy count was needed): 7.1;
2. smarter starts for Xi (`sigma_sig` at its limit, yield-scaled, both, neighbour-informed) against random starts: 7.2;
3. `IntegrateBins` at 1e-4, 1e-5 and 1e-6 at high statistics, the single IB fit, and the coverage of the bin-centre error around the IB value: 7.3;
4. the yield-scaled start on the 40 family cells and on the 12 cells of section 1: 7.4.

Nothing is left NOT DONE within the scope of this note. Limits that remain by construction (not tasks for this note):
- The toys are perfect-model data, so the rates are upper limits for the real-data rates where the model is imperfect (6.2). Testing the final procedure on the real data, cell by cell, belongs to the end-to-end run of note 11, not to a toy study.
- The neighbour-informed start (7.2) used the study09 best fits of the other cells; in production it would use fits made in the same run, whose own correctness then matters. Toys cannot test that.
- CPU numbers are relative and noisy (other jobs ran); a timing on an idle machine is the coordinator's task.

To rerun section 7: in `S3`, `bash build.sh last12` (and `OUT=last12b bash build.sh last12`, `OUT=last12c bash build.sh last12` for the variants); then `./run.sh fam-k0s.tsv all 200 4 out/fam-k0s.tsv`, `./run.sh fam-xi.tsv all 200 10 out/fam-xi.tsv`, `./runib.sh <labels>`, `./chain.sh`, `./chain2.sh` (at most two at a time); analysis with `famana.py`, `mktab.py` and `ibcov.py` as listed in 7.5, and `insert7.py` to rebuild section 7 from `sec7*.md`. Each family run takes a few minutes, each IB cell 1-4 minutes.
