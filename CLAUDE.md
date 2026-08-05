# Working on this repository

Offline analysis chain for φ–strangeness correlations in ALICE. Read `README.md`
for what it does and how it is laid out, and `DESIGN_NOTES.md` for what is decided
but not done. This file is only about how to work in it without breaking things.

## How to run it

```bash
cd Codes
./runAnalysis.sh mc pp        # or: purity, data, mcclosure, mcclosurewpdg, mcclosuregen
```

There is no build step: everything is ROOT/Cling headers, so a syntax error only
appears when the macro runs. **Run the chain after changing headers** — that is the
only compiler this project has. `Logs/log_output_<config>.log` keeps the output and
the script propagates ROOT's exit code.

The one exception is `RunEnvironment.h`, which includes no ROOT and can be checked
with a plain compiler:

```bash
g++ -std=c++20 -Wall -Wextra -I Codes -fsyntax-only <file including it>
```

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
  `ParticleTask::AnalysisBinning`, `LoadedMC::CreateCanvases`).
- One namespace per helper header: `BinningUtils`, `RootIO`, `JsonConfig`,
  `AnalysisUtils`, `RunEnvironment`, `PhiFitModels`.
- Tasks communicate through ROOT files, never in memory. `WorkflowManager` destroys
  each task right after `Terminate()`.
- `TCanvas` registers in ROOT's global list, which knows nothing about
  `TDirectory`: two canvases with the same name alive at once collide even in
  different directories. This caused a segfault; keep canvas names unique.

## Cling constraints

- `std::format` with any non-empty format spec fails: `{:g}`, `{:.2f}`, `{:%F}`.
  Plain `{}` is fine. Hence `BinningUtils::FormatEdge` (`snprintf`) and
  `RunEnvironment::TimestampNow` (`strftime`).
- C++20 is required beyond `std::format`: `emplace_back` on aggregates uses
  parenthesised aggregate initialisation (P0960).
- The filesystem on macOS is case-insensitive and hides include-name mismatches
  that would break on Linux. `MCTask.h` was already renamed once by accident.

## Do not touch without asking

- `YieldMean.h` — ALICE-standard code kept as received. Its four known defects have
  been fixed and `DESIGN_NOTES.md` records what they were, because syncing this file
  from upstream would bring them all back. Anything else in it stays as received:
  changing it changes published-style numbers.
- `BkgMattia` / `VoigtBkgMattia` in `FitPhiSignalAndBkg.h` — an alternative
  background model, not dead code.
- The commented-out lines that survived the cleanup are parked switches, not
  leftovers: the CCDB object naming in `MCTask`, the debug canvas in
  `CorrelationTaskBase`, the old parameter limits in `ExtrapolationModelFactory`.

## Verify, do not assume

Where behaviour matters, it was measured and the measurement is in the comment —
see `AnalysisUtils::RebinToTargetBinning`, which records what ROOT actually does
with the bins outside the target range. Keep doing that: a number in a comment
outlives an assertion in a chat.
