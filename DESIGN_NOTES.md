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

**`SpectrumExtrapolator`.** `hlo`, `hhi`, `hInt`, `hMean`, `hInt_tmp`, `hout_rnd`
are created without `SetDirectory(0)` while `gDirectory` is the spectra output
file: they end up written into `PhiAssocSpectra.root` on close, and produce
"Replacing existing TH1" warnings. Also `hIntegral_tmp` has range
`0.5*integral … 1.5*integral`, degenerate when the integral is ≤ 0.

**`YieldMean.h`** (legacy extrapolation path, `use_legacy_extrapolation: true`):

- `htotextra` is created with `htot->GetNbinsX() + 1` bins but with edges from a
  hardcoded 11-element vector — with a 33-bin binning ROOT reads ~34 doubles out
  of an array of 11;
- `htotextra` is left uninitialised when `part` contains none of K0S/Pi/Xi;
- `binlo` and `lo` in `YieldMean_LowExtrapolationHisto` are uninitialised when all
  bins are empty;
- `using namespace std;` at global scope in a header;
- default arguments repeated between declaration and definition, which the
  standard forbids — Cling tolerates it, a compiled build would not.

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
