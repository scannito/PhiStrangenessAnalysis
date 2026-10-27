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
