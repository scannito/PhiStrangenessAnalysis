# Design notes — open items

Written after the refactoring session of 31 July 2026, extended on 3 August,
reorganised on 13 October. Everything here is *decided but not done*, or *found
but not fixed*. Items are ordered by how much they matter, not by how much work
they are.

What has been resolved moved to `DESIGN_HISTORY.md`, together with the reasoning
that led to it. Read it before reopening a closed question: several of the fixes
recorded there look wrong out of context.

---

## 1. Split CorrelationTaskBase in two

`CorrelationTaskBase` currently does five jobs: loads corrections and purities,
extracts correlations from the THnSparse, normalises and builds the spectra,
extrapolates, and computes trends and cross-species ratios.

The first three need the heavy containers. The last two need **only the spectra**,
which are already written to `PhiAssocSpectra.root`. That file boundary is the
natural seam, and it is the same pattern already used between `PhiFitTask` and
`CorrelationTask`.

**Task A — containers to spectra.** Corrections, purities, extraction,
normalisation, writing the spectra.

**Task B — spectra to results.** Extrapolation, measured and extrapolated
multiplicity trends, extrap/measured ratios, cross-species ratios.

### Why it is worth doing

- **Systematics force the split anyway.** A systematic uncertainty on a spectrum
  is the spread over N varied runs of Task A, so it does not exist until all of
  them have finished. Extrapolation consumes that spread - `SpectrumExtrapolator`
  now takes it through `SetSystematicSpectrum` - which makes extrapolation a step
  that runs *after* the whole set, not inside the task that produces one member of
  it. As long as the two live in one task, the extrapolation can only ever see
  statistical errors, whatever machinery is bolted onto it.

- **Mixed provenance.** Task B can take `K0S` from a correlation task run without
  PDG matching and `Xi` from one run with it. Today a ratio between two species
  is only computable if both were produced by the same task, hence by the same
  method — which is exactly the constraint we want to lift (see "Per-particle
  choice of PDG matching").
- **Re-running the cheap half.** Extrapolations and ratios can be redone without
  repeating the extraction. The projection caches exist to work around this; a
  dedicated task makes it structural.
- **Dependencies.** Task A would lose `ExtrapolationModelFactory`,
  `SpectrumExtrapolator` and `ExtrapConfigManager` - the whole spectral-model
  machinery, which today is included by every path that goes through the
  correlation task, i.e. by the whole data analysis. Task A also loses
  `AnalysisConstants`: the particle mass in `AssocParticleConfig` is used *only*
  by `ExtrapolateSpectrum`, so that struct reduces to name, dirName and binning.

The division is honest: A knows about detector effects and corrections, B knows
about particle masses and spectral models.

### Prerequisite (small, and worth doing anyway)

`PhiAssocSpectra.root` must carry two things it does not carry today:

- the **multiplicity binning stamp**, because task B would loop over multiplicity
  indices of spectra produced elsewhere — the same hole that was closed for
  `Corrections.root` and the purity files;
- the **relative normalisation uncertainty per multiplicity bin**. It is computed
  in the correlation task (`TriggerYield::RelativeError()` combined with the
  event-loss error) and applied at trend level, because it is 100% correlated
  across the bins of a spectrum. If the trends move to task B, that number has to
  travel with the spectra.

---

## 2. Per-particle choice of PDG matching

`CorrelationWPDGTask` differs from `CorrelationTask` along two independent axes:

- the **trigger**: `h3PhiMCGen` vs `h3PhiData` — a task-level choice, since there
  is one φ;
- the **associated container**: `ClosureMCGen` vs `DataSignal` — this could
  become a key in each entry of `associated_particles`.

Generalising only the second covers the mixed case (PDG matching for species
where the conditions for doing without it are not met, no matching for the
others) without touching the structure. Do it *after* splitting
`CorrelationTaskBase`, not before: it adds
responsibility to a class that already has too much.

---

## 3. DynamicRooFitter: what it does not do yet

`DynamicRooFitter` is the only fitter, used by both `PhiFitTask` and `PurityTask`;
how the TF1-based `FitPhiSignalAndBkg` came to be retired is in `DESIGN_HISTORY.md`.
What follows is what the engine still lacks.

### Small things found while measuring it (14 October 2026)

The decision to build one fitter per configuration, and the numbers behind it, are in
`DESIGN_HISTORY.md` (*The fitter is built once per configuration*). Two things came out of
that review and are not done (a third, `RooFit::NumCPU(4)`, which did nothing, was removed;
a comment in `DoFit` says why and points at the measurement):

- **The constructor sets process-wide RooFit state** (`RooMsgService::instance()
  .setGlobalKillBelow(RooFit::WARNING)` at `DynamicRooFitter.h:56`, on every construction).
  Anything that builds a fitter silences RooFit's INFO messages for the whole program. It
  belongs in the two entry points, with the other process-wide state.
- **`DoFit()` must be called before `ExtractYieldsAndPurity()`**, enforced by a throw
  ("Call DoFit() first!"). It works; the order is a coupling the interface does not express.
  Not worth restructuring on its own, since the object has to stay alive for `DrawFit`
  anyway.

### One range for everything: the observable's range is also the fit range and the plot range

Today `observable.min`/`max` in `fitConfig.json` builds the `RooRealVar` (`DynamicRooFitter.h:62`), and that
single range is used for everything: the `RooDataHist` imports only the bins inside it, the likelihood runs over
it, the window and sideband fractions are normalised to it (`NormSet(*obs)`), and `DrawFit` builds its frame and
pull axis on it. There is no way to show more of the spectrum than is fitted, or to fit less than is shown.

That couples two decisions of different weight. The fit range changes the numbers; the plotted range should not.
The Xi section shows the cost: commit 3e2878a (27 August) narrowed the Xi range from [1.2, 1.4] to [1.29, 1.36],
and the Xi purity dropped by a median 0.074 (`RooFitKnowledge/09-fit-quality-and-yields.md`, section 9.1). Whether
that was a fit decision or a display one, the configuration cannot say. Note also that RooFit silently widens the
range to the histogram's bin edges, so the Xi fit actually runs on [1.289, 1.361] (`RooFitKnowledge/05-...`, 5.6).

Two ways to separate them, in increasing order of risk:

- **A plot range used only by the drawing.** Keep the observable's range as the fit range and add an optional
  `plot_range` that only `DrawFit` reads: draw the data histogram over the wider range and the curves over the
  fit range (mark where the fit stops). No number changes, so the key can stay optional (the rule in `CLAUDE.md`,
  *Configuration*, asks to declare only keys that change a number). This is a drawing change only.
- **A fit range narrower than the observable** (`obs->setRange("fit", a, b)` and `fitTo(..., Range("fit"))`). It
  changes the physics and RooFit has measured traps here (`RooFitKnowledge/05-datasets-plotting-gof.md`, sections
  1-2; `04-...`):
  - in an extended fit restricted to a range, `nsig` and `nbkg` refer to the WHOLE observable range,
    extrapolated (toy: ns + nb = 8430 for 5867 events in the fit range). The window yield `nsig * fraction` with
    the fraction normalised to the whole range should stay consistent, but this was not tested on the project's
    models;
  - the plot normalises a fitted curve to the fit range automatically, and a component drawn with
    `RelativeExpected`, as `DrawFit` does, comes out inflated by 1/(range fraction) (toy: 4351 for ns = 3026), so
    `DrawFit` would need changing too;
  - `IntegrateBins`, toys and `RooMCStudy` need `obs->setBins` equal to the histogram's bins inside the range
    (`RooFitKnowledge/04-...`, `07-...`), and bins are selected by their centre.

The first is cheap and probably what is wanted most of the time; the second needs a validation on real cells before
it is used. Not started; which of the two (or both) is the owner's choice.

### Future direction: arbitrary N-component decomposition in DynamicRooFitter

Currently, `DynamicRooFitter` supports composite models within `sig_model` or `bkg_model` via `+`
(relative fractions `frac_sig_0`, etc.) and `*` (`RooProdPdf`). However, at the top level the
architecture enforces a strict two-component macro-decomposition:
$$\text{Model} = N_{\text{sig}} \cdot \text{sigPdf} + N_{\text{bkg}} \cdot \text{bkgPdf}$$

**Current limitations:**
- **Yield extraction**: `ExtractYieldsAndPurity` only returns aggregate integrals for `sigPdf`
  and `bkgPdf`. Yields and propagated covariance uncertainties of individual sub-components
  (e.g., secondary signal peaks, resonance impurities, or reflection backgrounds like $K^{*0}$
  or $\rho$) cannot be extracted independently.
- **Diagnostics and plotting**: `DrawFit` only draws `Sig_Plot` (red) and `Bkg_Plot`
  (green); sub-components are not individually plotted with distinct styles/colors or separate
  legend entries.

**Planned extension:**
- Generalize `DynamicRooFitter` to accept an arbitrary list or map of $N$ top-level components
  (each declaring its role: primary signal, secondary signal, combinatorial background, resonant
  contamination), each with its own independent extended yield variable $N_i$.
- Extend `ExtractYieldsAndPurity` and `FitResults` to return per-component yields and propagated
  covariances.
- Extend `DrawFit` to plot each component individually with configurable visual styles and
  legend entries.

### Future direction: RooWorkspace persistence (Decoupling Fit from Extraction & Systematics)

Currently, `PhiFitTask` and `PurityTask` execute the MINUIT fitting (`DoFit()`) and the yield/purity
extraction (`ExtractYieldsAndPurity()`) in a single pass in memory. If an analyst wants to vary the
integration window ($N\sigma$ vs fixed range, sideband boundaries) or perform systematic variation
studies on the signal extraction, the entire compute-intensive fitting loop over hundreds of $(p_T, \text{mult})$
bins must be re-executed from scratch.

**Proposed architecture (inspired by `IntroRooFit.C`):**
- **Fit phase (`SaveFitToWorkspace`)**:
  `DynamicRooFitter` fits each bin once and imports the fitted composite model (`model`), the binned
  data (`RooDataHist`), the observable (`obs`), and the covariance result (`RooFitResult`) into a
  `RooWorkspace` within the task's output ROOT file.
- **Extraction & Systematics phase (`LoadFromWorkspace`)**:
  Downstream tasks can re-instantiate `DynamicRooFitter(ws, cfg, ...)` directly from the stored
  workspace. They can evaluate multiple integration ranges, sideband subtractions, and yield integrals
  instantaneously using the preserved covariance matrices without ever invoking MINUIT again.
- **Portability & Inspection**:
  The full analytical models and data distributions are preserved in the ROOT file, allowing
  physicists to inspect fit quality interactively in ROOT or generate updated ALICE-styled diagnostic
  canvases without rerunning the full data chain.

### Future direction: Fit quality monitoring, convergence diagnostics, and evaluation error handling

Currently, `DynamicRooFitter::DoFit()` returns `fitResult->status()`, but downstream tasks (`PhiFitTask`,
`PurityTask`) do not systematically track convergence codes, covariance matrix quality, or parameter
boundary behavior across batch fits.

A baseline from the measurement in `DESIGN_HISTORY.md` (*The fitter is built once per
configuration*), 14 October 2026, with the real mass histograms of
`AnalysisResults_136_2026_PtMod_23luglio.root` and the real `fitConfig.json`: of the cells
fitted, `status` is 0 for 28 of 340 (`k0s`), 126 of 260 (`xi`) and 16 of 360 (`pi_tpc`);
it is **3** for 310, 134 and 338 of them, and 304, 303 or 1 for a handful (3 in total per
cell would mean, in MINUIT's codes, an estimated distance to the minimum above the
tolerance). Nothing reads these values today. **Caution before drawing a conclusion:**
those fits are per *source* bin, not the merged analysis bins `PurityTask` fits, and the
per-bin overrides in `fitConfig.json` are keyed by the analysis index, so some cells used a
different override than the real run would; and it was not checked whether status 3 means a
bad fit here or the usual outcome of this setup. The number says only that the status
cannot be left unread: if it is the usual outcome, the audit described below has to be
tuned so that it does not flag 90% of the fits.

**1. Convergence and covariance matrix diagnostics:**
- In RooFit/MINUIT, a converged fit with full covariance error propagation satisfies `status == 0`
  and `covQual == 3` (accurate covariance matrix).
- Lower values of `covQual` (e.g. `1`: forced positive-definite, `2`: full but made positive-definite,
  `0`: approximate) mean that parameter errors and propagated yield uncertainties cannot be trusted.
- **Planned mechanism**: Centralize fit quality auditing. When a bin fails `status == 0` or
  `covQual >= 2`, log a concise warning or record a diagnostic quality flag alongside the output
  matrices so that downstream correlation tasks can identify or exclude compromised bins.

**2. Evaluation errors and parameter-at-limit detection:**
- When MINUIT calculates numerical gradients via finite differences, it tests parameter variations
  $\theta \pm \delta$. If a trial point produces negative PDF values ($f(x) \le 0$) or mathematical
  domain errors, RooFit generates an *evaluation error* and assigns a large penalty to $-\ln L$.
- **Benign vs. pathological errors**:
  - *Benign*: Normal exploratory gradient steps in early iterations; MINUIT discards the step and
    converges cleanly inside the physical region (`status == 0, covQual == 3`).
  - *Pathological*: Range or model misspecification in the JSON configuration (e.g., background
    polynomial permitted to dip below zero, width parameter allowed to reach $\le 0$, or a parameter
    colliding with its hard boundary `min` or `max`).
- **Log control strategy**:
  - `PrintEvalErrors(-1)` completely suppresses all evaluation error output, eliminating log pollution
    but also hiding symptoms of ill-bounded configurations.
  - `PrintEvalErrors(0)` offers a clean middle ground: zero per-step spam during minimization, but a
    single summary line at the end if evaluation errors occurred (`Notice: N evaluation errors...`).
  - Programmatic tracking: inspect `fitResult->numInvalidNLL()` and check whether fitted parameters
    are glued to their boundary limits (`|val - min| < \epsilon` or `|val - max| < \epsilon`). If so,
    alert the analyst that the JSON parameter ranges require tuning.

---

## 4. Name objects by interval, not by index

Half the framework already does this. `CorrelationTaskBase::CellSuffix` names its
objects `_mult0-1_ptPhi0.8-1_pt0.3-0.5` through `BinLabel`. `MCTask`, `PurityTask`
and `PhiFitTask` still name theirs `_multBin0`, `_ptBin3`.

The difference is not cosmetic. With an index, a producer and a consumer that
disagree on the binning still *find* each other: `_multBin3` exists on both sides
and matches, it just means two different intervals. With an interval, `_mult10-15`
simply is not there, and a missing object is an error instead of a wrong number.

This is why the binning stamps exist: they are the patch that is needed *because*
the names are indices. Interval names would make a whole class of mismatch
impossible rather than detectable after the fact.

### Scope

Producers: `MCTask` (efficiency, signal loss, canvases), `PurityTask` (purity
spectra, fit canvases), `PhiFitTask` (mass projections, fit canvases).

Consumers that must change in lockstep: `CorrelationTaskBase::LoadCorrections`
(`h1{name}Efficiency_multBin{i}`) and `CorrelationTask::LoadPurities`
(`h1{key}Purity_multBin{i}`).

`BinLabel` should move from `CorrelationTaskBase` to `BinningUtils` — it needs only
edges, an index and `FormatEdge`, which already lives there — so that the five
tasks share one definition.

### Cost

Every file already produced becomes unreadable to the new code: corrections,
purities, projection caches. The whole chain has to be re-run once, in order.
Nothing is silently wrong in the meantime — the objects are simply not found — but
it is not a change that can be deployed halfway.

Keep it as its own commit, unmixed with anything else. It is the change with the
highest potential to break things quietly, and it should be revertible alone.

### The same disease in the fit configuration

`fitConfig.json` keys its per-bin overrides as `mult<i>_pt<k>`. Today `k0s` has
twenty of them, all on `pt4` and `pt5`, and those indices refer to the source
binning because no `rebinning_pt` is declared anywhere yet. The moment one is
declared, the analysis loop will look up `pt4` with a *coarse* index and apply
parameters tuned for a different pT interval, silently.

Two ways out: key the overrides by interval as above, or hand the binning to
`FitConfigManager` and have it verify — the same choice made everywhere else in
this codebase. Worth deciding before `rebinning_pt` is used in anger.

---

## 5. Known bugs, not yet fixed

**`hIntegral_tmp` in `SpectrumExtrapolator` is degenerate when the integral is
≤ 0.** It was left alone while the class had to match `YieldMean` number for
number, because the reference behaves the same way. The reference left the chain
in `6c78e4c`, so that reason is weaker now: it can be fixed, provided the fix is
recorded as a deliberate departure from `YieldMean`.

**Unknown keys are not rejected.** A key that no task reads passes in silence.
`use_legacy_extrapolation` showed the cost: after the legacy branch was removed it
stayed in `globalConfigDatapp_OnlineEffonly.json`, set to `true` in three blocks,
which then ran `SpectrumExtrapolator` while declaring the opposite - the
configuration-that-lies case from `CLAUDE.md`. The key has been removed from that
file (13 October); nothing would catch the next one. Rejecting unknown keys needs
each task to declare the keys it reads, `inherits` resolved first, so it is a
larger change than it looks.
What is done (17-18 October): every block whose set of keys is closed and has optional
keys refuses an unknown one (`JsonConfig::RequireKnownKeys`): the sections and nodes of
`fitConfig.json` and `extrapConfig.json`, the entries of the arrays (`associated_particles`,
`yield_ratios`, `mc_particles`, `purity_particles`, `comparisons` and their `objects`),
`global_binning` and the entries of `corrections.particles`. The blocks whose keys are all
required (`purity_sources`, the top of `corrections`, `acceptance_efficiency`) were given it
on 17 October and had it removed on 18 October: there a typo already stops as a missing key. What is left is the case above: the task
blocks themselves, where `inherits` mixes keys that different tasks read.

*The way to close it, and to stop maintaining the lists (idea, 18 October, agreed with the
user, not designed).* `RequireKnownKeys` is not redundant: `Require*` and `Optional*` only see
the keys the code asks for, so a misspelled **optional** key (`n_sigma_integation`,
`dir_nme`, `signal_los`) reads as "not written" and the fallback applies in silence; only a
check of every key present catches it. Where every key is required (`purity_sources`) it adds
little. Its real cost is that the list of keys is a second copy of the reads, kept aligned by
hand (a key added to the code and not to the list is refused: loud, but one more rule to keep
in step). The fix is to derive the list instead of writing it: read a block through a small
object that records every key asked for (`Require*`, `Optional*`, `Try*`) and, at the end,
refuses any key present and never asked for:

```cpp
JsonConfig::Reader r(node, ctx);
auto name  = r.RequireString("name");
auto label = r.OptionalString("label", name);
r.RequireAllRead();   // any key present and never asked for stops the run
```

One source of truth, and the same mechanism on the merged block of a task, at the end of
`Init`, would close the task-level case above: what the task did not read is unknown. The point
to design is the keys a task reads only conditionally (`extrapolation_config_file` with the
extrapolation off, `corrections_file` with `"corrections": null`): they must count as read, or
be declared as known-but-unused, or every configuration that switches a feature off would be
refused.

**Fit status is discarded.** `DynamicRooFitter::DoFit()` returns
`fitResult->status()` and nobody looks at it. On ~240 fits per run, the number
that failed is information that exists and is thrown away. A counter and one line
at the end of `Run` would be enough.

**`mcclosure pp` stops at its first task.** Since `65a2c76` ("Remove duplicates in json
configs", 13 August) the blocks `phi_fit_task_{offline,online}_efficiency_{2024,2026}` in
`globalConfigMCClosurepp.json` are empty (`{}`) and inherit nothing, so `PhiFitTask` stops
with `'input_data_file' missing in JSON!`. Before that commit the generic blocks read
`MCClosure/AnalysisResultsMCclosure_136_7maggio.root`; which files the 2024 and 2026 blocks
should read is the user's to say. Found on 16 October 2026 while running the chain to check
the corrections refactoring; left for later on the user's request, and that check was done
without `mcclosure`. `globalConfigMCClosureppref.json` was changed by the same commit and was
not looked at.

**The extrapolated error of a species depends, in the last digits, on what was fitted before
it.** Found on 16 October 2026 while checking the corrections block: with only the K0S efficiency
changed (integrated to binned), the errors of the extrapolated trends of Pi and Xi
(`h1MultTrendExtrap_*`, `Ratio_Extrap_Meas_*`, `Ratio_Xi_Pi_Extrap_*`) moved by 1e-14 to 6e-12
relative, their contents and every measured quantity unchanged. No physical weight, but it means
some state survives from one extrapolation fit to the next (a minimiser, a `TF1`, a cached integral:
not looked into). With identical inputs the outputs are identical bit for bit, which is what the
before/after check relies on; a check that changes one species must expect the others' extrapolated
errors to move at that level. Seen again on 18 October: `run_mode: "optimized"` and `"legacy"` with the 1D
engine give the same contents and extrapolated K0S trend errors that differ by up to 3.5e-12 relative (the
two modes fit in a different order). Runs of one mode on the same inputs are identical.

**The L2 signal cache does not say which settings produced it.** Found by the review of
19 October 2026. With `use_signal_cache` the final signals of a cell are read back as they were
written, and they already carry the mixed-event normalisation and the corrections. Nothing records
which `me_normalization` (or `apply_mixed_events`, or `corrections`) made them: a run with `fit` and the
cache off, then one with `bins_at_zero` and the cache on, gives the first run's numbers (up to 9.6% on
the K0S) under a provenance that says `bins_at_zero`. The hole existed for the corrections before; the
declared normalisation widens it. The general fix, rather than one check per key: stamp the scheme
directory, when the L2 cache is written, with the settings that change its numbers, and refuse a mismatch
on read, as `RootIO::RequireMatchingBinningStamp` does for the binning.

**A key retired by a migration still passes when a parent declares its replacement.** Found by the same
review. `RejectReplacedKey` was dropped on 19 October because the required `corrections` stops a
configuration that was not migrated. But a child block that still says `"apply_efficiency": false`, and
inherits `corrections` from a migrated parent, satisfies the required key through the parent and runs
with the parent's corrections, in silence: the "off must be a value" trap of `CLAUDE.md`. A table of
retired keys was considered and set aside (one more list to grow with every rename); this is the
strongest argument for the key-recording reader described above, which refuses any key a task did not
read.

**A zero event-loss bin is skipped in silence.** In `CorrelationTaskBase`, where the
number of triggers is corrected for the event loss, `if (eventLossCorr > 0.0)` divides
only when the bin is positive: a multiplicity class whose event-loss bin is 0 keeps its
uncorrected trigger count and nothing says so. Found on 16 October 2026 while reading the
corrections; it is the same kind of problem as the zero-efficiency candidates of the
O2Physics task (section 13). At least a `Logger::Warn` with the multiplicity class; whether
the run should stop instead is a choice to make.

**`h2TriggerSignal` axes are not cross-checked.** `PhiFitTask` resolves the φ pT
binning from `h3PhiData`, `CorrelationTask` resolves it from axis 1 of the sparse.
The two must agree because those matrices are read by index. Now that they carry
real axes it is checkable — three lines in `ResolveBinningAndCache`.

**Background-ratio uncertainty is computed but not propagated.** `bkgRatioAndError`
now reaches `h2TriggerBkgRatio`, but `CorrelationCalculator` still scales the
sideband by an exact factor. Adding it per bin would be wrong: σ_R is fully
correlated across the Δy bins of a cell, so quadrature at integration time would
dilute it. It should either be propagated as a per-cell scale uncertainty or
treated as a systematic. Decision pending.

---

## 6. Physics questions still open

**Cells where the mixed event is empty at zero. To decide.** Since 19 October 2026 a mixed event that
cannot be normalised goes through `Logger::Warn` instead of passing in silence, and the first `data pp`
run showed where: the **sideband** mixed event of the **Ξ**, multiplicity **70-100%**, associated pT
**4.2-6.5 GeV/c**, is 0 at Δy = 0 for the four φ pT bins from 2 GeV/c up (2-2.8, 2.8-4, 4-6, 6-10), in every
task and year (24 warnings). There the sideband is divided by raw mixed-event counts instead of a mixed
event scaled to 1, then subtracted from the signal: those cells are not corrected as the others are.
It has always been so; the numbers did not change, only the silence. What to do is the user's call: drop
the sideband subtraction in such cells, normalise with a wider window or the fit, merge the cell with a
neighbour, or stop the run.
The fit is no way out as it stands: with `me_normalization: "fit"` and the 2D engine (one block,
`data pp` 2024, 19 October) the same four cells give a0 = -0.059 in the first (left unnormalised) and a
fit status 1 in the other three, whose a0 is used as it is.

**Δφ in the 1D engine: which normalisation of the mixed event? To decide.** Recorded on
18 October 2026. `CorrelationCalculator` has two engines, and they differ in more than the
normalisation:

| | 1D engine (`Extract1D`) | 2D engine (`Extract2D`) |
|---|---|---|
| projection | SE and ME projected **at once on Δy**, integrated over Δφ | SE and ME projected on **(Δφ, Δy)** |
| mixed-event correction | **1D** division, (∫SE dΔφ)/(∫ME dΔφ) | **bin-by-bin 2D** division SE/ME, then the projection |
| ME normalisation | chosen by `me_normalization` (since 18 October; before, fixed per engine): `bins_at_zero` takes the bins at Δy = 0, `fit` the a0 of a0 + a1·\|x\| + a2·x² near Δy = 0 | `fit`: a0 of that fit on the Δy projection, divided by the number of Δφ bins (the value **averaged over Δφ**); `bins_at_zero`: the bins at (Δφ, Δy) = (0, 0) (the value **at Δφ = 0**) |
| final axis | Δy only | Δy or Δφ, with the orthogonal cut and the QA |

The second row is the one of substance: ∫SE/∫ME ≠ ∫(SE/ME) in general. The pair acceptance
depends on Δφ and Δy, and the 2D engine corrects it cell by cell before integrating; the 1D
engine corrects only its average over Δφ. They agree only if the ME is flat in Δφ.

`run_mode: "legacy"` (every current configuration) and the WPDG task always build the 1D
engine, and `projection_axis: "delta_phi"` there used to give Δy spectra with no error. It is
now refused at `Init` (`CorrelationTaskBase::RequireEngineSettingsSupported`, which asks
`CorrelationCalculator::Supports`, where the rule is written once). When the 1D engine learns Δφ,
`Supports` is the one line to change. The user wants Δφ in the 1D engine eventually; what is open is the
normalisation. In Δy the ME peaks at 0 and is normalised there; integrated over Δy the ME is
about flat in Δφ, so "the value at Δφ = 0" has no special role. Candidates: the 2D engine's
convention (the value at (0, 0)), the mean over the whole Δφ range, or the value around Δφ = 0
by symmetry with Δy. They give different spectra; the user's call.

The normalisation itself is now declared, `me_normalization`, for both engines: every configuration
says `bins_at_zero`, what the 1D engine always did. On `data pp`, `fit` instead moves the spectra by up to
about 10% (K0S, one bin of 0-1% multiplicity); which one is right is open. The fit has two parameters
fixed in the code, the range (±0.5 in Δy) and the shape (a0 + a1·|x| + a2·x²): with the fit available to
both engines they may need to become configurable.

**Primaries in the efficiency numerator.** The efficiency is
`MCReco / MCGenAssocReco`. If the numerator is not restricted to primary
particles, the correction silently absorbs secondaries from weak decays instead of
subtracting them. Relevant especially for pions. Needs checking on the O2 side.

**The extrapolation fit does not describe the data.**

```
NDF=3  Chi^2=157.241  Chi^2/NDF=52.4138
NDF=3  Chi^2=36.9467  Chi^2/NDF=12.3156
```

The fit converges, which is not the same as being good. A Levy-Tsallis over
[0.2, 1.0] with three free parameters is rejected by the data at that chi^2, and
between 26% and 30% of the yield comes from extrapolating it - for pi, the tail
above the measured range contributes only ~3% of the yield but enters the mean p_T
weighted by p_T, so it matters far more there than the yield fraction suggests.

Two readings, and they call for different fixes:

- the model or the fit range is wrong for this spectrum, or
- the uncertainties on the spectrum are underestimated, which inflates chi^2
  without the model being at fault - the usual cause being bins correlated through
  the efficiency and purity corrections but treated as independent.

Separate these before trusting the extrapolated yields.

This was recorded with a condition attached: keep the legacy branch
(`use_legacy_extrapolation`) until the question is settled, because if the fit
turns out to be wrong the reference is what tells whether the new path is wrong in
the same way. The branch was removed in `6c78e4c` (8 August) with the question
still open.

What still holds is the reproduction recorded in `DESIGN_HISTORY.md`: on the same
spectra the two paths gave the same chi2 to six digits, so they perform the same
fit, and this chi2 belongs to the model, the fit range and the uncertainties, not
to either implementation. What was lost is the ability to repeat the comparison on
new spectra or after changing the fit: `OldCodes/YieldMean.h` is still there, but
`YieldMean.C` was deleted and nothing in the chain calls it, so a new check means
wiring it back by hand.

**Systematics.** Results carry statistical uncertainties only, with one exception:
`SpectrumExtrapolator` propagates the four upstream variations (extreme high, hard,
extreme low, soft) when a systematic spectrum is supplied, and fills
`yieldSysHi/Lo` and `meanPtSysHi/Lo` in `ExtrapolationResult`. Nothing supplies one
yet: `SetSystematicSpectrum` is never called, and the place where it *would* be
called says so — see the comment in `CorrelationTaskBase::ExtrapolateSpectrum`, just
before `Extrapolate()`, which is the one line to change once a driver exists.

The mechanism is therefore present and dormant, deliberately. It exists so that the
day `hsys` is produced the extrapolation already takes it; until then it takes a
`nullptr` and `hasSystematics` stays false.

What is missing is everything upstream of that: `hsys` itself.

### What `SpectrumExtrapolator` already does with `hsys`

**The systematic machinery is back, and waiting for its input.** It was half
ported and wired to the wrong errors: the four builders existed, nobody called
them, and they read `fMeasuredSpectrum`, whose errors are statistical - so
switching them on would have returned the statistical error propagated
coherently, wearing the name of a systematic.

Now `SetSystematicSpectrum(TH1*)` supplies the systematic band explicitly,
`ComputeSystematics` refits each of the four extremes exactly as the central value
was fitted (same `FitOrWarn`, extracted so the two cannot drift), re-integrates,
and takes the absolute shift. `nullptr` is the normal state today and the
variations are simply skipped.

Three things worth keeping in mind about it:

- `hasSystematics` exists so that "not computed" and "computed and found to be
  zero" are different states. Four zeros in a result that carries no such flag is
  exactly the ambiguity that `extrapolatedFraction` cost us.
- The pairing is the reference's: the **yield** uncertainty comes from the shifted
  pair (every bin moved coherently by +-sigma, which moves the integral and barely
  the shape) and the **mean** from the tilted pair (a pT-dependent pivot, chosen by
  scanning for the node that moves the mean most, which does the opposite). Reading
  all four numbers from all four variations would not be more conservative, only
  noisier.
- The model is fitted in place, so the four extra fits would leave it on the last
  variation's parameters - and the caller integrates it and writes the curve next
  to the spectrum. `ComputeSystematics` saves the central parameters and restores
  them. The covariance matrix in the global fitter is *not* restored; nothing reads
  it afterwards today, but anything added there that calls `TF1::IntegralError`
  would silently use the wrong one.

What is still missing is the input. Nothing in this chain produces per-bin
systematic uncertainties (see "Systematics" above), so this is machinery ready for
a driver that does not exist yet. `CorrelationTaskBase` carries the one line to
change when it does.

When that driver is written, two things belong with it. First, the systematic
spectrum is the SPECTRUM - measured contents, systematic errors - not a histogram
of the errors; `SetSystematicSpectrum` documents the contract because getting it
backwards runs silently and produces "systematic plus systematic" in the
variations. Second, that contract is the place for a content-versus-measured
check, which is deliberately absent today: it needs a tolerance, and the tolerance
depends on how the band is built. Written now it would be a guess, and the first
false alarm would widen it into nothing. The bin count is checked already, being
structural.

Note also that this is only *one* of the two things called a systematic on the
extrapolation. It propagates the spectrum's own band. The systematic on the
**choice of extrapolation** - refitting with the other four models the factory
already builds, and with alternative fit ranges, then taking the spread - is a
separate contribution, needs no `hsys`, and is not computed anywhere. Given a
chi2/ndf of 52 and an extrapolated fraction near 30%, it is likely the larger of
the two.

### Where `hsys` has to come from

`OldCodes/ratioK0SPiCorrectedUpdated.C` already does all of this, in the
pre-framework style, and it is the reference for the port — `BarlowVar2`, the
per-source breakdown, the pass/fail histograms against multiplicity, the five
extrapolation models as a source. This is a port, not a blank page.

Reading it corrects an assumption that was written here before: **most systematic
sources are not offline variations.** They are separate O2 productions —

```
Systematics/K0S/DCADauToPV/phik0shortanalysis_tightDCADauToPV_id25651Systematics.root
Systematics/K0S/NSigmaTPC/phik0shortanalysis_tightNSigmaTPC_id25957Systematics.root
Systematics/K0SPiCorrelated/NTPCClusters/..._tightTPCClusters_id25957Systematics.root
```

— different Hyperloop trains, different selections, arriving as files. The offline
chain does not generate them; it consumes them. Only the fit systematic and the
extrapolation model are born offline.

That changes what the driver is. It is not primarily a configuration generator:
it is a **collector**, and the thing that produces variation-over-nominal ratios
already exists — `ComparisonTask`, which was built to divide the same quantity
produced by two separate runs.

Steps, in dependency order:

1. Run the chain on each variation production. One configuration per variation,
   changing `input_data_file` and `output_prefix` and nothing else. No new code.
2. Ratio each variation to the nominal with `ComparisonTask`, `target: "spectra"`.
3. A new collector task reads the N ratios and emits `hsys`: a TH1 shaped like the
   spectrum whose bin errors are the combined systematic. Barlow decides whether a
   variation is significant; the uncorrelated sources add in quadrature.
4. **Correlated sources are propagated on the ratio, not per species.** A cut that
   moves K0S and pi together partly cancels in K0S/pi, so the ratio needs its own
   `hsys` and it is NOT the quadrature of the two. This is the detail a port loses
   most easily, because the result stays plausible either way.
5. Decide about the `Smooth(1)` the old macro applies to the systematic-versus-
   multiplicity profiles: port it with the reason written next to it, or leave it
   out deliberately.

Note the level: `hsys` is a **spectrum**, per particle and multiplicity bin. Once
it exists, `SetSystematicSpectrum` carries it to the trend by itself, and the four
`ExtrapolationResult` numbers stop being zero. Producing a trend-level systematic
directly would be solving the problem at the wrong layer.

### `Macros/run_systematics.py`

The skeleton of the offline half only, and **not usable as it stands**. Three
reasons, two of which fail silently:

- The example variations set `extrap_function` and `extrap_fit_range` inside
  `correlation_task`. **No such keys exist.** The extrapolation model and its
  ranges are `model`, `fit_range` and `domain_range`, and they live in
  `extrapConfig.json`, a *different file* the task only points at. A variation
  written this way runs to completion and reproduces the nominal numbers under a
  new prefix.
- `if task_name in current_config` skips a block that is not found instead of
  failing. Blocks are named `correlation_task_2024`, not `correlation_task`, so a
  literal name matches nothing — again silently, and again producing a full set of
  "varied" results identical to the nominal.
- There is no collection step. Generating and running is the cheap half.

The first two are the same defect the C++ side is built to prevent — a
configuration that lies — reintroduced in Python, where none of the checks apply.
Whoever finishes this should resolve keys against the same configuration chain the
tasks use, and treat an unmatched block name as fatal.

### Two things to check when wiring it up

- `yieldSysHi` and `yieldSysLo` are `TMath::Abs` of a shift, so they are
  **magnitudes and not directions**. Drawing them as a +Hi/−Lo band assumes the
  +1sigma variation moves the yield upwards. Usually true, not guaranteed if the
  fit responds non-monotonically, and nothing verifies it.
- They need somewhere to go. Today the only thing that touches them is a
  `std::cout` in `CorrelationTaskBase`. Two TH1 beside the trends, `_systHi` and
  `_systLo` — two and not one, because they are genuinely asymmetric. And decide
  what `ComparisonTask` should do with them: `ListHistograms` discovers by content,
  so it would divide them on its own.

**MC closure has no verdict — half of it now exists.** `ComparisonTask` computes
the corrected/generated ratio bin by bin, with `divide_option: "B"` for the
correlated case, and writes it. What it deliberately does *not* do is state whether
it closes: no threshold, no pass/fail. That was a design decision rather than an
omission — the task divides, the physicist reads — but it means the second half of
this item is still open. Closure not closing is a systematic, and nothing yet turns
the ratio into one.

---

## 7. Comparing against pre-framework results: `objects` is the temporary answer

`ComparisonTask` has two modes. Normally it discovers what to divide and verifies
three levels of binning. When `objects` is listed, the denominator is a file this
framework did not write, nothing can be discovered, and nothing can be verified
beyond the axes of the objects themselves.

That second mode is deliberately a stopgap, and the reasoning is worth keeping
because it will look like an incomplete feature otherwise.

**Why it is not a separate task.** It arguably should be: `objects` sits in a
per-comparison entry while what it denotes — "the denominator is foreign" — is a
property of the whole task, since `target` lives at task level and cannot mean two
things at once. That is why mixing declared and discovered comparisons in one block
is now refused outright. Once mixing is forbidden, a task is entirely in one mode or
the other, which is very nearly two tasks already.

It was left as one because splitting costs about 280 lines moved for roughly 30
duplicated, and buys structure for a mode that should not outlive the checks it
exists for.

**What to do instead when these comparisons stop being occasional.** Not a richer
`objects`, and not a task of its own: **convert the old results into this
framework's layout, once.** `Macros/convertGraphHist.C` already takes the target
binning from the new result, so it can equally write the converted histogram at the
same path and under the same name in a file of its own. The ordinary discovered
comparison then works, with `RequireSameAxis` on every pair.

The stamps are the part that decides this, and the answer is not the obvious one:
**do not copy them from the reference.** That would verify the numerator against
itself, and a circular check is worse than no check because it looks like a check.
Write the converted file *without* stamps —
`RootIO::RequireMatchingBinningStamp` warns and continues when a stamp is missing,
rather than throwing, so the comparison runs and says out loud, per stamp, what it
could not verify. Which is the honest outcome: the assertion is still yours, it is
just made once at conversion time instead of once per JSON entry.

One thing to fix if that route is taken: the wording of that warning is written for
the projection cache — it ends with "delete the cache files" — and would read as a
non sequitur in a comparison.

---

## 8. Deciding by itself whether the projection cache is usable

`use_projection_cache` is set by hand, and the obvious wish is for the task to work
it out: build the projections the first time, reuse them afterwards. It is worth
writing down why that is not a matter of checking whether the file exists.

**The stamps answer the wrong question.** A cache carries `binning_mult`,
`binning_ptPhi` and `binning_ptAssoc`, so it can say *with what binning* its
projections were built. Nothing in it says *what they were built from*. Two
productions - 2024 and 2026, or the same one re-run after a change in the O2 task -
routinely share a binning, so a cache reused across them passes every check that
exists and holds the projections of the wrong input.

That is the same shape as the trigger-container bug recorded under "Known bugs":
the binning was right, the contents came from elsewhere, and every check passed
because every check was about binning. Auto-switching would reintroduce it one
level up, on objects that are expensive enough that nobody rebuilds them casually.

**Note which direction is already safe.** Asking for the cache when none exists is
fatal today, with a message saying to run once with `false`. So forgetting to build
it cannot hurt. The unguarded direction is reusing a stale one - and that is
precisely the one an automatic switch would make more likely. The flag is not
protecting against forgetting; it is protecting against remembering wrong.

**What has to come first, and is now done.** The cache writes its provenance -
`config_block`, timestamp, git commit - beside the stamps. Documentation only:
nothing reads it back on reuse. But without it the question "is this cache mine?"
had no answer at all, and `Macros/ShowProvenance.C` had nothing to say about these
files.

**The remaining two steps.** On reuse, compare a *restricted* key rather than the
whole block: `input_data_file`, `base_path_data`, `is_pure_gen`, and the names and
`dir_name` of the particles. Not the whole block, or changing `output_prefix` would
invalidate a perfectly good cache. Then the rule becomes statable: use the cache if
it exists, came from this input, and agrees on the binnings; rebuild if it is
absent; and **refuse** if it exists but came from something else - because
rebuilding silently there would mean two productions are quietly fighting over one
file.

Do not land this while output files are being rebuilt: introducing a cache key
invalidates every existing cache at once.

---

## 9. The delivery format, and the stage that is missing

The chain ends at "corrected trends, as TH1". A published result is not that, and
the gap is not cosmetic — it is three things a TH1 structurally cannot carry:

- **two errors per point.** Statistical and systematic need two objects.
- **asymmetric errors.** `ExtrapolationResult` already produces `yieldSysHi` and
  `yieldSysLo` separately.
- **an error on x.** In `OldCodes/ratioK0SPiCorrectedUpdated.C` the abscissa is
  `mult[] = {28.51, 24.54, ...}` with `errstatmult` and `errsystmult`: it is
  <dN/deta>, a *measured quantity with its own statistical and systematic
  uncertainty*, not the centrality percentile the trends carry on their axis.

The old macro delivers pairs of `TGraphAsymmErrors` sharing the same points and
differing only in the error, grouped in `TMultiGraph`, and finally a canvas.

**The decision: TH1 stays the internal currency, the graph is a terminal
rendering.** Everything that compares works on histograms — `ComparisonTask`,
`RequireSameAxis`, `TH1::Divide` with `"B"` — and a `TGraph` has neither an axis to
verify nor a division. So the conversion happens once, at the exit, in one
direction. Old results come *in* as TH1 through `Macros/convertGraphHist.C`; new
results go *out* as graphs at the last step and nothing downstream consumes them.

What is missing is that last stage: a task taking trends, the two systematic
histograms and the <dN/deta> table, and emitting the graph pairs. Two notes for
whoever writes it:

- `histToGraph` is not enough as it stands. It uses bin centres, and the published
  abscissa is <dN/deta> — so it needs the same explicit map `graphToHist` demands,
  in the other direction, and the table of <dN/deta> values is an input with a
  provenance, not a constant in the code.

  This is also, in hindsight, *why* `graphToHist` refuses to compute the
  correspondence from the x values. That was argued on general grounds when it was
  written; the old macro shows it is not general at all. Going from a centrality
  interval to <dN/deta> is a measurement, not a reformatting.

- **Keep the data and the figure apart.** In the old macro the canvas *is* the
  result — saved to `/Users/stefanocannito/Downloads/updateK0SPi2.pdf`, a personal
  path, no provenance, and the numbers reachable only by opening a `TCanvas` and
  digging the graphs out of it. Every intermediate stage of this framework already
  solves that; it would be a poor place to lose it.

---

## 10. Engineering

**Splitting RootIOHelpers further — a suggestion, not a decision.** The header now
holds two families: file access (open, get, navigate) and the records written into
those files (binning stamps, provenance). Both touch `TDirectory`, so by mechanism
they belong together; by concern they do not.

The first split was already made and was not a matter of taste: `RunEnvironment.h`
came out because those functions do not include ROOT at all, which is a fact and
not an opinion — and it left behind the only header in the framework that compiles
with a plain compiler, which is worth something on its own.

A second split into `Provenance.h` and `BinningStamps.h` is defensible but would be
three headers where there is one, on an argument that could go either way. The
signal to wait for is concrete: a caller that uses the stamps without the
provenance, or the reverse. As long as the four tasks always use them together,
separating them adds files without removing confusion.

**What a Try\* returns is a pragmatic choice, not a principled one.**
`TryArray` hands back a `ConstArray`, `TryObject` hands back the node. That looks
like a statement about arrays versus objects and it is not: rapidjson's
`GenericObject` has `operator[]`, `HasMember` and `FindMember`, and is as capable
as `GenericValue`.

The real reason is that the typed accessors in `JsonConfigHelpers.h` take a
`const rapidjson::Value&`. An object view cannot be passed to `OptionalString`, so
returning it would force the caller to look the key up a second time. Arrays escape
this only because their one consumer, `ReadNumberArray`, is ours and takes the view.

Two ways to make it principled, neither urgent:

- overload the typed accessors to take a `ConstObject` as well — comfortable at the
  call sites, eight or so extra overloads to maintain;
- have `TryArray` return the node too, like `TryObject`, and let callers write
  `GetArray()` — uniform and smaller, one more word per call site.

The signal to act is a caller that suffers from the current shape. There is none
today, and the comment above `TryObject` says all of this in place.

**`AnalysisDataStructures.h` has no namespace.** Every other shared header lives in
one (`AnalysisConstants`, `BinningUtils`, `HistogramUtils`); this one pours
`NSigma`, `FixedRange`, `ModelConfig`, `FitConfig`, `MathParam`, `ObservableConfig`
and a dozen more into the global namespace, next to ROOT and RooFit. They are
generic names, so a collision with a dependency is plausible, and when it happens it
depends on include order and the message points nowhere useful. A qualified name
(`Analysis::FitConfig`) also says where a type comes from at the call site.

Nine headers include it, so the change is mechanical but touches many lines; it is
best done with the compiled build at hand, since the compiler is what finds the
references that were missed. Check first that nothing outside the headers (the `.C`
macro, `Macros/`, strings in the JSON) refers to these names, because the build
would not report that.

The single common header itself is fine. It holds the model shared across tasks,
which is the case for a common header, and it should stay *physics* vocabulary:
configurations, results, loaded data. Types that describe how something is drawn
(`PadMargins`, `YRangeOptions`, `RatioCanvas`) belong with `CanvasManager`, whose
interface they are, and `DynamicRooFitter` already depends on it. The signal to
split the header by theme (fit and extrapolation / loaded data) is it growing well
past today's 241 lines, or pulling ROOT includes that only one theme needs.

**What the logger still does not cover.** Every task and helper logs through `Logger::`
(see `DESIGN_HISTORY.md`), but three things remain:

- **There is no way to switch `Debug` on from outside.** The threshold is `Info` and can
  only be changed with `Logger::SetLevel` in code, so the twelve `Debug` lines (the
  `[DEBUG EXTRAP]` block, the per-fit "Integrating in", the per-bin "Found custom
  parameters") are unreachable without editing a header. An environment variable read by
  the two entry points (`main.cpp` and the macro, which is where process-wide state
  belongs) would do. The same goes for `SetTimestamps`.
- **ROOT's and RooFit's own messages** go through other channels (`gErrorIgnoreLevel`, the
  ROOT error handler, `RooMsgService`) and do not reach the summary. Installing a ROOT
  error handler in the entry points that routes warnings to `Logger::Warn` would put them
  there.
- **The banners** of `WorkflowManager` (`>>> [STARTING TASK]`, the separators) and
  `RunAnalysisChain` are direct `std::cout`, on purpose.

`Codes/spdlog/` (1.1 MB, 102 tracked files) is no longer included by anything. It is
kept until the decision to remove it is made; `DESIGN_HISTORY.md` records why it
was not adopted.

**Which particles are extrapolated is decided in a different place from which are fitted
or corrected. To decide.** Two models coexist:

| | who decides | a missing section |
|---|---|---|
| purity and trigger fits | the task configuration declares them (`purity_particles` with their `purity_key`, the fixed `"phi"`); `fitConfig.json` has to follow | error (`FitConfigManager::GetConfig` throws) |
| efficiency | the task configuration, per particle (`corrections.particles`, since 16 October 2026) | a particle not listed is not corrected, by declaration; an unknown name or an empty entry stops the run |
| extrapolation | **`extrapConfig.json`**: the task configuration has only the switch `apply_extrapolation`, and a section's presence decides per particle | the particle is skipped, with an `[INFO]` |

No rationale is written down. The behaviour comes from `87e5c89` (7 July, "Make extrapolation
configurable per particle", empty message body); the diff shows what it allows: a species
without extrapolation next to one with it, including a ratio of an extrapolated numerator over
a plain denominator (`numHasExtrap` / `denHasExtrap`). That is a real feature and should
survive. Today it is not exercised: `associated_particles` is `[K0S, Xi, Pi]` and
`extrapConfig.json` has exactly those three sections, so nothing is skipped.

The cost of the implicit model is the case this framework is built against: a typo in
`extrapConfig.json` (`K0s` for `K0S`) switches extrapolation off for that particle with one
`[INFO]` line, and the numbers change with no error. The fit side stops at the start for the
same mistake.

Options, in the order I would take them:

1. **A declared list**, symmetric to the per-particle `corrections` block (it was `apply_efficiency_to`
   when this was written): `"apply_extrapolation_to":
   ["K0S", "Xi", "Pi"]`, required when `apply_extrapolation` is true. A listed particle must
   have its section (error at `Init`); one not listed is not extrapolated, by declaration and
   not by absence. Results do not change with the current lists. `HasConfig` and the
   `Skipping` message go away. Cost: one key in the correlation blocks of the JSONs
   (`correlation_base_settings`, `correlation_wpdg_base_settings` and their `ref` variants) and
   about ten lines in `CorrelationTaskBase`. Required and not defaulted, because it changes a
   number (`CLAUDE.md`, "A key that changes a number is declared").
2. **A key in each `associated_particles` entry** (`"extrapolate": true`), in the spirit of
   the per-entry choice proposed in item 2 above for PDG matching. Same guarantee as 1, and
   the two per-particle choices would then sit in the same place; more restructuring of the
   entries.
3. **Keep the model and raise the `[INFO]` to a warning** (`Logger::Warn`), so the case shows
   in the end-of-run summary. Cheapest; it makes the silent case visible but leaves two ways of
   deciding in one framework.

Already done around it: `ExtrapConfigManager::GetConfig` throws for a particle with no section
(its only caller asks `HasConfig` first, so that branch is reachable only by misuse). The
efficiency side of the same problem (`apply_efficiency_to` missing fell back to "all active
particles") is gone with the `corrections` block, where nothing is defaulted.

**Purely static classes that would read better as namespaces.** `EfficiencyCalculator`
is ten `static` functions, no data member, nothing private, and a deleted constructor
(commented "Purely static class"); only `MCTask` uses it. A class used that way is a
namespace in a more elaborate form: it gives no encapsulation because there is no state
and no private part, and it is out of step with the other stateless utilities
(`AnalysisConstants`, `BinningUtils`, `HistogramUtils`, `RootIO`, `RunEnvironment`,
`JsonConfig`, and now `Logger`), which are namespaces. `ExtrapolationModelFactory` has the
same shape (twelve `static` functions, deleted constructor) but also has a private
section, so it is a separate decision. `CorrelationCalculator` is not a case of this: it is
a real class, built with its configuration (`applyME`, `useCacheMode`, ...) and holding
it as instance state.

Converting is mechanical and callers do not change: `EfficiencyCalculator::Compute...`
reads the same, and the nested `ParticleCorrectionMode` and `Maps1D` stay reachable under
the same names. The one thing that must not be forgotten is `inline` on every function:
inside a class a function defined in the body is implicitly `inline`, in a namespace it is
not, and a definition in a header without it breaks as soon as a second `.cpp` includes
the header. With the single translation unit of today it would still compile, so the
omission would stay invisible (the same family as the missing `inline`s found by hand
before the compiled mode existed).

Low priority, a matter of consistency and not of behaviour. Not worth a change on its own;
a good moment is the next time `EfficiencyCalculator.h` is edited for another reason. The
compiled build catches a missing `inline` only once there is a second `.cpp`, so do it
together with the check mentioned under the translation-unit note below.

**Regression test.** A large amount changed on 31 July with the intention that the
numbers stay identical except where deliberately changed. Nothing verifies this. A
script that runs a known configuration and compares bin contents against a saved
reference would catch exactly the class of regressions this codebase produces:
silent ones.

**Compiled build is one translation unit: fine today, with a known way out.** The
whole framework is header-only and `main.cpp` is the only `.cpp`, so touching any
header recompiles everything. Measured on 14 October 2026 (ROOT 6.36.10, Apple
clang, `-O3`): touching `CanvasManager.h`, which everything includes, gives a
rebuild plus link of **4.5 s**. That is shorter than the wait an interpreted run
already pays, so there is nothing to fix. Re-measure before acting; the threshold
worth reacting to is roughly ten seconds.

Splitting into several `.cpp` is not free just because the code is header-only.
That style does work across translation units as long as every definition is
`inline`, in-class or a template - the same rule that found the missing `inline`s by
hand - but each `.cpp` re-parses every header it includes, so the total grows with
the number of files unless they share little. The 49 distinct ROOT/RooFit headers
included are the likely bulk of the cost, not this code. In order of effort:

- **Precompiled headers**, `target_precompile_headers` on the main ROOT/RooFit
  headers. No code change, and it works with a single translation unit. First thing
  to try.
- **Split by how often a header changes, not by class.** Move only the heavy and
  stable ones (`DynamicRooFitter`, `SpectrumExtrapolator`) into `.h`/`.cpp` as a
  library; the tasks being edited stay header-only.
- **One `.cpp` per task**, methods out of the headers. Most work, and the one that
  pays most when a single task is edited.

The cost shared by the last two: once a header no longer holds its definitions it
stops being enough for the interpreted mode. The macro would have to load the
compiled library (`R__LOAD_LIBRARY`) or include the `.cpp` files, which gives up the
"edit a header and rerun" property that is most of the reason for keeping both
modes.

**`global_binning` is per workflow, but the expectation is per production.** A
single JSON can only describe one family of tasks. If one ever mixes an MC task
(fine binning) and a correlation task (coarse), one declaration cannot describe
both. The inheritance mechanism already supports the fix: allow a `global_binning`
block inside a task node, overriding the global one.

**`AnalysisSettings` is no longer "global".** What is genuinely shared by every
task of a workflow is now only the scheme name and the two colour palettes; the
binnings are declarations about the production being read. Options: rename the
JSON key `global_binning` → `expected_binning` (with a deprecation period
accepting both), or split the struct into `ExpectedBinning` + `AnalysisSettings`.
Best done together with the decision above, so the naming falls out instead of
being chosen.

---

## 11. Cling constraints learned the hard way

- **`std::format` with floating-point specs does not compile.** `{:g}`, `{:.2f}`
  and friends make the consteval format-string validation fail: for float specs
  libc++ reaches `__builtin_clzg` through the unicode handling, which the
  interpreter cannot fold. Plain `{}` placeholders are fine. Hence
  `BinningUtils::FormatEdge`, which uses `snprintf("%g")`.
- **The same applies to the chrono specifiers.** `{:%F %T}` and friends go through
  that same consteval validation, so `RunEnv::TimestampNow` formats the time with
  `strftime` rather than `std::format`. Assume any non-empty format spec is
  suspect under Cling, not just the numeric ones.
- **The code already requires C++20** beyond `std::format`: `emplace_back` on an
  aggregate uses parenthesised aggregate initialisation (P0960), which does not
  compile in C++17.
- O2 is C++20 with `CMAKE_CXX_STANDARD_REQUIRED`, and the framework is now compiled
  as well (`--compile`, see `DESIGN_HISTORY.md`); `CMakeLists.txt` takes the
  standard from ROOT.
- **A header is only checked when something loads it.** The first compiled build found
  errors in `CanvasManager.h` that Cling reports too (verified by loading the previous
  version): it had not been run since it was edited. The compiled build checks every
  header whether or not a run reaches it; see `DESIGN_HISTORY.md`.

---

## 12. The online configuration is not checked (ideas, nothing decided)

Recorded on 15 October 2026 as possible directions, not as decisions.

**What is covered today.** Everything the framework checks against its inputs is about binning and bin
indices (*The rule the whole framework is built on* in `CLAUDE.md`): the binnings are read from the
`AnalysisResults` files, whose axes the O2Physics tasks build from their configurables
(`AxisSpec{(std::vector<double>)binsMult, ...}`), and compared with the JSON. That part already gets its
information from the online run, through the axes.

**What is not.** The framework knows nothing of the configuration the O2Physics train ran with: mass windows,
PID cuts, selection flags, `eventSelectionType`, `analysisMode`, the cuts of the producers and of
`track-selection`. Two productions with the same binning and different cuts pass every check, and so does a
data file paired with an MC/efficiency file produced with different cuts.

A concrete case. In the train configuration `configuration-8.json` the windows of `phi-strange-correlation`
are `rangeMK0sSignal` = [0.475, 0.515], `rangeMXiSignal` = [1.305, 1.335], `pidTPCMax` = 3: the same windows
as `integration_range` in `fitConfig.json`, so the purity is computed in the window used online to select the
candidates. But they agree only because they were set by hand: the defaults in `phiStrangeCorrelation.cxx`
are [0.47, 0.53] for the K0S and [1.31, 1.33] for the Xi. A train run with the defaults would give a purity
for a different sample, and nothing would say so.

Two smaller cases of the same kind are caught but not recognised: a directory renamed in O2Physics stops the
run as "histogram not found" (`CLAUDE.md`, *Configuration*, now says to ask for the new path), and a change of
histogram type (the `Corrections` files hold TH1D where `CorrelationTaskBase::LoadCorrections` asks for TH1F)
stops it with a type error that does not name the cause.

**Idea 1: read the train's `configuration.json`.** Hyperloop already writes it, with every task of the train,
producers included. Keep it next to each `AnalysisResults` in `DataFile/`, have the framework read it, record
it (or a fingerprint of the relevant blocks) in the provenance, and check a list of keys declared in the
configuration: online windows and PID cuts against `integration_range`, and the keys that must agree between
data and MC inputs. A mismatch throws, as the binning checks do. No change to O2Physics and no question of
merging. The open point is how a file is associated with its train configuration (a naming rule, or a key in
the global configuration next to each input file).
Part of it exists already: `HYConfigs/Trains/*.yaml` maps each `train_id` to the `output_name` of its
`AnalysisResults`, so the download step could fetch the train's configuration too (whether Hyperloop allows
it from there was not checked).

**Idea 2: an `OutputObj` in the O2Physics task.** `OutputObj<T>` (`Framework/AnalysisHelpers.h`) writes any
TObject into `AnalysisResults.root`; a `TNamed`/`TObjString` holding the relevant configurables as JSON would
make each file describe itself. Two limits: it covers only the tasks that are modified (not the producers or
`track-selection`), and Hyperloop merges the outputs of many jobs, so the object needs a sensible `Merge` —
how the merger treats a `TObjString` is not known and must be tested by merging two outputs before relying on
it. Writing the binnings this way as well is not worth it: they are already on the axes, and a second copy is
one more thing to keep aligned.

Suggested order: idea 1 first (complete, no O2Physics change), idea 2 only as a complement once the merge
behaviour is verified.

## 13. Corrections across O2Physics and the framework: a plan in phases

Recorded on 16 October 2026, after a review of `phiStrangeCorrelation.cxx`. Several changes on the
O2Physics side have a counterpart here, and the order matters; this section is the plan, with the decisions it
still needs marked as such.

**What is already done.** On the O2Physics branch `phistrange-ccdb-ownership` (fork `scannito/O2Physics`,
pushed, no PR yet), three commits:

- `4d3da8d48` — the efficiency maps are non-owning. With caching on, which is the default
  (`mCachingEnabled = true` in `BasicCCDBManager.h`; `setCaching(true)` only states it), the manager owns
  every object it returns and hands out a raw pointer: the task's `shared_ptr` deleted each map twice, and at
  a run change with `perPeriodEfficiency` the reassignment deleted a map the cache was still serving
  (use-after-free on the second run). They are also fetched once as `TH1` and dispatched with
  `dynamic_cast`: the cache is keyed by path only and returns `reinterpret_cast<T*>` of what it holds, so
  asking a path as `TH3D` and then as `TH2D` returned a `TH2` as a non-null `TH3` on a cache hit. `TH1` works
  for both because the CCDB API reads with `TFile::GetObjectChecked`, which accepts the class or a derived one.
- `61d1ea081` — the code commented out in #18061 is removed (the user's rule: commented code goes at the next
  PR), with seven unused axes and the "TO BE REMOVED" tags on `cutZVertex`/`vertexZAxis`, which stay.
- `1cef879bf` — `propagateEffError` is replaced by `effErrorOffline` (point 3 below).

Checked: the file compiles (`-fsyntax-only` with the compile command of the local build; a planted error is
caught), clang-format and the O2 linter pass, each commit applies alone on upstream `master`. Not yet done:
rebase on upstream, a full build, a run on data, the PR description (phase 0).

### Where the two sides touch

1. **The efficiency maps.** `MCTask` produces them, `uploadToCCDB` puts them in CCDB, the O2Physics task reads
   them. The axis order must agree: 3D = (multiplicity, pT, y), 2D = (pT, y), as `BoundEfficiencyMap` reads
   them. Nothing checks it. The bin errors matter too: the online propagation uses them.
2. **Where a correction is applied.** The efficiency of a particle is applied online (`applyEfficiency`,
   `applyPhiEfficiency`) or here (`acceptance_efficiency` in the `corrections` block), never both. With
   `applyEfficiency` on, the online task corrects **every** active associated species; only the φ can be left
   out. Valid combinations: everything online and nothing here; associated online and `["Phi"]` here; nothing
   online and everything here. Signal loss and event loss are always applied here, which is what the
   `*_online_efficiency_no_phi` blocks do. A double correction passes today without an error, because the
   framework does not know how the train ran (section 12).
   The split by particle works because the pair weight factorises, 1/(ε_φ · ε_assoc): one factor can come
   from the task, the other from here (`totalEff = phiEff * assocEff` in `CorrelationTaskBase`). For the
   *same* particle the two are not interchangeable even in principle: online, each candidate gets its own
   efficiency in (multiplicity, pT, y); here, a pT cell is divided by one average efficiency, with the y
   dependence already integrated out.
   The `no_phi` blocks listed `["K0S", "Xi", "Pi"]` in `apply_efficiency_to` with `active_corrections:
   ["signal_loss", "event_loss"]`: no double efficiency. The key name was what misled (it meant "corrections
   to", not "efficiency to"), one of the reasons for the per-particle `corrections` block, where those blocks
   now list the three particles with `signal_loss` only.
3. **The uncertainty of the maps.** It is the same for every entry that reads a map bin, so it is fully
   correlated: summed in quadrature per entry it is underestimated by √N, and it is also correlated across the
   Δφ/Δy bins it feeds. A worked case: ε = 0.50 ± 0.05 (10%), so w = 2 and wErr = 0.2; 100 pairs in one
   (Δy, Δφ) bin, all from the same map bin; content 200. Per entry: statistical √(100·4) = 20, map
   √(100·0.04) = 2, i.e. 1%. Correct: the map term is 100·0.2 = 20, i.e. the 10% of the map. The per-entry
   sum makes a systematic shrink with statistics, which a systematic does not do.
   - Online, the task adds `wErr²` per entry by default. This was `propagateEffError: true`, which the user
     always set, so the default reproduces the productions; `propagateEffError: false` (the old default) gave
     only `w²` because the map error was ignored, not by design. The default is the wrong version on purpose
     and temporarily: there must be no case where the efficiency is applied online and its uncertainty goes
     nowhere. `effErrorOffline: true` adds only `w²` and leaves the map uncertainty to be propagated
     afterwards. `customFillHist`/`customFillTHn` are unchanged: they add whatever `wErr` they are given, and
     with `effErrorOffline` it is 0.
   - "Propagated afterwards" cannot mean here, from the output alone: the map is read at the y of each
     particle, and the output keeps only Δy, so the map bins behind an output bin are unknown. It means runs
     of the task with the map shifted by ±1σ, compared here as a systematic (phase 4).
   - Here, it is not propagated at all: `totalEff` is read with `GetBinContent`
     (`CorrelationTaskBase`, where `phiEff` and `assocEff` are built) and divides the spectrum with
     `Scale(1.0 / totalEff)`, so its error is lost. Here it *can* be propagated from what is at hand,
     because a whole pT cell is divided by one number: a shift of `totalEff` by ±1σ, fully correlated across
     the Δφ bins of the cell. It belongs at trend level, as the normalisation does.
4. **The mass windows.** The defaults in the O2Physics task, the train configuration and `integration_range`
   in `fitConfig.json` agree only because they were set by hand (section 12).
5. **Candidates with zero efficiency.** The online task gives weight 1 to a pair whose efficiency product is
   ≤ 0 (for instance a candidate outside the map), and loses the correction of the other particle too.
   `hStrangeCorrelation` sets the efficiency of that particle alone to 1, silently. Either way the number is
   wrong and nothing says so. A zero efficiency comes from a candidate outside the range of the map, or from
   a map bin without MC statistics; in both cases the right correction is not computable, and weight 1 mixes
   corrected and uncorrected entries in the same bin. Recommended: check per particle, discard and count
   (an acceptance cut that closure tests see); the fallback is efficiency 1 for that particle alone, as
   `hStrangeCorrelation` does, plus the counter. `hStrangeCorrelation` also adds the efficiency error per
   entry (`fillCorrelationHistogram`), so it is no reference for point 3 either.

### Phases

**Phase 0 — close the current O2Physics branch.** No dependency on this side. A run on data with
`perPeriodEfficiency` and more than one run, compared with an earlier output produced with
`propagateEffError: true`: the numbers must agree. Then the PR to `AliceO2Group/O2Physics`, and
`propagateEffError` removed from the wagon configurations.

**Phase 1 — replace the correction keys here with a per-particle structure. Done on 16 October 2026** (see
*The correction configuration: done* below). Independent of O2Physics. The
names say "efficiency" but the keys cover every correction: `apply_efficiency` decides whether
`LoadCorrections` runs at all (efficiency, signal loss, event loss), `apply_efficiency_to` lists the particles
efficiency *and* signal loss go to, `input_efficiency_file` is the `Corrections` file. First proposed as a
rename (`apply_corrections`, `apply_corrections_to`, `input_corrections_file`), then replaced by a structure:
see *The correction configuration is per particle* below. The C++ side is designed first, the JSON after. The
old keys were first made to throw, naming what replaces them; that check was removed on 19 October,
since `corrections` is required and an unmigrated configuration stops at it (`CLAUDE.md`, *Configuration*). `DESIGN_HISTORY.md` keeps the old names, being a record of what was.

**Phase 2 — small O2Physics changes, with a reader here.** Defaults aligned with the train: not only the mass
windows, several configurables have defaults that differ from what the trains use (deferred by the user, 16 October).
Zero-efficiency candidates checked per particle, not on the product, and counted in a histogram per species;
this side reads the counter and issues a `Logger::Warn` when it is not empty.

**Phase 3 — let the framework know how the train ran.** Section 12, ideas 1 and 2. What is needed for the
corrections: `applyEfficiency`, `applyPhiEfficiency`, `effErrorOffline`, the CCDB path of the maps, the mass
windows. With that, the framework throws when `acceptance_efficiency` is applied to a particle already
corrected online, compares the windows with `integration_range`, and records the online settings in the
provenance. A file without the information (every production made so far) gets a `Warn`, not an error, or the
existing productions become unusable.

**Phase 4 — propagate the map uncertainty properly.** O2Physics: a way to shift the map by nσ, either a
configurable used in separate wagons or extra histograms filled in the same pass with the shifted weights.
Here: a systematic from those outputs, and, for the corrections applied here, the uncertainty of `totalEff`
propagated at trend level. Only then `effErrorOffline` becomes the default online and the per-entry version
goes, together with `customFillHist`/`customFillTHn`, which a plain `Fill` with `Sumw2` replaces.

Order: phase 3 before phase 4, because without knowing how the train ran the framework cannot choose which
propagation applies; and the default of `effErrorOffline` changes only when phase 4 is in place on both sides.

### The correction configuration: done

Phase 1 is done (16 October 2026): the correction keys are replaced by a per-particle
`corrections` block. The design, its reasons and how it was checked (identical numbers on every
output object, 12 refusals tested) are in `DESIGN_HISTORY.md` (*The corrections block*, *How the
corrections block was checked*); the keys are in `README.md`. What is left of it:

- **The K0S to `"mult": "binned"`.** Every configuration still says `integrated` for every particle,
  which reproduces the earlier numbers. Moving the K0S, as the user's efficiency studies suggest, is
  a physics change of its own: one key per mode block, and a K0S whose trends move by up to 2.4% in
  the one task where it was tried.
- **The species-specific corrections**, below: their keys are reserved and refused.
- **Phase 3** checks `perParticle[p].accEff` against the particles corrected online.

### How the O2Physics side is delivered

- **The current branch.** Its three commits are independent (each applies alone on upstream `master`, checked
  with `git merge-tree`), so they can go as one PR or as three. As three: the ownership fix (a crash, small,
  to go in fast), the clean-up (pure removal, 320 lines that would hide the fix), `effErrorOffline` (the only
  one that changes numbers, for whoever did not set `propagateEffError`).
- **The later phases form a stack.** A is the current branch; B (zero efficiency, phase 2) and C (`OutputObj`,
  phase 3) both build on A and not on each other; D (shifted maps, phase 4) builds on A and C. The defaults
  are independent and go straight to `master`.
- **Stacking from a fork.** GitHub's native stacked PRs and ghstack need every branch in the target
  repository, and the user has no push on `AliceO2Group/O2Physics`. Sapling (`sl pr submit`) works from a
  fork: it pushes `prNNNNN` branches to the fork and opens overlapping PRs upstream, each containing the
  commits below it, with the stack listed in a footer. Giulio Eulisse uses it in O2 and O2Physics (O2 #15443
  and #15444: merging the top PR landed both commits; O2Physics #16158, #17725). Without Sapling the same
  shape is kept by hand: PR n on `master` with "depends on #…", and `git rebase --update-refs` after each
  squash-merge.
- **Sapling and aliBuild**, measured on a scratch clone (Sapling 0.2.20260929, aliBuild 1.17.44). aliBuild
  supports Sapling (`alibuild_helpers/sl.py`) and uses it for a development package whose source has `.sl` or
  `.git/sl`, when `sl` is in `PATH`; any `sl` command in a git checkout creates `.git/sl`. With Sapling the
  build is named `O2Physics-latest-none` unless a bookmark is active; commit and clean-tree hash are the same
  as with git (whether the switch forces a rebuild was not checked). Git and Sapling must not both *write* in
  one checkout: `sl commit` moved the checked-out git branch, `sl prev` detached HEAD, `sl hide` deleted a git
  branch that pointed to hidden commits. Read-only git commands are harmless. A second checkout for Sapling
  is costly with aliBuild (it needs `alidist` and `O2` next to it, and two copies on the same commit may share
  a build directory configured for the other's sources, not checked), so the plan is one checkout, Sapling
  only. `brew install sapling` upgrades `openssl@3` and `python@3.13`: not while a build runs.

### Species-specific corrections not implemented: Λ feed-down, π primary fraction

The Λ needs a feed-down correction that the framework does not have. The histograms it needs were added to
the O2Physics task in #18061 ("Added necessary histograms for Lambda feeddown"); nothing reads them here yet.
How it enters the correlation (a scale factor on the spectrum, or a feed-down correlation to subtract) is still
to be decided. In the correction structure it is an optional per-particle correction, accepted only for the
species that have it; until it is implemented, a configuration that asks for it must throw, not be ignored.

The π needs the fraction of primaries (the pions from weak decays and from the material are not part of the
measurement; the user, 16 October 2026). Nothing provides it today: the framework has no such correction, and
the O2Physics task only selects `isPhysicalPrimary()` at generator level. Same treatment in the structure: an
optional per-particle correction, accepted only for the π, throwing until it is implemented. Where the fraction
comes from (MC, or a fit of the DCA distribution in data) is still to be decided.

### Decisions still open

1. Zero efficiency: discard the candidate and count it, or efficiency 1 for that particle and count it. If
   discarded, the φ must be removed consistently from the trigger count and from every pair it forms.
2. ~~Where the per-particle corrections live~~ — decided on 16 October: a block of their own (see *JSON*
   above).
3. How the framework learns the train configuration: idea 1, idea 2, or both (section 12 suggests idea 1
   first).
4. Shifted maps: separate wagons with a configurable, or shifted histograms in the same pass (three times the
   CPU against two or three times the memory of the THnSparse).
