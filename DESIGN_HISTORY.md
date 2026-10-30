# Design history — resolved items

Moved out of `DESIGN_NOTES.md` on 13 October 2026. Everything here is done; it
is kept because the reasoning is what stops the same decision from being undone,
and several of these fixes look wrong without it. Open items stay in
`DESIGN_NOTES.md`.

---

## snap_to_bin removed

**30 October 2026.** `fitConfig.json`'s `snap_to_bin` moved the ends of the signal window outward to the edges of the
mass bins that hold them (`DynamicRooFitter::CalculateIntegrationLimits`). It was `false` in every node, so no number
came from it, but it was a switch that gave wrong ones: the signal yield and the background of the signal window are
integrals of the pdfs, which need no bin edges, and the window is the train's (`rangeMPhiSignal` [1.0, 1.05] for the φ),
so a widened one counts triggers and background over candidates whose pairs were not selected. With [1.0, 1.05] on
the 1.5 MeV bins it gave [0.999, 1.0515], 5% wider, the upper end a whole bin further although 1.05 is an edge
(`FindFixBin` on an edge returns the bin that starts there; `RooFitKnowledge/04-integrals-errors-normalization.md` had
recorded it). Found with *The φ sideband counted on its window*, which was the same error on the denominator. Removed,
the key, the member and the code, at the owner's choice ("at most it comes back"); a configuration that still writes it
stops at `RequireKnownKeys`. `fitConfig_DONOTUSE.json`, which no block reads, still has it.

**Checked.** purity and the phi fit + correlation of the 2026 HighIR offline block identical to main.

---

## The φ sideband counted on its window

**29 October 2026.** The background ratio R = B(signal window) / N(sideband) that scales the sideband pairs was
computed with N counted over the wrong range. The train selects the sideband candidates in [1.06, 1.08]
(`rangeMPhiSideband` {1.05999994, 1.08000004}, checked against the train's configuration; `fitConfig.json` says the
same), and `DynamicRooFitter` counted them on the mass histogram with `IntegralAndErrorPair`, which found the bins of
the ends with `FindBin(x ± 1e-5)` and took them whole. The mass axis of every production is 200 bins of 1.5 MeV/c² on
[0.9, 1.2]: 1.08 is an edge, 1.06 is not, so the count covered [1.059, 1.08], with the candidates of [1.059, 1.06) in N
and not in the pairs. Measured on the 2026 HighIR production: N 4.5-5.0% too large in every cell, R too small by as
much; the 2024 and 2026 productions have the same axis and the same window.

**What changed.**
- The sideband is counted with `HistogramUtils::IntegralAndErrorFractional`: the bin an end cuts counts for the
  fraction of its width inside, its entries taken as evenly spread (the slope of the spectrum across it, measured in
  every cell, makes that good to 1.5·10⁻⁴ of the count); its variance by the square of the fraction, as `TH1::Scale`.
  The method is unchanged: N is still the histogram count (`from_fit: false`).
- `IntegralAndErrorPair`, renamed `IntegralAndErrorWholeBins` next to `IntegralAndErrorFractional`, requires its ends
  on bin edges and stops the run otherwise, and refuses an empty range, which ROOT would integrate up to the overflow.
  "Is x an edge" is one test, `BinningUtils::FindEdgeIndex` (and `EdgeIndex`, which throws), with one tolerance,
  `kEdgeTolerance`, used by `MapToSourceBins`, `Compare` and `FindRebinMismatches` as well. Its other callers (the |Δy| windows of `ConstructSpectrum`, the event counts of `MCTask`) were already on
  edges and are unchanged; the next one that is not stops instead of being widened. The 2D overload, with no caller,
  is gone; `FillTrendFromSpectrum` integrates its whole axis directly.
- `JsonConfig::TryRange` refuses low ≥ high, and an empty sideband warns, naming the cell, instead of leaving R = 0,
  which subtracts nothing.

**Effect** (2026 HighIR, offline efficiency, against main): R +4.46 … +5.04%; the trends of every species fall by
6-10% (π −6.1 … −9.9%, 56-247 statistical errors; K0S −5.6 … −9.8%; Ξ −5.4 … −11.0%; Ω −2.7 … −14.5%), because the
subtracted background is 0.6-3.4 times the signal; the ratios between species move by at most about 1% (K0S/π +0.07 …
+0.69%, Ξ/π −1.4 … +1.4%, Ω/π −5.1 … +4.3%, all within 1.2 statistical errors), R being the same for every species of
a cell. Every result produced before this change has its yields and trends too high by that much.

**Found with it, open.** The sideband also holds the tail of the φ (0.4-6.3% of its candidates, growing with S/B), so
the histogram count and the background of the fit (`from_fit: true`) differ by up to 7% in R and 3% in the trends.
Which is the right denominator is a method question: `QUESTIONS_FOR_EXPERTS.md`, *The φ sideband: which count goes
into the background ratio*, with every number of both methods.

**Checked.** mc, purity and the WPDG closure identical to main; the phi fit and the correlation of the 2026 HighIR
offline block identical to the prototype measured first (R to 5·10⁻¹⁵, the trends exactly); the helpers tested on an
axis with filled under/overflow, on |Δy| windows on and off the edges, on the mass window, on an empty range.

---

## An invalid correction bin warns by default

**29 October 2026.** Two days after *A correction that cannot be computed stops the run* (below), the owner asked for
the run not to stop by default: a run of many blocks stopped by one map with an empty bin has to be split into
single runs. A required task key, `on_invalid_correction_bin`, next to `corrections_file` and declared once as
`warn` in the four base blocks of the correlation tasks (`correlation_base_settings`,
`correlation_wpdg_base_settings`, pp and pp_ref):
- `warn`: the map is applied as before 27 October, an efficiency or signal-loss bin divides its cells (inf or NaN,
  for every particle of the classes when it is the φ map) and an event-loss class at zero is left without that
  correction, which gives numbers that look normal: the owner's choice, with a NaN in that class as the
  alternative put to him. Every such map is one `Logger::Warn` naming its bins, counted in the end-of-run summary
  (the minimum `CLAUDE.md` asks of a correction that cannot be computed), and one line in the provenance of the
  results (`invalid_correction_bins`).
- `stop`: what 27 October made the only behaviour.

Required and not optional, and outside the `corrections` block, from the reviews of a first version where it was an
optional key in the block: optional, the policy in force left no trace in the provenance; in the block, every block
that writes its own `corrections` would have had to repeat it. `JsonConfig::RequireEnum` came with it, the required
sibling of `OptionalEnum`, for the three enum keys read that way.

A third value, `drop_particle` (an associated particle with an invalid bin out of the task, the run going on), was
written and tested, then deferred: the per-particle state is in parallel containers, and dropping one mid-`Init` has
to erase from all of them by hand (`DESIGN_NOTES.md` section 10, *One struct per associated particle*).

---

## The names between the producers and CorrelationTask in OutputNames

**28 October 2026.** The move of 20 October (*The PhiAssocSpectra names in one place*) left out what crosses the
other boundaries: the directories of the correction maps (`AccEff/MultBin`, `SigLoss/MultBin`, `EvLoss`) and
`hEventLoss` in `Corrections.root`, the `Summary` directory of `PhiDataHistograms.root` and `Purity.root`, and the two
trigger matrices `CorrelationTask` reads (`h2TriggerSignal`, `h2TriggerBkgRatio`). Each was spelled by the writer and
again by the reader, the trigger matrix a third time in an error message. They are in `OutputNames`, the directories
as functions of the `binning_name` that return the whole path (`EfficiencyPath`, `SignalLossPath`, `EventLossPath`,
`SummaryPath`): the layout, not only the leaf name, is written once. What stays inside one task stays there: the
`Summary` canvas directories of `MCTask`, `Fits`, `SourceBinning`, the two trigger matrices nobody else reads.

One change of behaviour: `MCTask` wrote `hEventLoss` into whatever directory was current when `{binning_name}/EvLoss`
could not be created. It now goes through the `writeInto` of the maps, which writes nothing and leaves the error to
`GetOrCreatePath`, so a consumer with `event_loss` stops at the missing object instead of reading nothing.

Checked: mc, purity and data (the `2026_highIR_updated` blocks) before and after; `Corrections.root`, `Purity.root`,
`PhiDataHistograms.root` and `PhiAssocSpectra.root` identical object by object (`CompareRootFiles`) and key by key,
canvases included.

---

## A correction that cannot be computed stops the run

**27 October 2026.** Offline, a correction map with a bin at or below zero (or not a number) was applied as it was:
an efficiency or signal-loss bin at 0 divided its cell by zero (an infinite or NaN yield), and an event-loss bin at 0
was skipped, the class left uncorrected without a word (an open item of `DESIGN_NOTES.md` section 5 since
16 October, "whether the run should stop instead is a choice to make"). `LoadCorrections` now stops at loading,
naming the map, the file and the interval, for every map it applies: all go through one loader, `fetchHist`, the
event loss included, which so also gets its axis compared with the multiplicity classes like the pT maps with the
analysis binning.

**Why stop, not warn.** No number is right in such a bin: 1 is wrong, and skipping it is the same as 1 (`CLAUDE.md`,
*A correction that cannot be computed is not a correction of 1*). A warning would leave a result that looks complete.
The remedy the message gives is the real one: more MC statistics in that cell, or data and MC productions whose
binning merges it. Not `rebinning_pt` in the MC alone: the maps must have the binning of the data, read from the data
file, so a coarser map stops at the comparison just before.

**What it meets today.** None of the maps loaded by the configured blocks: the 2024 and 2026 maps have no bin at or
below zero. The `WTrigger` files have φ efficiency maps that are zero everywhere, but the blocks that read them do
not apply the φ efficiency, and only what is applied is loaded. `2026_LowIRae_updated` has 36 empty Ω bins in the
per-class maps; a block using them would now stop. Online, the O2Physics task still gives a weight of 1 to a pair
whose efficiency is zero (section 13, decision 1, open there).

**Checked.** The 2026 offline block identical to the branch it sits on (301 objects); a copy of the corrections with
one bin of the Ω map at 0, and one with the event loss at 0 in a class, each stop at loading with the map, the file
and the interval; the real event loss passes the new comparison.

---

## The normalisation uncertainty, next to the trends

**27 October 2026.** The correlation tasks divided every spectrum by the trigger count as if it were exact: the error
of the φ fits, which `PhiFitTask` writes with the yield, was dropped, and so was the error of the event-loss
correction. A branch of 1 August (`test`) had propagated them into the trends' errors; it was redone on `main`, and
the design changed twice under review.

**What it does.** `GetTriggerSignal` returns the yield with its error. `CountTriggers` sums the trigger-pT bins of a
class, errors in quadrature; `GenerateSpectraAndTrends` adds the event loss and writes the relative uncertainty of the
normalisation, per multiplicity class, in `h1NormalisationRelError` at the root of the section (`OutputNames`), with
the real edges and zero bin errors. The trends keep their statistical errors only. The log prints the count, its
error and the uncertainty per class. On the 2026 HighIR production it is 0.04-0.10%, which matters for the π trends
only (statistical error of the same order); the K0S, Ξ and Ω trends do not see it.

**Left out, on purpose.**
- *The φ efficiency's error.* The pairs of each trigger-pT bin are divided by the same efficiency as the triggers,
  so in the per-trigger yield it largely cancels; counted on the triggers alone it would add what the yield removes.
  For the same reason the one reader of the corrections, `Efficiency(particle, class, bin)`, serves the triggers and
  the pairs both: the yield is corrected only if the two use the same number.
- *A key that folds it into the trends.* The first version had one (`propagate_trigger_error`, then
  `propagate_normalisation_error`), and both review agents objected: a trend file does not say that its errors hold
  the normalisation, and `comparison_task`, dividing the trends of two files that share the triggers (a closure,
  two variants on one φ fit), counts it twice where it cancels, with a README line as the only guard. The
  ratios inside the task had to be taken from separate copies for the same reason. Kept apart, it is right in every
  ratio by construction and added once by whoever builds a total error; no stored number depends on a choice, so no
  key (the owner's choice).
- *How much of the fit error belongs there.* Its Poisson part fluctuates with the pairs; the whole error is taken,
  the conservative choice, while the question is open (`QUESTIONS_FOR_EXPERTS.md`, the first entry).

**WPDG.** The trigger count integrates the cell of the TH3 in place instead of a projection: the counts are identical
in the 70 cells of `h3PhiData` and of `h3PhiMCClosureGen` in the 2024 closure files.

**Checked.** `correlation_task_offline_efficiency_2026_highIR_updated`, compiled, against `main`: the 300 objects of
`PhiAssocSpectra.root` identical, the new histogram the only difference.

---

## Fit quality: which limit; polynomials by order

**26 October 2026, `0490ca6`.** Three changes to what `FitQuality` calls a limit, and one branch of the model parser for
the polynomials.

**The two ends apart.** `FitDiagnostics` and the tree had one list, `atLimit`. They now have `atLowerLimit` and
`atUpperLimit`: which end a parameter sits on is the first thing whoever reads the row asks, and the rule below treats
the two differently.

**A yield is at its lower limit below one count.** The test was the one of note 09 for every parameter, within 1e-5 of
the range from an end. A yield's range is 0 to 1e7-1e11, so that called every yield below 1e2-1e6 counts "at its
limit", and the flag hid the yields that do sit on a limit (the K0S `nsig` cut by its maximum of 1e8, ROADMAP B1). The
threshold was measured, not chosen: in the scans of the model study the yields Minuit pushed onto zero sat at 0 or
between 1e-7 and 1 count (2537 of them), then nothing between 1 and 10 counts but 34, then the real small yields.
Other parameters keep note 09's test.

**A model can call a parameter's upper limit a shape** (`upperIsShape`, declared where the model is built, recorded in
the tree). The DSCB does for its tail exponents n1 and n2: at a large n the power-law tail is as steep as an
exponential, which is a shape the data choose, not a fit stopped by its box. `FitQualityRecorder::Reasons`, the one
verdict, skips the upper limit of those parameters and nothing else; the tree keeps the fact. Only the upper end: an n
on its lower limit (1) is still a fit pushed against the box (the owner's choice).

**Chebychev<n> and Bernstein<n>** through one branch each, the order read from the name (`PolynomialOrder`, a regex
`family + "([1-9][0-9]*)"`: `Chebychev0` or `Chebychev01` is not a polynomial but an unknown model, which stops the
run), in place of a branch per order (`Chebychev1`, `Chebychev2`, `Chebychev3`, each written out). Chebychev keeps c1..cn, the constant implicit, the top term staged at 0. Bernstein has b0..bn, one redundant with the
yield: the configuration fixes which one, because the choice shapes the curve (near the KK threshold the φ background
is small, a poor anchor), and must fix it above zero; all coefficients must be non-negative, the reason to choose
Bernstein over Chebychev (a curve positive by construction). Both stop the run otherwise. No staged start for
Bernstein: with its last coefficient at 0 it is still of degree n, pinned to 0 at the upper end, not one order lower.

**Checked.** Purity and φ outputs of the 22 October production unchanged; only the flags change.

---

## The φ line shape, and the data imported before the model

**24 October 2026, `51488a1`.** The φ is fitted with `RelBWKK` on `BkgMattia`, and the fitter imports the histogram
before it builds the model.

**`RelBWKK`.** A P-wave relativistic Breit-Wigner, the width growing with the kaon momentum in the pair frame
(Blatt-Weisskopf radius), convolved by FFT with the Gaussian resolution, with the Voigtian's parameter names plus the
radius. The FFT grid and buffer come from the declared limits of `sigma_sig` (bins of a quarter of the smallest
resolution over the range, a buffer of five times the largest), not RooFit's defaults, which are about one sigma for
the φ. Measured against `RooVoigtian` with a constant-width Breit-Wigner: the fraction in the window agrees within 2e-6
and the shape moves by 25 keV, which `mean_sig` absorbs. Checks: the mass lower limit above the KK threshold, a
positive resolution. On the 22 October production: no φ fit flagged, χ²/ndf 4-9 instead of 26-48 from 0.8 GeV/c (mult
15-20%), φ yields +6 to +18% at 1.4-2 GeV/c and -24% in one cell where the old fit had failed. Below 0.8 GeV/c the
Voigtian stays (an override per multiplicity class), because it describes the data better there and nobody yet knows
why (`DESIGN_NOTES.md` section 6).

**The import before the model.** Importing a histogram into a `RooDataHist` moves the observable's range outwards onto
the bin edges (0.995-1.1 became 0.9945-1.101). A model built before the import kept the old range, and `RelBWKK`'s FFT
grid with it: the signal came out normalised 22% high (`nsig` 1.31e6 instead of 1.10e6 in a φ cell), with a fit that
looked fine. The fitter now imports the data once, in its constructor, and builds the model on the range that results.
The models already in use did not depend on it: purity and φ outputs bit-identical before and after. Do not move the
import back after the model.

---

## Several starts per fit

**23 October 2026.** Phase 3 of `RooFitKnowledge/ROADMAP.md`, in a smaller form. `DynamicRooFitter::DoFit` fitted
each cell once, from the values in `fitConfig.json`. A study of the purity models on every cell of the production
of 22 October (13 candidate models per cell, each restarted from the best fits of the others) found that this
single fit stops far from the minimum with every flag clean, and that a configuration selected on the study's
minima is not reproduced by the task: with the selected models the task's NLL was above the study's by a median of
161 for the Xi and 2.4e5 for pi_tpc, and the purities differed by more than 0.002 in 154 of 440 K0S cells and 198 of
330 Xi cells. A more precise `fitConfig.json` was worth nothing until the fit reached the minimum of the model it
is given, and the study has to call the same `DoFit` the tasks call (the design point both review agents made
first).

**What it does.** Three starts, every one from the configured values (a snapshot taken when the fitter is built)
and from the cell itself, and the lowest NLL wins:

- `config`: the configured values, fitted exactly as before (`fitTo` without offset). The winner can therefore
  never be worse than the old fit, and where `config` wins the result is the old one to the last digit. A first
  version made this start an `Offset("bin")` fit too, and it lost the old minimum in a few cells, by up to 1.6e7 of
  NLL in the phi: the plain fit follows another path, sometimes the better one.
- `yields`: the free yields at half the cell's counts. They come from one configuration for every cell and start
  100 to 10000 times off (the simplifier's finding: `nsig` 1e6 for pi_tpc cells of 3e8 counts).
- `staged`: the yields as above, and a first fit with the parameters the model declares with `Stage` held: the
  highest Chebychev term at 0 (a background one order lower), the DSCB tails at their configured values. Declared
  where each model is built, the one place that knows its parameters, so no list of names can go stale.

The two derived starts are an `Offset("bin")` fit and a refit from its result. The winner is the lowest NLL, the
likelihood itself without offset (`createNLL`), among the starts where it is finite, a later start winning only when
lower by more than 0.01. Nothing else decides it, and both alternatives were tried. Requiring an accurate covariance,
as first designed, picked in the phi a start far worse than one whose matrix had been forced positive: the covariance
is the estimate of the errors, not the place of the minimum. Requiring no invalid evaluation of the likelihood, as
the second version did, could throw away the lowest minimum, since those evaluations count Minuit's path and not the
point it ends at (the second review found it), and made "never worse than the old fit" false. Both stay flags on the
winner. The `FitQuality` tree records the start that won (`start`) and the NLL the `config` start reached
(`nllConfig`), next to `minNll`, which is now the winner's.

**Left out, on purpose.** Random starts, seeding from a neighbouring cell, an escalation to xRooFit and fitting
several models inside the run were all candidates (ROADMAP phase 3 and the two agents). Neighbour seeding makes a
cell depend on the order of the loop; several models make the model an output instead of a declared input; xRooFit
needs a library and a `gDirectory` workaround; random starts need a seed and a count per family, both numbers to
declare. Measured against the study's best minima, the three derived starts were enough: among the models that
compete in a cell (AIC within 20 of the best), the new `DoFit` stayed above the best known minimum by more than 1 in
13 of 653 K0S fits, 36 of 844 Xi, 110 of 2155 Omega, 9 of 460 pi_tpc and 21 of 698 pi_tof, against 138, 52, 392, 362
and 272 for the study's own cross-model search. Random starts come back if a measurement asks for them.

**Checked.** MC (the two HighIR `_updated` blocks), purity and the five data tasks on the 22 October production,
against `main`: no fit has a higher NLL than before (purity and both phi fits), and lower by more than 0.1 in 368
of 440 K0S fits, 157 of 330 Omega, 43 of 330 Xi, 430 of 460 pi_tpc, 45 of 460 pi_tof, 26 and 58 of 70 phi fits. The
670 purity cells won by `config` are identical to `main`. The analysis purities move by more than 0.002 in 47 of 100
K0S cells (up to 0.051), 12 of 70 Omega, 6 of 70 Xi, 16 of 100 pi_tpc and 3 of 100 pi_tof. Winning starts in the
purity pass: K0S yields 410, config 19, staged 11; Xi yields 189, config 122, staged 19; Omega config 140, staged 104,
yields 86; pi_tpc staged 238, yields 218, config 4; pi_tof config 385, yields 75. The purity pass takes about 18 s
instead of 6-13 s. `minNll` is now the likelihood evaluated by `createNLL`, no longer `RooFitResult::minNll()`: at
the same fit (the `config` start against `main`'s single fit, all 2020 purity cells) the two differ by a median of
1e-4 and at most 0.49 on values of order 1e8, the order of the sums; a comparison of `FitQuality` across this change
sees that, and nothing else, where `config` won.

**Not solved here.** The pi_tof cells at pT 0.2-0.3, where no start converges (covQual 0 before and after). The
at-limit test flags small yields as at their lower limit (`DESIGN_NOTES.md` section 5).

---

## Objects and overrides named by interval

**23 October 2026.** What was section 4 of `DESIGN_NOTES.md`. The producers and readers that still named a cell by
its index (`_multBin3`, `_ptBin7`) name it by its interval, as `CorrelationTaskBase::CellSuffix` already did:
the efficiency and signal-loss maps of `MCTask` and their reader, the purity spectra and their reader, the spectra
of `PhiAssocSpectra.root` (`OutputNames::Spectrum`), the projections and fit canvases of `PhiFitTask` and
`PurityTask`. `BinLabel` moved to `BinningUtils`; the tags built on it (`OutputNames::MultTag`, `mult10-15`, and
`CellTag`, `mult10-15_pt1.2-1.6`) and the names one task writes and another looks up (`Efficiency`, `SignalLoss`,
`Purity`) are in `OutputNames`, so that the producer and the reader cannot spell them differently: the first
version of this change wrote `"_mult" + BinLabel(...)` by hand in about ten places, both sides of two task
boundaries included, and both review agents found it. The remote branch `naming` (13 August) had done most of the
renaming; 90 commits later it was redone on `main` with that branch as a guide.

**The overrides.** `fitConfig.json` keyed its `bins` by `mult<i>_pt<k>`, and `extrapConfig.json` by `multBin<i>`.
Since `rebinning_pt` the purity task fits two binnings with the same configuration, and the twenty K0S overrides
(`mult*_pt4`, `mult*_pt5`, a Chebychev2 background) applied to 1.2-1.6 and 1.6-2.0 GeV/c in the analysis fits and
to 0.7-0.8 and 0.8-0.9 in the source fits; nobody remembered which was meant. The keys are now the intervals
(`mult10-15_pt1.2-1.6`, `mult10-15`), decided with the owner, and a multiplicity-integrated fit writes
`mult0-100`; a key without multiplicity, valid for every class, was considered and not wanted. The key is the
`CellTag`, so it is also the suffix of the canvas of the fit it acts on. The twenty keys became the forty that
reproduce what ran, both readings, to be replaced by the outcome of the model study (`RooFitKnowledge/ROADMAP.md`;
the source-binning fits stay, the owner may apply the fine purity maps from CCDB). **A key that names no cell stops
the run**, agreed with the owner: with interval keys a typo, or a key written for another binning, is no longer an
index that happens to exist, it is an override never applied. The task declares the cells it fits
(`FitConfigManager::DeclareCells`, `ExtrapConfigManager::DeclareCells`), which checks the overrides, and
`GetConfig` takes the cell's tag and refuses one that was not declared: a first version had the check as a call
each task had to remember, with the binnings listed a second time, and the reviewer pointed out that forgetting it
would bring the silence back. The keys are checked with an overload of `JsonConfig::RequireKnownKeys` for a set
built at run time. One consequence, written in the README: a scheme with its own edges needs its own fit and
extrapolation configuration files.

**What stayed.** The binning stamps: with interval names a mismatch is a missing object, but the stamp stops the
run before anything is read and names the edge. Its message, which spoke of "cached projections" to the readers of
the corrections and the purities, now names neither. `_multIntegrated` is not an index and kept its name
(`OutputNames::kMultIntegrated`). The `FitQuality` columns `multBin`/`ptBin` were dropped: the canvas and the
override are named by the intervals, which the tree has, and a cut on an index across two productions is the
mismatch this change removes. Every file produced before this change is unreadable to the code after it, so the
chain is re-run once, in order.

**How it was checked.** The same chain (MC for the two HighIR `_updated` blocks, purity, and the five data tasks
on the production of 22 October) in a copy of `main` and in the changed copy: every object of the 55 files equal
bin by bin through a map from `_multBin<i>` to the interval, nothing missing or extra, the `FitQuality` rows equal
and the 40 overridden rows carrying the expected keys. A wrong key put on purpose in each configuration stops the run
at `Init`, with the message expected (test configurations kept outside the repository).

---

## Fit quality recorded, the purities in one file, the fit files by UPDATE

**21 October 2026.** Phase 1, item 1 of `RooFitKnowledge/ROADMAP.md`: every fit is now recorded, and nothing it
records changes a number. Three decisions came before the code, each put to both review agents first; the
second and third changed on the way.

**The form of the record: a TTree.** Asked first, both agents answered "a `TNamed` with JSON, as the provenance".
The owner objected that what the framework already does is not a reason, and the question was asked again in
general, a framework-wide change included. Answer, kept in CLAUDE.md (*Conventions*): a record takes the shape of
its data. The provenance is a set of named facts and stays one `TNamed` per fact (changing it would leave a second
reader in every tool, for every file already produced, for nothing). The fit record is a table, rows of the same
typed fields, and is a `TTree`: queried in one line, a missing column an error. The argument that settled it was
found by the reviewer: `CompareRootFiles` compared any `TNamed` by its title, and a `TTree` is a `TNamed`, so a
tree would have passed every before/after check whatever it held. A detached tree with `std::string` and
`std::vector<std::string>` branches, written into a directory and read back, was measured to work in the compiled
build before anything was written on it.

**The purity files: one `Purity.root` per block.** `PurityTask` shared a file between the particles whose
`output_file_suffix` agreed (pi_tpc and pi_tof), which cost a map of files, a raw `TFile*` in `ParticleTask`, the
two keys written by hand to pick a subdirectory (`task.name == "pi_tpc" || ...`), the stamp and the provenance
written twice, and a suffix kept equal by hand on the reader's side (`file_suffix`). Of the two agents' options,
one file per particle or one per block, the owner chose one per block, as `PhiFitTask`, `MCTask` and the
correlation tasks: `{output_prefix}Purity.root`, the fits of each particle under `Fits/{purity_key}`, the name in
`OutputNames::kPurityFile` for both sides (with `kPhiDataFile`, which was spelled in two places too). Both keys are
gone; a configuration that still has `output_file_suffix` stops at `RequireKnownKeys`. `CorrelationTask` opens the
file, prints its provenance and checks its stamp once, not once per particle.

**The fit files by UPDATE.** Both fit tasks write everything under `{binning_name}/` and opened their file with
`RECREATE`, so a second scheme into the same file destroyed the first (the reviewer's example: the online-only
configuration and the base purity block, both with the prefix `Data_`, write the same purity file). They now open it `UPDATE` and clear their own
binning (`RootIO::ClearPath`), row two of CLAUDE.md's table, as `MCTask` does.

**The record.** `DynamicRooFitter::Diagnostics` reads the saved `RooFitResult` (status, covQual, edm, minNll,
numInvalidNLL, floating parameters within 1e-5 of their range from a limit, note 09's definition) and the counts
the likelihood saw (`sumEntries` of the `RooDataHist`). `FitConfig::binOverride` carries the `fit_setup.bins` key
the manager applied. `FitQualityRecorder` fills the tree, writes it, and gives one warning per section and binning
with the count per reason; `Reasons` is the one public rule, status not among them (note 09).

**`CompareRootFiles`**: trees compared branch by branch (numbers through their leaves, strings and vectors of
strings by type); any other class, and any `TNamed` subclass, listed as NOT COMPARED, which fails the check; two
NaN equal (the empty pi_tof purity is NaN on both sides of any change, and was reported as a difference every
time); every directory starting with `Provenance` skipped (`ComparisonTask`'s copies were compared, timestamp
included). In the 742 output files of a sandbox (spectra, caches, corrections, purities, comparisons) the classes are TH1D/F, TH2D/F, TH3D, TCanvas and
plain TNamed: nothing that passed before fails now.

**Reviewed** after the code by `analysis-reviewer` and `analysis-simplifier`. Taken: an empty cell was also counted
as "at a limit" (the verdict was half in the classifier and half in `Add`; now one function returns the reasons as
the labels they are counted under); `Write` and the warnings fused, the place from the directory itself; the cell's
edges read by the recorder from the binnings, not paired by each caller; `SetDirectory(0)`, the convention; one
"DoFit first" guard; a guard that could not fire in `PhiFitTask`; the duplicated lookups and loops.

**Checked**, data pp and purity pp, compiled, against main in a sandbox: the purity spectra (83 per year, compared
path by path from the three old files into the new one) and the eight CCDB maps identical; the six spectra files
and the four φ-fit files identical but for the new tree; another binning's directory, put into a purity file
before the run, still there after it; the tree read back and queried; `CompareRootFiles` on trees equal, with one
value changed and with one branch more. What the first record says is in `DESIGN_NOTES.md` section 3.

---

## The toy fits and the saved curves: two ROOT globals

**20 October 2026**, both found while checking the second fit of the high side (below).

**The toy distributions were fitted with gROOT's shared `gaus`.** A π-only configuration change moved the
statistical errors of the K0S and Ξ by up to 1e-10 in 9 of the 10 classes. The mechanism, read in ROOT 6.36's
`HFitImpl.cxx`: for a predefined `gaus`, `TH1::Fit` recomputes the starting values every time (`InitGaus`), but
takes Minuit's initial step sizes from the parameter errors the function carries (`GetParError` before
`SetStepSize`), which are what the previous fit wrote into it. Measured on one toy distribution fitted after two
different ones: σ 0.013324856901797291 against …905001868; with the errors reset before the fit, or with a `TF1`
of its own, …918743088 both times. So only the errors carried over, not the values; and within one spectrum the
fit of ⟨pT⟩ took its steps from the fit of the yield. Now each fit has a `TF1` of its own, out of the global list,
built on the histogram's range (without `"R"` the range is not a window, and `[0, 1]`, the first version, read as
one). A failed fit is said with `Logger::Warn`, since its width was written out as the error regardless; the two
`GetEntries() > 0` guards went (1000 toys always fill them, and a run with none now fails the fit and warns).

**The curves written next to the spectra were interpolations of 100 points.** The models are `TF1`s built from C++
functions, which ROOT streams as their values on `GetNpx()` points (`TF1::Streamer`). `Clone()` goes through the
streamer, so the clone attached to the spectrum was already a 100-point interpolation, in memory as in the file,
and a `SetNpx` on it changed nothing (the first attempt, measured identical for 100 to 2000). A copy made with the
copy constructor keeps the function (and does not register in gROOT's list), and `SetNpx(1000)` then decides what is
written. Read back from the chain's output against the live function rebuilt from the saved parameters: π
blast-wave, worst 23 % below 0.3 GeV/c and the integral over 0-0.2 GeV/c 7 % low before, 0.26 % and 0.07 % after;
K0S Lévy-Tsallis 3 % before, below 0.03 % after. A χ² recomputed from the old curves had given 98 for a fit of 67.
The yields never used these curves; plots and any reader of the `_extended` curves did (`phiplot` too).

**Checked**, data pp, compiled, each sandbox built from its own sources: the run with the π default and the run
with the two-fit π differ in nothing outside the π (they differed in the K0S and Ξ errors before); against the code
before, every content identical and the statistical errors of the extrapolated trends moved by at most 9e-6
relative (another minimum within Minuit's tolerance, far inside the toys' few percent), no new warning. A spectra
file grows from 0.46 to 1.7 MB, the canvases carrying the curves as well; 500 points would give 0.8 % (measured)
for about 1.0 MB (estimated). Both behaviours and both fixes are checked by `OldComparison/extrapolation/checks/`
(`toyGaus`, `savedCurves`). Reviewed by `analysis-reviewer` and `analysis-simplifier`: the range and the
unread fit status taken, the guards removed; the literal `1000` kept, with one use and its reason above it.

---

## A second fit for the high side, and the blast-wave for the π

**20 October 2026.** The comparison with the pre-framework K0S/π (`OldComparison/`) found that the π yield
carried a tail of up to 3.5 % above the data, integrated from a Lévy-Tsallis fitted over 0.2-1.0 GeV/c that
overshoots everything above 1 GeV/c. What was measured, model by model, is in `DESIGN_NOTES.md` section 6; this
records what changed and how it was checked.

**What changed.**

- `ExtrapConfig::high` (extrapConfig key `high`): an optional second fit, with its own `model`, `fit_range` and
  `params`, all three required. When present it alone builds the part above the last measured edge, and the
  main fit the part below the first. Replaced whole by a `bins` override; `"high": null` there switches it off.
- `ExtrapConfig` derives from `ExtrapFit` (the three keys of a fit), so a fit is declared once, and
  `HighConfig()` is the configuration with the fit replaced: whatever belongs to the species (domain, mass, what
  is added later) comes along. One reader, `loadFit`, for the main fit and for `high`.
- `BaseConfigManager::MergeParams` replaces `LoadParam` and the loop around it, which `FitConfigManager` and
  `ExtrapConfigManager` each had a copy of: the caller passed the name twice (for the message, and to find the
  slot) and an output parameter that a fixed value only half overwrote, leaving the `min`/`max` of an earlier
  pass (read by nobody: both readers branch on `isConstant`). `ReadParam` now returns a whole parameter. The
  errors name the section, the high fit included, and no longer quote the name twice (`'parameter 'n''`).
- `SpectrumExtrapolator::FitAndBuildExtrapolations`: every fit is followed at once by the histogram built from
  it, because `TF1::IntegralError` reads the covariance of the last fit. The central value, the statistical refit
  and the systematic variations all go through it.
- `ExtrapolationResult::fit` and `highFit` (`FitQuality`, with its reduced χ²) replace `chi2`/`ndf`. The warning
  of a fit that does not converge says that the part built from that fit is lost, no longer the whole
  extrapolation.
- The curves on the `_extended` spectrum: with `high`, each is drawn only on its side of where the high fit
  starts, the high one dashed, so that the plot does not show two tails on each side.
- Configuration: K0S `fit_range` 0-6.0 → 0-6.4 (the last measured edge; no number changes, see below). π: the
  blast-wave over 0.2-3.0 GeV/c instead of the Lévy-Tsallis over 0.2-1.0, the owner's choice; the two fits stay
  available, their block is in the README.

**Reviewed** by `analysis-reviewer` and `analysis-simplifier` on the first version. Taken: the duplicated fields
(both), the second hand-written reader with its own null-or-object check (both; it now uses
`RequireObjectOrNull`), the `RequireMember` + `*TryRange` pair (both; gone with the shared reader), the curve
code written twice, the reduced χ² missing for the high fit, the curves over the whole domain, the warning that
overstated. Not taken: skipping the high fit when there is no high region (a configuration asking for a fit it
cannot use; the warning that its fit failed is right), and the claim that extrapConfig.json is not in the
provenance (it is: `provenance["extrap_config"]`, the whole file).

**How it was checked**, all in a sandbox copy, compiled mode, data pp:

| test | what | result |
|---|---|---|
| regression, no `high` | the whole data chain before and after, object by object (`Macros/CompareRootFiles.cpp`), on the six spectra files it writes (2024 and 2026; offline, online, online without φ) | identical, run twice (before and after the review changes); same single warning. The other 48 files of `FinalResults` come from MC runs and are not written by the data chain: they compare equal and say nothing |
| K0S `fit_range` 6.0 → 6.4 | same comparison, the four projection caches included | the ten files identical: the last bin, 4.2-6.4, has its centre at 5.3 and was already in |
| chain with `high` | π Lévy-Tsallis 0.2-1.0 plus Lévy-Tsallis 1.0-3.0, against the run without | only the six data files with a π spectrum change, 39 objects each: the π trends and the ratios over π. The K0S and Ξ trends move by 1e-10 in their errors only, traced to the global `gaus` (*The toy fits and the saved curves*, above) |
| the same, against a standalone program | the π yields of the chain and of `SpectrumExtrapolator` driven directly | the same to the 0.02 % that the different `n` limit (20 against 30) explains |
| curves | functions attached to `_extended` | main [0, 1], high [1, 10] dashed; without `high` one curve over [0, 10] as before |
| parser | a valid block with a bin override `null` and another replacing it; `high` not an object; `model` missing; a misspelled key next to it; `fit_range` of the wrong type | each as intended: the override switches off and replaces, the four errors stop with key and section |
| parameters | a bin override fixing two parameters (one written as a string) and leaving the third; an array of two; the same inside `high`; a boolean | the override replaces by name and keeps the rest, a fixed parameter has `min`/`max` 0; the errors stop naming section (and `high`) and parameter |
| `MergeParams` | data pp before and after, the six spectra files and the four φ-fit files (the φ fit reads `fitConfig.json` through it) | identical |
| model scan | every model of the factory over 0.2-1.0 and 0.2-3.0, the ten classes, standalone | `DESIGN_NOTES.md` section 6 |
| blast-wave as default | the whole data chain | every fit converges, in every Δy window and file; no new warning; `OldComparison` 2024/old mean 0.947 → 0.971 |
| MC chain | `mc pp` before and after | the 355 files it writes (`Corrections/`) identical, same warnings: nothing there is extrapolated |

The χ² comparisons were first made from the curves in the output file and gave numbers that did not match the
fits (98 against 67): those curves were interpolated from 100 saved points (*The toy fits and the saved curves*, above). All the
numbers recorded were recomputed with the live functions.

**Not run:** the MC closure, whose files hold extrapolated π, K0S and Ξ trends but are written by no mode of
`runAnalysis.sh` today (the ones in `FinalResults` are of August, in the old layout), the
systematic variations with `high` (no configuration supplies a systematic spectrum, so `ComputeSystematics`
does not run), and the interpreted mode (it does not start in this environment, CLAUDE.md).

The model scan, the comparison of one fit with two and the parser cases are programs in
`OldComparison/extrapolation/` (`modelScan`, `twoFits`, `checks/configHigh`), with what they gave in its
`reference/`; `checks/asTheChain` there verifies that they extrapolate as the chain does, by remaking the yields
of a chain output from the configuration it records (21 October 2026: the 30 of the 2024 file within 4e-8, the
precision of the trends' `TH1F`).

---

## `trend_composition` removed: its two values were the same yield

**20 October 2026**, found while comparing the 2024 K0S/π ratio with the pre-framework one
(`OldComparison/`). The key chose how the extrapolated trend was assembled: `total_extrapolated`,
`SpectrumExtrapolator`'s `res.yield`, or `measured_plus_extrapolated`, the integral of the spectrum
plus `res.ExtrapolatedYield()`. The README described the first as "the integral of the fitted model
over the whole domain", which it is not: `Integrate` adds the measured bins and the model only below
and above them (`SpectrumExtrapolator.h`, the scheme of `YieldMean_IntegralMean`), so both values gave
the same number by construction. They could differ only for a bin with a content and a non-positive
error, which `Integrate` skips and `TH1::Integral` counts; the second value existed to reveal that case
by comparison. Measured: with each of the two, the six data spectra files (2024 and 2026, offline,
online, online without φ) identical object for object.

So the key went (a key that changes no number is better absent, CLAUDE.md), the trend takes
`res.yield`, for which the toy error is computed, and the case the alternative revealed is said directly:
`SpectrumExtrapolator` warns when a measured bin with a content is left out of the yield. The old
pre-framework macro did the same as the framework (its hand-made sum of the bins plus `YieldMean`'s
`kExtra` equals `kYield`); what differs between it and the 2024 result is `domain_range`, which extends
the 2024 yields with a model tail up to 10 GeV/c that the old macro did not have (`OldComparison/README.md`).

---

## The PhiAssocSpectra names in one place

**20 October 2026.** The names of the objects in `PhiAssocSpectra` (spectra, trends, extrapolated
trends, the two kinds of ratio) were built by hand at seven sites of `CorrelationTaskBase`, and
`ComparisonTask` spelled the directories (`Spectra`, `Trends`, `Ratios`, `Extract1D`) again. They are now in `OutputNames`, which both use; the rule that recognises a species directory (it
holds `Spectra` or `Trends`) moved there from `ComparisonTask::ListParticles`. The binning stamps' names
are not only `PhiAssocSpectra`'s - the corrections, the purities and the projection caches carry the same
three - and were spelled by hand at seventeen sites in five tasks, with a reader telling stamps from data
by the prefix `"binning_"`: they went next to `RootIO::WriteBinningStamp` (`kStampMult`, `kStampPtPhi`,
`kStampPtAssoc`, and `IsBinningStamp`, which recognises them by those names rather than by the prefix,
so that a renamed stamp cannot slip into the data), the review of the move having found the split
between the two halves of `CorrelationTaskBase`. The provenance directory's name, the same in every file
and spelled by hand at ten sites in seven tasks, went next to `RootIO::WriteProvenance` for the same
reason (`kProvenanceDir`).
Done first on the branch `plot-on-demand`, where the plotting tool reads the same layout, and brought
to main on its own because the branch stays separate for a while; the reading side of the names
(`OutputNames::Parse`, the table of kinds) stays on the branch, since only the plotter uses it.

Then the first run of the second reviewer (`analysis-simplifier`, added the same day) on the committed
change found four things the reviews had passed, all taken: every provenance call built the path to
`Provenance` and chose the read-only flag by hand (ten sites), so `WriteProvenance`, `ReadProvenance` and
`PrintProvenance` now take the scheme path and add the directory themselves; `IsBinningStamp` filtered
stamps out of directories where they never are (they sit one level up from `Spectra`, `Trends` and
`Ratios`), so it went, with the two comments that said the opposite; `SectionName` had one caller that
only forwarded to it; and the |dy| tag was formatted twice in the same loops, once for the object names and
once for the summary canvases, which are created in one place and looked up by name in another - their
names joined the builders, and `SpeciesRatio(..., bool)` became two named builders. The two reviews of that
follow-up, and a last round of both, made the provenance functions take the directory the record sits
under as a string (`owner`: no caller passed more than one level) and, for writing only, a record qualifier
instead of a free directory name (`comparison_task` passes "numerator" and "denominator", and `RootIO` names
the directories `Provenance_numerator`/`_denominator` as before); the provenance directory's name is now
`RootIO`'s own (`Detail`), and the one message that described the path does not any more;
`SectionPath()` builds {binning_name, section} where seven call sites spelled it;
`BinningUtils::FormatDeltaY` returns the title only, the name form being `OutputNames::DyTag`;
`SchemeDirName` became `SectionDirName`, since it returns Extract1D/2D and the scheme is the binning_name;
and `LoadCorrections` names itself with `GetName()`. On the branch `plot-on-demand`, the next merge of main
has to move `SectionName`, `SpeciesRatio`, `FormatDeltaY(...).first` and the provenance calls to these.
Checked on that state: `mc pp`, `data pp` and `comparison_task` (spectra, trends, ratios) identical to the
baseline of main, objects and the place of every provenance record alike.

*How it was checked.* Compiled. `data pp` with the projection cache, and `mc pp` (which writes the
corrections' stamp): the output files identical to the baseline of main, object by object.
`comparison_task` on the spectra, trends and ratios of the 2026 offline production against the 2024 one,
old and new code: the same divided objects.

---

## CorrelationCalculator: the review of the unpushed commits

**19 October 2026, two `analysis-reviewer` passes on the commits from `c1b748d` on.** What changed:

- `Inputs<H>` derives the names of the four inputs and the axes to project from the histogram type
  (`TH2` = 2D engine), instead of a separate `InputNames` and a suffix and axis list passed by each
  engine: the two had to agree and nothing made them. The names are `const` members set by the
  constructor from the base name (a first version nested a `Names` struct in `Inputs<H>`, one type
  per engine for the same four strings). Not stored, through an enum of roles and a static name
  function, was considered and set aside: the names are the ones given to the histograms, not a
  second copy that can drift, and the enum costs more than the four strings.
- `targetAxis` moved from `Request` to `Options`: it is the same for every cell of a run.
- With mixed events, a sideband comes with its mixed event. `ReadFromCache` reads the mixed-event
  sideband only when there is a sideband, and one missing stops the run (`GetUniqueOrThrow`; before,
  a warning, and the cell subtracted a sideband never divided by the mixed event). `ProjectAndCache`
  throws on a sideband without its mixed-event THnSparse; the loader (`CorrelationTask`) always loads
  the two together, so that check guards a future loader, not the present one.
- A mixed event that cannot be normalised (not positive at zero) and a fit of the normalisation that
  returns a non-zero status now go through `Logger::Warn`. Both were silent. The status is reported,
  not interpreted (some non-zero codes leave a usable minimum). The warning texts are constant, so the
  end-of-run summary counts each once with (xN) instead of filling its 500 distinct entries cell by
  cell; which cell, and the value, is in an `Info` line just before each.

The first warning found something: the Ξ sideband mixed event is 0 at Δy = 0 in 24 cells
(`DESIGN_NOTES.md` section 6). Set aside from the first review: a constructor check of `targetAxis`
against the engine (`RequireEngineSettingsSupported` already refuses the combination at `Init`) and a
second guard on the cache directory (`ProjectionCacheDir` and RootIO already cover it). Recorded, not
fixed (`DESIGN_NOTES.md` section 5): the L2 signal cache carries no stamp of the settings that made it,
and a retired key still passes when a parent declares its replacement.

*How it was checked.* In a scratch copy, compiled. `data pp` with the projection cache: 113 output files
compared with the baseline, 0 differences, and the summary shows one warning, (x24). The 2D engine with
`fit`, cache off, one block (`optimized`, `use_2d_me_normalization`): the spectra and the four projection
caches, about 15,800 objects, identical to the baseline; that run used the code before the last four
changes, which only add warnings and throws. The cache read with a sideband present and its mixed event
missing was not produced: that change is argued, not measured. After the names became members of `Inputs`, both runs
again: identical (113 files; the 2D block, now on the final code, reports the four Ξ cells, one mixed event
not positive at zero and three fit statuses 1).

---

## The mixed-event normalisation is declared

**`me_normalization`, required, `bins_at_zero` or `fit`, for both engines** (18 October 2026, at the
user's request). Before, it was fixed per engine: the 1D engine summed the bins in [-w, +w] around Δy = 0
and divided by 2, the 2D engine fitted. The 1D computation was `GetZeroAvg` written by hand, and only for
an even binning: with zero at the centre of a bin it took three bins and would have normalised to about
1.5, in silence. `GetZeroAvg` and `GetZeroZeroAvg`, removed the day before as unused, are back as the
implementation of `bins_at_zero` (1D: the bins at Δy = 0; 2D: the bins at (0, 0)); `fit` is the 2D
engine's fit, now available to the 1D engine as well, on a copy of the histogram. In the 2D engine the two
measure different things: the fit gives the value averaged over Δφ, the bins the value at Δφ = 0.

Every configuration declares `bins_at_zero` (all 134 merged correlation blocks checked), which reproduces
the previous numbers: with the 20 bins in [-1, 1] the two bins around zero are the same bins, and
2/(a+b) and 1/((a+b)/2) are the same double. Measured on a mixed event of the cache: integral/2 = 3902.5,
`GetZeroAvg` = 3902.5.

*How it was checked.* `data pp` in a scratch copy: `bins_at_zero`, 10 files and 1,360 objects identical to
the baseline; `fit` runs and moves the K0S spectra by up to 9.6% in one bin; a misspelled value is refused
with the available ones. The 2D engine with either value was not run.

---

## CorrelationCalculator made lighter

**Four commits on 18 October 2026.** (1) A Δφ projection with the 1D engine is refused at `Init`
(`RequireEngineSettingsSupported`; the rule is `CorrelationCalculator::Supports`, written once there, after a
first version that also checked it again in `ExtractCorrectedSignal`, which could never fire): `run_mode: "legacy"` and the WPDG task
always build the 1D engine, which projects on Δy only, so `projection_axis: "delta_phi"` gave Δy spectra with
no error. Δφ in the 1D engine is open (`DESIGN_NOTES.md` section 6). A QA write of a sideband that may not
exist is guarded. (2) Two helpers never called removed, `kAxisDeltaY` instead of `{3}`. (3) `Options` and
`Request` structs, built with designated initialisers, instead of four positional `bool`s and nine
parameters; `LoadOrProject` instead of four copies of load-or-project-and-cache; the mixed-event
normalisation stays per engine (`NormalizeME1D`, `NormalizeME2D`), the two differ by design. The order of
every numerical operation is unchanged. (4) A missing projection cache stops with the file, the directory and
what to do (`ProjectionCacheDir`), instead of the calculator's "no ioDir provided". Later the same day,
`LoadOrProject` was split into `ReadFromCache` (reads only) and `ProjectAndCache` (the only place the cache
is written), because the early return that kept a read histogram from being written back was easy to
miss; and the calculator's own "no ioDir" check went: the rule is the task's (`ProjectionCacheDir`), and a
null directory now stops in `RootIO::GetUniqueOrThrow`/`GetUniqueOrWarn` for every caller, instead of
being dereferenced. Checked on `data pp` with the cache (10 files) and, for one block, without it (the
spectra and the three projection caches the run writes, 7,793 objects): identical. (5) Found while doing
it, and refused at `Init` by the same check, now `RequireEngineSettingsSupported`:
`use_2d_me_normalization: true` with an engine that is 1D anyway (`run_mode: "legacy"`, the WPDG task)
filed the 1D results under `Extract2D` (`SchemeDirName` follows the key, not the engine), where a later
`optimized` run with the cache on would read them as 2D projections. No configuration had the combination.
Then, on 19 October, the engine was decided in one place: it had been worked out four times (the `Init`
check, `RunOptimized`, `RunLegacy`, `SchemeDirName`), each its own way. A task now states only
`CanBuild2DEngine()` (`CorrelationTask`: `run_mode` optimized; WPDG: no), `Engine2D()` derives the engine
from that and the key, and the calculators, `SchemeDirName` (which now follows the engine, not the key) and
the check all ask it. Checked on `data pp`: identical; the two refusals stop as before.

*How it was checked.* In a scratch copy, `data pp` only (the user's choice): the 10 output files, 1,360 objects,
identical to `00df196`, same warning summary. That run uses the 1D engine only. The 2D engine
(`optimized`, `use_2d_me_normalization`, `do_more_qa`, cache off) ran to the end on `data`; a comparison
with the baseline for it was not completed. Δφ with `legacy` and a missing `Extract2D` cache stop with their
new messages. After (5), `data pp` again, the baseline and the new code each on fresh copies of the
inputs: identical, 10 files and 1,360 objects. (A first comparison against outputs that the 2D test had
partly rewritten in `optimized` mode showed differences of 1e-12 in extrapolated errors: those were the
mode, not the code; `DESIGN_NOTES.md` section 5.)

---

## Style made uniform, and closed blocks refuse unknown keys

**Three commits on 17 October 2026, none of which changes a number.**

- *`dynamic_cast` on `TDirectory::Get`.* Three places still used `static_cast` (the event-loss
  histogram in `LoadCorrections`, the L2 signal cache in `CorrelationTask`, `ReadBinningStamp`),
  against the reason `RootIO::GetUniqueOrThrow` gives: with the wrong class under the name, a
  `static_cast` gives a pointer that passes the null check and fails on first use. The same kind of
  mismatch as the `TH1F`/`TH1D` one, which stopped the corrections.
- *`RequireKnownKeys` on every closed block* (`DESIGN_NOTES.md` section 5 says where; on 18 October
  removed again from the three whose keys are all required, where a typo already stops as a missing key), and
  `RequireString` instead of `operator[]` for the array entries of `purity_sources`, `mc_particles`
  and `purity_particles` and for `inherits`, which `CLAUDE.md` rules out (a missing key asserts, or
  reads as null in Release). The worst case it closes: a misspelled `n_sigma_integration` in
  `fitConfig.json` left the previous window in force and changed the fit in silence.
- *One way to say each thing.* The tasks named themselves by `GetName()` and by their class name
  written by hand (about 70 places), so one task had two prefixes in one log, and the WPDG task printed
  "CorrelationTask: Efficiencies loaded" from the shared base: now `GetName()`, and
  `CorrelationCalculator` names itself. Exceptions all start with `[FATAL]` (12 said `[FATAL ERROR]`,
  2 had no prefix). One form of `SetDirectory` everywhere: first made `SetDirectory(nullptr)` (43 were
  `SetDirectory(0)`, 14 `nullptr`), then, on 18 October, `SetDirectory(0)` on the user's preference for
  ROOT's own idiom. The two are the same call (the parameter is a `TDirectory*`).

*Kept as it was:* `HasMember` on the keys built from bin indices in the two config managers; the comment
there argues it (absence is the normal case), and `CLAUDE.md` says to read such a comment before
changing the line.

*How it was checked.* In a scratch copy, as for the corrections block. Every key of every current
configuration is in its set (112 objects, fitConfig and extrapConfig included), so no configuration
breaks. A misspelled `n_sigma_integation` in `fitConfig.json` and `dir_nme` in `associated_particles`
stop the run with the key, the entry and the known keys. `data`, `mcclosurewpdg` and `mcclosuregen` pp:
the 50 output files, 43,730 objects, identical to the baseline of the corrections check, and identical
warning summaries. `mc pp` (`MCTask` was touched): the 355 `Corrections` files it wrote, 5,330 objects,
identical to the ones produced on 14 October by the code before all of this. Not run: `purity`,
`comparison`, the pp_ref configurations.

---

## The corrections block

**The five correction keys of the correlation tasks are replaced by one `corrections` block,
per particle** (16 October 2026). Designed in `DESIGN_NOTES.md` section 13 with the user, C++
first and JSON after; the open parts of that plan (phases 0, 2-4, the species-specific
corrections) stay there. Two commits: the `TH1` fix below, then the block.


The keys were replaced by a structure, not only renamed: `apply_efficiency`,
`active_corrections`, `apply_efficiency_to` and `use_integrated_efficiency` were independent, so they could say
things that were not applied (`apply_efficiency: false` with an inherited `active_corrections`) and could not
say things that are needed. The structure is per particle, not per correction, for two reasons from the physics:

- **The multiplicity dependence differs between species.** From the user's efficiency × acceptance studies,
  the K0S depends on the multiplicity class, in some classes markedly, and the other species show no such
  dependence. Binned or multiplicity-integrated is therefore a choice per particle, not per correction. The
  `Corrections` file already has both versions for every particle (`_multBin{i}` and `_multIntegrated`).
- **Some corrections exist only for some species.** The Λ (and Λ̄) feed-down correction and the fraction of
  primary pions, see below.

The event loss stays outside the per-particle part: it corrects the number of events.

What the old keys allowed, and the structure makes impossible to write:

| Combination | What happens | Where |
|---|---|---|
| `apply_efficiency: false` with an inherited `active_corrections` | nothing applied, but the merged block (so the provenance) lists three corrections | `correlation_task_online_efficiency` |
| `apply_efficiency: true` without `active_corrections` | the file is opened and nothing is applied, with one `Warn` | possible |
| one `apply_efficiency_to` for efficiency *and* signal loss | "efficiency to the φ, signal loss to the associated" cannot be said | structural |
| `apply_efficiency_to` and `event_loss` | meaningless: the event loss is per event | structural |
| `use_integrated_efficiency` always required | used only with `acceptance_efficiency`, and for all particles at once | every block |
| `apply_efficiency_to` absent | falls back to "all particles" with a `Warn` | possible |

The idea is the one of `std::variant<FixedRange, NSigma>` in `IntegrationConfig`: a state that must not exist
has no representation. `std::optional<T>` is a `variant<nothing, T>`.

### C++

In `AnalysisDataStructures.h`, next to `FitConfig`:

```cpp
enum class MultDependence { Binned, Integrated };

struct AccEffCorrection     { MultDependence mult; };   // required: it changes a number
struct SignalLossCorrection {};                         // always per multiplicity class (user): no option
struct FeedDownCorrection   {};                         // Λ, Λ̄: not implemented, see below
struct PrimaryFractionCorrection {};                    // π: not implemented, see below

struct ParticleCorrections {
  std::optional<AccEffCorrection> accEff;
  std::optional<SignalLossCorrection> sigLoss;
  std::optional<FeedDownCorrection> feedDown;
  std::optional<PrimaryFractionCorrection> primaryFraction;
};

struct CorrectionsConfig {
  std::string file;
  std::map<std::string, ParticleCorrections> perParticle;  // "Phi", "K0S", ...; absent = not corrected here
  bool eventLoss{false};                                   // per event, not per particle
};
```

In `CorrelationTaskBase`, `applyEfficiency`, `useIntegratedEfficiency` and the two local lists of
`LoadCorrections` (`activeCorrections`, `effParts`) become one member, `std::optional<CorrectionsConfig>
corrections` (empty = no correction). One function, `ParseCorrections`, turns the JSON into it, as
`FitConfigManager` does for the fits; after that the task reads the structure and never the JSON.

What follows from it, deriving instead of storing:

- `LoadedCorrections` holds one vector instead of two. Today `h1Corrections` and `h1CorrectionsEffMultInt`
  are both built and the consumers pick one with `useIntegratedEfficiency` (`CorrelationTask.h` and
  `CorrelationTaskBase.h`, where `phiCorrs`/`assocCorrs` are set). With the choice known per particle at load
  time, the one vector already holds the right variant, and the choice leaves the consumers.
- For each particle the efficiency is fetched only if `accEff` is set (with `_multBin{i}` or
  `_multIntegrated` by `mult`), the signal loss only if `sigLoss` is set; `combineHists` already multiplies
  what it gets.
- The event loss is `corrections && corrections->eventLoss`.
- Phase 3 checks `perParticle[p].accEff` against the particles corrected online.

### JSON

```jsonc
// correlation_base_settings
"corrections_file": "../DataFile/pp/DeltaY/Corrections/Corrections.root",
"corrections": {
  "event_loss": true,
  "particles": {
    "Phi": { "acceptance_efficiency": { "mult": "integrated" }, "signal_loss": true },
    "K0S": { "acceptance_efficiency": { "mult": "integrated" }, "signal_loss": true },
    "Xi":  { "acceptance_efficiency": { "mult": "integrated" }, "signal_loss": true },
    "Pi":  { "acceptance_efficiency": { "mult": "integrated" }, "signal_loss": true }
  }
}

// correlation_task_online_efficiency
"corrections": null

// correlation_task_online_efficiency_no_phi
"corrections": {
  "event_loss": true,
  "particles": { "K0S": { "signal_loss": true }, "Xi": { "signal_loss": true }, "Pi": { "signal_loss": true } }
}

// correlation_task_offline_efficiency_2024
"corrections_file": "../DataFile/pp/DeltaY/Corrections/2024_Corrections.root"
```

The shape follows how the blocks are organised today: a base block declares everything, the mode blocks
(`offline_efficiency`, `online_efficiency`, `online_efficiency_no_phi`) change the corrections, the year
blocks (`..._2024`, `..._2026`) change only the file. And it follows from how `inherits` merges (see
`CLAUDE.md`, *Configuration*): at the top level only, a child replacing a key and never removing it.

- **"No correction" is a value, `null`.** If it were the absence of `corrections`, a mode block could not
  express it: the merged block would carry the parent's `corrections` and apply everything, without an error.
  `null` replaces the parent's object, and the provenance records `null`, which is what was applied.
- **A child's `corrections` replaces the parent's whole.** A mode block that lists only the K0S gets only the
  K0S: the particles are not merged one by one. Every mode declares its whole set, and nothing is half
  inherited, which is the defect of the first row of the table.
- **The file is outside the block** (`corrections_file`), because it changes per year while the corrections
  change per mode. Inside, a year block that only changes the file would have to repeat all the corrections
  of its mode. `corrections_file` is read only when `corrections` is not `null`; `online_efficiency` inherits
  it and cannot drop it, so there it is not an error.
- **Per-particle corrections are a block of their own**, checked against `associated_particles`, not keys
  inside each entry of `associated_particles` (where section 2 puts other per-particle settings): the
  corrections change with the mode and the particle list rarely, and with a top-level merge every mode block
  would have to repeat the whole list.

Parsing rules. Each one throws, with the key and what to write instead:

- `event_loss` is required inside `corrections`; `mult` is required inside `acceptance_efficiency`, read with
  `ResolveEnum` so that "Available: binned, integrated" comes from the same table. Both change a number.
- `signal_loss` is a `bool`, since it has no option; `true` reads better than `{}`.
- `particles` is required (`JsonConfig::RequireObject`, added for it on 17 October): `{}` when only the event loss
  is applied, absent is an error, because absent could be a child block that replaced `corrections` and
  forgot its particles. First written as `TryObject` plus a hand-written throw, which read as optional.
- Every entry of `particles` activates at least one correction (not correcting a particle is done by not
  listing it), and the block as a whole activates at least one. A name must be `Phi` or one of the
  `associated_particles`.
- `feed_down` and `primary_fraction` are accepted only for the species that have them (a table: Λ and Λ̄ for
  the first, π for the second), and until they are implemented they throw.
- `null` is handled before the typed readers: in `JsonConfigHelpers` a key present with the wrong type is
  fatal, so `TryObject` on `null` would stop the run. A small helper, or an explicit check.
- The old keys (`apply_efficiency`, `active_corrections`, `apply_efficiency_to`, `input_efficiency_file`,
  `use_integrated_efficiency`) throw, naming the key that replaces them. *Removed on 19 October* with
  `RejectReplacedKey`: since `corrections` is required, a configuration not migrated already stops at the
  missing key, and a check per renamed key would pile up with every rename. An old key left next to the new
  ones is the general unknown-key case (`DESIGN_NOTES.md` section 5).

### Migration: numbers first, physics after

The translation must leave every number unchanged. Today `use_integrated_efficiency: true` holds for every
particle, so the first version writes `"mult": "integrated"` for all of them, the K0S included; moving the K0S
to `binned` is a physics change of its own, made afterwards and visible as such. Check: run `mc pp` and
`data pp` before and after, copying the output directory each time (the two modes and the two runs write the
same files, see `CLAUDE.md`), and compare the objects: they must agree.

### The file format: JSON stays

Asked on 16 October: with a compiled mode, could the configurations be YAML? Possible, kept apart:

- It does not solve this problem. The impossible states go with types and parse-time checks, in any format.
- The interpreted mode is the default, so the YAML library would have to load in Cling too, and the ALICE
  stack here has neither yaml-cpp nor rapidyaml (only Homebrew's `libyaml`, a low-level C library): a new
  dependency, also on lxplus and other machines.
- It would cost much: the format is tied to the three layers of `JsonConfigHelpers`, to the provenance
  (`JsonConfig::Serialize`), to `inherits` and to 19 files. YAML's merge key `<<` is shallow as well, so the
  `inherits` design questions would stay.
- What YAML gives in practice, comments, rapidjson gives at almost no cost: `kParseCommentsFlag` (and
  `kParseTrailingCommasFlag`) in `ParseStream`, which today is called without flags (`WorkflowManager.h`,
  `BaseConfigManager.h`).

The two point in the same direction, though: the more the parsing is concentrated in functions like
`ParseCorrections` that turn the file into C++ structures, the less the format is spread across the tasks,
and the easier a change of format becomes one day.

### Implemented as designed, with three details

- The parser is `CorrelationTaskBase::InitCorrections`, called from `InitCorrectionsAndExtrapolation`
  (after `associated_particles`, which it checks names against); `LoadCorrections` takes no JSON.
  The old keys were refused in `InitCommonFlags`, before anything is loaded (removed on 19 October, see
  the parsing rules above).
- The in-memory names of the combined histograms are those of the two former vectors
  (`h1Corrected_<p>_multInt<i>` for the integrated variant, `..._multBin<i>` otherwise), so nothing
  that could reach an output changed name.
- The run log lists, per particle, what is corrected ("K0S corrected for acceptance x efficiency (per
  multiplicity class)."), every particle of the analysis that is not listed ("Phi not corrected here (not
  listed in \"corrections\")", added on 17 October: a particle left out by mistake would otherwise show only as a
  missing line; `Info`, not `Warn`, since leaving it out is how a configuration asks for it), and whether the
  event loss is applied: the configuration in force is readable from the log as well as from the provenance.

## Corrections read as TH1, not TH1F

`MCTask` writes the efficiency and signal-loss histograms as `TH1D`; `LoadCorrections` asked for
`TH1F` (`GetUniqueOrThrow<TH1F>`), so every correlation task with corrections stopped on every
`Corrections` file produced since. On 16 October 2026 `data pp` and `mcclosurewpdg pp` did, with the
files of the 14 October `mc` run (all `TH1D`; the multiplicity-integrated ones are in
`AccEff/MultBin`, where they are looked for). `LoadedCorrections` holds `TH1` and the files are read as
`TH1`. The values are read with `GetBinContent`, so the class only decided the stored precision. A
separate commit, before the corrections block, so that the block could be checked against a chain
that runs at all.

## How the corrections block was checked

*Where.* Not in the working copy: in a scratch area holding two clones of the repository (one with
the `TH1` fix only, one with the block) and, for each, a copy-on-write clone of `DataFile/pp` (`cp -c`,
APFS: no extra space, and a write touches only the clone). Both runs therefore started from the same
inputs, and the user's files were never written. ROOT 6.36.10 (`ROOT/latest-master-o2`), compiled
mode: the interpreted one does not start in this environment (`CLAUDE.md`, *Cling constraints*).

*The baseline.* The current code failed on three of the four configurations that read corrections:
`data` and `mcclosurewpdg` on the `TH1F`/`TH1D` mismatch (hence the fix first), `mcclosure` on its
own configuration (`DESIGN_NOTES.md` section 5; left out on the user's request). The baseline is the
code with the `TH1` fix, on `data pp`, `mcclosurewpdg pp` and `mcclosuregen pp`; all three ran to the
end (16, 115 and 212 s). `mc pp` was not run: `MCTask` is untouched, and the cloned `Corrections`
files are the inputs of both sides.

*Numbers: identical.* Every ROOT file the runs wrote (50: the spectra files in `FinalResults` and
the projection caches in `ProjData`) compared object by object with `Macros/CompareRootFiles.cpp`:
43,730 objects (histograms of every dimension, `THnSparse`, graphs, strings), contents, errors and bin
edges equal, exactly. Canvases and the `Provenance` directories (timestamp, commit, merged block) are
skipped. The warning summaries of the runs are identical too (`mcclosurewpdg`: 38 warnings, 7
distinct, on both sides). The tool was checked both ways: a file against itself gives no difference,
the 2024 against the 2026 corrections give 354.

*Configurations: equivalent.* The JSON was migrated by a script (not kept: one-off) that rewrites only
the correction keys of the blocks that declared them (`json.dumps(indent=4)` reproduces the seven files
byte for byte, checked before writing) and then, for every block of every (master, base) pair the
run script builds - 12 pairs, pp and pp_ref, `Datapp_OnlineEffonly` alone and with its base - resolves
`inherits` as `MergeTaskConfiguration` does and compares what the old and the new merged block apply:
143 correlation blocks, no difference, no old key left. This is also what covers the configurations
that were not run.

*Refusals: each one tested.* Twelve configurations, each a block inheriting from a working one and
changing one thing, all stop at initialisation with their message: an old key (`apply_efficiency`,
`input_efficiency_file`), `particles` or `event_loss` missing, `"corrections": "none"`, an unknown
particle (`Lambda`, not among the associated), a misspelled key (`signal_los`), `"mult": "binnned"`,
an empty particle entry, a block that applies nothing, `feed_down` on the K0S ("does not apply"),
`primary_fraction` on the π ("not implemented yet"). On 17 October, with `RequireObject`: `particles` absent
and `particles: []` stop, `"particles": {}` with `"event_loss": true` runs to the end applying the event loss
only (the log lists no particle).

*The new path: run.* `"mult": "binned"` had never run, since every configuration uses `integrated`.
The same task with the K0S switched to binned: the K0S spectra, trends and ratios change (trends by up
to 2.4%), nothing else does, with one exception recorded in `DESIGN_NOTES.md` section 5: the errors of
the extrapolated trends of Pi and Xi move at the 1e-12 relative level.

*Repeated after the follow-ups.* On 17 October the same three configurations were run again at `16c1252`
(after `RequireObject`, the "not corrected here" log lines and the signal-loss flag) and compared with the
same baseline: the same 50 files and 43,730 objects, identical, no file more or less, identical warning
summaries.

*Not checked.* `mcclosure` (configuration broken), the pp_ref configurations and `Datapp_OnlineEffonly`
by running (by the equivalence script only), the interpreted mode, and anything with
`"mult": "binned"` beyond the one task above.

---

## The TF1 fitter was retired in favour of DynamicRooFitter

`FitPhiSignalAndBkg` was a legacy TF1-based implementation tightly coupled to a single
model (`Voigt + BkgSourav`), hardcoded 7 parameters, magic index split `4`, hardcoded
integration ranges `[1.0, 1.05]` and `[1.06, 1.08]`, and lacked modern RooFit covariance
error propagation and pull/ratio diagnostic plots.

Rather than building an artificial `IPhiFitter` abstract interface to bridge two fundamentally
different designs (violating YAGNI since only one fitter is needed), `FitPhiSignalAndBkg.h`
was retired to `OldCodes/FitPhiSignalAndBkg.h`.

`DynamicRooFitter` is now the sole, unified fitting engine across both `PhiFitTask` and
`PurityTask`. This eliminated the 50-line branching in `PhiFitTask::Execute`, removed manual
TF1 allocations in the loop, and made `PhiFitTask` completely configuration-driven.

---

## SpectrumExtrapolator replaced YieldMean

`SpectrumExtrapolator` replaced the ALICE-standard `YieldMean` path. Until
`6c78e4c` (8 August 2026) both were reachable through `use_legacy_extrapolation`;
the record below is what established that they agree, and it is what the removal of
the legacy path rests on. `YieldMean.h` now lives in `OldCodes/`. The open question
about the quality of the fit itself is in `DESIGN_NOTES.md`, section 6.

**`SpectrumExtrapolator` — fixed, kept here for the record.** The scratch
histograms (`hlo`, `hhi`, `hInt`, `hMean`, `hInt_tmp`, `hout_rnd`, `hout_cohrnd`)
now call `SetDirectory(nullptr)`. They were being created while `gDirectory` was
the spectra output file, so they were written into `PhiAssocSpectra.root` and
warned "Replacing existing TH1" once per toy iteration.

**`SpectrumExtrapolator` did not reproduce `YieldMean`.** It was written to
replace the legacy path, and both were reachable from the same `if`, but nobody
had checked they agreed. Five parameters had drifted, none of which looks wrong
when read on its own:

| | legacy | was | effect |
|---|---|---|---|
| fit option | `"0QI"` (argument) | `"R0V"` | no `I`: function evaluated at the bin centre instead of integrated over the bin |
| domain | `domain_range` (argument) | members `0…10`, never set | `domain_range` controlled only the TF1's range |
| domain vs fit range | widened to cover `minfit`/`maxfit` | not widened | measured region between the two dropped from the yield |
| coarse window | `0.75…1.25×` | `0.5…1.5×` | coarser sampling at 1000 bins either way |
| fine window | `±10·RMS` | `±5·RMS` | clipped the tails, and that RMS *is* the quoted statistical error |

The rule this settles, since the class exists to replace the reference: **what
`YieldMean` takes as a parameter is settable here with the legacy value as the
default; what is hardcoded there is hardcoded here to the same number.** The three
values the caller supplies (`kFitOption`, `kLoPrecision`, `kHiPrecision` in
`CorrelationTaskBase`) are now written once and passed to both branches, so the
two cannot drift again without someone editing the line that feeds both.

The fit option in particular is a member and not a constant *because* it is an
argument in the reference — hardcoding it would have been a second, quieter
divergence of the same kind.

**A sixth divergence, in what the two branches returned.** `ExtrapolationResult`
had a field `extrapolatedFraction` that the legacy filled with `YieldMean`'s
`kExtra` — an *integral* — while `SpectrumExtrapolator` filled it with
`extra/integral`, an actual fraction. Same field, two units, selected by a JSON
flag. No result was wrong because nothing read it yet; the first reader would have
been.

The field is now `extrapolatedYield`, absolute, because that is what both
extrapolators compute, and `ExtrapolatedFraction()` derives the ratio. Storing the
fraction would have meant one of the two branches converting on the way in, which
is exactly where they had already diverged once.

Worth noting where this one came from: it was not found by comparing the two
implementations line by line, but by asking what each `extra` *meant*. The five
above are parameters, and a parameter that differs shows up in a diff. A field
that means two things does not.

**Why there are two fits again.** The upstream `YieldMean.C` takes `hstat` and
`hsys` and builds `htot = sqrt(stat^2 + sys^2)`. It then fits twice on purpose: the
central value comes from the fit weighted with the *total* error, because that is
the measurement, while the toy MC must start from a fit weighted with the
*statistical* error alone - otherwise the statistical uncertainty it returns
carries the systematic inside it.

`hsys` had been removed from our copy, so `htot` was a bare clone of `hstat`, the
two fits ran on identical data, and a single fit was correct. That is what
`SpectrumExtrapolator` was written with, and the condition was recorded here as
"the day systematics come back, the two-fit structure is the thing to restore".

It has now been restored, conditionally: `MakeTotalErrorSpectrum()` returns null
when no systematic spectrum was supplied, `hCentral` falls back to the measured
spectrum, and the second fit is skipped. So with no `hsys` - today's state - the
behaviour is exactly the single-fit one that was verified number for number against
the reference. With `hsys` the two fits separate again, as upstream.

Upstream: https://github.com/alisw/AliPhysics/blob/master/PWGLF/SPECTRA/UTILS/YieldMean.C

**The toy MC seed.** The legacy path uses `gRandom`; the new one had a member
`TRandom3` seeded with `SetSeed(0)`, which in ROOT does not mean seed zero - it
draws one from `TUUID`. The quoted statistical error therefore changed on every
run over the same files. Now seeded with 4357, `TRandom3`'s own default and
`gRandom`'s starting point, with `SetRandomSeed` to override.

This splits the comparison between the two branches in two, and the split is not
a detail:

- **Yield and mean p_T involve no random numbers.** They come from the fit and
  from `Integrate`, both deterministic. They must agree; a difference there is a
  real bug.
- **The two statistical errors are the RMS of 1000 toys drawn from different
  streams.** At N = 1000 the RMS estimate itself carries about 1/sqrt(2N) ~ 2%,
  so a few percent of disagreement is expected even from identical code.
  Comparing them for exact equality is chasing noise.

`Integrate` was checked line by line against `YieldMean_IntegralMean` and matches,
`err <= 0.` guard included; the error extraction (`central * sigma / mu`, `gaus`
from `gROOT`) matches too.

**A seventh divergence, and the one that was actually breaking the run: the fit
model was cloned.** `SpectrumExtrapolator` used to do
`fitModel->Clone(Form("%s_clone", ...))` and fit the copy. Under Cling a `TF1`
built from a compiled function pointer (`LevyTsallisFunc`) does not survive that
clone as a usable function, and everything downstream followed: GSL `qags`
roundoff errors from integrating it, a fit that never converged in 10 trials, and
`IntegralError` returning 0 - which the `err <= 0` guard in `Integrate` then turned
into an extrapolation contributing exactly nothing. The reported yield was the raw
data integral, with no error raised.

Note what the earlier hypotheses were: the `"I"` fit option, and a fit range too
narrow for the free parameters. Both were wrong. The clone was found by asking who
owns the TF1, not by looking at the numerics.

The clone is gone; the model is fitted in place, as `YieldMean` does. That is the
right choice *here* - not in general - because the caller creates the model fresh
per spectrum (`ExtrapolationModelFactory::CreateModel`, a local `unique_ptr`), so
there is nothing to protect, and it wants the fitted parameters back: it prints
them and attaches the curve to the extended spectrum it writes. With a clone it
silently got the initial guess in both places, so the saved plots carried an
unfitted curve. The clone was also never deleted, and since every `TF1` registers
in `gROOT->GetListOfFunctions()`, each spectrum added another object under the
same name - the `TCanvas` trap in `CLAUDE.md`, on a different global list.

**Reproduction verified.** Data pp, both branches on the same two spectra:

| | legacy | new |
|---|---|---|
| yield (1) | 4.05537 | 4.05537 |
| mean pT (1) | 0.445138 | 0.445138 |
| chi2/ndf (1) | 157.241/3 | 157.241/3 |
| yield (2) | 4.08771 | 4.08771 |
| mean pT (2) | 0.449833 | 0.449833 |
| chi2/ndf (2) | 36.9467/3 | 36.9467/3 |

Every printed digit, and the raw integral, the extrapolated part and the integral
of the fitted function agree as well. The chi2 is the strongest of these: equal
yields could in principle come from slightly different fits compensating inside the
integrals, but an identical chi2 to six digits means the two branches performed the
same fit, not merely reached the same total.

The extrapolation also cross-checks internally. YieldMean prints its own running
totals, and for the first spectrum they decompose as low = 4.03671 - 2.81778 =
1.21893 and high = 4.05537 - 4.03671 = 0.01866, summing to 1.23759 against the
1.2376 reported as the extrapolated part - while 1.21893 is exactly the
`Fitted function [0, 0.2]` line, which is computed by direct integration of the TF1
rather than by summing a 0.01-wide histogram. The diagnostic added to find the bug
validates itself against a number obtained another way. The statistical errors differ by -2.1%,
-7.0%, +2.4%, -4.1%: varying sign, which is the signature of toy noise rather than
of a systematic difference. Roughly 2.2% is expected from 1/sqrt(2N) at N = 1000;
the outlier is larger because the errors are not the bare RMS but sigma/mu of a
Gaussian fitted to the toy distribution, which carries its own uncertainty.

---

## YieldMean.h defects

**`YieldMean.h` — fixed, kept here for the record.** All four defects listed in
earlier versions of this file are gone: the `htotextra` block that read ~34 doubles
out of an 11-element array, and its uninitialised twin when `part` matched none of
K0S/Pi/Xi, were removed with the block itself; `binlo`/`lo` in
`YieldMean_LowExtrapolationHisto` now start at -1 with a guard, and the comment
above it records what the bug was; `using namespace std;` is gone from the header;
and the default arguments now appear only in the declaration.

Worth keeping written down because this is ALICE-standard code kept as received:
the next person to sync it from upstream will reintroduce all four.

The header is now `OldCodes/YieldMean.h`, out of the chain but kept as the
reference the extrapolator was validated against.

---

## PhiFitTask, legacy fitter branch — obsolete

**`PhiFitTask`, legacy fitter branch.** Only parameters 1, 2 and 3 of
`VoigtBkgSourav` are set. Parameter 0 (the Voigtian normalisation) and 4–6 (all of
`BkgSourav`) start at zero, so Minuit starts from an identically null function.
This is a plausible cause of the poor convergence in that path.

Never fixed: the branch went with `FitPhiSignalAndBkg.h` in `0a371bc`. Kept in
case that fitter is ever revived from `OldCodes/`.

---

## CanvasManager

**Centralized Canvas Management (`CanvasManager`) & Data Decoupling.** Replaces the proliferation of
hardcoded `unique_ptr<TCanvas>` members across data structures (`LoadedMC`, `ParticleTask`),
and standardizes figure generation and output across the framework:
- **Separation of Concerns & Task Integration**: Data structures (`LoadedMC`, `ParticleTask`)
  are pure physics containers holding only histograms, binnings, and maps. Canvases are completely
  decoupled from them and owned exclusively by tasks (`MCTask`, `PurityTask`, `CorrelationTaskBase`).
  In `CorrelationTaskBase`, this eliminated raw nested 2D canvas matrices (`spectraCanvases`,
  `spectraExtrapCanvases`) and manual nested loops in `Terminate()`.
- **Registry & Collision Defense**: Manages canvas ownership in a dictionary, preventing collisions in
  ROOT's global list (`gROOT->GetListOfCanvases()`) when tasks or loops re-run. Throws on foreign colliding
  canvases to prevent memory corruption.
- **Path-Aware Hierarchical IO**: Each canvas can register its target directory path inside ROOT output files
  (e.g., `{binningName, "AccEff", "Summary"}` or `{binningName, "Extract1D", pName, "Spectra"}`). Tasks flush
  all diagnostics to their proper subtrees with a single `canvasManager.WriteAll(file)` call.
- **Layout Factory**: HEP/ALICE standard ratio pads (70% top, 30% bottom with seamless zero-margin
  junction, automatic font scaling for smaller pads, unity lines), and N×M grid pads.
- **Visual Standards**: ALICE watermark headers (`#bf{ALICE} <status>`, collision system/energy),
  transparent legends (`CreateLegend`), and histogram visual styling (`SetHistogramStyle`, consolidated from
  `HistogramUtils` to centralize all presentation rules).
- **IO & Export**: Static and instance methods for writing into `TDirectory` trees via `RootIO`,
  individual image/PDF export with auto-created directories, and multi-page PDF generation
  (subsuming standalone macros like `esporta_fit.C`).
- **DynamicRooFitter Integration**: `DynamicRooFitter::DrawFit(pad)` and `DrawFit(ratioCanvas)` draw the
  fit into pads the task owns: data, model, components, integration and sideband windows, legend, and
  optionally the pulls (`(Data - Model) / Error` on the bottom pad of a ratio canvas, with $\pm 2\sigma$
  reference lines and font scale compensation). The fitter creates no canvas and writes nothing: the task
  creates the canvas through its `CanvasManager`, adds the ALICE label, and calls `WriteAndDestroy`, so
  each fit canvas is destroyed as soon as it is written. (It used to be `SaveFitToCanvas`, which did all
  of that itself and left every fit canvas registered in the task's manager until the task ended.)
- **Statistics Box (`TPaveStats`) Control**: The stats box is disabled by default (`optStat = 0`)
  across `CanvasManager` to adhere to clean ALICE publication standards. Methods `SetOptStat`,
  `SetStats`, `RemoveStats`, and `EnableStats` provide explicit opt-in control when diagnostic numbers
  are desired.
- **Multi-Histogram Automatic Y-Range Determination (`AdjustYRange`)**: Scans arbitrary collections of
  1D histograms (via `std::span<TH1* const>`, initializer list, or pad primitives via `AdjustPadYRange`)
  to find the global minimum and maximum, accounting for bin error bars, strictly positive bins in log scale,
  and customizable headroom (`topMargin`, `logHeadroom`). Strictly filters 1D histograms (`GetDimension() == 1`),
  ignoring 2D/3D objects to prevent corrupting physical phase-space Y-axes. Replaces manual boilerplate
  like `std::max(h1->GetMaximum(), h2->GetMaximum())` across tasks. Summary canvases declare their
  adjustment once, at creation, with `SetAutoYRange(canvas, opts)`; the manager applies it whenever it
  writes or exports them. It replaced adjustments made in `Terminate` on names rebuilt there, which a
  `HasCanvas` guard skipped in silence whenever the rebuilt name drifted from the original.

---

## Compiled mode

**A second way to run the same chain, and what building it found.** `runAnalysis.sh`
takes `--compile` (`-c`) next to the default `--interpret` (`-i`). Both run
`RunAnalysisChain`, which moved out of `PhiStrangenessCorrelation.C` into
`RunAnalysisChain.h` so the two entry points cannot drift: the macro keeps `gSystem`
because a macro cannot turn a return value into an exit status, `main.cpp` returns
the status directly. `CMakeLists.txt` builds `build/phistrangeanalysis`.

*Why.* A compiler reads every line; Cling reads the ones a run reaches. The missing
includes, missing `inline` and duplicated default arguments that used to be found by
hand were the case for it, and the first build confirmed it: 14 errors, all in
`CanvasManager.h`. This is not Cling being lenient: loading the version before the
fix (`8248114`) in Cling reports both kinds below as well. The header had not been
run since it was last edited, and the compiled build checks every header whether or
not a run goes through it.

- **Nested struct as a default argument.** `PadMargins` and `YRangeOptions` were
  nested in `CanvasManager` with default member initialisers and used as
  `= YRangeOptions{}` in members of `CanvasManager` itself. Standard C++ rejects that
  (the initialisers are needed before the enclosing class is complete), and so
  does Cling. They now live in `namespace CanvasInfo` and `CanvasManager` re-exports them
  with `using`, so `CanvasManager::PadMargins` and `CanvasManager::YRangeOptions`
  still work for every caller. They stay in `CanvasManager.h` and not in
  `AnalysisDataStructures.h`: they are the interface of the canvas manager, not
  physics data, and `RatioCanvas` stays nested because nothing constructs it as a
  default argument.
- **Instance overloads that could not exist.** `AdjustYRange` (twice) and
  `AdjustPadYRange` had a non-static "instance wrapper" with the same parameter
  types as the static function, which C++ forbids - and each would have called
  itself. Nobody used them: the one outside caller uses the static form, and a static
  function can be called on an instance anyway. Removed.
- **Batch mode.** `root -b` made the interpreted run headless; the binary is not
  started that way and `CanvasManager` creates canvases, so `main()` calls
  `gROOT->SetBatch(kTRUE)`. Process-wide ROOT state belongs to the entry points.

*Decisions about the build.*

- CMake takes the standard from `ROOT_CXX_STANDARD` and links only the imported
  `ROOT::` targets, which also makes ROOT's headers system headers and keeps their
  warnings out of the log. A missing ROOT stops with a message saying what to load.
- The script rebuilds through CMake when the binary is stale, and does not decide
  staleness itself. A `find -newer` check was considered and rejected: CMake already
  holds the exact header dependencies, so a hand-written check could only be cruder.
  Running the binary merely because it exists was rejected too - it executes an old
  version of the code with no sign that it is old. Build output goes to
  `Logs/build.log`; a failed build is shown and the analysis is not launched.
- CMake keeps the ROOT it found in `CMakeCache.txt` and does not notice a different
  one. The script compares the cached `ROOT_DIR` with `root-config --prefix` and
  recreates `build/` when they differ (tried by switching between two installed
  ROOT versions and back). A change of compiler is not checked.
- Compile time is 4.5 s for the single translation unit, so nothing was split; see
  `DESIGN_NOTES.md` for the options if that changes.

*State when this was written (14 October 2026).* The build passes under
`O2Physics/latest-master-o2` (ROOT 6.36.10, C++20, libc++), with two warnings left
in `SpectrumExtrapolator.h` (`binlo`, `binhi` set but unused). The `mc pp` run was
done in both modes after these changes, both completed successfully, and the
results coincide. Only `mc pp` was compared: the other keywords (`data`, `purity`,
the closure tests, `comparison`) have not been run in compiled mode.

---

## String construction with std::format

**Error messages and index-built names moved from `+` to `std::format`.** About 80
sites in 18 headers: every `throw std::runtime_error("..." + x + "...")`, and the
object, canvas, key and RooFit-variable names built from bin indices
(`"h1" + name + "_multBin" + std::to_string(i)` and its siblings). Plain `{}`
placeholders only, which Cling accepts (see *Cling constraints* in `CLAUDE.md`); no
format specs, so nothing here needs the `snprintf` workaround.

*What was left alone on purpose:* paths (`dir + "/" + file`), suffix appends
(`histNameBase + "Signal2D"`), canvas names and titles without an index, and
`strftime`/`FormatEdge`. There `+` reads as well as `format` does and rewriting it
would only add risk. The names built from indices are the same ones DESIGN_NOTES
item 4 proposes to change, so a later change to interval-based naming starts from
the `std::format` form.

*Two things that differ from a literal translation:*

- `HistogramUtils::RebinToTargetBinning` reported bin edges with `std::to_string`,
  which prints `0.100000`. They now go through `BinningUtils::FormatEdge`, like
  every other message that prints an edge. Plain `{}` was rejected for these: it
  prints the shortest round-trip form, which for an edge computed by arithmetic can
  be `0.30000000000000004`.
- `rapidjson`'s `GetParseError()` is an enum, which `std::to_string` takes and
  `std::format` does not, so it is cast to `int`.

*How it was checked.* The compiled build passes and all headers load in Cling.
Every replacement was then compared with the code it replaced by reducing both to
the string they build (literals and argument expressions, in order): of 84, the only
differences are the two intended ones above and the removal of the `iStr` local in
`CorrelationTaskBase::LoadCorrections`. The check is static. The analysis itself was
**not** re-run end to end after this change, and `mc pp` exercises only part of the
sites: `purity`, `data`, and the closure keywords reach the others.

---

## Logger

**A minimal `Logger::` instead of spdlog.** `Logger.h` had been an empty class wrapping
an include of spdlog that nothing used. What the framework needs from a logger is
small: levels, one line format, and a count of the warnings of the run that is printed
at the end, because a warning buried in a log of a million bytes is as good as silent.
Sinks, rotation and asynchronous output are not needed by an offline batch analysis.

*Why not spdlog* (v1.17, tried on ROOT 6.36.10 with Cling):

- With `SPDLOG_USE_STD_FORMAT`, the setting the old `Logger.h` asked for, it does not
  load in Cling at all: its own `fmt_helper.h` formats with `{:02}`, which hits the
  same `consteval` limit as any numeric spec (`CLAUDE.md`, *Cling constraints*).
- With its bundled `fmt` it loads and even accepts `{:.2f}`, but costs **11 s on every
  interpreted run** (11.4 s against 0.2 s without it); about 1 s more to compile.
- The old header's premise, "spdlog at no runtime cost", held only for the compiled
  mode.

*Design.* `Logger::Info/Warn/Error/Debug(fmt, args...)` format with `std::vformat`, which
reads the format string at run time and therefore accepts numeric specs under Cling
(checked: `{:.2f}`, `{:g}`, `{:08.3f}`, `{:04d}`). The price is that a bad format
string is found when the line is printed and not at compile time. If it throws, the
logger catches it and prints the line with a note, so a diagnostic never becomes a
failure. In the compiled mode that was checked; in Cling it depends on exception
handling being available (see *Cling constraints*: on macOS arm64 with ROOT 6.36 a
throw through JIT-compiled frames is a known upstream bug) and was checked in a context
that includes the project headers.

- INFO and DEBUG go to stdout, WARN and ERROR to stderr; `runAnalysis.sh` merges them
  with `2>&1 | tee`.
- A warning is counted even when the threshold hides it: silencing the output must not
  hide from the summary that something was said.
- The summary lists each distinct warning once with `(xN)`, up to 500 distinct
  messages, and a run with no warnings prints `[SUMMARY] No warnings.` so that the
  absence of a summary cannot be mistaken for a clean run.
- Single-threaded by design, and says so.

*Status.* `RunAnalysisChain` prints the summary on success and on a fatal error. The
tasks have not been migrated, so the summary is still empty in practice; see
`DESIGN_NOTES.md`.

---

## The fit configuration: required sections and observable

**A missing section of `fitConfig.json` throws, naming the section.**
`FitConfigManager::GetConfig` used to print `[ERROR] Particle section not found` and
return a half-built `FitConfig`: names empty, models empty, and `obs.min` / `obs.max`
never set (no initial value, so `OptionalDouble(..., config.obs.min)` could read an
uninitialised `double`). The fit then failed on RooFit's own message ("Each RooFit
object needs a name"), which names neither the section nor the file. Reproduced with a
copy of the file without `k0s` before the change.

- `GetConfig` throws before the lookup `document[particle]`: rapidjson's `operator[]` on
  a missing key asserts, and in a Release build (asserts compiled out) returns a null
  value in silence. The check is the first of the function's own, not an extra: it has to
  verify what it reads, the `observable` block and its keys included, and none of that can
  be verified without calling it.
- A section is required and not defaulted: unlike `ExtrapConfigManager`, where a missing
  section means "skip this particle", here there is no sensible model, observable or
  range to invent. The task configuration decides which sections are needed (the purity
  task by each particle's `purity_key`, the trigger fit by `"phi"`), `fitConfig.json` has
  to follow, and a mismatch between the two is what this catches.
- The `observable` block, its `name`, `min` and `max` are required, and an empty or
  inverted range is rejected; `title` stays optional. `ObservableConfig::min` and `max`
  now also have an initial value, so no default-constructed `FitConfig` holds an
  indeterminate number.
- `ExtrapConfigManager::GetConfig` throws as well for a particle without a section: its
  only caller already skips such particles through `HasConfig`, so the branch is
  reachable only by misuse and used to return an empty config that failed later.

*Considered and dropped:* a check in each task's `Init` (`PurityTask`, `PhiFitTask`) that
asks for every section before the first fit. It does not change the outcome, since the
run stops either way, only when: a missing section that is not the first particle's would
show up after the earlier particles have been fitted. Dropped as redundant with the throw
in `GetConfig`. If purity runs become long enough for that to hurt, the way back is a
`FitConfigManager` method that throws, called from the two `Init`s.

Checked on a small program against the real file (all five sections load with the right
observable) and against copies without a section, without the `observable` block,
without `min`, and with an inverted range. No task was run against a broken file.

---

## Output moved onto Logger

**Every `std::cout` / `std::cerr` log line of the tasks and helpers now goes through
`Logger::`** (84 statements in 19 headers; the hand-written `[INFO]` / `[WARNING]` /
`[ERROR]` prefixes are gone, the level carries them). With it the end-of-run summary
reports real warnings, which was the point of the logger.

*Levels were reviewed, not translated.* Three lines that were printed as `[INFO]` mean the
run is doing something other than what was asked, and became `Warn`:

- `WorkflowManager`: no `global_binning`, "the axes ... will be used WITHOUT
  verification" (the check the framework is built on, switched off);
- `CorrelationTaskBase`: `active_corrections` missing, "Applying NO corrections!";
- `CorrelationTaskBase`: `apply_efficiency_to` missing, "Defaulting to all active particles".

Three lines that repeat per bin or per fit became `Debug` ("Found custom parameters for
bin" in both config managers, "Integrating in ... range"), and so did the whole
`[DEBUG EXTRAP]` block. That block's comment says the `chi2/ndf` line is "not decoration":
the only visible sign that a converged fit does not describe the data. It was moved to
`Debug` as decided, so it no longer shows by default; the `Warn` for an extrapolation
that added nothing, printed by the same function, still does.

*What was kept as it was:* the banners and separators (`>>> [STARTING TASK]`, the `====`
lines) stay direct output, being layout.

*Format.* Numbers that `cout` printed as `double` use `{:g}` (6 significant digits, as
`cout` did); `{}` would print the shortest round-trip form and change the digits. A
`{:g}` on an integer would be an error at run time with `vformat`, so the types were
checked.

*How it was checked.* A converter did the mechanical part and compared, for every
statement, the text and the arguments before and after (84 of 84 agree). The whole
project was then compiled against a variant of `Logger.h` whose functions take
`std::format_string`, which validates every format string and argument type at compile
time; it compiled without errors, and the same variant rejects an integer with `{:g}` and a
missing argument, so the check is not vacuous. The real `Logger.h` (run-time format) is
what the project uses. The compiled build and the interpreted macro both start and report
`[SUMMARY]`. **No analysis was run end to end** after this change.

*Visible differences in the output:*

- Lines that were continuations without a prefix (`  -> Overridden ...`, ` ---> Processing
  particle: ...`) now carry `[INFO]`.
- The three lines that became `Warn` go to stderr instead of stdout; `runAnalysis.sh`
  merges the two, so the log file keeps them in order.
- Timestamps are off by default, so a line looks as it did.

---

## The fitter is built once per configuration

**`PurityTask` and `PhiFitTask` construct a `DynamicRooFitter` for every cell**
(multiplicity bin by pT bin): `DynamicRooFitter fitter(h1, cfg); fitter.DoFit(); ...`.
This was reviewed and kept; the reasons and the measurements are here so the question is
not reopened on a hunch.

*Why one per configuration is right.*

- A cell has its own configuration: the sections of `fitConfig.json` carry per-bin overrides
  (`mult0_pt4`, ..., 20 for `k0s` alone) that change parameters, limits and integration
  windows, and may change the models. The fitter is built from that configuration.
- Nothing is left over from the previous cell: the starting values come from the
  configuration each time, so a fit cannot start where the last one ended.
- The RooFit objects depend on one another and have to be torn down in reverse order of
  creation (the destructor does it, see the comment there). A long-lived fitter would have
  to redo that clean-up at every reconfiguration.
- `DrawFit` needs the model and the data alive, so the object has to outlive the fit anyway.

*What it costs.* Measured on 14 October 2026 with the real `k0s` configuration
(`sig_model` DSCB, `bkg_model` Chebychev1, observable `mass` in [0.45, 0.55]), on a
synthetic histogram of 100 bins (40 000 Gaussian entries at 0.4976, width 0.0045, plus
60 000 flat), Apple M5 with 10 cores, ROOT 6.36.10 (ALICE build), `-O2`. The fit ends
with status 3 on this data, which is not the real spectrum: the times are the point, not
the yields.

| | per fitter |
|---|---|
| construct and destroy, no fit | **0.008 ms** |
| `DoFit`, steady state | **2.2 ms** |
| share of construction in construct + `DoFit` | 0.35% |

Memory: peak RSS 254.6 MB after a warm-up fit, still 254.6 MB after 20 000
constructions, 254.66 MB after 600 constructions with a fit each. No leak shows.

**A correction.** An earlier measurement gave `DoFit` at about 27 ms. That was the mean of
the first five fits, and the first one carries a one-off initialisation of RooFit (about
120 ms); with a warm-up fit before timing, the steady state is the 2.2 ms above. The
conclusion did not change (construction is negligible either way), but the number did.

*`NumCPU(4)` does nothing here.* `DoFit` passes `RooFit::NumCPU(4)`. Time per `DoFit` and the
signal yield (`ExtractYieldsAndPurity`, 10 decimals) for the same fit, 20 repetitions after
a warm-up:

| `NumCPU` | compiled | interpreted (Cling) | signal |
|---|---|---|---|
| 1 | 2.16 ms | 2.20 ms | 39925.7197908157 |
| 2 | 2.17 ms | | 39925.7197908157 |
| 4 | 2.14 ms | 2.28 ms | 39925.7197908157 |
| 8 | 2.18 ms | | 39925.7197908157 |

The same time and, to the last digit, the same result, in both modes. RooFit's own
documentation says why: *"`NumCPU(int num, int istrat)`: Parallelize NLL calculation on num
CPUs. (Currently, this setting is ignored with the **cpu** Backend.)"*, and the `cpu` backend
is the default since ROOT 6.23. `DoFit` names no backend. The two ways to make a parallel
setting take effect, tried on the same fit:

| | per `DoFit` | signal | |
|---|---|---|---|
| `EvalBackend("legacy")`, `NumCPU(1)` | 6.4 ms | 39925.94 | 3 times slower than `cpu` |
| `EvalBackend("legacy")`, `NumCPU(2)` | **340 ms** | 39852.27 | 50 to 100 times slower, result differs |
| `EvalBackend("legacy")`, `NumCPU(4)` | **615 ms** | 39833.08 | (same figures in Cling: 634 ms) |
| `RooFit::Parallelize(2)` or `(4)` | throws | | *"Parallel minimization requested, but ROOT was not compiled with multiprocessing enabled"*, `std::logic_error` |

*On real spectra, compiled.* The synthetic histogram is not the real data, so the same
comparison was repeated on the mass histograms the purity task reads (the file
`AnalysisResults_136_2026_PtMod_23luglio.root`: `K0S` 10 x 34 x 200 bins, `Xi` 10 x 26 x 200,
`n#sigma` TPC of the pion 10 x 36 x 100), every cell with at least 1000 entries, with the
real configuration of each cell, once with the code as it is (`NumCPU(4)`) and once with the
option removed:

| | cells | mean `DoFit` with `NumCPU(4)` | mean without it |
|---|---|---|---|
| `k0s` | 340 | 4.66 ms | 4.74 ms |
| `xi` | 260 | 1.56 ms | 1.53 ms |
| `pi_tpc` | 360 | 3.62 ms | 3.58 ms |

960 fits, total 3292 ms against 3296 ms (ratio 0.999), and **every one of the 960 gives the
same status, signal and error to 12 digits**. The tests above for the interpreted mode were
on the synthetic histogram only; the real-data run was compiled only.

So no setting of `NumCPU` helps: it is ignored with the default backend, and with the
backend where it is not ignored it spawns processes whose start-up dwarfs a 6 ms fit and
changes the result. The answer to "how would you set it" is to leave it out; see
`DESIGN_NOTES.md`, section 3. It behaves the same compiled and interpreted, so the mode
plays no part.

*Method, to repeat it.* A small program including `FitConfigManager.h` and
`DynamicRooFitter.h` builds the histogram above with `TRandom3(1)`, takes
`FitConfigManager("fitConfig.json").GetConfig("k0s", 0, 0)`, does one warm-up
`DynamicRooFitter(&h, cfg).DoFit()`, then times 20 repetitions of
`DynamicRooFitter f(&h, cfg); f.DoFit();` with `std::chrono::steady_clock` around `DoFit`
only. For the `NumCPU` rows a scratch copy of `DynamicRooFitter.h` reads the option from an
environment variable instead of the literal 4. The interpreted rows run the same code as a
macro (`#pragma cling add_include_path`). The real-data run projects each source cell
(`ProjectionZ` on one multiplicity and one pT bin) with the configuration indexed by that
source bin: it is not the fits `PurityTask` does, which merges source pT bins into the
analysis bins before fitting. Not repeated on other machines or other ROOT builds.
