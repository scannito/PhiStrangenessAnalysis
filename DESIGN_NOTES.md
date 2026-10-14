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

**Fit status is discarded.** `DynamicRooFitter::DoFit()` returns
`fitResult->status()` and nobody looks at it. On ~240 fits per run, the number
that failed is information that exists and is thrown away. A counter and one line
at the end of `Run` would be enough.

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

**Regression test.** A large amount changed on 31 July with the intention that the
numbers stay identical except where deliberately changed. Nothing verifies this. A
script that runs a known configuration and compares bin contents against a saved
reference would catch exactly the class of regressions this codebase produces:
silent ones.

**Compile instead of interpret.** A small `main()` calling `RunAnalysisChain`
would give real compiler diagnostics (all the missing includes, missing `inline`
and duplicated default arguments found by hand today would have been caught in
seconds), a natural exit code, spdlog at no runtime cost, and freedom to choose
the language standard. `runAnalysis.sh` and the macro entry point are already
structured so that the switch is deleting the wrapper.

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
- O2 is C++20 with `CMAKE_CXX_STANDARD_REQUIRED`, so that is the standard to
  target if the framework is ever compiled.
