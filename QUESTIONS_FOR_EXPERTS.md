# Questions for experts

Physics and method questions this analysis has met and could not answer by itself, collected to be put to someone
who knows. Each one says what it is about, the options with the numbers measured so far, what the answer would
change, and what the code does meanwhile (always the conservative choice, with a pointer back here). An answered
question moves to `DESIGN_HISTORY.md` with the answer and who gave it, and the code is changed to match.

---

## The uncertainty of the trigger count

**Opened 27 October 2026.** `h1NormalisationRelError` (`correlation_task`, `correlation_wpdg_task`).

**Context.** The result is a per-trigger yield, Y = P / N: the φ-associated pairs P (after the sideband subtraction
and the efficiency correction, per bin of trigger pT) divided by the number of φ triggers N. N is the sum over the
trigger-pT bins of the φ signal from an invariant-mass fit, divided by the φ efficiency. The fit gives N an error
σ_N, and the question is how much of it belongs to the error of Y.

σ_N has two parts:

1. **The Poisson fluctuation of the number of φ**, about √S on the raw signal S. If there happen to be 1% more φ,
   there are also about 1% more pairs, since each φ brings its own associated particles: P and N move together and
   Y does not. This part is already, in effect, in the statistical error of P.
2. **The uncertainty of the signal extraction**, from the background under the peak. A fit that puts S 1% too high
   does not move P the same way, so Y moves: a genuine normalisation uncertainty. It is not fully independent of P
   either: the same fit gives the background fraction used to subtract the sideband pairs.

**Numbers** (2026 HighIR production of 22 October, offline efficiency, summed over trigger pT):

| multiplicity | raw φ signal | fit error | Poisson part 1/√S |
|---|---|---|---|
| 0-1% | 3.0e6 | 0.101% | 0.057% |
| 20-30% | 1.5e7 | 0.043% | 0.026% |
| 70-100% | 1.3e7 | 0.043% | 0.028% |

The Poisson part is about a third of the variance. The normalisation uncertainty (with the event loss, negligible
here) is 0.04-0.10% per class; it matters only for the π trends, whose statistical error is of the same order
(0.05-0.11%, becoming 0.07-0.14% with it). For the K0S it is at most 0.01% (absolute, on 0.4-0.7%), for Ξ and Ω
nothing. In the ratios between species and in extrapolated/measured it cancels exactly.

**The question.** Which part of σ_N should be propagated to Y?

- **Only part 2**, √(σ_N² − S) per trigger-pT bin. Correct if part 1 is indeed contained in the error of P. It needs
  S to be a plain count: with the efficiency applied online the fitted histogram is weighted and √S is not its
  Poisson error. On the WPDG path (MC closure, a plain count of true φ, no fit) it would propagate nothing.
- **All of σ_N**, as independent of P. Simpler and larger: at 0-1% 0.101% instead of about 0.083%. On the WPDG path
  it propagates the whole Poisson error of the count, which there is entirely of part 1.
- Or something else: a treatment of the correlation between N and P (and the background fraction) proper.

**Meanwhile.** All of σ_N (`CorrelationTaskBase::CountTriggers`), written per multiplicity interval in
`h1NormalisationRelError`, next to the trends and not in their errors.

---

## The uncertainty of the background ratio, and how it goes with the trigger count

**Opened 27 October 2026.** `CorrelationCalculator` (the sideband subtraction), `h2TriggerBkgRatio`.

**Context.** The pairs of the φ signal region contain φ-background candidates. They are subtracted with the pairs of
the sidebands scaled by R = B(signal region) / B(sidebands), per (multiplicity, trigger pT) cell: Y ∝ P_SR − R·P_SB.
R comes from the same invariant-mass fit as the trigger count N (the previous question), with an error σ_R that
`PhiFitTask` computes and writes; the subtraction uses R alone, so σ_R goes nowhere.

**Numbers** (2026 HighIR, offline efficiency; `DESIGN_NOTES.md` section 5 has the method). σ_R/R is 0.06-0.72% per
cell; the background is 36-77% of the signal region. Shifting R by σ_R moves the extrapolated trends by 0.15-0.17%
at 0-1% and 0.02-0.10% in the other classes, almost the same for every species of a class, so it cancels in the
ratios between species (≤ 0.05%). It matters against the statistical error for the π trends only (0.16% against
0.09% at 0-1%), as the trigger-count error does (0.10% there).

**The questions.**
- Is σ_R a statistical uncertainty of the yield, to be propagated as one per cell (100% correlated across the Δy
  and associated-pT bins of the cell, independent between cells), or a systematic, an R ± σ_R variation?
- N and R come from one fit, where signal and background are anticorrelated. Should the two be propagated together,
  with the covariance of the fit, rather than each on its own? Taken apart, as the code does now for N
  (`h1NormalisationRelError`), their sum in quadrature is neither the upper nor the lower bound without knowing the
  sign of the correlation as it reaches Y.

**Meanwhile.** σ_R is not propagated; nothing in the output depends on the answer.

---

## The φ sideband: which count goes into the background ratio

**Opened 29 October 2026.** `DynamicRooFitter::ExtractYieldsAndPurity` (the sideband integral), `fitConfig.json`
(`phi.fit_setup.default.sideband`), `h2TriggerBkgRatio`; the correction of its binning is in `DESIGN_HISTORY.md`,
*The φ sideband counted on its window*.

### What the analysis does

Per (multiplicity, trigger pT) cell, the φ triggers are the K⁺K⁻ candidates in the signal window of the invariant
mass, [1.00, 1.05] GeV/c², and their pairs P_SR contain the pairs of the background candidates under the peak. These
are subtracted with the pairs P_SB of the candidates in a sideband window, [1.06, 1.08] GeV/c², scaled by

    R = B_SR / N_SB,        Y ∝ P_SR − R · P_SB

where B_SR is the background in the signal window and N_SB the number of candidates the sideband pairs were built
from. The two windows are applied twice, and must be the same both times:
- **online**, in the O2Physics task `phiStrangeCorrelation`, which fills P_SR and P_SB: `rangeMPhiSignal`
  {1, 1.04999995} and `rangeMPhiSideband` {1.05999994, 1.08000004} in the train configuration (the float spelling of
  1.00-1.05 and 1.06-1.08; the task's defaults are different, {1.0095, 1.029} and {1.1, 1.2});
- **here**, in the fit of the φ mass spectrum of each cell (`PhiFitTask`, `DynamicRooFitter`): `integration_range`
  [1.0, 1.05] and `sideband.range` [1.06, 1.08], with `from_fit: false`.

The fit (model `RelBWKK` + `BkgMattia`, fit range [0.995, 1.1]) gives B_SR as the integral of the background model
over [1.00, 1.05]. N_SB, with `from_fit: false`, is counted on the mass histogram (`h3PhiData`, projected on the cell).
With `from_fit: true` it would be the integral of the background model over [1.06, 1.08] instead.

Numbers of the 2026 HighIR production (22 October, offline efficiency), 10 multiplicity classes × 7 trigger-pT bins:
R = 1.28-2.19; σ_R/R = 0.06-0.72% (the fit's error); background over signal in the signal window B/S = 0.57-3.42
(B/(S+B) = 0.36-0.77). Since the subtracted term R·P_SB is 0.6-3.4 times the signal, a relative change δ of R moves
the yield by about δ·B/S: 5% on R is 3-17% on Y before the other species-independent factors.

### First: the count was not on the window (a mistake, corrected)

The mass histogram has 200 bins of 1.5 MeV/c² on [0.9, 1.2]. Their edges are 0.9 + k·0.0015: 1.059, 1.0605, ...,
1.0785, 1.08. So 1.08 is an edge, 1.06 is not: it cuts the bin [1.059, 1.0605]. The sideband was integrated with
`HistogramUtils::IntegralAndErrorPair`, which finds the bins of the ends with `FindBin(x ± 1e-5)` and takes them
whole: it counted [1.059, 1.08], 1 MeV/c² more than the window the pairs were selected in, i.e. the candidates in
[1.059, 1.06) are in N_SB and not in P_SB.

The excess, measured cell by cell as (count over [1.059, 1.08]) / (count with the cut bin taken for 1/3) − 1:

| mult \ pT(φ) | 0.4-0.8 | 0.8-1.4 | 1.4-2 | 2-2.8 | 2.8-4 | 4-6 | 6-10 |
|---|---|---|---|---|---|---|---|
| 0-1 | 4.89 | 4.91 | 4.48 | 4.78 | 4.71 | 4.63 | 4.73 |
| 1-5 | 4.88 | 4.95 | 4.46 | 4.80 | 4.76 | 4.60 | 4.72 |
| 5-10 | 4.87 | 4.95 | 4.47 | 4.84 | 4.78 | 4.60 | 4.70 |
| 10-15 | 4.85 | 4.94 | 4.49 | 4.84 | 4.72 | 4.62 | 4.65 |
| 15-20 | 4.89 | 4.95 | 4.49 | 4.83 | 4.74 | 4.66 | 4.70 |
| 20-30 | 4.89 | 4.96 | 4.48 | 4.89 | 4.85 | 4.58 | 4.71 |
| 30-40 | 4.90 | 4.97 | 4.49 | 4.87 | 4.80 | 4.68 | 4.69 |
| 40-50 | 4.90 | 4.98 | 4.48 | 4.97 | 4.82 | 4.61 | 4.65 |
| 50-70 | 4.92 | 5.00 | 4.52 | 4.91 | 4.85 | 4.62 | 4.69 |
| 70-100 | 4.95 | 5.04 | 4.56 | 4.92 | 4.90 | 4.68 | 4.72 |

(%, 2026 HighIR. The 2024 and 2026 productions have the same mass axis and the same `fitConfig.json`, so the same
cut bin; their excess was not measured.)

**Corrected** by counting the cut bin for the fraction of its width inside the window
(`HistogramUtils::IntegralAndErrorFractional`), i.e. assuming its entries spread evenly over its 1.5 MeV/c². The
slope of the spectrum across that bin, measured from its two neighbours in every cell, is −0.3% to +2.1% per bin,
which makes the even spread good to 1.5·10⁻⁴ of the sideband count. The variance of a partial bin is the fraction of
the bin's (the Poisson variance of the entries it stands for). This is the analysis's own method with the window it
always meant, and does not depend on the question below. Its effect, against the results before the correction:
R up by 4.46-5.04% in every cell, and

| | trends (|Δy| < 1, 0.5, 0.1) | in statistical errors | ratios to π (measured, extrapolated) | in statistical errors |
|---|---|---|---|---|
| π | −6.1 … −9.9% | −56 … −247 | | |
| K0S | −5.6 … −9.8% | −5.8 … −31 | K0S/π +0.07 … +0.69% | +0.12 … +1.16 |
| Ξ | −5.4 … −11.0% | −0.9 … −6.1 | Ξ/π −1.39 … +1.35% | −0.16 … +0.47 |
| Ω | −2.7 … −14.5% | −0.16 … −1.20 | Ω/π −5.14 … +4.33% | −0.26 … +0.39 |

The trends of every species fall by about the same amount, because R is the same for every species in a trigger
cell; the ratios between species keep only a residual, from how much the sideband subtraction weighs for each
species and associated pT.

### The question: the φ in the sideband

The sideband is not pure background. The tail of the φ line shape reaches [1.06, 1.08]: the fit puts there, as
n_sig × ∫_{1.06}^{1.08} f_sig, a signal S_SB that is this fraction of the candidates N_SB (%):

| mult \ pT(φ) | 0.4-0.8 | 0.8-1.4 | 1.4-2 | 2-2.8 | 2.8-4 | 4-6 | 6-10 |
|---|---|---|---|---|---|---|---|
| 0-1 | 0.36 | 1.29 | 1.74 | 3.56 | 4.33 | 3.03 | 1.81 |
| 1-5 | 0.40 | 1.43 | 1.90 | 3.87 | 4.67 | 3.16 | 1.79 |
| 5-10 | 0.44 | 1.58 | 2.06 | 4.16 | 4.92 | 3.26 | 1.80 |
| 10-15 | 0.48 | 1.70 | 2.19 | 4.38 | 5.13 | 3.33 | 1.80 |
| 15-20 | 0.51 | 1.80 | 2.30 | 4.57 | 5.28 | 3.39 | 1.83 |
| 20-30 | 0.55 | 1.95 | 2.44 | 4.80 | 5.44 | 3.45 | 1.81 |
| 30-40 | 0.61 | 2.14 | 2.62 | 5.07 | 5.64 | 3.48 | 1.79 |
| 40-50 | 0.68 | 2.34 | 2.81 | 5.34 | 5.78 | 3.52 | 1.79 |
| 50-70 | 0.78 | 2.66 | 3.05 | 5.71 | 5.96 | 3.53 | 1.78 |
| 70-100 | 1.01 | 3.35 | 3.56 | 6.26 | 6.08 | 3.41 | 1.74 |

(it grows with the signal-to-background ratio: with pT up to 2.8-4 GeV/c, and with multiplicity). The model accounts
for the histogram: the count on the window, N_SB, equals B_SB + S_SB (background and signal of the fit over [1.06,
1.08]) to −0.2 … +1.1% in all 70 cells, so the background model describes the sideband and the difference below is
the φ tail, not a mismodelling.

The two ways the code can compute R therefore differ by that tail:

    histogram (from_fit false):  R_hist = B_SR / N_SB = B_SR / (B_SB + S_SB)
    fit       (from_fit true):   R_fit  = B_SR / B_SB

R_fit / R_hist, per cell (both on the window [1.06, 1.08]):

| mult \ pT(φ) | 0.4-0.8 | 0.8-1.4 | 1.4-2 | 2-2.8 | 2.8-4 | 4-6 | 6-10 |
|---|---|---|---|---|---|---|---|
| 0-1 | 1.0025 | 1.0118 | 1.0160 | 1.0366 | 1.0526 | 1.0429 | 1.0214 |
| 1-5 | 1.0031 | 1.0134 | 1.0186 | 1.0409 | 1.0531 | 1.0411 | 1.0241 |
| 5-10 | 1.0038 | 1.0142 | 1.0208 | 1.0445 | 1.0562 | 1.0450 | 1.0275 |
| 10-15 | 1.0040 | 1.0162 | 1.0228 | 1.0471 | 1.0598 | 1.0429 | 1.0229 |
| 15-20 | 1.0048 | 1.0170 | 1.0227 | 1.0492 | 1.0640 | 1.0440 | 1.0204 |
| 20-30 | 1.0047 | 1.0184 | 1.0244 | 1.0499 | 1.0611 | 1.0443 | 1.0237 |
| 30-40 | 1.0059 | 1.0205 | 1.0260 | 1.0541 | 1.0655 | 1.0427 | 1.0235 |
| 40-50 | 1.0058 | 1.0221 | 1.0289 | 1.0566 | 1.0687 | 1.0455 | 1.0211 |
| 50-70 | 1.0074 | 1.0258 | 1.0309 | 1.0614 | 1.0706 | 1.0448 | 1.0223 |
| 70-100 | 1.0094 | 1.0329 | 1.0365 | 1.0655 | 1.0689 | 1.0454 | 1.0192 |

(+0.25 … +7.1%, against σ_R/R ≤ 0.74%; the pattern is the tail table's.) What R_fit gives instead of R_hist, both
corrected for the window:

| | trends | in statistical errors | ratios to π | in statistical errors |
|---|---|---|---|---|
| π | −2.85 … −3.50% | −20 … −75 | | |
| K0S | −2.77 … −3.14% | −2.4 … −8.7 | K0S/π +0.03 … +0.39% | +0.05 … +0.60 |
| Ξ | −2.70 … −3.43% | −0.32 … −1.72 | Ξ/π −0.44 … +0.68% | −0.04 … +0.19 |
| Ω | −1.17 … −4.46% | −0.08 … −0.35 | Ω/π −1.53 … +2.58% | −0.11 … +0.22 |

and against the results before any correction, R_fit gives trends −8.2 … −13.9% (π −9.3 … −12.5%, K0S −8.5 …
−12.3%, Ξ −8.2 … −13.9%, Ω −3.8 … −18.2%) and ratios K0S/π +0.13 … +1.06%, Ξ/π −1.82 … +1.81%, Ω/π −6.6 … +7.0%.

**Neither R is exact.** Write the sideband pairs as P_SB = B_SB·y_b + S_SB·y_φ, with y_b and y_φ the pairs per
candidate of a background candidate and of a φ (the latter is the result, per trigger, before the corrections). What
the subtraction must remove from P_SR is the background pairs of the signal window, B_SR·y_b (assuming background
candidates have the same pairs per candidate in both windows, which the sideband method assumes anyway). Then

    R_hist · P_SB = B_SR·y_b · [1 + f·(y_φ/y_b − 1)]       f = S_SB / N_SB, the tail fraction above
    R_fit  · P_SB = B_SR·y_b · [1 + (S_SB/B_SB)·(y_φ/y_b)]

- R_hist is exact if a φ and a background candidate bring the same pairs (y_φ = y_b); it removes too little if y_φ >
  y_b, too much if y_φ < y_b, by f·(y_φ/y_b − 1) of the subtracted term.
- R_fit always removes too much: the real φ correlations of the tail, scaled by R, by (S_SB/B_SB)·y_φ/y_b of the
  subtracted term. It is exact only if the tail carries no pairs.
- The exact subtraction is R_fit·(P_SB − S_SB·y_φ): it needs y_φ, the result itself, so it would be iterated
  (start from y_φ of R_hist or R_fit, compute Y, recompute the tail term; one or two steps should converge, since f
  is at most 6%).

**What we would like to know.**
1. Which R is the standard choice for a sideband subtraction with a sideband close enough to the peak to contain its
   tail: the candidate count (R_hist), the background integral (R_fit), or the iterated correction?
2. Is y_φ/y_b known or measurable here (from P_SR − R·P_SB per signal candidate against P_SB per sideband candidate),
   so that the size of the bias of each choice can be put on the result?
3. Should the sideband be moved further from the peak (e.g. [1.07, 1.09] or beyond), where the tail is smaller, at
   the cost of a background extrapolated over a longer distance? That needs a new production: the window is applied
   online.
4. Is the difference between R_hist and R_fit (the tables above) to be quoted as a systematic of the method, if the
   answer to 1 is not the iterated correction?

**Meanwhile.** The code keeps the method it had, R_hist (`from_fit: false`), now counted on the window the pairs were
selected in (the correction above). Measurements: `~/alice/claude-sandbox/sideband-check/` (the outputs of the three
versions) and `~/alice/claude-sandbox/tools/` (`sbexcess`, `sbslope`, `r3`), not in the repository.
