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
| `reference/` | Snapshots of the result, in git: the overlay and the table as `run.sh` printed them, one per date (20 October 2026, the π extrapolated with the Lévy-Tsallis over 0.2-1.0; 21 October, with the blast-wave over 0.2-3.0). |
| `extrapolation/` | The study of the π extrapolation that the comparison led to, and the checks of what was changed in the framework because of it (its README). |

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
offline-efficiency 2024 result, |Δy| < 1.0, the extrapolated yields (main README, *The extrapolated yield*).

The comparison has been made twice, and the two are kept apart: what was found on 20 October, as it was found,
and what changed on 21 October because of it. The old side is the same remake both times.

## 20 October 2026: the original comparison

The framework at commit `a3f6899`, the π extrapolated with the Lévy-Tsallis fitted over 0.2-1.0 GeV/c, its
default then (`reference/overlay_2026-10-20.png`, `reference/table_2026-10-20.txt`).

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

### The differences between the two analyses

Everything found so far that differs between the old macro and the framework's 2024 result, with what each
does to the 2024/old ratio where it was measured (20 October 2026, 2024 offline, |Δy| < 1.0). The ratio is
0.947 on average (0.92-0.98 per class); the aim is to account for the 5 %.

**Measured:**

| Difference | Old macro | Framework 2024 | Effect on 2024/old |
|---|---|---|---|
| π extrapolation above the data | none: `YieldMean` is called with `max = 1.0`, below the last edge (3.0), so it drops the high part | the Lévy-Tsallis fitted in 0.2-1.0 GeV/c (`fit_range`) and integrated from 3 to 10 GeV/c (`domain_range` `[0, 10]`), where no data constrain it | removing it lowers the π yield by 0.4 % (70-100 %) to 3.5 % (1-5 %) and raises the ratio as much: mean 0.947 → 0.966. **The largest single difference found.** |
| K0S extrapolation above the data | none (`max = 6.0`, the last edge) | from 6.4 to 10 GeV/c | 0.1 % of the K0S yield in every class: negligible |
| How the yield is assembled | sum of the measured bins + `YieldMean`'s `kExtra` (equal to `kYield`) | measured bins + model outside them (`SpectrumExtrapolator`, the same scheme) | none: the same quantity (main README, *The extrapolated yield*) |
| Normalisation | per event with a φ candidate, purity-corrected | per trigger | none: it cancels in the ratio |
| Multiplicity classes, \|Δy\| window | 0-1 … 70-100 %, \|Δy\| < 1.0 | the same | none (`RequireSameAxis` checked the classes) |

**Not measured yet**, each able to move the ratio as a whole:

| Difference | Old macro | Framework 2024 |
|---|---|---|
| K0S at low pT | first edge 0.1 GeV/c, 0-0.1 extrapolated | first bin 0-0.3 measured, nothing extrapolated |
| K0S upper edge of the data | 6.0 GeV/c | 6.4 GeV/c |
| K0S efficiency | multiplicity-integrated, per Δy window | multiplicity-integrated (`acceptance_efficiency.mult: integrated`); the MC production behind it not compared |
| Signal loss | per class, always taken at \|Δy\| < 1.0 | applied (`signal_loss: true`); how it is binned not compared |
| π reconstruction | TPC below 0.5 GeV/c (tracking efficiency), TOF above (tracking × matching), the 0.5-0.6 bin a weighted mean, the 0.15-0.2 bin dropped | the framework's π spectrum from 0.2 GeV/c; the PID scheme not compared |
| Event loss | not applied | applied (`event_loss: true`); per class, so it would cancel in K0S/π if equal for both |
| Data | the productions of the old analysis (which train is not recorded in its inputs; the systematics files name trains 19977-25965) | 2024 |

**Open, about the errors.** The old statistical errors are 3-4 times smaller than the 2024 ones (≈0.0004
against ≈0.0015). Unless the old sample was much larger they are suspect: `YieldMean`'s resampling error of the
total yield may leave out the errors of the efficiencies and the correlation between the K0S and π yields.
The old systematic band includes an unlabelled 6 % (line 1100 of the macro); it does not move the central
values.

**Reading.** A shift the same in every class points to a factor (an efficiency, a normalisation that does not
cancel) rather than to a multiplicity effect. The π tail is the exception: it grows with multiplicity, since
the spectra harden, but here it is the model's tail and not the data that says so. Whether the framework
should extrapolate the π above 3 GeV/c from a fit that stops at 1 GeV/c is a choice for the framework's
results, not for this comparison: no extrapolation above the data, a `fit_range` reaching the last measured
edge, or the tail as a systematic.

How the "measured" rows were obtained: the 2024 file remade with `domain_range` up to the last measured edge
(6.4 for the K0S, 3.0 for the π) and every other setting unchanged, and the extrapolated trends of the two
species compared bin by bin; with `trend_composition` switched, before it was removed, for the assembly row.

## 21 October 2026: after the π extrapolation changed

What the comparison of 20 October led to, in the framework: the π default became a blast-wave over 0.2-3.0 GeV/c,
with a second fit of the high side available as the alternative (commit `d0b4b52`; the study, its figures and
checks in `extrapolation/`), and the statistical errors of the extrapolated yields stopped depending on the
spectrum extrapolated before them (`25fe5fb`, a shift of at most 1e-5 relative). Everything else unchanged. The
framework at commit `25fe5fb` (`reference/overlay_2026-10-21.png`, `reference/table_2026-10-21.txt`):

| class (%) | 2024 (extrap., stat) | old (stat) [syst] | 2024 / old | on 20 October |
|---|---|---|---|---|
| 0-1 | 0.1199 ± 0.0023 | 0.1232 ± 0.0005 [0.0085] | 0.974 ± 0.019 | 0.929 |
| 1-5 | 0.1226 ± 0.0013 | 0.1234 ± 0.0004 [0.0085] | 0.994 ± 0.011 | 0.955 |
| 5-10 | 0.1197 ± 0.0013 | 0.1239 ± 0.0005 [0.0086] | 0.966 ± 0.011 | 0.936 |
| 10-15 | 0.1195 ± 0.0015 | 0.1215 ± 0.0003 [0.0084] | 0.984 ± 0.013 | 0.959 |
| 15-20 | 0.1134 ± 0.0016 | 0.1210 ± 0.0003 [0.0084] | 0.938 ± 0.013 | 0.921 |
| 20-30 | 0.1176 ± 0.0013 | 0.1212 ± 0.0003 [0.0084] | 0.970 ± 0.011 | 0.955 |
| 30-40 | 0.1184 ± 0.0015 | 0.1208 ± 0.0003 [0.0084] | 0.980 ± 0.013 | 0.962 |
| 40-50 | 0.1201 ± 0.0018 | 0.1209 ± 0.0005 [0.0085] | 0.993 ± 0.016 | 0.976 |
| 50-70 | 0.1148 ± 0.0016 | 0.1207 ± 0.0003 [0.0084] | 0.951 ± 0.014 | 0.934 |
| 70-100 | 0.1213 ± 0.0020 | 0.1268 ± 0.0005 [0.0089] | 0.956 ± 0.017 | 0.941 |

Mean 0.971 (0.947 on 20 October): 1 within the statistical error in two classes (1-5 and 40-50 %), and within
the old systematic band in all.

**What this settles of the differences above.** The π extrapolation above the data, the largest of them, was the
model's tail and not the data: fitted to the data up to 3 GeV/c, by the blast-wave or by a second fit of the high
side alone, the tail is 0.1-0.3 % of the yield, and the ratio moves from 0.947 to 0.971 (0.966 with no high-pT
extrapolation at all; the extra 0.5 % comes from the blast-wave's low-pT side). About 3 % remains, the same in
every class, and every row of *Not measured yet* stays open; so do the old statistical errors.

## Removing it

`git rm -r OldComparison`, delete the `OldComparison/` lines in `.gitignore`, commit. The data in
`OldComparison/data` are a copy; the originals are in `~/Downloads/OldAnalysis`.
