# Phi–Strangeness Correlation — offline analysis framework

Offline analysis chain for two-particle correlations between the φ(1020) meson and
strange hadrons (K⁰ₛ, Ξ, π) in pp collisions, running on the output of the ALICE O²
task `phiStrangenessCorrelation`.

The framework takes the O² histograms (`THnSparse`/`TH3`) and turns them into
corrected per-trigger yields: it fits the φ invariant-mass peak to count triggers,
extracts the correlated signal per multiplicity and pT interval, applies efficiency,
signal-loss and purity corrections, extrapolates the unmeasured low-pT region, and
writes spectra, multiplicity trends and yield ratios.

Everything is written as ROOT/Cling headers (C++20) — no build step, no CMake. The
whole chain is driven by JSON.

## A note on how this was written

Large parts of this code were written with heavy use of large language models —
Google Gemini and Anthropic Claude Opus — used as pair programmers rather than as
code generators: proposing structure, arguing about trade-offs, writing and
rewriting implementations under review.

The physics is not theirs. The observable, the corrections, the choice of what is
a check and what is a default, and every judgement about whether a number is right
belong to the author. What the models contributed is engineering: the layering of
the helpers, the error messages, the consistency checks that make a mismatch loud
instead of silent.

This is stated because it matters to anyone reading or reusing the code. Two
consequences in particular:

- **The comments explain reasoning, not just mechanics.** Where a comment says why
  something is done a certain way, it records a decision that was argued through,
  and is worth reading before changing the line under it.
- **Review it as you would any code.** Fluency is not correctness. The checks in
  this framework exist precisely because plausible-looking code that quietly does
  the wrong thing is the failure mode that costs the most, whoever wrote it.

---

## Repository layout

| Directory | Contents |
|---|---|
| `Codes/` | The framework. Header-only, plus the entry macro and the run script. |
| `JSONConfigs/` | Analysis configurations, one file per campaign (`globalConfigMCpp.json`, …), plus the shared `globalConfigBase<system>.json`. |
| `Macros/` | Standalone diagnostic, conversion and plotting macros, independent of the chain. |
| `HYConfigs/` | HEPData / Yield configuration inputs. |
| `DataFile/` | Input `AnalysisResults*.root` from O² and the produced outputs. |
| `Logs/` | One log per run, named after the configuration. |
| `OldCodes/`, `OldDataFile/` | Previous monolithic macros, kept for reference. |
| `DESIGN_NOTES.md` | Open design questions and known issues. |
| `CLAUDE.md` | How to work in the repository without breaking things — the invariants, not the features. |

---

## Running

```bash
cd Codes
./runAnalysis.sh <keyword> <system>
```

`<keyword>` selects the configuration file — `data`, `purity`, `mc`, `mcclosure`,
`mcclosurewpdg`, `mcclosuregen`, or a path to a `.json` — and `<system>` is appended
to the file name, so `./runAnalysis.sh mc pp` runs `JSONConfigs/globalConfigMCpp.json`.
Anything else is passed through as a file name. Output is teed to
`Logs/log_output_<config>.log`, and the script exits with ROOT's own status.

Paths inside the configurations are relative to `Codes/`, and the script `cd`s there
itself, so it can be invoked from anywhere.

---

## Anatomy of a configuration

A configuration file has two parts: a `workflow` block — the binning and the list of
tasks to run — and one block per task.

```jsonc
{
  "workflow": {
    "global_binning": { ... },      // pT and multiplicity intervals
    "active_tasks": [ ... ]
  },
  "mc_base_settings": { ... },      // a block others inherit from
  "mc_task_2024": { "inherits": "mc_base_settings", ... }
}
```

The top level is a flat namespace of task blocks, and `global_binning` is not one:
it is read once and handed to every task. Keeping it inside `workflow`, next to the
list of tasks that will receive it, is what says so — at the top level it read as a
block that some task might claim.

### `workflow.global_binning`

```jsonc
"global_binning": {
  "binning_name": "DefaultBinning",     // names the output subdirectory
  "multiplicity_binning": [0, 1, 5, 10, ...],   // optional
  "pt_binning": {
    "Phi": [0.4, 0.6, ...],
    "K0S": [0.0, 0.3, ...]
  }
}
```

`binning_name` is not decorative: every output file is organised under a directory
with that name, so several binning schemes can coexist in the same ROOT file without
overwriting each other.

**The declared binning does not define the analysis binning — it verifies it.** The
intervals actually used are read from the axes of the input histograms; what you write
here is compared against them, edge by edge, and a mismatch stops the run with the
index and the two values:

```
[FATAL] AnalysisSettings: pT binning mismatch for 'K0S' (…/h3K0SMCGen in '…root'):
  - edge 31: configuration = 6.5, input file = 6.4
```

Declaring nothing is legal — the binning is then taken from the file without checks,
and a warning says so. Values may be written as numbers or as strings (`6.4` or
`"6.4"`); both are parsed.

### `workflow.active_tasks`

An ordered list of block names to execute. Order matters, because tasks communicate
through files: `phi_fit_task` writes the trigger-yield matrices that `correlation_task`
reads.

```jsonc
"active_tasks": ["phi_fit_task_2024", "correlation_task_2024"]
```

A block name is matched against the task registry **by prefix**, so
`correlation_task_online_efficiency_2026` is served by `correlation_task`. This is what
lets the same task run several times in one workflow with different settings. The
registry is checked longest-name-first, so `correlation_wpdg_task` is not swallowed by
`correlation_task`.

Six task types are registered: `purity_task`, `mc_task`, `phi_fit_task`,
`correlation_wpdg_task`, `correlation_task`, `comparison_task`. An unknown prefix is a
fatal error that lists the ones that exist.

### `inherits`

A block may inherit from another and override individual keys:

```jsonc
"mc_base_settings": {
  "mc_base_path": "phi-strange-correlation/phiStrangenessCorrelation/",
  "output_dir": "../DataFile/pp/DeltaY/MC/",
  "mc_particles": [{"name": "Phi"}, {"name": "K0S"}]
},
"mc_task_2024": {
  "inherits": "mc_base_settings",
  "input_mc_file": "../DataFile/.../AnalysisResultsMC_2024.root",
  "output_prefix": "2024_"
}
```

Resolution is recursive and the child always wins. The task never sees `inherits`
itself — it receives the merged block. Note that a key that looks absent in a block may
well be inherited, so read a configuration through the chain, not block by block.

A second configuration file can be passed as the macro's second argument
(`runAnalysis.sh data` does this): blocks are then looked up in the main file first and
in the base file as a fallback, which is how several campaigns share one set of base
settings.

### Values that are names, not numbers

Six settings are enumerations and are written as strings. An unrecognised value is a
fatal error that lists the accepted ones — the message is built from the same table
the value is matched against, so it cannot go stale.

| Key | Task | Values | Default |
|---|---|---|---|
| `fitter_type` | `phi_fit_task` | `dynamicroofitter`, `fitphisignalandbkg` | `dynamicroofitter` |
| `projection_axis` | correlation | `delta_y` / `DeltaY`, `delta_phi` / `DeltaPhi` | `delta_y` |
| `particle_correction_mode` | `mc_task` | `efficiency_only`, `signal_loss_only`, `combined` | `efficiency_only` |
| `run_mode` | `correlation_task` | `legacy`, `optimized` | `legacy` |
| `trend_composition` | correlation | `total_extrapolated`, `measured_plus_extrapolated` | `total_extrapolated` |
| `target` | `comparison_task` | `spectra`, `trends`, `ratios` | `ratios` |

### Keys per task

Required keys stop the run when missing. Optional keys fall back to a default, but a
key that is present with the **wrong type** is always an error rather than being
silently ignored.

**`mc_task`** — builds efficiency and signal-loss maps from MC.

| Key | | Notes |
|---|---|---|
| `input_mc_file` | required | Task-level default; a particle may override it. |
| `mc_base_path` | required | Directory prefix inside the ROOT file. |
| `output_dir` | required | CCDB-ready maps go to `output_dir/<binning_name>/`. |
| `mc_particles` | required | Array of `{"name": …}`. |
| `output_prefix` | optional | Prefix for the output file names. |
| `particle_correction_mode` | optional | See enumerations above. |
| ↳ `dir_name` | optional | Per particle. Defaults to the lowercased name; override when species share a directory (Λ and anti-Λ). |
| ↳ `input_mc_file` | optional | Per particle, when species live in different productions. |
| ↳ `rebinning_pt` | optional | Per particle. Merges counts *before* dividing. |

**`purity_task`** — fits candidate-mass distributions to get purity per pT and multiplicity.

| Key | | Notes |
|---|---|---|
| `input_data_file`, `fit_config_file`, `output_dir` | required | |
| `purity_particles` | required | Array of `{name, purity_key, hist_name, output_file_suffix}`. |
| `output_prefix` | optional | |
| ↳ `rebinning_pt` | optional | Per particle. Fits the merged sample, not merged fits. |

**`phi_fit_task`** — fits the φ peak per (multiplicity, pT) cell and writes the trigger matrices.

| Key | | Notes |
|---|---|---|
| `input_data_file`, `base_path_data` | required | |
| `fit_config_file` | required | Per-cell fit model, see `JSONConfigs/fitConfig.json`. |
| `output_dir_proj` | required | |
| `fitter_type`, `output_prefix` | optional | |

**`correlation_task`** — the main chain: signal extraction, corrections, spectra, trends, ratios.

| Key | | Notes |
|---|---|---|
| `apply_mixed_events`, `apply_efficiency`, `apply_extrapolation`, `apply_purity` | required | |
| `use_integrated_efficiency`, `use_projection_cache`, `use_2d_me_normalization` | required | |
| `input_dir_proj`, `input_dir_purity`, `output_dir_final` | required | |
| `input_data_file`, `base_path_data` | required | Unless the projection cache is enabled. |
| `input_me_file`, `base_path_me` | required | Only when mixed events are on. |
| `input_efficiency_file` | required | Only when efficiency is on. |
| `extrapolation_config_file` | required | Only when extrapolation is on. |
| `associated_particles` | required | Array of `{name, dir_name}`. |
| `purity_sources` | required | Array of `{name, purity_key, file_suffix}`. |
| `delta_y_limits` | optional | Δy windows to integrate. |
| `yield_ratios` | optional | Array of `{numerator, denominator, label?}`. |
| `active_corrections` | optional | `acceptance_efficiency`, `signal_loss`. None if absent. |
| `apply_efficiency_to` | optional | Defaults to φ plus every associated particle. |
| `use_signal_cache`, `do_more_qa` | optional | |
| `run_mode`, `projection_axis` | optional | See enumerations above. |
| `trend_composition` | optional | How the extrapolated trend is built — see below. |
| `*_prefix` | optional | `purity_prefix`, `trigger_prefix`, `output_prefix`, with `input_prefix` and `input_output_prefix` as fallbacks. |

**`correlation_wpdg_task`** — MC-closure variant reading PDG-matched pairs. Same keys,
plus `is_pure_gen` (required) and `output_dir_proj`; it has no purity stage.

**`comparison_task`** — divides results produced by two separate runs of the chain.

This is the axis the chain did not have. `yield_ratios` inside a correlation task
divides two *different* quantities produced *together*; everything here divides the
*same* quantity produced *apart* — MC closure against generated, an old production
against a new one, 2024 against 2026. No verdict is computed: the task writes the
ratio and stops.

| Key | | Notes |
|---|---|---|
| `output_dir_final` | required | Writes `output_dir_final/<output_prefix>Comparisons.root`. |
| `comparisons` | required | Array, one entry per ratio. |
| ↳ `numerator`, `denominator` | required | The two input files. |
| ↳ `divide_option` | required | `""` or `"B"`. Never inferred — see below. |
| ↳ `label` | optional | Names the output directory. Defaults to `comparisonN`. |
| ↳ `objects` | optional | Explicit pairs, for a foreign denominator. See below. |
| `target` | optional | `spectra`, `trends`, `ratios`. See enumerations above. |
| `input_binning_name`, `input_dir_name` | optional | Where to look inside each input. Default to the global binning name and `Extract1D`. |
| `output_prefix` | optional | |

N-vs-M is expressed by listing entries: 1-vs-1 is one entry, 1-vs-2 is two entries
sharing a denominator, 2-vs-2 is two entries. Deliberately not a `reference` plus a
list of `targets` — that shorthand saves typing in the common case and then grows a
rule for every other one.

`divide_option` is required rather than defaulted because the task cannot tell which
case it is in. Plain division is right when the two inputs are independent samples;
for a closure, where numerator and denominator come from the *same* events and are
strongly correlated, treating their errors as independent inflates the uncertainty and
makes a closure that does not close look compatible with unity. `"B"` is the binomial
treatment used elsewhere in this framework when the numerator counts a subset of the
denominator.

Objects are normally **discovered**: the task lists what the numerator directory holds
and divides whatever the denominator holds under the same name, so nothing in it
depends on how the producer spells its bins. Both files are then verified against each
other — the multiplicity and φ-pT stamps at scheme level, the associated-pT stamp per
particle, and `RequireSameAxis` on each pair.

Listing `objects` switches that off, and it is for one situation: a denominator this
framework did not produce, whose objects have unrelated names. A foreign file carries
no stamps, so what is left is `RequireSameAxis` — and what that covers depends on what
you listed. For a trend it is the multiplicity, which is exactly what the stamps would
have checked. For a spectrum it is the associated pT, and the multiplicity interval
stays unchecked, because that one lives in the name. **In this mode you assert that the
two objects are comparable**, and nothing restricts the pairs to one kind of object.
Convert graphs first with `Macros/convertGraphHist.C`, taking the binning from the new
result, and `RequireSameAxis` has something to say again: it checks that the conversion
landed where it was meant to.

`Comparisons.root` is opened `RECREATE` — the whole file is this task's — and holds one
directory per `label`, mirroring the input sub-paths, with the provenance of *both*
inputs copied in beside the ratios.

### `trend_composition`, in detail

Only the *extrapolated* multiplicity trend is affected; the measured trend next to it
is the integral of the spectrum either way.

| Value | Each bin holds |
|---|---|
| `total_extrapolated` | the integral of the fitted model over the whole domain |
| `measured_plus_extrapolated` | the integral of the spectrum over the measured bins, plus the extrapolated low and high parts |

The difference is where the model is allowed to speak. The first lets it replace the
data everywhere, including the region where data exist; the second keeps the measured
points and uses the model only outside them. They agree exactly when the fit describes
the measured region perfectly, so the gap between them is a readout of how well it
does — which is why the alternative is worth keeping rather than choosing once.

The error is the same in both cases — the RMS of the toy ensemble, which is an error on
the *total extrapolated* yield. Under `measured_plus_extrapolated` the content is a
different estimator from the one that error was computed for, so the bar is carried over
rather than derived. It is a reasonable approximation while the two compositions agree
closely, and it is one more reason to treat the alternative as a cross-check rather than
as a result to quote.

### Cache and provenance

`use_projection_cache` reuses projections written by a previous run instead of
re-projecting the `THnSparse`. Since cached histograms are addressed by bin **index**,
each cache carries a *binning stamp* — an empty histogram whose axis is the binning it
was built with. On reuse the stamp is compared with the current run and a mismatch is
fatal, because reusing the cache would silently mix two segmentations.

---

## Map of `Codes/`

The headers are layered: nothing in a lower layer includes anything from a higher one.

### Foundation

| File | |
|---|---|
| `BinningUtils.h` | Bin edges, comparison and diff reports, mapping a target binning onto source bins. Depends only on `TAxis`, so diagnostic macros can share it. |
| `AnalysisConstants.h` | PDG masses and `GetMass`. |
| `AnalysisDataStructures.h` | The structs passed between tasks and calculators (`LoadedMC`, `LoadedAssocData`, `FitConfig`, `ExtrapolationResult`, …). |
| `AnalysisSettings.h` | Global settings and binning resolution: reads the binning from an axis and verifies it against what the JSON declares. |

### Helpers

| File | |
|---|---|
| `RootIOHelpers.h` | `namespace RootIO` — opening files, fetching objects as `unique_ptr`, navigating and clearing directories, reading and writing binning stamps and provenance. |
| `RunEnvironment.h` | `namespace RunEnvironment` — timestamp, git commit, header fingerprint. Includes no ROOT, and is the one header a plain compiler can check. |
| `JsonConfigHelpers.h` | `namespace JsonConfig` — `Require*` and `Optional*` accessors, including `OptionalEnum`, whose table doubles as the error message. |
| `AnalysisUtils.h` | `namespace AnalysisUtils` — integrals with errors, `THnSparse` projections, spectrum and trend construction, ratios, rebinning with compatibility checks, plot style. |
| `Logger.h` | Thin spdlog wrapper. |

### Fitting and extrapolation

| File | |
|---|---|
| `DynamicRooFitter.h` | RooFit engine driven entirely by JSON: builds signal and background PDFs from names, owns every `RooAbsArg` it creates. |
| `FitPhiSignalAndBkg.h` | `namespace PhiFitModels` plus the older `TF1`-based fitter (Voigtian + phase-space background). |
| `SpectrumExtrapolator.h` | Extrapolates the unmeasured low-pT part of a spectrum and propagates the uncertainty. |
| `ExtrapolationModelFactory.h` | Builds the `TF1` model (Lévy–Tsallis, …) with its initial parameters. |
| `YieldMean.h` | ALICE-standard yield/mean integration, kept as received. |

### Configuration managers

| File | |
|---|---|
| `BaseConfigManager.h` | Loads a JSON document and holds the shared parameter parsing. |
| `FitConfigManager.h` | Per-particle, per-cell fit configuration. |
| `ExtrapConfigManager.h` | Per-particle extrapolation configuration, with per-bin overrides. |

### Calculators

| File | |
|---|---|
| `EfficiencyCalculator.h` | Efficiency, signal loss and their combination, as static methods over `LoadedMC`. Merges counts before dividing when a rebinning is requested. |
| `CorrelationCalculator.h` | Signal extraction from the sparses: sideband subtraction, mixed-event normalisation (1D or 2D), projection onto Δy or Δφ. |

### Tasks

| File | |
|---|---|
| `IAnalysisTask.h` | The interface: `GetName`, `Init`, `Run`, `Terminate`. |
| `WorkflowManager.h` | Reads the configuration, resolves `inherits`, matches names to the registry, runs and destroys each task in turn. |
| `MCTask.h` | Corrections from MC. |
| `PurityTask.h` | Purity from candidate-mass fits. |
| `PhiFitTask.h` | Trigger yields from the φ peak. |
| `CorrelationTaskBase.h` | Everything shared by the two correlation tasks: corrections loading, binning resolution, spectra, trends, ratios, `Terminate`. |
| `CorrelationTask.h` | Correlations on data. |
| `CorrelationWPDGTask.h` | Correlations on PDG-matched MC, for closure. |
| `ComparisonTask.h` | Ratios between results produced by two separate runs of the chain. |

### Entry point

| File | |
|---|---|
| `PhiStrangenessCorrelation.C` | The macro ROOT is given. Builds the workflow and returns a status. |
| `runAnalysis.sh` | Maps a keyword to a configuration, runs ROOT, logs, propagates the exit code. |

---

## Macros

Standalone, run with `root -l -b -q 'Macros/Name.C(args)'`. They are not part of the
chain and no task depends on them.

| File | |
|---|---|
| `CompareAxes.C` | Axis diagnostic: reports where two inputs disagree, edge by edge. |
| `ShowProvenance.C` | What produced the files in a directory — reads back the provenance records. |
| `convertGraphHist.C` | `TGraphErrors` ↔ `TH1`, in either direction. Feeds a pre-framework result into `comparison_task`. |
| `copyTFileContent.C` | Recursive copy of a ROOT file, excluding chosen directories. |
| `TestProjections.C`, `esporta_fit.C` | Ad-hoc checks. |
| `uploadToCCDB.{C,py}`, `downloadHYOutput.py` | Moving maps in and out of CCDB / HEPData. |
| `run_systematics.py` | Skeleton of the systematics driver — generates varied configurations and runs them. **Not usable as it stands**; see `DESIGN_NOTES.md` §6. |

`convertGraphHist.C` deserves a note, because the assumption it carries is not
recoverable from the file. The graph's abscissa is generally a *different quantity* from
the histogram's axis — a mean multiplicity where the other has a percentile, or a bare
index — so which point belongs to which bin cannot be worked out from the numbers. It is
passed in as `binForPoint`, the target binning is copied from the new result, and the
table of what went where is printed on every run. A converter that guessed would be the
worst outcome available: it would place the points somewhere plausible, and the axis
check downstream would then be verifying a correspondence the converter had invented.

---

## Conventions

Histogram paths inside the O² output follow
`<base_path>/<dir_name>/h3<Name>MCGen`, `h4<Name>MCReco`, `h5Phi<Name>DataSignal` and
so on: the directory identifies the *pair* or the species, the histogram name carries
the species. That is why `dir_name` and `name` are separate keys.

Output files are organised as `<file>/<binning_name>/<section>/`, where the section is
`Fits`, `Summary`, `Extract1D` or `Extract2D` depending on what produced it. Inside a
correlation section the spectra file is laid out per species:

```
PhiAssocSpectra.root
└── DefaultBinning/
    ├── Provenance/                 config block, timestamp, git commit, header fingerprint
    ├── Extract1D/
    │   ├── K0S/
    │   │   ├── binning_ptAssoc     stamp: the associated-pT binning these objects mean
    │   │   ├── Spectra/            one corrected spectrum per multiplicity interval
    │   │   └── Trends/             yields against multiplicity, measured and extrapolated
    │   ├── Xi/ …
    │   └── Ratios/                 trends of one species over another
    ├── binning_mult                stamp: the multiplicity binning
    └── binning_ptPhi               stamp: the trigger pT binning
```

The stamps are there because objects are addressed by bin **index**: a name that matches
does not by itself mean an interval that matches, and the stamp is what turns that into
an error instead of a wrong number. They sit at the level whose binning they describe —
multiplicity and trigger pT are common to the scheme, the associated pT belongs to the
species — and `comparison_task` is the main consumer.

The correlation tasks open this file `UPDATE` and clear only the subtree they own, since
another binning scheme or the other `Extract1D`/`Extract2D` may be sitting beside it. The
rule and its two alternatives are in `CLAUDE.md`.
