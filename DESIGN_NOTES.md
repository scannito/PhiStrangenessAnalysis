# Design notes — open items

Written after the refactoring session of 31 July 2026, extended on 3 August.
Everything here is *decided but not done*, or *found but not fixed*. Items are
ordered by how much they matter, not by how much work they are.

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

- **Mixed provenance.** Task B can take `K0S` from a correlation task run without
  PDG matching and `Xi` from one run with it. Today a ratio between two species
  is only computable if both were produced by the same task, hence by the same
  method — which is exactly the constraint we want to lift (see item 2).
- **Re-running the cheap half.** Extrapolations and ratios can be redone without
  repeating the extraction. The projection caches exist to work around this; a
  dedicated task makes it structural.
- **Dependencies.** Task A would lose `ExtrapolationModelFactory`,
  `SpectrumExtrapolator`, `ExtrapConfigManager` and — most importantly —
  `YieldMean.h`, which today is included by every path that goes through the
  correlation task, i.e. by the whole data analysis. See item 7 for why that
  matters. Task A also loses `AnalysisConstants`: the particle mass in
  `AssocParticleConfig` is used *only* by `ExtrapolateSpectrum`, so that struct
  reduces to name, dirName and binning.

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
others) without touching the structure. Do it *after* item 1, not before: it adds
responsibility to a class that already has too much.

---

## 3. Common interface for the two fitters

`DynamicRooFitter` is constructor → `DoFit()` → `ExtractYieldsAndPurity()` →
`SaveFitCanvas()`. `FitPhiSignalAndBkg` does everything in its constructor and
exposes getters. The consequence is the fifty-line `if/else` in `PhiFitTask::Run`
where the two branches share nothing, and every addition has to be written twice.

Proposed shape:

```cpp
struct FitYields { signal, background, bkgInSideband, bkgRatio, purity; int status; };

class IPhiFitter {
  virtual int DoFit() = 0;
  virtual FitYields Extract() const = 0;
  virtual void SaveFitCanvas(TDirectory*, const std::string&) = 0;
};
```

plus a `MakePhiFitter(type, hist, cfg)` factory, in the style of the task registry
in `WorkflowManager` and of `ExtrapolationModelFactory`.

Internal clean-ups that come almost free once the constructor is split:

- `FitPhiSignalAndBkg` hardcodes `Voigt` + `BkgSourav` for the decomposition while
  pretending to be generic: passing `VoigtBkgMattia` would silently decompose with
  the wrong background;
- `indexFirstBkgParam` is a magic integer the caller must get right (4 today);
  getting it wrong splits the covariance matrix at the wrong place, with no symptom;
- two `new TF1` per call are never freed, i.e. `nBinMult × nBinPt` leaks per run;
- `wSidebandFit` as a template parameter duplicates the class for a one-line
  difference and forces a compile-time choice where everything else is JSON-driven;
- three `std::cout` inside the constructor, i.e. ~720 lines of output per run.

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

**`SpectrumExtrapolator` — fixed, kept here for the record.** The scratch
histograms (`hlo`, `hhi`, `hInt`, `hMean`, `hInt_tmp`, `hout_rnd`, `hout_cohrnd`)
now call `SetDirectory(nullptr)`. They were being created while `gDirectory` was
the spectra output file, so they were written into `PhiAssocSpectra.root` and
warned "Replacing existing TH1" once per toy iteration. `hIntegral_tmp` is still
degenerate when the integral is ≤ 0 — untouched, because the legacy is too.

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

**Why there is only one fit, and when that stops being true.** The upstream
`YieldMean.C` takes two histograms, `hstat` and `hsys`, and builds
`htot = sqrt(stat^2 + sys^2)`. It then fits twice on purpose: the central value
comes from the fit weighted with the *total* error, while the toy MC must start
from a fit weighted with the *statistical* error only, or the statistical
uncertainty it returns would contain the systematic.

`hsys` was deliberately removed from our copy - `hout` went from 9 bins to 5, the
`kYieldSysHi/Lo` and `kMeanSysHi/Lo` entries went with it, and so did the whole
SYSTEMATICS section (four more fits: high, low, hard, soft). What is left of the
first fit is a bare `Clone` of `hstat`, so the two fits run on identical data and
the second is the first repeated. Only the comments ("create histo with stat+sys
errors", "Fit sys+stat") survive from the two-histogram version.

That is what makes `SpectrumExtrapolator`'s single fit correct - not a shortcut,
but not unconditional either. **The condition is that the central value and the
statistical error are measured on the same histogram.** The day systematics come
back, the two-fit structure is the thing to restore.

Upstream: https://github.com/alisw/AliPhysics/blob/master/PWGLF/SPECTRA/UTILS/YieldMean.C

**The systematic machinery is half ported, and the half that exists is wired to
the wrong errors.** `SpectrumExtrapolator::GetExtremeHighHisto`, `GetExtremeLow`,
`GetExtremeSoft`, `GetExtremeHard` and `ReturnExtremeHisto` are all there and
nobody calls them. Missing is the driver: fit each variation, integrate, take the
absolute difference from the central value. Missing more quietly is the input -
upstream builds these from `hsys`, while ours build them from
`fMeasuredSpectrum`, whose errors are statistical. Wired up as they stand they
would return the statistical error propagated coherently, wearing the name of a
systematic. Change the input before the driver.

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

`use_legacy_extrapolation` can now be removed. Keeping it costs nothing and it is
the only executable definition of "correct" this code has, so removing it should be
its own commit, reversible, and only after the open question below is settled -
because if the fit turns out to be wrong, the reference is what tells us whether
the new path is wrong in the same way.

**Open, and no longer a code question: the fit does not describe the data.**

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

Separate these before trusting the extrapolated yields, whichever branch produced
them: this chi^2 is the same in both.

**`YieldMean.h` — fixed, kept here for the record.** All four defects listed in
earlier versions of this file are gone: the `htotextra` block that read ~34 doubles
out of an 11-element array, and its uninitialised twin when `part` matched none of
K0S/Pi/Xi, were removed with the block itself; `binlo`/`lo` in
`YieldMean_LowExtrapolationHisto` now start at -1 with a guard, and the comment
above it records what the bug was; `using namespace std;` is gone from the header;
and the default arguments now appear only in the declaration.

Worth keeping written down because this is ALICE-standard code kept as received:
the next person to sync it from upstream will reintroduce all four.

**`PhiFitTask`, legacy fitter branch.** Only parameters 1, 2 and 3 of
`VoigtBkgSourav` are set. Parameter 0 (the Voigtian normalisation) and 4–6 (all of
`BkgSourav`) start at zero, so Minuit starts from an identically null function.
This is a plausible cause of the poor convergence in that path.

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

**Systematics.** There is no machinery at all: results carry statistical
uncertainties only. The framework is already parameterised in the right way —
signal and background models, integration and sideband ranges, differential vs
integrated efficiency, 1D vs 2D ME normalisation, five extrapolation models are
all JSON keys. What is missing is a driver that generates N varied configurations,
runs them, and collects the spread per bin (plus a Barlow check to decide whether
a variation is significant). This is the real reason to write a Python driver.

**MC closure has no verdict.** Both pieces exist (`CorrelationWPDGTask`,
`is_pure_gen`) but nothing computes the corrected/true ratio bin by bin and states
whether it closes within uncertainties — which is both a validity check and, when
it does not close, a systematic.

---

## 7. Engineering

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

## 8. Cling constraints learned the hard way

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
