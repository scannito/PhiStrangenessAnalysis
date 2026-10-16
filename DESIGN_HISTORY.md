# Design history — resolved items

Moved out of `DESIGN_NOTES.md` on 13 October 2026. Everything here is done; it
is kept because the reasoning is what stops the same decision from being undone,
and several of these fixes look wrong without it. Open items stay in
`DESIGN_NOTES.md`.

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

---

## RooFit's messages are counted

**`RooFitMessages::Configure()`, called by `DynamicRooFitter` the first time one is built, drops
RooFit's messages below WARNING and counts the rest for the end-of-run summary.** Two open points
came together: `setGlobalKillBelow(WARNING)` sat in the constructor of `DynamicRooFitter`
(process-wide state set by whoever happens to build a fitter), and RooFit's warnings, which are
plentiful, never reached the summary.

*Where it lives, which took three tries.* First the two entry points, with the other
process-wide state (`gROOT->SetBatch`); dropped, because RooFit is not the whole application's:
a workflow of efficiency, correlation or comparison tasks builds no fitter and has no business
touching RooFit's message service or carrying a RooFit section in its summary. Then the `Init`
of the two tasks that fit; dropped, because it put the knowledge "a fitter needs RooFit set up"
in every task that uses one, and a third such task, a test or a benchmark would forget it. Last,
and kept: the fitter itself, which is the only code that uses RooFit. The standard practice is
that what belongs to the whole application is set at the entry points, while the state of a
subsystem with one owner is the owner's, set once on first use and idempotent (a rule written
into `CLAUDE.md`). The objection to it, a side effect hidden in a constructor, is real and small:
it reaches RooFit's message service and nothing else, `Configure` is safe to repeat (called three
times it counts the same as once, 8 700 lines either way), and its absence would cost noise, not
correctness. Checked: a program that builds fitters and never calls `Configure` gets the summary
section and no INFO lines; one that builds none gets no section; the same interpreted.

*What was there to see.* The August purity log (11.5 MB) has 35 341 RooFit `[#1] INFO` lines
(what the kill below WARNING, added on 12 October, removes) and 10 003 `WARNING` lines. A run
of the 340 K0S cells of `AnalysisResults_136_2026_PtMod_23luglio.root` with the current code
gives **8 700 warning-level lines in 10 kinds**; the commonest are "getLogVal() top-level p.d.f
evaluates to NaN" (3 661) and "p.d.f value is less than zero, trying to recover @
numerator=Chebychev2_bkg=..." (3 466): a background polynomial dipping below zero during
minimisation in some cells, and the p.d.f going NaN with it, which is what `DESIGN_NOTES.md`
calls pathological evaluation errors. 319 of the fits (of 340) hit it.

*Design.* `RooMsgService` has three streams, all writing to `std::cout`; `Configure` points
them at a stream of ours that **passes every character on to `std::cout` untouched and counts
a copy** (`TeeBuf`), so the log keeps the text whole, with its fit around it. The count goes to
`Logger::Count(source, line, times)`, a tally kept apart from the warnings and printed in a
section of its own, the five commonest kinds, because the number of lines would bury the
framework's warnings if they were listed together:

```
[SUMMARY] RooFit: 8700 line(s) in 10 kind(s) (the text is in the log above); the 5 commonest:
  (x3661) getLogVal() top-level p.d.f evaluates to NaN @ ...
```

A kind is the line with its leading blanks dropped and every number replaced by `N`. RooFit
prints an error block as a header, the same message up to a dozen times and then
"... (remaining 9 messages suppressed)"; that last line stands for nine more occurrences of the
previous message and is added to its count, which is why 8 700 is larger than the 6 810 lines
that are actually printed. The stream is never destroyed (RooMsgService keeps the pointer for
the life of the process).

*Checked.* On the 340 real fits, compiled: the text printed with the tee is the same set of
lines as without it (compared sorted; nothing lost, nothing added). **The order of the blocks
inside an error report differs between the two runs**, while two runs without the tee agree
with each other: RooFit walks its error blocks in a map keyed by the objects' addresses, and
the tee changes what is allocated. Interpreted (Cling): the same 8 700 lines in the same 10
kinds with the same five commonest. Both entry points start and print `[SUMMARY]`. The tasks'
`Init` were not run (they open output files).

*What it does not do.* It says that RooFit warned and how often, not in which cell: for that
the per-fit diagnostics in `DESIGN_NOTES.md` ("Fit quality monitoring") are the tool. And it
does not touch ROOT's own messages (`Info in <...>`, `Warning in <...>`), which go through
ROOT's error handler.
