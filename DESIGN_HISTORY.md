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
  like `std::max(h1->GetMaximum(), h2->GetMaximum())` across tasks.
