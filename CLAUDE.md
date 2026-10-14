# Working on this repository

Offline analysis chain for φ–strangeness correlations in ALICE. Read `README.md`
for what it does and how it is laid out, and `DESIGN_NOTES.md` for what is decided
but not done. This file is only about how to work in it without breaking things.

## How to run it

```bash
cd Codes
./runAnalysis.sh mc pp               # default: interpreted, through ROOT/Cling
./runAnalysis.sh mc pp --compile     # compiled: the native binary build/phistrangeanalysis
./runAnalysis.sh mc pp --interpret   # the default, spelled out (-c and -i are the short forms)
```

Both modes run the same chain (`RunAnalysisChain.h`) and differ only in the entry
point: `PhiStrangenessCorrelation.C` for Cling, `main.cpp` for the binary. Either
needs ROOT in the environment (`enterO2P`); the compiled one needs it for CMake too.

- **Interpreted** has no build step. Its drawback is the one this file used to state
  as "the only compiler this project has": a header is checked only when a run loads
  it, so one that nobody has run since it was edited can hide errors until then (see
  *Cling constraints*).
- **Compiled** is the real compiler. Use it after changing headers: it checks every
  line, not just the ones a run happens to reach. The script asks CMake whether the
  binary is stale and rebuilds only if it is, then runs it. If the ROOT in the
  environment is not the one `build/` was configured with, it recreates `build/`
  first, which CMake would not do on its own. The build output goes to
  `Logs/build.log` and the screen gets one line; if the build fails it is shown and
  the analysis is not launched. To build without running:
  `cmake -B build -S . && cmake --build build`.

**The two modes write to the same files.** Output paths come from the JSON, not from
the mode, and so does `Logs/log_output_<config>.log`. Running one after the other
overwrites the first, and `mc` writes `Corrections/`, which the data runs read back.
Copy the output directory before comparing the modes.

Process-wide ROOT state (`gROOT->SetBatch`, `gSystem->Exit`) belongs to the two entry
points, not to `RunAnalysisChain.h`, which stays free of ROOT globals. The compiled
binary is not started with `-b` the way the macro is, so `main()` sets batch itself.

## The rule the whole framework is built on

**Binnings are read from the input files, never taken from the configuration.**
What the JSON declares is compared against what the file contains, and a mismatch
stops the run with the index and both values. `AnalysisSettings::ResolvePtBinning`
and `ResolveMultBinning` do both steps in one call.

This exists because histograms are addressed by bin **index** across task
boundaries: `..._multBin3` means whatever the producer's binning said, and a
consumer with a different binning finds that object and reads the wrong interval.
Every check in the codebase is a version of closing that hole:

- `BinningUtils::RequireSameAxis` between containers that get divided
- binning stamps (`RootIO::WriteBinningStamp`) in files whose objects are
  index-addressed, verified by `RequireMatchingBinningStamp`
- `CorrelationTaskBase::VerifyTriggerAxes` between the trigger matrices and the
  associated-particle data
- real bin edges on the axes of anything that crosses a task boundary, instead of
  0..N counters — this is what makes the checks above possible at all

When you add something that crosses a task boundary, ask what happens if the two
sides disagree. If the answer is "wrong numbers, no error", that is the bug.

## Checks versus documentation

Two categories, deliberately different:

- **Checks throw.** Binning mismatches, incompatible axes, unknown enum values,
  missing required keys. They stop the run and the message says what to do about it
  (which key, which file, which production to re-run).
- **Provenance never throws.** `RootIO::WriteProvenance` records the merged config
  block, timestamp, git commit and a fingerprint of the headers. A path that moved
  is not a reason to fail a run that is otherwise fine.

Do not turn one into the other.

A third category sits between them: **diagnostics that must not stop the run but must
not be lost either**. `Logger::Warn` (`Logger.h`) prints the line and also counts it, and
`RunAnalysisChain` prints every distinct warning once, with its count, just before
the verdict. A warning that only scrolls past in a log of a million bytes is
effectively silent, which is the failure this framework is built against. The tasks
have not been moved to `Logger::` yet (see `DESIGN_NOTES.md`): until they are, the
summary only covers what goes through it. Text that is not a literal goes in as an
argument (`Logger::Warn("{}", msg)`), never as the format string.

## Configuration

`JsonConfigHelpers.h` — never read a key with `HasMember` + `operator[]`. Three
layers, and the right one depends on what you already hold:

| | |
|---|---|
| `ToNumber`, `ToArray` | you have the value; interpret it and name it if it is wrong |
| `RequireMember`, `OptionalMember` | navigate by key, no opinion on the type |
| `Require*`, `Optional*`, `Try*` | key and type together, which is what tasks use |

The third layer differs only in what absence means: `Require*` is fatal, `Optional*`
takes the fallback you pass, `Try*` returns an empty optional and lets you decide.
Arrays and objects have no `Optional*` form — their views are not values, so there
is no fallback to hand over.

**Present with the wrong type is always fatal**, in every one of them, because
"silently ignored" is how a configuration lies to you.

Two more worth knowing: `ReadNumberArray` accepts `6.4` and `"6.4"` alike, because
both spellings appear in the configurations and a value copied from one key to
another must not change meaning; and `OptionalEnum`/`ResolveEnum` build the
"Available: ..." message from the same table they match against, so it cannot go
stale.

Blocks resolve `inherits` recursively in `WorkflowManager::MergeTaskConfiguration`;
a key that looks absent in a block may well be inherited, so read a configuration
through the chain and not block by block.

**A key that changes a number is declared even when it equals the default.**
Provenance records `JsonConfig::Serialize(taskConfig)` — the merged block, so only
the keys the JSON actually contains. A run that leans on a C++ default leaves no
trace of which value was in force, and changing that default later silently
reinterprets every file already produced. `trend_composition` is spelled out in the
base configurations for exactly this reason, at its default value.

The converse holds too, and it is why the `dir_name` entries came out: a key that
changes no number is better absent, because it is one more copy of a rule to keep
aligned. The test is not "is it obvious" but "would a reader of the output need to
know it".

## Physics that the code depends on

- **Efficiency and purity are ratios.** A coarser binning is obtained by merging
  the *counts* before dividing (`EfficiencyCalculator::RebinCountsIfRequested`), or
  by fitting the *merged* mass distribution (`PurityTask::FitPuritySpectrum`).
  Never by rebinning the ratio: that sums efficiencies.
- Both ratios use `"B"` in `TH1::Divide` because the numerator counts a subset of
  the denominator.
- The normalisation uncertainty is 100% correlated across the bins of a spectrum,
  so it belongs at trend level, not per bin.

## Conventions

- Comments explain **why**, not what. Where one records a decision, it was argued
  through — read it before changing the line under it.
- Prefer deriving over storing: if a member follows from other members, a
  constructor or an accessor should produce it (`AssocParticleConfig::mass`,
  `ParticleTask::AnalysisBinning`).
- One namespace per helper header, in `PascalCase` and named after the file, without
  `Helpers` when the file has it: `BinningUtils`, `RootIO` (`RootIOHelpers.h`),
  `JsonConfig` (`JsonConfigHelpers.h`), `HistogramUtils`, `RunEnvironment`, `Logger`.
  What a namespace keeps to itself goes in a nested `Detail`, capitalised like the rest.
  Dedicated managers: `WorkflowManager`,
  `CanvasManager`, `BaseConfigManager`, `FitConfigManager`, `ExtrapConfigManager`.
- Tasks communicate through ROOT files, never in memory. `WorkflowManager` destroys
  each task right after `Terminate()`.
- **Decouple data from presentation**: Data structures (`LoadedMC`, `ParticleTask`)
  hold strictly physics inputs and binnings, never graphics or canvases. Tasks
  (`MCTask`, `PurityTask`, `CorrelationTaskBase`) own the presentation lifecycle via `CanvasManager`.
- **ROOT's global lists know nothing about `TDirectory`.** `TCanvas` registers in
  one: two canvases with the same name alive at once collide even in different
  directories, which caused a segfault, so keep canvas names unique. `CanvasManager`
  centralizes canvas lifecycle, unique naming, layout factories (ratio pads, grids),
  path-aware hierarchical IO (`WriteAll(file)`), and safe IO/export. `TF1` registers in
  `gROOT->GetListOfFunctions()` the same way — an undeleted fit model created once per
  spectrum piles up objects under one name, which was half of the `SpectrumExtrapolator`
  clone bug. The rule generalises: when a ROOT class keeps itself in a global list, a
  unique name and a clear owner are not style.

## How a task opens the file it writes

Three regimes, and the choice is about **ownership of the file**, not about taste.
Opening mode and clearing are two separate decisions:

| | when | who |
|---|---|---|
| `RECREATE` | the whole file is this task's | `PhiFitTask`, `PurityTask`, `ComparisonTask`, the per-particle maps |
| `UPDATE` + `RootIO::ClearPath` | the file is shared; the owned subtree is rewritten from scratch | `CorrelationTask`, `CorrelationWPDGTask` (`PhiAssocSpectra.root`), `MCTask` (`Corrections.root`) |
| `UPDATE`, no clearing | the file is shared; the owned subtree is defended by REFUSING | the projection caches |

`UPDATE` says "this file may hold things that are not mine" - another
`binning_name`, the other `Extract1D`/`Extract2D`. `ClearPath` says "and what IS
mine I rewrite from nothing". Without the second, a task whose output has changed
shape leaves both generations in the file and nothing says which is current; with
`RECREATE` instead of the pair, the siblings are destroyed along with the stale
objects.

The third row is not an oversight. A projection cache costs too much to rebuild for
it to be replaced silently, so ownership there is exercised by throwing when the
directory already holds projections of another binning - see
`ResolveBinningAndCache`. Clearing and refusing are opposite answers to the same
question, and which one is right depends on what it costs to lose the contents.

A new task that writes into a file somebody else also writes into belongs in row two.

## Cling constraints

- `std::format` with any non-empty format spec fails: `{:g}`, `{:.2f}`, `{:%F}`.
  Plain `{}` is fine. Hence `BinningUtils::FormatEdge` (`snprintf`) and
  `RunEnvironment::TimestampNow` (`strftime`).
- C++20 is required beyond `std::format`: `emplace_back` on aggregates uses
  parenthesised aggregate initialisation (P0960). CMake takes the standard from
  ROOT (`ROOT_CXX_STANDARD`), so the binary and the headers agree with ROOT's.
- **In the interpreted mode on macOS arm64 (ROOT 6.36), a `try`/`catch` works only by
  accident.** A `throw` that Cling's JIT code has to unwind through ends in
  `libc++abi: terminating due to uncaught exception`. This is a known upstream problem
  ([ROOT #7541](https://github.com/root-project/root/issues/7541),
  [LLVM #49036](https://github.com/llvm/llvm-project/issues/49036), reported fixed with
  LLVM 20 / ROOT 6.38); the ALICE stack's 6.36 is affected. Code compiled ahead of time is
  not: the binary from `--compile`, or a macro built with ACLiC (`.C+`).
  What was measured: the JIT emits unwind info (`__eh_frame`) for a module only if one of
  its functions needs DWARF-mode unwind info, which ordinary functions do not. A module
  with a static initialiser gets ORC's `__orc_init_func`, which does, so every function of
  that module gets an FDE and `catch` works. The project's macro has such initialisers
  (e.g. the global `std::map` of `AnalysisConstants.h`, in the same unit as everything
  included), which is the only reason `RunAnalysisChain`'s `catch` works; this was checked
  with a missing configuration. A second hazard exists even when the unwind info is
  emitted (an alignment hole that stops LLVM's registrar). Data and the full analysis, written
  up for a ROOT issue, are in `root-issue-cling-eh/`.
  Consequences: **`--compile` is the safe mode whenever the error path matters.** A small
  standalone macro written to test exception behaviour proves nothing. Removing every
  load-time initialiser from the interpreted unit could turn each `throw` into a
  `terminate`; one global with an initialiser is not a guarantee either.
- **Cling is not more lenient than the compiler, here.** Do not assume it is. The
  first compiled build found 14 errors in `CanvasManager.h`, and loading the previous
  version of that header in Cling reports the same two kinds: a nested struct with
  default member initialisers used as a default argument of a member of its
  enclosing class (`= YRangeOptions{}`, now `CanvasInfo::PadMargins` and
  `CanvasInfo::YRangeOptions`), and member overloads differing only in `static`. The
  header had simply not been run since it was edited. What the compiled build adds is
  that it checks everything, whether or not a run goes through it.

## Do not touch without asking

- `OldCodes/YieldMean.h` — ALICE-standard code kept as received, no longer in the
  chain: `SpectrumExtrapolator` replaced it after reproducing it number for number.
  It stays because it is the only reference to repeat that comparison against. Its
  four known defects have been fixed and `DESIGN_HISTORY.md` records what they
  were, because syncing this file from upstream would bring them all back. Anything
  else in it stays as received.
- `BkgMattia` in `DynamicRooFitter.h` — an alternative background model that the
  current `fitConfig.json` does not select, not dead code.
- The commented-out lines that survived the cleanup are parked switches, not
  leftovers: the CCDB object naming in `MCTask`, the debug canvas in
  `CorrelationTaskBase`, the old parameter limits in `ExtrapolationModelFactory`.

## Verify, do not assume

Where behaviour matters, it was measured and the measurement is in the comment —
see `HistogramUtils::RebinToTargetBinning`, which records what ROOT actually does
with the bins outside the target range. Keep doing that: a number in a comment
outlives an assertion in a chat.
