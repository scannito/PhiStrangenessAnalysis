# Design notes — a second opinion (Claude)

Written on 15 October 2026 by Claude, at the owner's request, as a second voice next to `DESIGN_NOTES.md`.
These are **opinions, not decisions**: nothing here is agreed, planned or started. Where an item overlaps an
entry of `DESIGN_NOTES.md`, the entry is named; that file stays the place for what has been decided.

**What the opinions rest on.** I know well the fitter (`DynamicRooFitter`), the purity step (`PurityTask`),
the configuration layer (`JsonConfigHelpers`, the config managers, the global configs), the logger and the
binning checks; I studied them closely during the RooFit research (`RooFitKnowledge/`). I know
`CorrelationTaskBase`, `SpectrumExtrapolator`, `MCTask` and `ComparisonTask` much less: on those, the
remarks below are about structure, not about what the code computes.

**What I would not do: rewrite it.** The framework works and reproduces its results bit for bit (the purity
step reproduces the archived files exactly, `RooFitKnowledge/11-...`). Everything below can be introduced
piece by piece, and several items are already in `DESIGN_NOTES.md`.

---

## 1. What I would keep as it is

- **Binnings are read from the input files, never trusted from the configuration**, and a mismatch stops the
  run (`CLAUDE.md`, *The rule the whole framework is built on*). This is the best decision in the framework.
- **Checks throw, provenance never throws**, and the warnings come back in the end-of-run summary.
- **A configuration that drives the run**, with no physics parameters scattered in the code.
- **Objects that cannot be half-built.** `ParticleTask` derives `mappedSourceBins` in its constructor, and
  `MapToSourceBins` throws there, before any fit has run. That is the right style: invariants guaranteed by
  construction rather than checked later.
- **The task factory in `WorkflowManager`** (a map from name to constructor): simple and sufficient.

---

## 2. The framework as a whole, by impact

### 2.1 Address results by interval, not by index

Results live in many `TH1` named `..._multBin3`, and a large part of the checks (binning stamps,
`RequireSameAxis`, `VerifyTriggerAxes`) exist to close the hole "index 3 here is not index 3 there". One
multidimensional container per quantity, with real axes (multiplicity x pT x ...), makes consistency
structural instead of checked by hand. It is `DESIGN_NOTES.md` section 4 carried all the way. Highest value in
the long run, and the most invasive: best done when `CorrelationTaskBase` is reorganised.

### 2.2 A pipeline of stages with artefacts and manifests

Each stage (projections, purity, phi fits, correlations, extrapolation, delivery) reads declared inputs and
writes its outputs together with a manifest: hash of the resolved configuration, hashes of the inputs, the
O2Physics train configuration. Only what changed is rerun. This replaces the reasoning about whether the
projection cache is usable (`DESIGN_NOTES.md` section 8) and gives the traceability that is missing on the
online side (section 12). A small hand-written DAG is enough; no workflow engine is needed.

### 2.3 An explicit contract with O2Physics

One place that declares the histogram paths read from `AnalysisResults`, and the keys of the train's
`configuration.json` that must agree with the offline choices (mass windows and PID cuts against
`integration_range`; the cuts that must agree between data and MC inputs). The two problems found on
15 October — directories renamed in O2Physics, and online windows that agree with `fitConfig.json` only
because they were set by hand (the task defaults differ) — would become clear errors. Details and the two
possible routes in `DESIGN_NOTES.md` section 12. Note that the association between an `AnalysisResults` file
and its train already exists: `HYConfigs/Trains/*.yaml` maps each `train_id` to its `output_name`. If the
download step can fetch the train's configuration as well (not checked), the association problem is solved.

### 2.4 The fit as a function that returns a record

Input: the histogram and the model specification. Output: a persistent record with values, covariance,
status, EDM, covQual, NLL, quality flags and provenance. Plots are made from the records, not from a fitter
object kept alive. The coupling "`DoFit()` before `ExtractYieldsAndPurity()`, and keep the object alive for
`DrawFit`" (`DESIGN_NOTES.md` section 3) disappears. The robust procedure (offset, refit, a few random starts
in the cells at risk, `IntegrateBins`; `RooFitKnowledge/00-summary.md`) lives in one place, and the quality
of every cell becomes data instead of a line lost in the log. The HS3 provenance record prototyped in
`RooFitKnowledge/02-json-hs3.md` section 10 is a ready-made format for such a record.

### 2.5 Compiled mode only

Running under Cling imposed a whole class of constraints: `std::format` limited to what Cling accepts,
exceptions that cannot be caught in some modules (`root-issue-cling-eh/`), headers that are only checked when
something loads them. I would keep ROOT as a library inside a compiled executable, and keep macros for quick
plots only. The cost is losing the interactive loop, which the incremental CMake build mostly replaces.

### 2.6 Tests with a known truth

I found no test suite (only `Macros/TestProjections.C`). I would add closure tests on toys and MC
(`RooFitKnowledge/12-purity-bias-toys.md` is effectively a prototype), numerical regression tests against a
reference (as the bit-for-bit reproduction of the archived purities), unit tests for the binning utilities,
and a CI job that runs them.

### 2.7 A configuration validated by a schema

A JSON schema instead of key-by-key checks in the code; the fully resolved configuration (with `inherits`
expanded) written to every output; per-bin overrides keyed by interval rather than by bin index (today the
K0S overrides `mult*_pt4`/`mult*_pt5` hit different pT ranges in the analysis and in the source binning,
`RooFitKnowledge/09-...` section 1.1); physics choices kept apart from plumbing.

### 2.8 Systematics as first-class citizens

Variations declared in the configuration and run as separate processes (RooFit does not survive threads,
`RooFitKnowledge/06-...` and `10-...`), producing the same records and manifests as the nominal run.

---

## 3. Tasks, classes and structs

**How it is today.** Every task implements `IAnalysisTask` (`Init(json, settings)` → `Run()` → `Terminate()`)
and does everything itself: reads its JSON, opens and closes files, computes and often draws.
`CorrelationTaskBase` (1312 lines) is specialised by two subclasses through virtual hooks (`GetTriggerSignal`,
`TriggerAxes`, `GetPurityHist` whose default returns `nullptr`). The config managers derive from
`BaseConfigManager` and resolve defaults and overrides on every call. `AnalysisDataStructures.h` mixes three
roles: configurations (`FitConfig`, `ExtrapConfig`), loaded data (`LoadedMC`, `LoadedPurity`) and results
(`ExtrapolationResult`). There are purely static classes (`EfficiencyCalculator`). Plotting is `CanvasManager`
(981 lines), called from inside the tasks.

### 3.1 Three layers

- **Configuration**: the JSON is read and validated once at start-up and becomes typed, immutable structs.
  Tasks never see a `rapidjson::Value`; they receive a `PurityConfig` already checked.
- **Computation**: pure functions in namespaces, input → result, no files, no global state, no plots. Purity,
  efficiency, correlation and extrapolation become testable with hand-built histograms.
- **Orchestration and I/O**: thin tasks that load inputs, call the computation and write outputs.

### 3.2 Tasks that declare their inputs and outputs

Instead of the `Init`/`Run`/`Terminate` lifecycle, each task declares what it reads and what it produces and
has a single `Execute`. This is what the pipeline of 2.2 needs, and it makes explicit the dependencies between
tasks that today live in file names and in the order of the configuration.

### 3.3 Composition instead of inheritance for the correlations

The virtual hooks of `CorrelationTaskBase` are strategies: where the trigger signal comes from, where the
purity comes from, which trigger axes apply. As small components (`TriggerSource`, `PuritySource`) passed to a
single correlation task, `CorrelationTask` and `CorrelationWPDGTask` become two configurations of one task, and
a default such as "purity = `nullptr`" becomes a visible choice instead of a method that was not overridden.
Compatible with the split of `DESIGN_NOTES.md` section 1 (containers → spectra, spectra → results): the split
cuts the task by stage, this cuts it by source; both can be done.

### 3.4 Data structures by role

Configuration types next to their parser, loaded data next to their loader, results next to their
computation, each in the namespace of its domain (`Fit::Config`, `Fit::Result`, `Purity::Input`, ...), instead
of one global header. This answers the question left open about `AnalysisDataStructures.h` (whether a common
header of structs, outside any namespace, is good practice): I would not keep it.

### 3.5 Config managers as functions that return values

`ParseFitConfig(json) → FitConfigTable`, resolving defaults and overrides once, with cells keyed by interval.
No `BaseConfigManager` hierarchy, no questions to the configuration on every cell. Purely static classes become
namespaces (already in `DESIGN_NOTES.md`).

### 3.6 Fitting separated from drawing

`Fit(hist, config) → FitRecord` (2.4) and a plotting module that draws from the record, rebuilding the model
from the stored parameters. `CanvasManager` becomes a plotting stage that reads outputs, instead of being called
from inside the tasks.

### 3.7 Explicit ownership

No raw `TFile*` (there is one in `ParticleTask`): a writer object that closes its file when it goes out of
scope. `TH1::AddDirectory(false)` set once at start-up, so that no object lands in `gDirectory` by accident —
the same mechanism behind the xRooFit bug found on 15 October (`RooFitKnowledge/11-...`: `xRooFit::minimize`
caches fits in `gDirectory`, which inside `PurityTask` is the output file).

---

## 4. Suggested order, if any of this is taken up

1. **3.1 and 3.5** (typed configuration, pure computation): they make the rest testable.
2. **2.4 / 3.6** (fit record, drawing apart): it fits the revision of `DoFit` that the RooFit research calls
   for anyway.
3. **2.3** (contract with O2Physics): small and isolated, and it closes real holes.
4. **3.3 and 2.1** (correlations by composition, results by interval): when `CorrelationTaskBase` is opened.
5. **2.2, 2.6, 2.7** (pipeline, tests, schema): they grow with the above.
