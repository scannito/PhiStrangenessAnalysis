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
