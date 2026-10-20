# OldComparison: the 2024 K0S/π ratio against the pre-framework one

A self-contained corner of the repository, kept apart so that it can be removed in one step
(`git rm -r OldComparison`, and the two `OldComparison/` lines of `.gitignore`). Nothing outside it
depends on it.

**What it does.** It remakes the K0S/π ratio of the analysis done before this framework existed,
with the macro of the time (`OldCodes/ratioK0SPiCorrectedUpdated.C`) on its own inputs, and divides
the framework's 2024 ratio by it with `comparison_task`.

```bash
enterO2P                                      # ROOT in the environment
cd Codes && ./runAnalysis.sh data pp --compile && cd ..   # a current 2024 result (see below)
OldComparison/run.sh
```

It writes `OldComparison/output/`: `oldK0SPiRatio.root` (the old ratio), `Old_Comparisons.root` (the
division), `overlay.png`, and the logs (the chain also leaves its own in
`Logs/log_output_globalConfigComparisonOld.log`). The table goes to the screen, headed by when and from which
commit the 2024 file was produced: the comparison is only as current as that file. The `DataFile/`
outputs of an older run (before the current layout of 19 October 2026) give a wrong or missing
numerator, so run the chain first.

| | |
|---|---|
| `macro/ratioK0SPiCorrectedUpdated_export.C` | The old macro, made to export its result (its header says exactly what changed). |
| `macro/YieldMean.C` | The extrapolation code the old macro includes, as it was then (not `OldCodes/YieldMean.h`, which has another signature and four fixed defects). |
| `macro/show.C` | The table (checked bin by bin against the division) and the overlay, with the old systematic band. |
| `globalConfigComparisonOld.json` | The `comparison_task` block: one declared pair, the 2024 ratio over the old one with its statistical errors. The only place the numerator's file and object are written: `run.sh` reads them from here. |
| `run.sh` | All of it, in order. |
| `data/` | The old inputs (not in git): copied from `~/Downloads/OldAnalysis` on 20 October 2026. |
| `output/` | What `run.sh` makes (not in git). |
| `reference/` | A snapshot of the result of 20 October 2026, in git: the overlay and the table, as `run.sh` printed them. |

## The old result

The old macro (`OldCodes/ratioK0SPiCorrectedUpdated.C`, mode 0) computes 2·K0S/(π⁺+π⁻) in the ten
FT0M classes 0-1, 1-5, 5-10, 10-15, 15-20, 20-30, 30-40, 40-50, 50-70, 70-100 %, for |Δy| < 1.0 only. From `data/ResultsData2.root` (spectra per Δy window and class,
normalised per event with a φ candidate, purity-corrected) and `data/ResultsEfficiency2.root`:

- **K0S**: divided by the multiplicity-integrated efficiency of the window (the per-class one is loaded and
  not used), then by the per-class signal loss taken at |Δy| < 1.0 whatever the window.
- **π**: TPC below 0.5 GeV/c with the tracking efficiency, TOF above with tracking × matching, the
  0.5-0.6 bin a weighted mean of the two, the 0.15-0.2 bin dropped; then the signal loss as for the K0S.
- **Yields**: measured part plus a Lévy-Tsallis extrapolation to pT = 0 (`YieldMean`), K0S over 0-6 GeV/c,
  π over 0-3; no extrapolation at high pT. The statistical error is `YieldMean`'s resampling error of
  the total yield.
- **Systematics**: K0S (DCA of the daughters to the PV, nσ TPC, nσ of the fit), π (DCAxy, DCAz, nσ of the
  fit, low-pT extrapolation), correlated (TPC clusters), smoothed and summed in quadrature, **plus a
  hard-coded 6 % with no label** (line 1100). Each source is one variation (tight or loose).
- No branching-ratio or feed-down correction in the macro (a branching ratio could only be in the MC
  efficiency; not checked). The per-φ-event normalisation cancels in the ratio.

The macro of the time saved the ratio only as unnamed graphs inside a canvas, against hard-coded ⟨dN/dη⟩
values (28.83 … 7.19), to a path on another machine (`/Users/stefanocannito/Downloads/updateK0SPi2.root`),
which is not available. Hence the remake. Point i of its graphs is class i of the list above, which is how
the exported histograms carry it. `oldK0SPiRatio.root` holds those two histograms
(`Ratio_K0S_Pi_dy1_00_stat`, `_syst`) and the two graphs against ⟨dN/dη⟩ as the macro built them.

**What the export changes**, and nothing else: the include of `CustomTF1.h` points to `OldCodes`, the include
of `Plot.h` (not available, used only by the model section) is dropped, the function is renamed; an
`[export]` block after the ratio is built writes it out, for |Δy| < 1.0; the model, Run 2, final-canvas and MC-closure
sections are removed (their inputs are not available). Lines 1-1136 of the original, the whole computation,
are otherwise the same. Not checkable: whether `macro/YieldMean.C` is exactly the version used then.

## The comparison

`comparison_task` in its declared mode (`objects`): the denominator is not a product of the framework, so
only the axes are checked (`RequireSameAxis`; the classes coincide), with `divide_option: ""` (independent
samples). The old systematic uncertainty is shown as a band in the overlay and as a column in the table, not
divided: a ratio carrying 2024's statistical and the old systematic error would be neither. Numerator: `Ratio_K0S_Pi_Extrap_dy1_00` of `Data_OfflineEff_2024_PhiAssocSpectra.root`, i.e. the
offline-efficiency 2024 result, |Δy| < 1.0, the extrapolated yields (see *The extrapolation* below).

**Result of 20 October 2026** (2024 file produced at commit `a3f6899`):

| class (%) | 2024 (extrap., stat) | old (stat) [syst] | 2024 / old |
|---|---|---|---|
| 0-1 | 0.1145 ± 0.0022 | 0.1232 ± 0.0005 [0.0085] | 0.929 ± 0.018 |
| 1-5 | 0.1178 ± 0.0013 | 0.1234 ± 0.0004 [0.0085] | 0.955 ± 0.011 |
| 5-10 | 0.1159 ± 0.0013 | 0.1239 ± 0.0005 [0.0086] | 0.936 ± 0.011 |
| 10-15 | 0.1165 ± 0.0015 | 0.1215 ± 0.0003 [0.0084] | 0.959 ± 0.012 |
| 15-20 | 0.1114 ± 0.0016 | 0.1210 ± 0.0003 [0.0084] | 0.921 ± 0.013 |
| 20-30 | 0.1157 ± 0.0013 | 0.1212 ± 0.0003 [0.0084] | 0.955 ± 0.011 |
| 30-40 | 0.1162 ± 0.0015 | 0.1208 ± 0.0003 [0.0084] | 0.962 ± 0.013 |
| 40-50 | 0.1179 ± 0.0018 | 0.1209 ± 0.0005 [0.0085] | 0.976 ± 0.015 |
| 50-70 | 0.1128 ± 0.0016 | 0.1207 ± 0.0003 [0.0084] | 0.934 ± 0.014 |
| 70-100 | 0.1193 ± 0.0020 | 0.1268 ± 0.0005 [0.0089] | 0.941 ± 0.016 |

The 2024 ratio is about 5 % below the old one in every class (0.92-0.98), within or at the edge of the old
systematic band (about ±7 %); both are flat with multiplicity.

**The extrapolation.** Both compute the yield the same way (the main README, *The extrapolated yield*; the
old macro's hand-made sum of the bins plus `YieldMean`'s `kExtra` is `YieldMean`'s `kYield`). Where they
extrapolate differs:

| | K0S | π |
|---|---|---|
| old macro | below 0.1 GeV/c (first edge 0.1); nothing above 6.0, the last edge (`max = 6.0`) | below 0.2; nothing above 3.0 (`max = 1.0`, below the last edge, so `YieldMean` drops the high part) |
| framework 2024 | nothing below (its binning starts at 0); above 6.4 up to 10 GeV/c (`domain_range` `[0, 10]`) | below 0.2; above 3.0 up to 10 GeV/c |

Measured on 20 October with the 2024 file remade without the high-pT part (`domain_range` up to the last
measured edge, 6.4 for the K0S, 3.0 for the π): the mean 2024/old ratio goes from 0.947 to 0.966, per class
from 0.92-0.98 to 0.94-0.99. The high-pT tail explains about 2 of the 5 %; about 3.5 % remains.

**Before reading the rest as physics:**

- **The old statistical errors are 3-4 times smaller** than the 2024 ones (≈0.0004 against ≈0.0015). Unless
  the old sample was much larger, they are suspect: `YieldMean`'s resampling error of the total yield may
  leave out the errors of the efficiencies and the correlation between the K0S and π yields.
- **Different choices**, any of which can move the ratio as a whole: the K0S efficiency integrated in
  multiplicity and the signal loss always at |Δy| < 1.0; the TPC/TOF combination of the π; the unlabelled
  6 % in the old systematics; and different data-taking years.
- A shift the same in every class points to a factor (a normalisation, an efficiency) rather than to a
  multiplicity effect.

## Removing it

`git rm -r OldComparison`, delete the two `OldComparison/` lines in `.gitignore`, commit. The data in
`OldComparison/data` are a copy; the originals are in `~/Downloads/OldAnalysis`.
