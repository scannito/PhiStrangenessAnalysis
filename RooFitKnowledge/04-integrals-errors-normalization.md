# 04 - Integrals, error propagation and normalization

STATUS: COMPLETE (second pass 14 Oct 2026; third pass 15 Oct 2026: tolerant snap recipe). Tags: [doc], [source] (file:line in
`~/alice/sw/SOURCES/ROOT/v6-36-10-alice2/v6-36-10-alice2/roofit/roofitcore/src`, abbreviated `src/`), [verified] (I ran it; script and
output file named), [inferred]. Two scratch directories: `int/` = `/Users/scannito/alice/Analysis/RooFitKnowledge/scratch/integrals/`
(first pass: e1-e4) and `s405/` = `scratch/study0405/` (second pass). Environment: `source scratch/rooenv.sh`, compile with
`s405/build.sh x.cpp`. Everything uses the project's own `DynamicRooFitter.h` (`#define private public`, read-only) on the real
`fitConfig.json` and the real data file. Nothing in the project was modified.

## Summary (what is true after both passes)

1. The formulas in `ExtractYieldsAndPurity()` are correct uses of the API: `nsig * createIntegral(obs, NormSet(obs), Range(win))` is the window
   yield, `getPropagatedError` uses the full covariance (S-B correlation included). The same window yield obtained by fitting it as a
   parameter (RooExtendPdf) agrees to 0.09 sigma in all 13 cells where both fits reach the same minimum; the S errors agree to 0-2% (12% in one cell). [verified: s405/extend.cpp, extend.out, extend-analysis.txt]
2. Propagated errors against a bootstrap (12 cells x 500 refits): when `covQual==3` and the fit is well behaved, median propagated dS / robust bootstrap spread of S is 0.90-1.02 (resampling the observed histogram) and 0.95-1.05 (resampling the fitted model); the status (0 or 3) does not change it. The "7-22% too small" finding of the first pass was an artefact of Gaussian covariance toys; against the bootstrap the API is right to about 10%. [verified: s405/an-e2.py, an-e2b.py -> e2-analysis.txt, e2b-analysis.txt]
3. When `covQual==2` (and in cells (3,20), (4,12), (9,17), (7,25) even for covQual-3 replicates) the propagated error is not an error estimate: S of the covQual-3 replicates spreads by 14-61 times their dS ((3,20), (9,17), (7,25); in (4,12) the replicates agree among themselves but sit more than 5 dS from the observed-data fit), and a restart from the project's "minimum" lowers the NLL by 1.3e4 to 3.8e5 and moves S by 2-4 dS_propagated (= 4-13% of S) in 5 of 16 selected K0S cells. [verified: e2b-analysis.txt, s405/extend2.cpp -> extend2.out]
4. The window can be snapped to bin edges (`snap_to_bin`), but the snap widens a window by ONE bin on a side whenever an edge lies exactly on a bin edge and floating-point noise puts it on the wrong side: 35-94% of on-edge inputs, depending on the axis. With the project's configs: xi [1.305,1.335] becomes [1.304,1.336]; pi_tpc [-3,3] becomes [-3,3.2]; K0S and phi are fine. No config in `JSONConfigs/` sets `snap_to_bin: true` (grep), so it is latent. [verified: s405/snap.cpp, snap.out] A tolerant rounding (1e-3 bin) keeps every edge-aligned window and still rounds off-edge windows outward; the code change is in "Tolerant snap". [verified: s405/snapfix.cpp]
5. A window that extends beyond the observable is not clipped or rejected: the pdf is extrapolated (K0S [0.40,0.60]: S = 1.011 nsig, B = 2.0 nbkg exactly, purity 0.89; window [0.56,0.60] gives S = 5074, B = -309719 (negative), purity -0.017); a reversed window returns 0, 0 and purity NaN silently. [verified: snap.out]
6. **The observable keeping 100 bins for a 200-bin histogram changes nothing in the project's results** (fit values, minNll, errors, S, B, purity are bit-identical for 50/100/200/1000 observable bins on 10 real cells of 4 kinds). It only changes the sampling grid of the plotted curve (and so the plotted curve and the RooPlot chi2/pulls by up to ~2% (K0S) and 7% (pi_tpc)). It matters only for `IntegrateBins`, which the project does not use. See "Verdict on the observable's bin count".
7. If `IntegrateBins` is ever used, `obs->setBins(n)` must be the number of histogram bins INSIDE the observable's range, not `GetNbinsX()`: K0S 200 (correct), but phi 71, xi 72 (the xi range silently becomes [1.289,1.361] instead of [1.29,1.36] because of float-rounded axis edges). With the wrong count the result is worse than without IntegrateBins. [verified: s405/ibtime.cpp, phisynth.cpp]
8. The default bin-centre rule has a small systematic: S low by 0.04% (2.2 dS) at 3e7 entries (K0S), negligible below ~5e6; for phi the fitted Voigtian sigma is 12% too large (sigma/bin = 0.5), S is not affected. IntegrateBins with the right bins removes both but costs 130x (K0S) to 2400x (phi). [verified: s405/k0ssynth.cpp, phisynth.cpp]
9. `getPropagatedError` for a numeric-integral model (phi: Voigtian + BkgSourav2): RooFit's numeric window integrals are accurate to 2e-8 (signal) and 3e-7 (background); effect on S below 1e-3 sigma. The propagated dS is right to 5-10% against the toys (median dS / robust spread 0.93-1.10). [verified: s405/phisynth.cpp -> phisynth-AT.tsv, phisynth-analysis.txt]

## Integrals and normalization (source reading, first pass; spot-checked)

### createIntegral semantics
- `RooAbsReal::createIntegral(iset, NormSet(nset), Range(name), NumIntConfig(cfg))`: lazy integral of the function over `iset`; with `Range` over that named range, else the full domain. [source: RooAbsReal.cxx:497-534, 552-560]
- `NormSet`: without it the integral of a pdf is NOT normalised. [doc: RooAbsReal.cxx ~513; source: RooAbsPdf.cxx:362-370 `analyticalIntegralWN` = analyticalIntegral(code)/getNorm(nset)]
- Hence `createIntegral(obs, NormSet(obs), Range(win))` = (integral over win)/(integral over the normalisation range) = fraction of the normalised pdf in the window. [source] [verified: analytic fraction equals an independent GSL integral to 1e-12 on 20 cells, int/e4.cpp, e4_k0s.tsv]
- Multiple ranges `Range("a,b")` build a RooAddition and throw if ranges overlap. [source: RooAbsReal.cxx:552-585]
- The normalisation range is `_normRangeOverride`, else `_normRange` (`setNormRange`/fit `Range`), else the full observable range. [source: RooAbsPdf.cxx ~531] The project sets neither, so fractions refer to the whole fit range (after the RooDataHist import has expanded it to bin edges). [inferred, consistent with snap.out section C where the full window gives S = nsig exactly]
- `getNorm(nset)` = normalisation integral (1 if null). `getVal(nullptr)` returns the raw value. [source: RooAbsPdf.cxx:317-340, 417-435]

### Analytic vs numeric
- RooRealIntegral picks analytic if the pdf advertises a code, else numeric. [doc: RooRealIntegral.cxx:20-30] Defaults: epsAbs = epsRel = 1e-7, 1D method RooIntegrator1D (Romberg). [source: RooNumIntConfig.cxx, RooNumIntFactory.cxx] A `NumIntConfig` argument does not force numeric integration; `forceNumInt(true)` does. [doc/source]
- K0S model (DSCB + Chebychev): window and normalisation integrals are analytic (accuracy 8e-13 / 4e-16); forced numeric 7e-7 median, 9e-6 max on the signal fraction (< 0.04 sigma of S). [verified: int/e4.cpp, e4_k0s.tsv]
- phi model (RooVoigtian + RooGenericPdf background): both numeric. Accuracy against GSL adaptive (rel. 1e-13), at the truth parameters of 4 synthetic phi truths: signal window 1.9e-8 and -5.2e-8; background window 2.7e-7; background sideband [1.06,1.08] 3.3e-7; tightening epsAbs/epsRel to 1e-13 does not improve the background number (3.3e-7), so it is a floor (the reference or the integrator) far below any physical effect: 3e-7 of B = 0.1 event for B = 3e5, dB = 700. [verified: s405/phisynth.cpp, phisynth-AT.out]
- Changing the global integrator tolerance from 1e-7 to 1e-12 for the whole fit+extraction (phi toys, same histograms): median |dS|/dS = 0.002-0.02, 90th percentile 0.03-0.2; rare toys move by 10-60 sigma (those are non-converged fits where the minimiser path changes, not an integral effect). Fit time rose about 3x (the runs overlapped other jobs, no timing conclusion). [verified: phisynth-analysis.txt, A vs T paired]
- The project silences everything below WARNING (`RooFitMessages::Configure`, at every fitter construction), so the numeric-integrator INFO lines never reach its logs. [source]

### RooAddPdf, ranges, fractions
- Coefficients refer to the default range unless `fixCoefRange`; `fitTo` freezes the coefficient normalisation (`fixAddCoefNormalization`). [doc/source RooAddPdf.cxx ~319-384; FitHelpers.cxx ~384]
- Extended mode: `expectedEvents` = sum of coefficients. [source RooAddPdf.cxx:751-783]
- Sum signals ("A+B" -> `RooAddPdf(sig; frac)`): window integral = sum_i f_i x window fraction_i. [inferred; the project's real configs have no sum signal, not tested]

### Bin centre vs bin integral
- Default NLL evaluates the pdf at the bin centre ("correct only if the second derivative vanishes"). [doc: RooBinSamplingPdf.cxx:21-30; arXiv:2012.02746] `IntegrateBins(p)`: p<0 off (default -1); p==0 on, precision 1e-4, only for continuous pdfs on a RooDataHist; p>0 forced. [doc RooAbsPdf.cxx:915-923; source FitHelpers.cxx:621]
- `RooBinSamplingPdf` uses the OBSERVABLE's own binning (`obs->getBinning()`), which spans the observable's RANGE. [source RooBinSamplingPdf.cxx ~140-150, 330-352] Hence the correct `setBins` = (range)/(histogram bin width), see the verdict below.

### RooExtendPdf
- `RooExtendPdf(name, title, pdf, N, "range")`: N = events inside `range`; `expectedEvents = N / frac` with frac = the same normalised window fraction as the project's `sigInt`. [source RooExtendPdf.cxx:59-136] `RooAbsPdf::expectedEvents` default returns 0. [source RooAbsPdf.cxx:2396]

## getPropagatedError (source reading)

Source: `src/RooAbsReal.cxx:2417-2520`. [source]
- sigma_f^2 = F^T C F, F_i = (f(theta_i + sigma_i) - f(theta_i - sigma_i))/2, C the correlation matrix; the step is the full +-1 sigma (a secant, not a tangent). [source]
- `RooArgSet nset`: normalisation set for the evaluated function and exclusion of observables from the parameters. Only parameters that are floating in the fit result and that the function depends on; zero-error parameters skipped; THROWS if the function's parameter differs from the fit-result value by more than 0.01 sigma. A function that IS a RooRealVar of the fit result returns its Hesse error. [source]
- Correlations of the used subset use `reducedCovarianceMatrix` (marginal sub-matrix of V). [source RooFitResult.cxx:949-1005]
- Steps use `RooRealVar::setVal`, which clips to [min,max], so at a parameter limit the +-sigma evaluation does not move. [source RooRealVar.cxx:225-235] [verified: first-pass e1, three covQual-2 cells]
- The code never checks `status` or `covQual`. [source]
- The window edges in `n_sigma` mode are numbers fixed by `setRange`, so the propagation ignores their dependence on the fitted mean and sigma. Quantified in "Audit v2", item B7.

## E1 (first pass, 30 K0S cells, covariance toys) [verified: int/e1.cpp, e1_sel.tsv]
- covQual 3, high statistics (N > 1e6, 19 cells): dS_roofit / dS_toy = 0.97-1.01 for S, B, P.
- covQual 2 (cells (3,20), (4,12), (9,17)): propagated dS 7.3e4, 2.1e5, 2.6e5, tangent propagation 6.3e3, 4.4e4, 3.9e5, robust toy spread 2.6e3, 1.2e3, 5.1e3 (S about 6e6 to 1.5e7). Resolved by the bootstrap and by the restart test below: in these cells the project's fit is not at the minimum.
- Low-statistics cells (107k-230k entries): propagated dS was 7-22% below the robust covariance-toy half-width; the bootstrap below shows that the toys, not the API, were off (Gaussian toys of the poorly-constrained DSCB tail parameters have heavy tails).
- Window counts vs model: (H_window - (S+B))/sqrt(H) is below 0.3 for the 12 low-statistics cells and -2 to -5.6 for the 10 high-statistics cells (cell (5,10): H - S - B = -32461 of 3.3e7, 5 dS): the model over-predicts the window by ~0.1% at 1e7 events, a model-misfit systematic that dS does not cover.

## E2: bootstrap analysis (second pass) [verified: an-e2.py, an-e2b.py; data in int/e2_data.tsv, int/e2_model.tsv; outputs e2-analysis.txt, e2b-analysis.txt, e2_summary_data.csv, e2_summary_model.csv]

Design (first pass, int/e2.cpp): 12 cells x 500 replicates, each replicate a Poisson fluctuation either of the OBSERVED histogram ("data") or of the FITTED model's exact bin integrals ("model"), refitted with the project's DoFit and extracted with ExtractYieldsAndPurity. Spread measured as the half-width of the central 68% interval of the replicate S ("robust"), because the plain std is dominated by a few runaway fits.

Well-behaved cells ((0,10), (5,10), (0,31), (0,32), (0,33), (4,33), (7,31), (9,33); covQual-3 replicates), median replicate dS divided by the robust spread of S:

| resampling | ratio range over the 8 cells | robust pull width (S - S_obs)/dS |
|---|---|---|
| observed histogram | 0.90 (cell (0,33)) to 1.02 | 0.99-1.10 |
| fitted model | 0.95 to 1.05 | 0.94-1.06 |

- Split by status: replicates with status 0 and with status 3 give the same ratio within the noise (e.g. (0,31): 0.96 / 1.02; (7,31): 0.92 / 1.02; (9,33): 0.91 / 0.98). Status 3 is benign in these cells; it is not a flag. [verified]
- Purity: robust spread vs mean dP agree equally (std of the pull of P 0.99 and 1.14 in the two high-statistics cells (0,10), (5,10); 1.3-4.5 in the low-statistics ones, outlier driven).
- The plain std of S is larger than the dS of the observed fit by factors 1.05-8.6 in the low-statistics cells ((0,31) 1.08, (0,32) 2.2, (0,33) 3.8, (9,33) 2.8, (4,33) 8.6) because 6-12% of the replicates have covQual 2 and wander (std 1254 against robust 381 in (0,33)); in the covQual-2 replicates of those cells, |S - S_obs| > 5 dS in 0-17% (cell (0,33): 7%, (4,33): 17%).
- Bias (model resampling): mean S_rep - S_obs = -0.6 sd (cell (0,10), 8e6 entries) and -2.2 sd (cell (5,10), 3.6e7), i.e. -0.025% and -0.043% of S: the bin-centre systematic, confirmed against a known truth in the synthetic test below. [verified]
- Bad cells (3,20), (4,12), (9,17), (7,25) (observed-histogram resampling): 51%, 58%, 53%, 16% of the replicates have covQual 2. The covQual-3 replicates have median dS 2.9e3, 4.9e3, 4.0e3, 1.4e3 for (3,20), (4,12), (9,17), (7,25), but the robust spread of their S is 1.05e5, 4.7e3, 2.45e5, 2.0e4: 36, 1.0, 61 and 14 times their own dS. In (4,12) the replicates agree among themselves (spread = dS) but |S - S_obs| > 5 dS in 100% of them: the fit of the observed histogram sits elsewhere than where its refits land (the restart test says the observed-data fit is not at the minimum). |S - S_obs| > 5 dS in 89%, 100%, 98%, 23% of the covQual-3 replicates. The covQual-2 replicates have dS of the same order as the spread (ratio 0.93-1.5). Conclusion: in such cells neither covQual 3 nor the size of dS certifies S. [verified: e2b-analysis.txt]
- Cell (7,25): resampling the fitted model behaves (ratio 0.99, covQual 3 in 94%), resampling the observed histogram does not (S spread 14x dS): the observed histogram has structure the model cannot follow, and refits of fluctuated copies hop between minima. Model misfit becomes fit instability. [verified: e2b-analysis.txt]
- Sideband (covQual-3 replicates, the 8 well-behaved cells): `dRatioFit` (fit sideband ratio, background shape only) / robust bootstrap spread of the ratio = 0.87-0.98 (data resampling), 0.90-1.00 (model resampling); the histogram-sideband ratio error (the project's formula, assuming independence) / robust spread of the ratio = 0.89-1.20 (data), 0.92-1.21 (model; 1.37 in cell (7,25)): the independence assumption is wrong by -11% to +37%, both signs. [verified: s405/an-sideband.py -> sideband-analysis.txt]

## E3: IntegrateBins on 25 real K0S cells [verified: int/e3.cpp, int/e3_sel.tsv, s405/an-e3.py -> e3-analysis.txt]

Variants: A default; B `IntegrateBins(0)` with the observable at 100 bins (the project's state); C `IntegrateBins(0)` with `obs->setBins(200)` (correct for K0S: range = full histogram); D `IntegrateBins(1e-6)` with 200 bins.
- Shift of S in units of dS_propagated, variant C against A: median 0.73, 75th percentile 1.5, max 6.0 (cell (5,10), 3.4e7). Shift of purity in units of dP: median 1.3, max 14.6. Cells below 3e5 entries: |dS| <= 0.25 dS. [e3-analysis.txt]
- Variant B (wrong bins) against A: S shifts up to 27.9 dS (cell (7,25)), purity up to 42 dP, the NLL value differs from A by 45 to 1.1e5 units in absolute value (both signs; it is a different likelihood): wrong bins integrated. [e3-analysis.txt]
- Cost: median 122x (C) and 129x (D) the fit time of A (A 3-40 ms, C 0.4-1.1 s; the runs overlapped other jobs, ratios only). Output also contains 364 GSL "could not integrate function" messages (int/e3.err) from the adaptive integrator in the IntegrateBins path. [verified]
- Which is right: see the synthetic test below. The shift of S under IntegrateBins is the bin-centre bias of the default fit, not noise.

## Synthetic tests with a known truth

### K0S: bin-centre bias and IntegrateBins [verified: s405/k0ssynth.cpp -> k0ssynth.tsv; 40 toys x 4 truths x 2 variants]
Truth = fit of a real cell (shape parameters and yields), toys = Poisson of the exact bin integrals, fit with the project's DoFit (A) or with `IntegrateBins(0)` and `setBins(200)` (C). Bias of the mean S in units of the median dS:

| truth cell | N | A (default) | C (IntegrateBins, 200 bins) | time A / C |
|---|---|---|---|---|
| (5,10) | 3.5e7 | -2.2 dS (-0.04%, 12.5 standard errors of the mean) | -0.37 dS (-0.01%) | 3.4 ms / 0.45 s |
| (0,10) | 8e6 | -0.38 dS | +0.10 dS | 3.7 ms / 0.49 s |
| (2,33) | 3e5 | -0.17 dS (pull mean) | -0.10 dS | |
| (0,33) | 1e5 | +0.10 dS (pull mean), no bias | +0.03 dS | |

The sigma_sig bias is below 0.7%, nsig below 0.2%. The relative S bias of the default fit is -0.04% at 3.5e7 and -0.02% at 8e6 entries and compatible with zero at 3e5 and below (40 toys; with only these four truths I do not claim a functional form); in units of dS it grows with N because dS/S falls as 1/sqrt(N).

### phi (Voigtian + BkgSourav2, numeric integrals) [verified: s405/phisynth.cpp -> phisynth-AT.tsv (150 toys x 4 truths x variants A and T), phisynth-BW.tsv, phisynth-C.tsv; analysis in phisynth-analysis.txt]
Truths from fits of real offline phi cells (6,2) and (0,5) at several scalings (1.1e5 to 1.1e6 events), 200 bins on [0.9,1.2] (1.5 MeV), fit range [0.995,1.1] expanded to [0.9945,1.101], window [1.0,1.05], toys from exact GSL bin integrals.
- Window yield S under the default fit: median relative bias -0.03% to -0.16% (10%-trimmed mean -0.04% to -0.11%), mean pull -0.13 to -0.18 (one truth gives NaN: 2 toys with non-finite dS), median(dS)/robust spread 0.93-1.10, robust pull width 0.92-1.12. The propagated error is right, the fit is slightly low.
- Convergence is poor: status 0 in only 26-57% of the toys, covQual 3 in 49-74% (49% for the (6,2) x 0.2 truth); 0-2% of the toys have |pull| > 5 (runaway fits).
- Voigtian sigma: truth 0.000807, default fit mean 0.000908 (+12.5%; sigma/bin = 0.54); with `IntegrateBins(0)` and the right bin count (71): 0.000796 (-1.4%), S unchanged (bias -0.02%, pull mean -0.11). Time 0.01 s vs 24 s per fit (about 2400x) on 12 toys.
- With the WRONG observable bin count `IntegrateBins(0)` gives a biased result: setBins(200): sigma 0.00143 (+76%), median S +1.6% above the truth (6 sigma); setBins(100) (the default): S biased in the same way and sigma 0.00145 (one toy in ten collapses to sigma 0.0001). On the real phi cell (0,5): default nsig 145241, sigma 0.001708; `IntegrateBins(0)` with 71 bins: nsig 145242, sigma 0.001652; with 200 bins: nsig 147039 (+1.2%), sigma 0.001992 (+17%); with 100 bins: nsig 147467, sigma 0.002066. [verified: s405/ibtime.cpp -> ibtime.out]

## Verdict on the observable's bin count (100 bins for a 200-bin histogram)

Question: does `obs` keeping RooFit's default 100 bins (the histogram has 200) affect any result of the project's actual calls (`DoFit`, `ExtractYieldsAndPurity`, `DrawFit`; no `IntegrateBins`)?

What it can affect [source]: (a) `RooBinSamplingPdf` (only if `IntegrateBins` is passed); (b) the number of points at which `plotOn` first samples the curve (frame bins), hence the curve's shape and the curve averages used for pulls and `RooPlot::chiSquare`; (c) `createHistogram`/`binnedClone`-type calls with the observable's default binning (not used). The NLL for a RooDataHist uses the dataset's own bin centres (internal clone with the histogram's 200 bins); createIntegral with `Range` does not use bins.

Demonstration [verified: s405/obsbins.cpp -> obsbins.out]: the project's own fitter run with `obs->setBins(n)` before `DoFit()` for n = 50, 100, histogram bins, 1000, on 10 real cells (k0s (5,10), (0,33), (3,20), (9,3); xi (3,5), (0,12); pi_tpc (4,10), (0,30); phi (3,3), (0,5)):
- minNll identical to all printed digits (10 decimals), all floating parameter values identical (max relative difference 0), status, covQual, S, dS, P, dP identical, in all 10 cells, including the covQual-2 cells and the numeric-integral phi cells. VERDICT: no effect on any fitted value or yield or error.
- Plot only: number of points of the plotted total curve 91-109 (n=50), 125-148 (100), 206-233 (200), 1006 (1000); chi2/ndf from the curve (with nFloat): k0s (9,3) 302.3 / 300.5 / 293.7 / 294.1 (n = 50 / 100 / 200 / 1000); pi_tpc (4,10) 34241 / 33613 / 33613 / 33141 (no 200: the histogram has 100 bins); k0s (0,33) 1.318 / 1.314 / 1.312 / 1.314; curve area / bin width vs data count: (9,3) +0.057% at 100 bins, +0.0001% at 1000; pointwise curve difference (interpolated at 41 points, relative to the 100-bin curve) 0.4-2.1% (K0S), 0.3-1.5% (xi, phi), 0.9-7.6% (pi_tpc) for the other bin counts; the sum of the pulls changes (K0S (9,3): -212 at 100 bins, -127 at 1000). So the plotted curve at 100 bins is a polygon that cuts the peaks by up to about 2%; nothing downstream reads it.
- Only for `IntegrateBins` would the 100 matter, and then it is not a small effect (E3 variant B, phi toys variant W): wrong bins integrated, S shifted by up to 28 dS (K0S) or +1.6% (phi). The right setting is `obs->setBins(round((range max - range min)/bin width))` evaluated on the range AFTER the RooDataHist import (K0S 200, xi 72, phi 71, pi_tpc 100; read from `obs` after DoFit, s405/rng.out), not `h->GetNbinsX()`.

## Snap to bin and windows beyond the range [verified: s405/snap.cpp -> snap.out]

`CalculateIntegrationLimits` (DynamicRooFitter.h:508-535): with `snapToBin`, `lowEdgeBin = FindFixBin(min)`, `upEdgeBin = FindFixBin(max)`, then `min = GetBinLowEdge(lowEdgeBin)`, `max = GetBinLowEdge(upEdgeBin + 1)`: round OUTWARD to whole bins. The data axes carry float-rounded limits (K0S xmin = 0.45 stored as float, 0.44999998807; xmax 0.55000001192; so a window edge written as 0.475 falls at bin coordinate 50.0000119 and 0.515 at 129.9999928).
- Window on bin edges, snap on, all interior edges tested (`CalculateIntegrationLimits` only, no fit; windows [e, e+10 bins] and so on): the lower edge is unchanged in 120/199 (K0S), 104/199 (xi), 93/99 (pi_tpc), 129/199 (phi) and widened by one bin otherwise; the upper edge is unchanged in 129/199 (K0S), 13/199 (xi), 6/99 (pi_tpc), 24/199 (phi) and widened by one bin otherwise (never narrowed). Whether it widens depends on rounding noise of the axis, not on the user's intent.
- The project's configured windows with snap on: K0S [0.475,0.515] -> [0.474999994,0.5150000036] (harmless; S 87999.398 vs 87999.395 unsnapped, cell (0,33)); xi [1.305,1.335] -> [1.304,1.336] (+2 bins of 15, +6.7% width); pi_tpc [-3,3] -> [-3,3.2] (upper edge exactly at a bin edge, widened by one bin); phi [1.0,1.05] -> [0.999,1.05] (lower edge inside a bin, rounded out by 0.667 bin: right) and sideband [1.06,1.08] -> [1.059,1.08].
- n-sigma windows (mean +- n sigma), snapped, K0S cells (5,10), (0,33): the window rounds out to bin edges, e.g. 2 sigma [0.487461,0.510656] -> [0.487,0.511], S 80100 -> 80891 (+1.0%, 2.7 dS) in (0,33); at 3 and 4 sigma the change is below 0.2% (0.2 dS). Yields and the purity are consistent with the snapped window (the same limits are used for the integral and for `setRange`). So the result is right for the window that was actually used, which is up to one bin wider than the requested one.
- Verdict on snap: it returns the yield in the snapped window correctly; defects are (i) the unintended one-bin widening for edge-aligned inputs, and (ii) it is not what a user who writes the window already on bin edges expects. A correct recipe rounds with a tolerance as `HistogramUtils::IntegralAndErrorPair` does; it was implemented and tested in the third pass (next subsection). [verified]
- Beyond the range [0.45,0.55] (K0S (5,10) and (0,33), snap off/on): [0.40,0.60] gives S/nsig = 1.0110 and 1.0015, B = 2.000 x nbkg (the linear Chebychev is extrapolated symmetrically), purity 0.8925 (cell (5,10); 0.943 inside); [0.44,0.52] S = 33564867 against 33461164 for the clipped window [0.4495,0.52] (the 0.44-0.45 piece is extrapolation, +0.3%); [0.56,0.60]: S = 5074 (should be 0), B = -309719, purity -0.017 (cell (5,10)); with snap on: [0.55,0.5505], S = 261, an invented window in the overflow; reversed [0.52,0.50]: S = 0, B = 0, purity NaN, no exception. The first case for the full window [0.45,0.55] gives S = nsig exactly (control). RooRealVar::setRange accepts limits outside the variable's range without a message. [verified: snap.out section C]

### Tolerant snap: proposed code change (third pass) [verified: scratch/study0405/snapfix.cpp -> snapfix.out]
Implemented in a scratch copy (`SnapTolerant`) and compared with the project's `CalculateIntegrationLimits` (called through the real class). Proposed replacement of DynamicRooFitter.h:527-534 (NOT applied to the project):

```cpp
    // 2. Adjust limits to the histogram bin edges if requested. An edge closer than a thousandth of a bin
    //    to a bin edge is ON that edge (the axis limits are float-rounded, ~1e-4 bin of noise); otherwise the
    //    window grows outward to whole bins. The upper edge is the UP edge of the bin containing max - tol,
    //    so an edge-aligned max is kept, and bins are clamped to the axis.
    if (config.integration.snapToBin) {
      const TAxis* ax = h1Data->GetXaxis();
      const double tol = 1e-3 * ax->GetBinWidth(1);
      const int lowBin = std::clamp(ax->FindFixBin(minRange + tol), 1, ax->GetNbins());
      const int upBin = std::clamp(ax->FindFixBin(maxRange - tol), 1, ax->GetNbins());
      minRange = ax->GetBinLowEdge(lowBin);
      maxRange = ax->GetBinUpEdge(upBin);
    }

    // 3. A window must lie inside the observable and have min < max: the integrals are not clipped to the
    //    range and would extrapolate the pdf (S > nsig, negative B) or return zeros with a NaN purity.
    if (!(minRange < maxRange) || minRange < obs->getMin() - 1e-9 || maxRange > obs->getMax() + 1e-9) {
      throw std::runtime_error(std::format("[FATAL] DynamicRooFitter: integration window [{}, {}] is empty or outside "
                                           "the observable range [{}, {}]", minRange, maxRange, obs->getMin(), obs->getMax()));
    }
```
(`tol` uses the first bin's width: the project's axes are uniform. Step 3 is a separate decision: with it, a window that extends beyond the observable stops the run instead of extrapolating; a softer variant clips and logs a warning.)

Results:
- All edge-aligned windows [e_k, e_k+10 bins], edges written as decimal numbers as in a JSON (190 windows each for K0S, xi, phi, 90 for pi_tpc): lower edge kept by the old code 100/190 (K0S), 57/190 (xi), 78/90 (pi_tpc), 66/190 (phi), by the new code in ALL cases; upper edge kept by the old code 100, 123, 10, 134, by the new one in all cases.
- Configured windows: K0S [0.475,0.515] unchanged by both; xi [1.305,1.335]: old [1.304,1.336] (one bin wider on each side), new [1.305,1.335]; pi_tpc [-3,3]: old [-3,3.2], new [-3,3]; phi [1.0,1.05] and sideband [1.06,1.08]: both [0.999,1.05] and [1.059,1.08] (1.0 and 1.06 lie 2/3 of a bin inside a bin: rounded outward, as intended). Off-edge windows (K0S [0.4752,0.5148], 0.4 bin inside) round outward identically in both; an edge 0.02 bin inside still rounds out (tolerance is 0.001 bin).
- Yields with the configured window on real cells (S change against no snap, in units of dS): K0S (2,33), (5,10): 0 for both; xi (3,5): old +0.13 dS, new 0; xi (9,25): old +0.05, new 0; pi_tpc (5,22): old +14.1 dS (S +0.11%), new 0; phi (0,5): +0.52 dS for both (intended outward rounding of 1.0 to 0.999).
- Out-of-range and reversed windows: the new rounding alone still returns something (outside [0.56,0.60] -> [0.5495,0.55], the last bin; reversed [0.52,0.50] -> [0.52,0.50]), hence the validation step 3.

## RooExtendPdf / direct window-yield fit vs the project's integral-times-nsig [verified: s405/extend.cpp -> extend.out, extend-analysis.txt; extend2.cpp -> extend2.out]

Model B: `RooExtendPdf(sigExt, sigPdf, Swin, "signalRegion")`, same for the background (Bwin), `RooAddPdf(sigExt, bkgExt)` (extended, no coefficients), fit with the same `fitTo` options; Swin, Bwin start at nsig0 x fraction(start), limits [0,1e10]. Same likelihood function, reparametrised (nsig = Swin / fraction). 28 real cells (16 K0S, 4 xi, 3 pi_tpc, 5 phi).
- Same minimum (|NLL_B - NLL_P| < 0.3) in 13 of 28 cells (7 K0S, 4 xi, 2 phi): S differs by at most 0.09 dS (median 0.02), B by at most 0.19 dB, dS_B/dS_P in 1.000-1.122 (K0S (4,33): 1.12, where the direct fit ended with covQual 2), purity error ratio 0.99-1.04 except K0S (5,10) (+10%) and (4,33) (+78%, covQual 2) (phi: purity is not configured, not compared). In the other 15 cells the two fits stop at different points (9 with NLL_B higher, 6 lower); there S differs by up to 46 dS (pi_tpc), 25 dS (K0S (0,0)): this is the minimiser (status 3, not at the minimum), not the parametrisation.
- At the SAME point (the project's result, then MIGRAD+HESSE of model B started there; extend2.cpp, 24 cells): in the 18 cells where model B stays (|dNLL| < 0.05), Hesse-direct dS / propagated dS = 0.9996-1.018 for the 10 covQual-3 K0S cells (cells (0,10), (9,3): 1.0000-1.0001; (5,10): 1.018; (0,33): 1.016), 1.0005-1.002 for the 4 xi cells, 1.000 for 2 converged phi cells (and 0.76 and 1.06 for the two covQual-2 phi cells (3,3), (9,4)); for B (K0S): 0.988-1.10 and for purity 0.988-1.10. So the propagation of ±1 sigma secants agrees with the true Hesse error of the reparametrised fit to 0-2% for S (up to 10% for B and P in K0S (5,10)): the secant/linearisation is a small effect. [extend2.out]
- In 6 cells restarting from the project's minimum lowers the NLL by 3 (K0S (7,31)) to 3.8e5 units (the five covQual-2 K0S cells (3,20), (4,12), (9,17), (2,15), (6,20): -1.3e4, -2.2e5, -3.5e4, -3.8e5, -1.3e4; S then moves by +3.0, +4.0, +2.1, +4.0, +3.1 dS_propagated, i.e. 3.6%, 5.9%, 4.3%, 13%, 5.6% of S): the project's fit had not found the minimum, and the propagated error there is that of a wrong point. The covQual-2 cells are a selection bias of mine (chosen to include them), not a fraction of the data. [extend2.out]
- Cost: for K0S/xi/pi_tpc the two fits take comparable time (1.4-20 ms) because the window fraction is analytic. For phi (numeric integral inside the NLL) model B takes 0.7-3.5 s against 5-15 ms for the project's approach (100-300x slower): the project's way of computing the window integral only after the fit is the right choice. (Timings overlapped other jobs: ratio only.) [extend.out]
- Verdict: the two approaches are equivalent; the direct parametrisation gives S and dS in one step and has no secant, but no advantage in the projects' cells and is much slower for phi. No change recommended.

## Audit v2 of ExtractYieldsAndPurity (DynamicRooFitter.h:114-180) and CalculateIntegrationLimits (508-535)

### Real defects (with evidence)
- D1. No guard on the fit state. `status`, `covQual`, parameters at their limit are never read. In 5 of 16 selected K0S cells (covQual 2) S is wrong by 4-13% (restart lowers the NLL by 1.3e4-3.8e5) and dS is meaningless (4-100x either way). Over all 960 real cells: `covQual != 3` in 80/340 (K0S), 249/360 (pi_tpc), 6/260 (xi); the quantity N_data/(nsig+nbkg) (exactly 1 at an extended minimum) differs from 1 by more than 1% in 8.5% of K0S, 10.6% of pi_tpc, 0.8% of xi cells (32% of covQual-2 K0S cells, 1.5% of covQual-3 ones). [verified: s405/allcells.cpp -> allcells.tsv (960 rows); counts from s405/an-allcells.py -> allcells-analysis.txt; status counts 310/134/338 agree with BOOKMARK.md] A cheap flag: |N - (nsig+nbkg)|/sqrt(N) > 5 flags 57 K0S, 102 pi_tpc, 2 xi cells.
- D2. Snap-to-bin widens an edge-aligned window by one bin depending on floating-point noise (details above; xi and pi_tpc configured windows; pi_tpc S would move by 14 dS). Latent: no config enables it. Fix proposed and tested: "Tolerant snap" above.
- D3. No validation of the window against the observable (beyond-range extrapolation, reversed window gives zeros and NaN purity, negative background yield, overflow window with snap). Latent for the shipped configs (K0S windows inside the range), reachable from an n-sigma window or a typo.
- D4. Systematics not in the quoted error: (a) bin-centre rule, up to 0.04% of S (2.2 dS at 3e7 entries); (b) model misfit in the window of 0.1% at 1e7 entries (E1, up to 5 dS). Not a defect of the function, but the function reports only the statistical dS.
- D5. For weighted data (online-efficiency phi) the Hesse errors the function propagates are understated by sqrt(mean weight) = 4-8 (note 05, item 8). The fit is in `DoFit`, the consequence is in the yield error.

### Behaviours to know (not defects)
- B1. `nsig * integral` is the number of signal events in the window; with `NormSet(obs)` the fraction refers to the fit range after the expansion to bin edges. Correct. [verified]
- B2. Purity error includes the S-B correlation; right to 1-3% against the Hesse of the direct parametrisation, to ~10% against the bootstrap. [verified]
- B3. Sideband from the histogram (`from_fit: false`): `IntegralAndErrorPair` uses `FindBin(x1 + 1e-5)`, `FindBin(x2 - 1e-5)`: whole bins, edges rounded outward and under/overflow included if outside the axis (not run); the ratio error assumes independence of the fit background and the sideband counts (which are part of the fitted data): off by -11% to +37% against the bootstrap in 12 cells. The histogram sideband contains signal tails: cell (5,10), [0.52,0.55]: fit background 242914 vs histogram 285230, ratio 3.65 vs 3.11. [verified: int/e2_test.tsv; bootstrap comparison above]
- B4. `bkgRatio` fromFit: `nbkg` cancels, only the background shape parameters contribute; right to 0.87-1.0 against the bootstrap spread of the ratio. [verified]
- B5. `getPropagatedError` clips its steps at the parameter limits and takes the secant over +-1 sigma; nothing wrong in cells with covQual 3 and parameters away from the limits. [verified]
- B6. The window is the same number for the integral and for `obs->setRange("signalRegion")`; the logged window is the one after snapping. [source]
- B7. n_sigma windows: the edges are fixed numbers in the propagation. With the dependence on the fitted mean and sigma included (manual finite-difference propagation, same method as RooFit; my fixed-edge value reproduces the project's dS to 1e-4), dS rises by 1.5-4% at 3 sigma and 4-14% at 2 sigma (up to 42% in cell (7,31), 2 sigma). Unused today (all configured windows are fixed), relevant if `n_sigma` is used. [verified: s405/nsigedge.cpp -> nsigedge.out]
- B8. The `n_sigma` window uses `sigma_sig` (the DSCB core), ignoring the tails. [source]

### Style remarks (no evidence of harm)
- `RooProduct`/`RooFormulaVar` created on the stack per call and the `obs` named ranges are never cleared; harmless.
- The two `getPropagatedError` calls recompute the integrals; cost is 2 x (number of parameters) integral evaluations, negligible for analytic integrals.
- `Logger::Debug` prints the (possibly snapped) window; `CalculateIntegrationLimits` is called again in `DrawFitOnPads`, so the drawn lines and the yield window agree.

## Recommendations (recommendations only; none applied)
1. Read `status()`, `covQual()` and N_data/(nsig+nbkg) after `DoFit`; flag the cell when covQual < 3 or |N - nexp|/sqrt(N) > 5; do not quote dS from `getPropagatedError` then. Evidence: D1, E2, restart test.
2. Replace the snap block with the tolerant version and add the window validation (code in "Tolerant snap"); until then do not enable `snap_to_bin` for windows already on bin edges (D2). K0S and phi windows are unaffected.
3. Reject or clip windows outside the observable range and windows with min >= max (D3).
4. Leave `IntegrateBins` off: it gains 0.04% in S at 3e7 events and costs 130x (K0S) to 2400x (phi). If it is ever used, set `obs->setBins(round(range/binwidth))` after the import (K0S 200, xi 72, phi 71, pi_tpc 100; read from `obs` after DoFit, s405/rng.out). Evidence: obs-bins verdict.
5. Keep the integral-times-nsig approach; do not move to RooExtendPdf (equivalent, 100-300x slower for phi).
6. For `n_sigma`, include the edge dependence in the error (B7), or document that the error is understated by 2-14%.
7. Report the window-model misfit (0.1% at 1e7 entries) and the bin-centre bias as systematics.

## Pitfalls
- `getPropagatedError` clips its +-1 sigma steps at parameter limits and never checks `covQual`.
- The observable's binning is not taken from the TH1 for a uniform axis, and `IntegrateBins` uses the observable's bins over its RANGE: K0S 200 is right only because the fit range equals the histogram range.
- `Range()` in `createIntegral` and `setRange` are not clipped to the observable's range; the normalisation stays on the fit range.
- Float-rounded axis edges in the real histograms (0.45 stored as 0.44999998807) make "on-edge" a matter of rounding noise.
- Concurrent `alienv printenv` calls from several sessions fail; capture the environment once.
- A `RooFormulaVar::getPropagatedError` with a fit result from `RooMinimizer::save()` after only `hesse()` crashed (null result); use `fitTo(Save)` results.

## Reproduce
`source s405/../rooenv.sh; cd s405; ./build.sh X.cpp; ./X`. Programs: `obsbins` (verdict), `snap`, `snapfix` (tolerant snap), `extend`, `extend2`, `phisynth M variants truths out.tsv` (A default, T tight eps, B/C/W IntegrateBins variants; `phisynth 150 AT 0123 phisynth-AT.tsv`, `phisynth 10 BW 1 phisynth-BW.tsv`, `phisynth 12 C 1 phisynth-C.tsv`), `k0ssynth 40`, `ibtime`, `nsigedge`, `allcells`; analysis `an-e2.py`, `an-e2b.py`, `an-e3.py`, `an-phisynth.py`, `an-sideband.py`, `an-allcells.py`, `an-misc.py`; first-pass programs and data under `int/` (`e1`..`e4`, `e2_data.tsv`, `e2_model.tsv`, `e3_sel.tsv`).

## Resume here
Nothing is pending for the questions of the brief. Done in the third pass: the tolerant snap recipe (section "Tolerant snap"). Optional, not done: the 960-cell tabulation of the window S shift under IntegrateBins (960 x 0.5 s); it would only extend the 25-cell E3 table.
