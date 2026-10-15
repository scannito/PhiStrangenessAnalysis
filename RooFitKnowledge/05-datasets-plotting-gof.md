# 05 - Datasets, plotting and goodness of fit (COMPLETE, third pass)

Written 14 October 2026 from the ROOT 6.36.10 ALICE source tree
(`~/alice/sw/SOURCES/ROOT/v6-36-10-alice2/v6-36-10-alice2/roofit/roofitcore/src`, abbreviated `src/`) and from
experiments on toy histograms and on the real files. Tags: [doc] Doxygen comment, [source] code read,
[verified] run here, [inferred] concluded but not tested.

STATUS: COMPLETE after the third pass of 15 Oct 2026 (sections 5.3 per-bin weights, 5.7 VisualizeError/createHistogram/RooHistFunc, 5.8 synthetic GOF below 1e5 entries). Sections 1-3 and the toy experiments are the first pass (expa/expb/expc; still valid, with the
refinements marked "second pass"); the real-cell experiments, the PNGs, the online (weighted) phi study, the low-count toy and the small
items are in sections 5-7. Scratch directories: `gof0/` = `/Users/scannito/alice/Analysis/RooFitKnowledge/scratch/datasets-plotting-gof/`
(first pass) and `s405/` = `scratch/study0405/` (second pass; compile with `s405/build.sh x.cpp` after `source scratch/rooenv.sh`).
All second-pass programs use the project's own `DynamicRooFitter.h` (`#define private public`) on the real `fitConfig.json` and real data;
nothing in the project was modified. The first-pass backup of this file: `s405/05-first-pass-backup.md`.

Environment note [verified]: `alienv printenv` failed when another process was refreshing
`~/alice/sw/MODULES` ("rm: ... Directory not empty"). A manual environment worked for compiled programs
(`env.sh` in the scratch dir below: ROOTSYS, DYLD_LIBRARY_PATH, ROOT_DYN_PATH, ROOT_INCLUDE_PATH with Vc/xsimd/TBB/boost includes).
Interactive `root` did not work (missing Vc.pcm); compiled programs did.

## Summary

- `RooDataHist(..., Import(TH1))` MUTATES the RooRealVar it is given: its range is moved OUTWARD to the nearest histogram bin edges (clipped to the axis range if wider). Its binning count is NOT changed for uniform axes (stays at 100 default or what `setBins` gave). [source RooDataHist.cxx:666-733] [verified expa]
- Under/overflow bins are never imported. [source importTH1 loops over bins 1..N] [verified expa A1: sum 210, excluded 100 and 200]
- `isNonPoissonWeighted()` is true iff any in-range imported bin CONTENT has |frac| > 1e-10. It ignores Sumw2 and bin errors. [source RooDataHist.cxx:2269] [verified expa A3]
- The "weighted data" warning in the project logs comes from the OnlineEfficiency phi histograms (non-integer contents in all 70 cells), not from the offline `h3PhiData`. [verified t2]
- With a continuous pdf and a RooDataHist, RooFit's NLL is the extended UNBINNED likelihood with one weighted entry per bin centre; it is not the binned Poisson likelihood. [source + verified expb B1, difference 0.000e+00]
- `RooPlot::chiSquare` uses asymmetric Poisson errors from the data, drops empty bins from both chi2 and ndf, and has ndf = nonempty bins in curve range - nFitParam. [source RooCurve.cxx:534]
- Default `plotOn` of an extended pdf normalises the total curve to the DATA count in the plot range, not to nexp; `Components` with `RelativeExpected` uses nexp. [source RooAbsPdf.cxx:1936-2200] [verified expb B4]
- Neither task draws pulls or any chi2: both call `DrawFit(TPad*)` (pull pad = nullptr). [source PhiFitTask.h:129, PurityTask.h:327]

Second-pass summary (details in sections 5-7):
- The observable's 100 bins do not change any fitted value, yield, error or NLL (bit-identical for 50-1000 bins on 10 real cells); they change only the plotted curve sampling (curve, RooPlot chi2/pulls by up to 2% (K0S), 7% (pi_tpc)). The right `setBins` for `IntegrateBins` is the number of histogram bins inside the observable's range (K0S 200, xi 72, phi 71, pi_tpc 100), not `GetNbinsX()`. [verified: s405/obsbins.cpp; details in note 04]
- Goodness of fit on 40 real cells (24 K0S, 8 xi, 8 pi_tpc): `RooPlot::chiSquare` (with nFloat) tracks the NEYMAN statistic (data errors): within 3% of it in 33 of 40 cells, within 10% in 37 (exceptions: the two lowest-statistics xi cells, RooPlot 14% and 20% below Neyman, and pi_tpc (9,0), factor 2.7); it differs from the Pearson/ndf (model errors) by more than 5% in 15 of 40 cells, up to 27% (K0S (5,16)) and 16% (K0S (9,21)); Pearson/ndf and Poisson deviance/ndf with the proper ndf agree to 1% in 23 of 40 cells and to 5% in 33, and to 1.5% in the 9 K0S cells below 1e6 entries; for covQual-3 K0S cells chi2/ndf = 1 + 3e-6 x N (median), so the DSCB + Chebychev model is statistically rejected (p < 0.05) from about 1e5 entries on; pi_tpc is 3-5 orders of magnitude from acceptable at every statistics. `createChi2` works in this build and equals the centre-rule Pearson to 1e-5. [verified: s405/gof.cpp -> gof.tsv, gof-table.md; s405/allcells.cpp]
- The pull pad of `DrawFit(RatioCanvas)` has a hard-coded +-5 axis: in bad fits 85-96% of the points are off-scale (k0s (5,16) 184 of 200, k0s (5,10) 170 of 200, pi_tpc 96 of 100, phi (6,2) 63 of 71) so the pad looks nearly empty exactly when the fit is worst. [verified: s405/png.cpp -> png.out, PNGs in s405/pngs/]
- The total curve (normalised to the data count) and the component curves (normalised to nexp) disagree by N_data/(nsig+nbkg) = 1.021 (k0s (5,16)) and 0.981 (pi_tpc (3,4)); the sum of the component curves at the peak is 2.1% below / 1.9% above the total curve. [verified: png.out]
- Online-efficiency (weighted) phi: the warning is printed by every default `DoFit` on an online cell (and not on the offline one); the default fit errors on S are understated by sqrt(mean weight) = 3.8-8.1 (effective weight 14-65): covQual-3 cells: SumW2Error error / default error = 5.3 (median) = sqrt(w_eff) x 0.998; in weighted toys the default error is 0.12-0.26 of the true spread, SumW2Error 0.79-1.06, AsymptoticError 0.88-1.13. [verified: s405/online.cpp]
- Below 1e5 entries (third pass, synthetic toys from two real K0S models at 1e3, 1e4, 1e5 entries): Pearson/ndf (all 200 bins, ndf = 200 - nFloat) has median 0.98-1.00 and rejects 4-8% at p < 0.05; the Poisson deviance is biased at 5 counts/bin (median 1.10 or 0.87 depending on the model, 21% or 0.5% rejections); `RooPlot::chiSquare` is 0.56-0.96 and rejects 0-4%. So Pearson with exact bin integrals is the statistic for the whole range. [verified: s405/gofsynth.cpp, section 5.8]
- Low counts: RooFit pulls are not N(0,1): their RMS is 0.68 at 1 count/bin, 0.84 at 3, 0.88 at 30, 0.94 at 100, 0.98 at 1000 (per-bin counts, correct model), and `RooPlot::chiSquare`/ndf is 0.49, 0.47, 0.83, 0.94, 1.01 at 1, 3, 30, 100, 1000 counts/bin: it undershoots 1 for a correct model below ~100 counts per bin. The hand Pearson/ndf is 0.99-1.02 at all statistics. [verified: s405/lowcount.cpp]

## 1. RooDataHist and binning

### Constructors [source RooDataHist.cxx:~100-420]
- `RooDataHist(name,title,const RooArgSet& vars, const char* binningName="")`: empty hist, binning from the variables.
- `(name,title,vars,const RooAbsData&, double wgt)`: bin an existing dataset (extra dims projected).
- `(name,title,const RooArgList& vars, const TH1* hist, double wgt=1)`: import TH1/2/3; dimension must match.
- `(name,title,vars, RooCategory& indexCat, map<string,TH1*>/map<string,RooDataHist*>, wgt)`: sliced import.
- Command-arg form: `Import(TH1&, bool impDens)`, `Weight(double)`, `Index(RooCategory&)`, `Import(const char*, TH1&)`, `Import(map)`, `GlobalObservables(set)`. There is NO `Range` option in the RooDataHist constructor; the range comes from the RooRealVar (see below). `Range` exists in plotOn, fitTo, createNLL, reduce, sumEntries(cutSpec,cutRange).
- `Import(TH1&, impDens)`: impDens matters only for variable-width axes (density correction by bin volume); for uniform axes bv = 1. [source importTH1 line ~385]

### What is imported [source importTH1]
- Per bin: `add(vset, content*wgt, error^2 * wgt^2)`. Error is `TH1::GetBinError` (= sqrt(content) if the TH1 has no Sumw2 array, sqrt(sumw2) if it has). [source]
- `add()` allocates the sumw2 array as soon as sumw2 > 0 or wgt != 1; so for an unweighted TH1 the stored sumw2 equals the content. [source RooDataHist.cxx:1690]
- Under/overflow ignored. [verified expa A1]
- `Weight(0.5)`: sumEntries 210 -> 105; SumW2 error scaled by 0.5. [verified expa A2]

### RooRealVar versus TH1 binning [source _adjustBinning] [verified expa A1]
Table (TH1: 20 bins on [0,10]; observed after constructing the RooDataHist):

| variable range given | range after ctor | datahist bins | sumEntries |
|---|---|---|---|
| [0,10] | [0,10] | 20 | 210 |
| [2,8] (aligned) | [2,8] | 12 | 126 |
| [2.1,7.9] | [2,8] | 12 | 126 |
| [2.24,7.76] | [2,8] | 12 | 126 |
| [-5,15] | [0,10] | 20 | 210 |
| [3.1,3.3] | [3,3.5] | 1 | 7 |

- Rule: lower edge = bin containing xlo+tol, upper edge = bin containing xhi-tol (tol = 1e-6 * mean bin width); range is expanded to those bin edges, message at INFO level only ("fit range of variable ... expanded to nearest bin boundaries"). [source RooDataHist.cxx:712]
- The user's variable range is changed in place, so `obs->getMin()/getMax()` after the constructor can differ from the JSON. The project's logs show this: `[0.995,1.055] --> [0.9945,1.056]` (log_output_globalConfigDatapp.log line 21). [verified in log]
- For a uniform axis only `setRange` is applied to the user's variable; its bin count is unchanged (100 by default), the internal copy of the variable gets the histogram's binning. For a non-uniform axis the variable receives the binning too. [source lines 696-710] [verified expa: var bins stays 100 for a 20-bin hist; `setBins(7)` stays 7]
- Consequence for the real data [verified t0, inferred for effect]: k0s mass axis is 200 bins on [0.45,0.55] and the config range is [0.45,0.55], so the range is unchanged but `obs->getBins()` = 100 (the 200 bins exist only inside the RooDataHist). The phi trigger axis is 200 bins on [0.9,1.2] (1.5 MeV) while fitConfig.json says [0.995,1.1]; 0.995 and 1.1 are not bin edges (63.33 and 133.33 bin widths), so the range becomes [0.9945,1.101] [inferred from the rule above, same behaviour as the log line; not re-run on the current config].
- Danger: `IntegrateBins` (RooBinSamplingPdf) uses the OBSERVABLE's binning, not the datahist's. [source RooBinSamplingPdf.cxx:140-150] [verified expb B3: with a 100-bin hist, `setBins(100)` gives minNll -52346.751 (equal to hand bin-integral Poisson -52346.741 within 0.01), while setBins 50/200/1000 give m = 5.0038/5.0268/5.0069 vs 5.0019 and minNll -52327/-52313/-52340]. So IntegrateBins is valid only if the observable's bin count equals the number of histogram bins inside the observable's range. Second pass: that is `(range max - range min)/bin width` evaluated after the import, NOT `h->GetNbinsX()`: K0S 200 (range = histogram range), but phi 71 (histogram 200 bins over [0.9,1.2], range [0.9945,1.101]) and xi 72 (range silently expanded from [1.29,1.36] to [1.289,1.361], see 5.6); with `setBins(200)` on phi the fit is wrong (sigma +17% on a real cell, S +1.2%), see note 04. The project does not use IntegrateBins; the project's `obs` bins (100) do not affect any fitted value (bit-identical, section 5.5). [verified: s405/obsbins.cpp, ibtime.cpp]

### Weights vs counts, isNonPoissonWeighted [source RooDataHist.cxx:2269] [verified expa A3]
- True iff some `_wgt[i]` has `abs(modf(w)) > 1e-10`. Results: integers (with or without Sumw2, even with errors inflated x3) -> false; one bin 5.5 -> true; 5+1e-9 -> true; 5+1e-11 -> false; negative integer -> false; non-integer only in the underflow -> false (not imported); integers with `Weight(0.5)` -> true, `Weight(2)` -> false.
- RooDataSet: `isNonPoissonWeighted` is also false for integer-valued weights (2,3) and true for 0.5. [verified expa A4]
- Where it matters [source FitHelpers.cxx:497-515]: (a) `fitTo` prints the "a likelihood fit is requested of what appears to be weighted data" warning when weighted AND neither `SumW2Error` nor `AsymptoticError` was given; the fit values use weights, errors are the plain Hesse ones (proportional to the sum of weights); (b) `RooAbsData::plotOn` DataError(Auto) -> SumW2 for weighted, Poisson otherwise [source RooAbsData.cxx:1778]; (c) `RooChi2Var` DataError(Auto) -> SumW2 if weighted, Expected (sqrt of pdf, Pearson) otherwise [source RooChi2Var.cxx:74].
- Real files [verified t2 over all `DataFile/pp/DeltaY/Data/*.root`]: `h3PhiData` under `phi-strange-correlation/phiStrangenessCorrelation/phi/` (note the extra `phi/` directory) has all-integer contents and err^2 == content in all 70 cells (2024_28luglio, 2024_5agosto, 2026_HighIRac, 2026_23luglio, 2026_28luglio, 2026_6agosto), although a Sumw2 array is stored (21816 entries). The `phi-strange-correlation_OnlineEfficiency/...` copy has non-integer contents in all 70 cells (6729 to 9422 of 14000 bins) and err^2 != content in 9940 bins; `..._OnlineEfficiencyNoPhi` is integer. The k0s histogram `h3K0sCandidatesMass` of 23luglio: all 68000 bins integer, err^2 == content, no Sumw2. Hence the 140 warnings in log_output_globalConfigDatapp.log (210 in ...pref, 70 in MCClosure) = 70 cells x (two online-efficiency phi tasks / three / one): they come from the OnlineEfficiency histograms. So the user's premise "h3PhiData has Sumw2 stored" is true but is not the trigger; the non-integer content of the online-efficiency histogram is. The second pass ran the project's own `DoFit` on online cell (6,2): the warning is printed once per fit, and not on the offline cell (6,2) (section 5.3). [verified: s405/online warn -> online-warn.out]
- Meaning for the online path [inferred]: the fit treats bin contents as counts with weights, Hesse errors are not corrected; `SumW2Error(true)` (uses the stored sumw2 imported from err^2) or `AsymptoticError(true)` would be the explicit choices; the size of the effect on the yield errors was not measured.

### sumEntries, binned vs unbinned, NLL [source + verified]
- `RooDataHist::sumEntries()` = sum of all weights (Kahan); `sumEntries(cut, range)` counts a bin if the bin CENTRE is in the range. [source RooDataHist.cxx:2108-2133] [verified expb B2: Range(2.07,7.93) gives 58 bins, those whose centre is inside]
- RooDataSet = unbinned events (each with a weight); RooDataHist = fixed bins, one weight per bin, internally the bin centre coordinates.
- NLL for RooDataHist + ordinary continuous pdf with the default `cpu` backend (RooNLLVarNew) [source RooNLLVarNew.cxx:doEval, BatchModeDataHelpers.cxx:96-133]: zero-weight bins are dropped (`skipZeroWeights` unless the pdf has `BinnedLikelihoodActive`); each remaining bin centre is an event of weight n_i; NLL = -sum n_i ln f(x_i) + [extended term nexp - N ln nexp]. [verified expb B1: hand computation equals `minNll` to 0.000e+00]
- The binned Poisson likelihood sum(mu_i - n_i ln mu_i) with mu = nexp f(xc) w differs from it by the constant N ln w and nexp(1 - sum f(xc_i) w_i) (here sum f w = 0.99998359). [verified expb B1]. Poisson deviance with mu from the centre rule = 109.154, with bin integrals = 109.173 (100 bins, toy). [verified]
- The true binned Poisson path (`doEvalBinnedL`, mu_i from bin yields) is only used when the pdf carries `BinnedLikelihoodActive` (RooHistPdf/RooRealSumPdf-type models, e.g. HistFactory), not for the project's pdfs. [source RooNLLVarNew.cxx:60-100, RooRealSumPdf.cxx:747]
- Default evaluation = bin centre; `IntegrateBins(prec)` wraps the pdf in RooBinSamplingPdf (default -1 = off; 0 = only RooDataHist with non-binned pdf; >0 forced) [source FitHelpers.cxx:384, RooBinSamplingPdf.cxx:311-352]. A doc comment states the centre rule is correct only if the second derivative vanishes [doc RooBinSamplingPdf.cxx:~70].
- Sub-range fit [verified expb B2]: `fitTo(dh, Extended(1), Range("fit"))` with fit = [2,8] of [0,10]: ns+nb = 8430.3 while the data in [2,8] are 5867 and in the full range 8562; the true full-range numbers are 3000 + 5507.5. I.e. the yields refer to the full variable range, they are extrapolated. Bins are selected by their centre.
- `RooHistPdf`, `binnedClone` (second pass) [verified: s405/misc.cpp -> misc.out, on real K0S cell (2,33)]: `RooHistPdf(obs, dh, order)` of the real histogram: window fraction [0.475,0.515] = 0.88156693 (order 0 and 1) and 0.88156650 (order 2) against the histogram bin sum 0.88156697 (so the normalisation is exact and the window integral uses whole-bin sums, 5e-7 relative at worst); pdf value at x = 0.4997: 55.4163 (order 0, = the bin density, piecewise constant), 55.5204 (order 1, +0.19%), 55.5796 (order 2, +0.30%). `RooDataSet::binnedClone()` uses the OBSERVABLE's binning: a dataset filled with the 200 bin centres gives a 200-bin RooDataHist if `obs->setBins(200)` was set and a 100-bin one with the default (sumEntries unchanged at 320772 in both): another place where the observable's 100 default bins would matter if it were used (the project does not call it). RooHistPdf is relevant only for template (MC shape) fits, which the project does not do.

## 2. RooPlot and plotOn

### Data points
- `RooDataHist::plotOn` without `Binning` uses the datahist's own binning (line 1099-1123), i.e. the 200 histogram bins, not the frame's 100. [source]
- DataError: Auto -> Poisson for unweighted, SumW2 for non-integer weights; Poisson = RooHistError central interval at 1 sigma, asymmetric, adjusted to start at n for small n; for n > 100 it is the analytic mu1 = n - sqrt(n+0.25) + 0.5, mu2 = n + sqrt(n+0.25) + 0.5 (so lower error = sqrt(n+.25)-0.5, upper = sqrt(n+.25)+0.5); lookup table for n < 1000. Uses `Int_t(weight()+0.5)` for RooDataHist. [source RooHistError.cxx:73-120, RooDataHist.cxx:1556-1590, RooHist.cxx:425-470]
- Other options [doc RooAbsData.cxx:1625-1700]: Binning, Cut, CutRange, Rescale, Invisible, AddTo, RefreshNorm, XErrorSize, Name, MarkerStyle/Color/Size, LineStyle/Color/Width, FillStyle/Color, DrawOption, Asymmetry, Efficiency. `CutRange` needs an explicit pdf Normalization (see warning in RooHist::getFitRangeNEvt, RooHist.cxx:360).

### Curve and normalisation [source RooAbsPdf.cxx:1936-2200]
- Options (table at RooAbsPdf.cxx:1850-1930, RooAbsReal.cxx ~1500-1600): NormRange, Normalization(scale, Relative|RelativeExpected|NumEvent|Raw), Components(name|set), Slice, Project, ProjWData, ProjectionRange, Range(name|lo,hi), VisualizeError(fitres,Z,linear), Precision, VLines, Invisible, AddTo, ShiftToZero, LineStyle/Color/Width, FillStyle/Color, Name.
- Scale = `Normalization` * (N_data_in_frame / 1) * data bin width for `Relative` (default); i.e. the curve is the pdf density times the DATA count, whether or not the pdf is extended. For `RelativeExpected` scale = nexp (`expectedEvents`) * bin width; for `NumEvent` scale / nExpected. [source lines 2118-2141]
- [verified expb B4, toy full-range fit]: area of the total curve inside the range / bin width = 8564.2 (data 8562, nexp 8561.83); Components(g) with RelativeExpected = 3037.2 (ns 3035.4); Components(e) = 5526.7 (nb 5526.4). Same fit, same normalisation. A full-range fit makes data count and nexp coincide, so the project's mixed convention (total: default; components: RelativeExpected) is consistent there. [inferred: it would not be for fits that differ from nexp]
- Fit-range behaviour [source lines 1948-1967, verified expb B5]: if the pdf has a `fitrange` attribute (set by `fitTo(Range(...))`) the curve is plotted and normalised in that range by default (INFO message). Total curve then integrates to the data in range (5868 vs 5867). A component with `RelativeExpected` and `Range("")` in a sub-range fit was inflated: 4351 vs ns = 3026 (factor 1/0.696, the range fraction), because the automatic NormRange multiplies by 1/fraction. This does not affect the project today (fit range = full obs range); it will if sub-range fits are introduced.
- Curve wings [source RooAbsReal.cxx:1775-1785, RooCurve.cxx:352-392] [verified expc]: when no `Range` is given the curve gets two extra "wing" points on each side, one frame-bin outside the range (frame bin = obs bins, 100 by default, so 1 MeV for k0s) at the edge value, plus zero points; with `Range` there are no wings. An area integral of the raw curve points over-counts by about 2% in the toy (5688.5 vs 5526.7). `RooCurve::average` and the pulls are not affected because they use only bins inside the xstart..xstop of the curve; but with wings the curve's range is wider than the variable range, so bins in the wing zone would also enter chi2 if the histogram extended there (not the case when var range = hist range).
- `RooPlot::SetMinimum(-1111)` resets to the default [source RooPlot.cxx:1006]. Slice/Project/ProjWData are for multi-dimensional pdfs (not used: 1D fits). Second pass [verified: s405/misc.cpp, misc.out, pngs/misc_k0s_2_33.png]:
  - `plotOn(..., VisualizeError(fitres, Z, linear))` draws the 1 sigma band of the plotted curve using the same secant formula as `getPropagatedError` (doc RooAbsReal.cxx:2525-2560): the half-width at the peak of K0S (2,33) is 20.20 events per 0.5 MeV bin against 20.19 from `getPropagatedError` of the normalised pdf x N x bin width (agreement 0.05%), 0.227% of the curve or 0.21 of the Poisson error of the data bin; cell (0,33): 11.50 vs 11.47 (0.39%, 0.21 of sqrt(n)). The band (linear, Z = 1) is invisible at the scale of the plot (0.2-0.4% of the curve). Time: linear band < 10 ms, sampling band (100 curves) 30 ms. Because the default `Normalization` scales the curve to the data count, the band shows only the shape uncertainty, not the uncertainty of nsig+nbkg. (Values off the peak in misc.out are not reliable: my extraction took the nearest polygon vertex.)
  - `paramOn(frame, Format("NEU", AutoPrecision(1)), Layout(0.58,0.97,0.95), Label(...))` writes every floating parameter in alphabetical order (alpha1_sig ... sigma_sig), with errors, including nsig and nbkg (the totals in the fit range, not the window yield S) and not purity or chi2; at 0.04 text size the lines `mean_sig = 0.49908 +/- 0.0000...` overflow the box on the right (clipped in the PNG). Not useful as it stands: a custom `TLatex` with S +- dS, purity, chi2/ndf and status would carry more information.

### pullHist, residHist, chiSquare [source RooPlot.cxx:1018-1117, RooHist.cxx:727-790, RooCurve.cxx:534-571]
- `pullHist(hist, curve, useAverage=true)` = `residHist(..., normalize=true, useAverage=true)` [source RooPlot.h:179]. Residual = data - (curve averaged over the bin [x - exl, x + exh], or interpolated at the centre if useAverage=false). Divided by the DATA error on the side toward the curve: lower error if data > curve, upper error if data < curve. Bins with zero error get residual 0 and a warning. Only bins whose centre is between the curve's first and last points are used.
- Hence the sigma is the asymmetric Poisson (or SumW2) error of the DATA, not sqrt(model). For an empty bin: Poisson interval 0..1.84, so pull = -avg / 1.84 (nonzero, included in the pull plot).
- `RooPlot::chiSquare(curve, hist, nFitParam)` -> `RooCurve::chiSquare`: chi2 = sum over bins with centre in curve range and y != 0 of ((y-avg)/err_side)^2 where avg is the curve average over the bin; ndf = (number of non-empty bins in range) - nFitParam; returns chi2/ndf. Empty bins are excluded from chi2 and ndf. The doc itself warns that it is computed from a plot and not from the pdf. Default nFitParam = 0 (so it is not corrected unless you pass the number of floating parameters; `fitResult->floatParsFinal().getSize()`). [verified expb B4: chiSquare 1.0366 with nFit=5, 0.9847 with 0]
- The sub-range ndf is "non-empty bins whose centre lies in the curve range, minus nFitParam": legitimate only if all the floating parameters constrain those bins. [inferred]
- Statistical meaning of these pulls at low counts (second pass, section 5.4): with a correct model the RMS of the RooFit pulls is 0.49 at 0.3 counts/bin, 0.68 at 1, 0.84 at 3, 0.84 at 10, 0.88 at 30, 0.94 at 100 and 0.98 at 1000 counts/bin: the pulls are compressed, not N(0,1), because the error used is the asymmetric Poisson interval of the DATA bin on the side facing the curve. [verified: s405/lowcount.cpp -> lowcount.out]
- `getHist/getCurve(name)`: plain lookups by name (RooPlot.cxx:807-817).

## 3. Goodness of fit

### Definitions [inferred from standard statistics, cross-checked with source]
- Pearson: sum (n - mu)^2 / mu, sigma^2 = model. This is what `RooChi2Var` computes for unweighted data (Auto -> Expected), with mu = pdf(bin centre) * nexp * bin volume (centre rule, no bin integration), bins skipped if the pdf and the data are both zero. [source RooChi2Var.cxx:42-125]
- Neyman: sigma^2 = n; undefined for n = 0; biased for small counts. RooFit offers it as `DataError(SumW2)` in `RooChi2Var` (symmetric sqrt of sumw2), and Poisson (asymmetric, side chosen like in the pull). [doc RooAbsReal.cxx:4093-4108]
- Poisson deviance / likelihood-ratio: D = 2 sum [mu - n + n ln(n/mu)] (with 2 mu for n = 0); asymptotically chi2 with ndf = nbins - nfloat; the right statistic for small counts, and it is the saturated-model version of the likelihood actually minimised (see section 1, NLL).
- RooChi2Var details [source]: `normFactor` = expectedEvents for extended pdfs, sumEntries otherwise; range selection by bin centre; error 0 for an empty bin with SumW2/Poisson aborts with "bin has zero error" [doc RooChi2Var.cxx:34-42].
- `createChi2` / `chi2FitTo` exist in the legacy backend only (`#ifdef ROOFIT_LEGACY_EVAL_BACKEND`, FitHelpers.cxx createChi2); `chi2FitTo` options: DataError, Extended, Range, IntegrateBins, SumCoefRange, SplitRange [doc RooAbsReal.cxx:4066-4110]. Whether the ALICE ROOT build has the legacy backend: the project's DESIGN_HISTORY says `EvalBackend("legacy")` ran (slower), so yes [doc DESIGN_HISTORY.md]; second pass: `createChi2(dh, Extended(true))` runs in this build and returns exactly the centre-rule Pearson chi2 (ratio to my hand centre-rule Pearson 1.00000 in all 40 cells), so the legacy backend is present. [verified: s405/gof.cpp]
- ndf for the project's fits [inferred]: ndf = (number of histogram bins inside the fit range) - (number of floating parameters in `fitResult->floatParsFinal()`); bins are counted all (including empty) for Pearson and deviance, and the floating parameters include nsig and nbkg. For a sub-range the bin count is the bins whose centre lies in the range. p-value = `TMath::Prob(chi2, ndf)`, valid only if expected counts are large (Pearson) or roughly >= 5 per bin; for merged bins use the number of merged bins. The chi2 from the RooPlot has a different ndf (non-empty bins) and differs from Pearson at low counts.
- Numerical comparison of the statistics on real cells: section 5.1.

## 4. Audit of DrawFitOnPads (DynamicRooFitter.h:193-264) [source; items marked verified were run on toys]

1. No chi2, no p-value and no ndf anywhere on the canvas or in the log (`grep chiSquare|chi2` over Codes: only SpectrumExtrapolator and CorrelationTaskBase for the extrapolation fits). [source]
2. Pulls are never drawn in production: `PhiFitTask.h:129` and `PurityTask.h:327` call `DrawFit(TPad*)`, which passes `pullPad = nullptr`; the `DrawFit(RatioCanvas)` overload has no caller. [source grep]
3. The Model curve is normalised to the data count (default Relative) while Signal/Background use `RelativeExpected` (nexp). Equal for a converged full-range fit (verified on the toy: 8564 vs 8562 vs nexp 8562) but this is a coincidence of the fit, not a property of the code; in a sub-range fit they would disagree by 1/fraction (verified on the toy). [verified expb B4/B5]
4. The observable is mutated by `RooDataHist` (range moved to bin edges) in both `DoFit` and `DrawFitOnPads`; `CalculateIntegrationLimits`, the pull-pad lines (`obs->getMin()/getMax()`) and `frame` all see the adjusted range; the adjustment is silent (INFO level, killed by `RooFitMessages::Configure`). For phi the current config range [0.995,1.1] is not on bin edges. [source; log evidence line 21; the config-specific values are inferred]
5. `obs->getBins()` stays 100 whatever the histogram has (k0s: 200 bins). Second pass: it changes only the number of points of the plotted curve (125-148 at 100 bins, 206-233 at 200, 1006 at 1000), hence the curve polygon and the RooPlot chi2/pulls by up to 2% (K0S) and 7% (pi_tpc); fitted values and yields are bit-identical (section 5.5). It would corrupt `IntegrateBins` use. [verified: s405/obsbins.cpp]
6. `frame->GetMinimum()/GetMaximum()` are read right after `frame->Draw()` and used as y limits of the window lines on a log-y pad; second pass: inspected on the PNGs, the dotted window and sideband lines run the full height of the log-y pad and are drawn correctly (pngs/prod_*.png). [verified]
7. Data plotted with Auto errors: Poisson (asymmetric) for integer histograms, SumW2 for the online-efficiency histograms (INFO message only). Pulls then use those errors. [source]
8. The pull uses data errors, not model errors; for low-count bins the pull is not N(0,1) (RMS 0.68 at 1 count/bin, 0.88 at 30; section 5.4). [verified]
9. Second pass, from the PNGs (section 5.2): the frame title is the RooFit default `A RooPlot of "M(pi+pi-) (GeV/c^2)"` because `Title("")` is empty and RooFit then uses its own, printed above the plot and again, small, inside the pull pad; the pull axis is fixed at +-5 so bad fits show an almost empty pull pad (85-96% of the points off-scale); the total curve is normalised to the data count and the components to nexp (disagree by 2% on unconverged fits); no chi2/ndf, yields, purity or fit status on the canvas; the legend and the ALICE label overlap the peak or the curves. [verified: PNGs]


## 5. Experiments (second pass; all numbers from s405/ = `scratch/study0405/`)

### 5.1 Goodness of fit on 40 real cells [verified: s405/gof.cpp -> gof.tsv, gof-table.md; allcells.cpp -> allcells.tsv (960 cells), an-allcells.py -> allcells-analysis.txt]
Cells: 24 K0S, 8 xi, 8 pi_tpc, log-spaced in statistics. Columns: RooPlot chi2/ndf = `RooCurve::chiSquare(hist, nFloat)` on the curve drawn by the project's `DrawFit`; Pearson, Neyman, Poisson deviance by hand with exact bin integrals, ndf = bins in range - floating parameters (K0S 200-9, xi 72-9, pi_tpc 100-10); no cell had an empty bin, so the RooPlot ndf equals this ndf. `createChi2(Extended)` equals the centre-rule Pearson (ratio 1.00000). Centre-rule Pearson differs from the exact-bin one by 0.1% (K0S) and by -10% on average (xi, 0.83-0.97).

| kind (i,j) | N | st/cq | RooPlot chi2/ndf | Pearson/ndf | Neyman/ndf | Deviance/ndf | p(Pearson) | pull mean / RMS |
|---|---|---|---|---|---|---|---|---|
| k0s (9,1) | 5.79e+07 | 3/3 | 292 | 287 | 286 | 287 | 0 | -1.23 / 16.64 |
| k0s (8,7) | 4.4e+07 | 3/3 | 265 | 256 | 257 | 256 | 0 | -1.01 / 15.87 |
| k0s (1,5) | 3.35e+07 | 3/3 | 224 | 220 | 220 | 220 | 0 | -0.85 / 14.56 |
| k0s (2,2) | 2.55e+07 | 3/3 | 156 | 155 | 154 | 154 | 0 | -0.85 / 12.19 |
| k0s (5,16) | 1.93e+07 | 3/2 | 1753 | 1375 | 1739 | 1435 | 0 | -7.15 / 40.29 |
| k0s (4,14) | 1.5e+07 | 3/2 | 1126 | 1025 | 1132 | 1041 | 0 | -4.37 / 32.50 |
| k0s (2,20) | 1.13e+07 | 3/3 | 329 | 318 | 325 | 320 | 0 | -5.78 / 16.76 |
| k0s (2,21) | 8.59e+06 | 3/2 | 101 | 98.03 | 99.54 | 98.43 | 0 | -2.84 / 9.40 |
| k0s (4,22) | 6.51e+06 | 3/3 | 32.86 | 32.99 | 32.81 | 32.91 | 0 | -0.21 / 5.60 |
| k0s (9,21) | 5.03e+06 | 3/3 | 234 | 278 | 234 | 260 | 0 | -1.01 / 14.90 |
| k0s (6,24) | 3.63e+06 | 3/3 | 164 | 163 | 165 | 163 | 0 | -2.78 / 12.21 |
| k0s (2,25) | 2.93e+06 | 3/3 | 164 | 189 | 165 | 179 | 0 | -0.87 / 12.50 |
| k0s (0,0) | 2.18e+06 | 3/3 | 8.04 | 7.93 | 7.91 | 7.92 | 1.7e-204 | -0.14 / 2.77 |
| k0s (2,27) | 1.69e+06 | 3/3 | 91.02 | 104 | 91.15 | 98.42 | 0 | -0.62 / 9.30 |
| k0s (5,28) | 1.26e+06 | 3/3 | 5.22 | 5.15 | 5.21 | 5.16 | 9.1e-107 | -0.08 / 2.23 |
| k0s (6,28) | 9.54e+05 | 3/3 | 3.84 | 3.79 | 3.81 | 3.79 | 5.2e-63 | -0.07 / 1.91 |
| k0s (7,28) | 7.23e+05 | 3/2 | 9.54 | 9.89 | 10.40 | 10.04 | 1.3e-276 | -0.37 / 3.00 |
| k0s (3,30) | 5.54e+05 | 0/3 | 2.68 | 2.67 | 2.68 | 2.67 | 7.1e-31 | -0.05 / 1.60 |
| k0s (1,32) | 4.21e+05 | 3/2 | 2.14 | 2.13 | 2.13 | 2.13 | 1.1e-17 | -0.04 / 1.43 |
| k0s (2,33) | 3.21e+05 | 3/3 | 1.61 | 1.60 | 1.60 | 1.60 | 2.2e-07 | -0.04 / 1.24 |
| k0s (4,31) | 2.49e+05 | 3/3 | 1.85 | 1.84 | 1.85 | 1.84 | 1.3e-11 | -0.04 / 1.33 |
| k0s (9,31) | 1.78e+05 | 3/3 | 1.52 | 1.54 | 1.54 | 1.54 | 2.6e-06 | -0.03 / 1.21 |
| k0s (0,32) | 1.36e+05 | 0/3 | 1.26 | 1.24 | 1.30 | 1.26 | 0.013 | -0.02 / 1.10 |
| k0s (0,33) | 1.07e+05 | 0/3 | 1.31 | 1.37 | 1.36 | 1.36 | 0.00058 | -0.02 / 1.12 |
| xi (5,12) | 1.58e+06 | 3/3 | 574 | 523 | 571 | 534 | 0 | -2.17 / 22.31 |
| xi (3,7) | 7.46e+05 | 3/3 | 152 | 141 | 150 | 144 | 0 | -0.98 / 11.48 |
| xi (3,1) | 3.49e+05 | 3/3 | 7.58 | 7.50 | 7.52 | 7.50 | 5e-64 | -0.16 / 2.57 |
| xi (3,20) | 1.64e+05 | 0/3 | 20.87 | 20.41 | 20.75 | 20.47 | 1.7e-227 | -0.33 / 4.26 |
| xi (3,22) | 7.73e+04 | 0/3 | 6.99 | 6.85 | 6.90 | 6.86 | 2.3e-56 | -0.20 / 2.47 |
| xi (0,22) | 3.72e+04 | 0/3 | 4.06 | 4.01 | 4.08 | 4.03 | 1.4e-24 | -0.13 / 1.88 |
| xi (4,25) | 1.67e+04 | 0/3 | 0.93 | 1.03 | 1.06 | 1.03 | 0.4 | -0.04 / 0.90 |
| xi (9,25) | 8.09e+03 | 0/3 | 0.85 | 0.96 | 1.02 | 0.97 | 0.56 | -0.02 / 0.86 |
| pi_tpc (9,0) | 5.45e+09 | 3/2 | 2.77e+07 | 1.59e+07 | 7.45e+07 | 2.22e+07 | 0 | -3074.65 / 3936.41 |
| pi_tpc (3,4) | 1.68e+09 | 3/3 | 1.38e+05 | 1.48e+05 | 1.48e+05 | 1.43e+05 | 0 | -60.11 / 347.35 |
| pi_tpc (8,16) | 5.32e+08 | 3/2 | 3.24e+04 | 3.01e+04 | 3.22e+04 | 2.89e+04 | 0 | -20.21 / 169.61 |
| pi_tpc (5,22) | 1.62e+08 | 3/3 | 7121 | 7423 | 7190 | 6963 | 0 | -9.72 / 79.46 |
| pi_tpc (7,27) | 5.19e+07 | 3/3 | 4936 | 5184 | 4989 | 4924 | 0 | -10.99 / 65.74 |
| pi_tpc (5,31) | 1.58e+07 | 3/3 | 1.78e+04 | 1.55e+04 | 1.8e+04 | 1.49e+04 | 0 | -26.73 / 123.89 |
| pi_tpc (3,34) | 5.01e+06 | 0/2 | 6810 | 4067 | 6887 | 4316 | 0 | -16.86 / 76.45 |
| pi_tpc (0,35) | 1.53e+06 | 3/3 | 2170 | 1367 | 2206 | 1430 | 0 | -8.88 / 43.30 |

- RooPlot chi2 follows Neyman (33 of 40 within 3%), not Pearson: in misfit cells they differ by up to 27% (K0S (5,16)), 16% ((9,21)). Deviance and Pearson agree to 1.5% in K0S cells below 1e6.
- Versus statistics (covQual-3 cells of all 960): K0S median Pearson/ndf 1.9 (1e5-1e6 entries), 28 (1e6-1e7), 248 (1e7+); (Pearson/ndf - 1)/N = 3.0e-6 median (2.2-3.4e-6, 90 cells below 3e6), i.e. chi2/ndf = 1 + 3e-6 N: all 24 selected K0S cells (down to 1.07e5 entries) have p < 0.05; xi becomes acceptable only below ~2e4 entries (p = 0.40, 0.56 at 1.7e4 and 8.1e3); pi_tpc is 4e3-6e4 per ndf at every statistics (model cannot describe the n-sigma shoulders). The statistical error dS does not include this misfit (note 04, D4).
- Convergence diagnostic over the 960 cells: N_data/(nsig+nbkg) must be 1; deviates by >1% in 8.5% of K0S, 10.6% of pi_tpc, 0.8% of xi cells (note 04, D1).

### 5.2 PNGs inspected (Read tool) [verified: s405/png.cpp -> png.out; s405/pngs/prod_*.png (production DrawFit(TPad*)), pull_*.png (DrawFit(RatioCanvas)), misc_*.png]
Cells: K0S (2,33) good (chi2/ndf 1.6), K0S (0,33) low statistics (1.3), xi (9,25) very low statistics (0.85), K0S (5,16) bad (covQual 2, 1753), K0S (5,10) high statistics (390), pi_tpc (3,4), phi (6,2).
What a physicist sees: (i) good/low-statistics cells look fine, but the pulls of the good K0S cell show structure around the peak (shoulders +2 to +3, dip at the peak), RMS 1.24; (ii) in the bad K0S cell the model is a narrow peak on a straight line while the data have broad tails and a falling edge at 0.55; the pull pad shows 16 points of 200 (184 off the +-5 axis); (iii) K0S (5,10): 170 of 200 pulls off-scale; pi_tpc: 96 of 100, data shoulders at |n sigma| > 3 and the background hump are not described; phi (6,2): the model overshoots by ~30% left of the peak, 63 of 71 pulls off-scale, the legend entry "Signal" is struck through by the curves; (iv) the frame title `A RooPlot of "..."` appears (Title("") is not honoured) and a tiny copy sits inside the pull pad; (v) the "Work in progress" label overlaps the peak; (vi) the window (and sideband) lines are correct on the log axis; (vii) the pi_tpc total curve has a visible polygon kink (100 sample points).
Normalisation convention (total = data count, components = nexp): good cells agree (K0S (2,33): total 8775 vs signal+background 8776 at the centre), unconverged ones do not: K0S (5,16) N_data/(nsig+nbkg) = 1.0214, total 7.005e5 vs sum 6.858e5 at the centre; pi_tpc (3,4) 0.9812, 1.168e8 vs 1.191e8. For xi and phi the ratio of histogram integral to nexp (1.136, 2.436) only reflects the histogram being wider than the fit range. The plotted total therefore hides a failed normalisation of the fit.

### 5.3 Online-efficiency (weighted) phi [verified: s405/online.cpp -> online-warn.out, online-all.tsv, online-toy.tsv, misc-analysis.txt]
- The project's own `DoFit` on online cell (6,2) (histogram integral 5.9e8, entries 1.3e7) prints the "likelihood fit is requested of what appears to be weighted data" warning (once per fit); the offline (6,2) cell prints none.
- All 70 online cells: effective weight sum(err^2)/sum(content) = 14.2-65.0 (sqrt: 3.8-8.1). Of the 31 cells with covQual 3, SumW2Error error / default error on S = 5.3 median (3.0-202) and equals sqrt(w_eff) x 0.998 (median): the default Hesse errors in the project are understated by 4-8. 39 of 70 default fits have covQual 2 (errors then erratic, ratios from 0.001 to 26000). Fit values are identical for all three options.
- Weighted toys (constant effective weight, 200 toys, cells (6,2), (0,5), (3,3), w = 44.8, 16.9, 29.4; truth = fit of the real cell): median reported dS / robust spread of S: default 0.117, 0.257, 0.162 (true error 4-9 times larger); SumW2Error 0.79, 1.06, 0.88; AsymptoticError 0.88, 1.13, 0.89; robust pull width 8.9, 4.0, 6.2 (default) vs 1.14, 0.91, 1.09 (SumW2). covQual 3 in only 14-55% of toys.
- Verdict: yes, the fitted errors of the online phi yields are wrong without SumW2Error/AsymptoticError (too small by sqrt(w_eff)); the histogram sideband error (from sumw2) is correct, so the bkg ratio error mixes the two.
- Third pass, toys with the REAL per-bin weights [verified: s405/online.cpp mode `toyreal` -> online-toyreal.out, online-toyreal.tsv, online-toyreal-analysis.txt; 200 toys per cell]: the online TH3 carries Sumw2 (GetSumw2N = 21816 = all cells incl. flow). Per bin, w_b = sumw2_b/sumw_b; toys draw n_b ~ Poisson(mu_b/w_b) and fill content n_b w_b, error sqrt(n_b) w_b, which reproduces the mean and the variance of every bin. Inside the fit range w_b varies little: 42.99-45.50 (cell (6,2), global 44.82), 16.73-17.07 ((0,5), 16.93), 28.65-29.46 ((3,3), 29.44). Median reported dS / robust spread of S: default 0.105, 0.247, 0.157 for (6,2), (0,5), (3,3) (true error 4-10 times larger), robust pull width 10.2, 4.3, 6.6. With SumW2Error / AsymptoticError the median ratio over all toys is dominated by covQual-2 fits (SumW2Error 12.8, 1.02, 1.88; AsymptoticError 0.76, 1.09, 0.85; covQual 3 in only 20-60% of the toys); over the covQual-3 toys: SumW2Error 1.10, 1.09, 0.87 and AsymptoticError 0.83, 1.15, 0.75 (default 0.12, 0.27, 0.14). Same conclusion as with a constant weight: default errors are wrong by sqrt(w); the corrected ones are right to 10-25% when the fit converged, and unusable when it did not.

### 5.4 Low-count toy [verified: s405/lowcount.cpp -> lowcount.out; 200 toys per row, Gaussian + Chebychev1, 100 bins, project-style extended fit]
| counts/bin | empty bins | RooFit pull RMS | RooPlot chi2/ndf | Pearson/ndf | Deviance/ndf | p<0.05: Pearson / Deviance / RooPlot |
|---|---|---|---|---|---|---|
| 0.3 | 76% | 0.49 | 1.02 | 1.00 | 0.76 | 5.5% / 0% / 0% |
| 1 | 42% | 0.68 | 0.49 | 0.99 | 1.09 | 3.5% / 6.5% / 0% |
| 3 | 10% | 0.84 | 0.47 | 0.99 | 1.11 | 3.5% / 17% / 0% |
| 10 | 0.1% | 0.84 | 0.72 | 1.00 | 1.02 | 4% / 6.5% / 0% |
| 30 | 0 | 0.88 | 0.83 | 0.99 | 1.00 | 3.5% / 4% / 0% |
| 100 | 0 | 0.94 | 0.94 | 1.02 | 1.03 | 7.5% / 8.5% / 2% |
| 1000 | 0 | 0.98 | 1.01 | 1.01 | 1.01 | 4.5% / 5% / 5% |
Pearson/ndf with all bins is the usable statistic at every statistics; the Poisson deviance is too liberal at ~3 counts/bin; RooPlot chi2/ndf (data errors, empty bins dropped) is biased low below ~100 counts/bin and almost never rejects (0% at <= 30 counts/bin).

### 5.5 Observable bins (100 vs 200)
No effect on fits/yields; plot-only effects. Details and numbers: section 4 item 5 and note 04 "Verdict on the observable's bin count".

### 5.6 Fit range actually used [verified: s405/rng.cpp -> rng.out]
After `DoFit` the observable range is: K0S [0.4499999881,0.5500000119] (200 bins, as configured); xi [1.2890000159,1.3609999901] (72 bins) although the config says [1.29,1.36]: the histogram axis edges are float-rounded (xmin 1.2000000477), so 1.29 is not exactly an edge and RooDataHist expands outward by one bin on each side (+2 bins, 1 MeV each); pi_tpc [-10,10]; phi [0.9944999987,1.1010000241] (71 bins; config [0.995,1.1] is not on bin edges). The fit range is thus not what the JSON says for xi and phi, silently. The yields are normalised to the expanded range.

### 5.7 VisualizeError (linear vs sampling), createHistogram, RooHistFunc (third pass) [verified: s405/visband.cpp -> visband.out, pngs/visband-k0s-*.png]
- Band half-width of the total curve (Z = 1), against `getPropagatedError` of the normalised pdf x N x bin width at the same x:
  - converged K0S (2,33) (status 3, covQual 3): peak 20.19 (propagated) / 20.20 (linear band) / 22.46 (sampling band, 100 curves), i.e. 0.227% / 0.227% / 0.253% of the curve; at +8 MeV 13.85 / 14.07 / 13.53; at 0.46 2.30 / 2.30 / 2.33. Cell (0,33) (status 0): peak 0.392% / 0.393% / 0.436%. The linear band IS the propagated error pointwise (same secant formula); the sampling band agrees to about 10% at the peak and 2-4% elsewhere; its centre is shifted by up to 0.7%.
  - non-converged K0S (5,16) and (3,20) (status 3, covQual 2): the linear band is huge (peak 4.9% and 2.75%; tails up to 9% and 8%; visible as a wide orange band in pngs/visband-k0s-5-16.png, the 5-sigma signal band spans 0 to 3e3 events per bin in the right tail), while the sampling band is 47 and 95 times narrower (0.10%, 0.03% at the peak). The two methods disagree by one to two orders of magnitude exactly when the covariance is bad (the linear one inherits the clipped/huge +-1 sigma secants of parameters at limits; the sampling one draws from the forced-positive covariance).
  - Relation to the error on S: the band is a pointwise uncertainty of the shape-normalised curve (the default normalisation scales it to the data count), not an uncertainty of the window yield; at the peak of the converged cells its relative size (0.23%, 0.39%) happens to be close to dS/S (0.22%, 0.38%), but it cannot be read as dS. Useful only as a visual diagnostic: a linear band much wider than the data errors flags a bad covariance (the cells where note 04 finds dS meaningless).
- `createHistogram` for storing the fitted model as a TH1: `model->createHistogram("h", *obs)` returns a histogram with the OBSERVABLE's binning (100 bins, not the data's 200) whose contents sum to 320815 = nexp (centre value x bin width, extended pdf). With `Binning(200, obs->getMin(), obs->getMax()), Extended(true)` it has the data binning, sum 320812.1 vs nexp 320811.7 and the exact bin-integral sum 320811.7; per bin it differs from the exact bin integral by at most 6.5e-4 (the centre rule). A signal pdf alone gives a unit-normalised histogram (sum 1.000002). So the fitted model can be stored as a TH1 with one call, provided `Binning` is given explicitly; for exact bin contents use per-bin `createIntegral` (as in s405/gof.cpp).
- `RooHistFunc(obs, datahist, 0)` returns the bin CONTENT (8888 at x = 0.4997, equal to the histogram), not a density (the density would be 1.78e7): a RooHistFunc is a lookup of counts, a RooHistPdf (section 1) a normalised density. Neither is used by the project; RooHistFunc would matter only to build a template model from a stored histogram.

### 5.8 GOF below 1e5 entries: synthetic toys from real K0S models (third pass) [verified: s405/gofsynth.cpp -> gofsynth-2-33.tsv, gofsynth-0-4.tsv; s405/an-gofsynth.py -> gofsynth-analysis.txt]
Truth = the project's fit of K0S (2,33) (DSCB + Chebychev1, purity of the totals 0.86) and (0,4) (DSCB + Chebychev2 override, 0.95); toys = Poisson of the exact bin integrals at N = 1e3, 1e4, 1e5 (5, 50, 500 counts per bin on average over 200 bins), 1000 toys per point; fit with the project's DoFit, RooPlot chi2 from the curves of the project's DrawFit with nFloat; hand statistics with exact bin integrals, ndf = 200 - nFloat (9 or 10). Medians (means are spoiled by the 1-3% runaway fits) and rejection rates at p < 0.05 (nominal 5%):

| truth, N | empty bins (median) | RooPlot chi2/ndf | Pearson/ndf | Neyman/ndf | Deviance/ndf | merged Pearson (mu>=5) | pull RMS | rejected: Pearson / Deviance / Neyman / RooPlot / merged |
|---|---|---|---|---|---|---|---|---|
| (2,33), 1e3 | 52 | 0.56 | 1.00 | 0.76 | 1.10 | 1.00 | 0.76 | 4.4% / 21% / 1.2% / 0.0% / 5.0% |
| (2,33), 1e4 | 0 | 0.80 | 1.00 | 1.22 | 1.01 | 1.00 | 0.88 | 5.0% / 7.4% / 60% / 0.6% / 5.0% |
| (2,33), 1e5 | 0 | 0.96 | 1.00 | 1.02 | 1.00 | 1.00 | 0.96 | 8.0% / 8.4% / 10% / 4.1% / 8.0% |
| (0,4), 1e3 | 109 | 0.74 | 0.98 | 0.82 | 0.87 | 1.04 | 0.62 | 5.1% / 0.5% / 2.8% / 0.2% / 7.6% |
| (0,4), 1e4 | 15 | 0.63 | 1.00 | 1.03 | 1.08 | 1.00 | 0.86 | 5.2% / 19% / 15% / 0.0% / 5.5% |
| (0,4), 1e5 | 0 | 0.89 | 1.00 | 1.07 | 1.01 | 1.00 | 0.92 | 5.3% / 6.5% / 23% / 0.2% / 5.3% |

- Pearson over all bins with the proper ndf is unbiased (median 0.98-1.00) and has the nominal rejection rate (4.4-5.3%) at every statistics; the 8% at (2,33), 1e5 comes from the 2.7% of toys whose fit ran away (Pearson/ndf > 2): there the test correctly rejects a failed fit.
- The Poisson deviance is not chi2-distributed at a few counts per bin (rejects 19-21% when many bins have 1-10 counts, 0.5% when most are below 1); the Neyman statistic is unusable; `RooPlot::chiSquare` (data errors, empty bins dropped) is biased low (0.56-0.96) and almost never rejects. Pearson with bins merged to mu >= 5 behaves like the plain Pearson but loses ndf (49-81 merged bins at 1e3).
- Together with section 5.1 (real cells 1e5-5e9 entries, where Pearson/ndf = 1 + 3e-6 N for K0S): the recommendation "Pearson with exact bin integrals, all bins, ndf = bins - floating parameters" holds over the whole range 1e3 to 5e9 entries.

## 6. Recommendations (recommendations only; none applied)
1. After building the RooDataHist read back `obs->getMin()/getMax()` and log if they differ from the config (xi: +2 bins; phi: +2 bins); or choose ranges on bin edges allowing for float rounding.
2. `IntegrateBins` stays off (note 04); if used, `obs->setBins(round(range/binwidth))` after the import (K0S 200, xi 72, phi 71, pi_tpc 100).
3. Online-efficiency phi: pass `SumW2Error(true)` (or `AsymptoticError(true)`) to `fitTo`; the default errors are 4-8 times too small. Fit values do not change.
4. If a chi2 is shown: Pearson/ndf with exact bin integrals, ndf = bins - floating parameters (unbiased with the nominal 5% rejection from 1e3 entries up, section 5.8); not `RooPlot::chiSquare` (Neyman-like, low-biased below ~100 counts/bin), not the deviance at a few counts per bin. Expect chi2/ndf = 1 + 3e-6 N for K0S: the misfit, not the statistics, dominates above 1e5 entries.
5. If pulls are drawn: widen or auto-scale the +-5 axis (85-96% of points are off-scale in bad fits), suppress the default title, add chi2/ndf, S, purity, status and N_data/(nsig+nbkg) to the canvas, and draw the components with the same normalisation as the total (or use `Normalization(1, RooAbsReal::NumEvent)` consistently).
6. Keep full-range fits or fix component normalisation before introducing `Range("fit")`. [verified on toy, first pass]

## Pitfalls
- `RooRealVar(..., 0.4, 0.1, 1)` with an int literal is ambiguous (use 1.0). [verified compile]
- `getVal(RooArgSet(x))` with a temporary set throws FATAL; build the RooArgSet first. [verified]
- `sumEntries("", range)` with an empty cut string throws; pass nullptr. [verified]
- The `RooPlot` is not in the pad's primitive list after `DrawFit`; get curves by name ("Model_Plot", "Data_Plot") from the pad primitives and use `RooCurve::chiSquare(const RooHist&, nFit)` / `RooHist::makeResidHist`. [verified]
- `SetLogy()` before drawing a frame whose minimum is <= 0 (band polygons) fails ("Cannot set Y axis to log scale").
- Interactive `root` fails without the environment file; compiled programs work.

## Resume here
Done: items 6-9 of the brief (second pass) and, in the third pass, the per-bin-weight online toys (5.3), VisualizeError linear vs sampling, createHistogram and RooHistFunc (5.7), and the synthetic GOF below 1e5 entries (5.8). Nothing from the brief is open. Known limits, not planned: the online toys reproduce each bin's mean and variance (n_b ~ Poisson with weight sumw2/sumw), not the per-candidate weight distribution; the 960-cell chi2 table is summarised by decade of N only.

Scratch: `scratch/study0405/` (programs gof, gofsynth, png, online, lowcount, misc, visband, rng, obsbins; outputs gof.tsv, gofsynth-*.tsv, png.out, online-*.tsv, lowcount.out, misc.out, visband.out, pngs/). Rebuild with `./build.sh X.cpp` after `source scratch/rooenv.sh`.
