# Bookmark: the RooFit research, paused on 14 October 2026

> **UPDATE, 14 October 2026 (evening): the second pass (four researchers) has been run and wrapped up early for token
> budget.** Start from `00-summary.md`, which supersedes the status table below. Notes 09 (fit quality and yields) and
> 10 (model-parsing recommendation) are new; each note has its own `## Resume here` and `NOT DONE` markers. The text
> below describes the state after the FIRST pass and is kept for history.
> The complete list of what is still NOT DONE, in order of value, is section 4 of `00-summary.md`.
> A third pass on JSON/HS3 (provenance record, phi/background export, Cling, 6.38/6.40) was run afterwards by the researcher of notes 01/02/10:
> see note 02 section 10, note 10 section 8 and the paragraph "Terza passata su HS3" in `00-summary.md` 1.5. The recommendation did not change.
> A fourth pass (relaxed limits, the Xi mismatch SOLVED, purity end to end on a scratch copy, note 11) was wrapped up early for usage limit: see
> **The scratch programs and outputs were copied out of /private/tmp into `RooFitKnowledge/scratch/` (see its README.md; same directory names as the paths cited in the notes).**
> `00-summary.md` 1.7 and 4, note 09 section 9 and note 11.
> A fifth pass (15 October 2026, six researchers) closed most items; see `00-summary.md` 1.8 and 4 and the new note 12. It was stopped for the daily
> usage limit while the note-09/03 researcher was wrapping up: check 09's `## Resume here` before resuming. Note 09 was truncated once by its writer and sections 1-8 rewritten from the researcher's own text.

The research was started at about 16:35 and paused at about 17:15 at the user's request. This file says what
was asked, what exists, and how to pick it up again without starting from nothing.

## What was asked
"A full immersion in all the existing RooFit documentation, really all of it. Maybe some things are already
automated - for instance there should be native functions to parse models from JSON. I want you to know everything
about that library." The project it serves is `DynamicRooFitter` (`Codes/DynamicRooFitter.h`), which builds its
models by hand from `JSONConfigs/fitConfig.json` and fits ~1000 small binned extended 1D fits.

## How it was split: eight notes, eight researchers
One researcher per note, all started at once, each told to read the local Doxygen (the ROOT 6.36.10-alice2 source
tree, `~/alice/sw/SOURCES/ROOT/v6-36-10-alice2/v6-36-10-alice2/`) and the online documentation, and to **verify by
running** on the real cells. The pause message asked each to write what it had to its file, mark unfinished
sections `NOT DONE`, and end with a `## Resume here` section.

| File | Topic | Main questions |
|---|---|---|
| `01-model-construction.md` | `RooWorkspace::factory`, the PDF catalog | Can the hand-written `ParseModelString` / `CreatePdfComponent` be replaced by factory strings? How does `RooCrystalBall` map to the project's DSCB? Which backgrounds can go negative? |
| `02-json-hs3.md` | native JSON / HS3 | What exists in this build (`RooFitHS3`, `RooFitJSONInterface`), which type names are supported, can HS3 describe the project's models, what it cannot cover (signal window, snap to bin, purity, sideband, per-bin overrides) |
| `03-fitting-api.md` | `fitTo` options, minimizer, status | Every option with its semantics; **what status 3, 303, 304 and 1 mean**; whether the ~90% status 3 of the real cells is benign; effect of Strategy, Hesse, tolerance, `RecoverFromUndefinedRegions` |
| `04-integrals-errors-normalization.md` | `createIntegral`, `getPropagatedError` | Audit of `ExtractYieldsAndPurity`; bin integration; propagated error against toys and bootstrap |
| `05-datasets-plotting-gof.md` | `RooDataHist`, `RooPlot`, GOF | The "weighted data" warning; the correct chi2/ndf and what `chiSquare` computes; audit of `DrawFitOnPads` |
| `06-backends-performance.md` | evaluation backends, codegen, multiprocessing | What `cpu`/`legacy`/`codegen` do here; why `Parallelize` throws; where the 2.2 ms of a fit goes; thread safety |
| `07-workspace-histfactory-roostats-xroofit.md` | `RooWorkspace` and the surrounding tools | Persistence of a fit; what xRooFit offers for diagnostics; a minimal fit-audit recipe |
| `08-docs-release-notes-tutorials.md` | the documentation layer | The manual chapter by chapter; release notes 6.26 to 6.36; catalog of the 257 tutorials; each RooFit message in the project logs explained and traced to its source line; known issues |

## What was already known before the pause (not to be redone)
- `DoFit` takes 2.2 ms in steady state with the default `cpu` backend; a fitter costs 0.008 ms to build; the first
  fit of a process costs ~120 ms. `RooFit::NumCPU(4)` was removed: it is ignored with the `cpu` backend.
  `EvalBackend("legacy")` + `NumCPU(2)` took 340 ms per fit; `RooFit::Parallelize(n)` throws in this ALICE build.
- Over all 960 real cells (k0s 340, xi 260, pi_tpc 360) the fit `status` is 3 for 310, 134 and 338 of them. Nobody
  reads it today. The caveat on that number is in `DESIGN_NOTES.md`, section 3.
- RooFit's warnings are counted in the end-of-run summary (`Codes/RooFitMessages.h`).
- The data in `AnalysisResults_136_2026_PtMod_23luglio.root` are unweighted (integer contents, no Sumw2) for K0S,
  Xi, pion and phi.
- The fitter does not parse JSON models natively today; it builds them by hand.

## How to resume
1. List `RooFitKnowledge/`: a note that exists has whatever its researcher managed to write; look for the
   `NOT DONE` markers and each file's `## Resume here`.
2. For each unfinished note, start a new researcher with the brief of that topic (the table above, plus the common
   context below), and tell it to **read the existing note first and continue from its `## Resume here`**, not to
   start over.
3. When all eight exist, read them together, check them against one another (where two disagree, the experiment
   wins), and write a short synthesis for the project.

### Common context to give every researcher
- ROOT 6.36.10, the ALICE build, macOS arm64. To run it: `cd ~/alice && eval "$(alienv printenv O2Physics/latest-master-o2 2>/dev/null)"` in the same
  command, then `clang++ -std=c++20 -O2 $(root-config --cflags | sed 's/-std=[^ ]*//') x.cpp -o x $(root-config --libs) -lRooFit -lRooFitCore -Wl,-rpath,$ROOTSYS/lib`
  or `root -l -b -q macro.C`.
- Real data, read only: `DataFile/pp/DeltaY/Data/AnalysisResults_136_2026_PtMod_23luglio.root`: TH3F
  `k0s-reduced-cand-producer/k0sReducedCandidates/h3K0sCandidatesMass` (10x34x200, axes mult, pT, mass),
  `xi-reduced-cand-producer/xiReducedCandidates/h3XiCandidatesMass` (10x26x200),
  `pion-track-producer/pionTracks/h3PionTPCnSigma` (10x36x100). A cell is `ProjectionZ(name, i+1, i+1, j+1, j+1)`;
  its configuration is `FitConfigManager("JSONConfigs/fitConfig.json").GetConfig(key, i, j)` with key `k0s`,
  `xi` or `pi_tpc`.
- A harness over all cells exists in the scratchpad of the earlier benchmarks (`scratch/real/real.cpp`); it
  are now in `RooFitKnowledge/scratch/` (the paths in all notes were rewritten to point there).
- Rules for every researcher: change nothing in the project except writing its own note; never write to
  `DataFile/` or `Logs/`; tag every claim `[doc]`, `[source]`, `[verified]` or `[inferred]` and say what was run;
  file names with hyphens only (the project `.gitignore` drops names containing `_c`).

## State after the pause (all eight researchers stopped; every note exists and is PARTIAL)

Each note ends with `## Resume here` (what remains, what was about to be done, scratch files). "NOT DONE" markers
per note, and what the researcher reported as done and not done:

| Note | NOT DONE markers | Done (as reported) | Not done (as reported) |
|---|---|---|---|
| `01-model-construction.md` | 10 | `RooWorkspace::factory` strings built from the same `FitConfig` reproduce `DynamicRooFitter` bit for bit (minNll, status, edm, values, errors) on DSCB+Cheb1, DSCB+Cheb2, Voigtian+Cheb2, a sum, a product, Voigtian+BkgSourav2/BkgMattia (EXPR::, synthetic phi); catalog of ~28 classes; Chebychev2 inside [-1,1]^2 is negative for 29% of the box (explains the "p.d.f value is less than zero" messages); factory quirks (`SUM::` without coefficients aborts, a failed `factory()` can return non-null, `$Typedef`/`$Match` do not exist) | PROD/EDIT/FCONV/$Alias/CEXPR tests (`grammar.cpp` has a compile error), custom class, ranges/refCoefNorm/Extended, HistPdf/KeysPdf, route-B yields |
| `02-json-hs3.md` | 11 | Interface is `RooJSONFactoryWSTool` (RooFitHS3, nlohmann backend only, "experimental", HS3 version 0.2); DSCB, Gaussian, Exponential, RooGenericPdf round-trip; RooChebychev and RooVoigtian are missing (export writes an empty node with a warning, import throws) but ~10 lines registering them give identical NLL; 16 defective JSONs tested, several import silently (clamped values, missing parameter becomes 0, no domain gives an infinite range, `extended` ignored); n-sigma window, snap_to_bin, purity, sideband, per-bin overrides are not representable | Cling/PyROOT run, a C++ Importer/Exporter class (sketch only), YML export, real phi data, phi Voigtian+BkgSourav test, real n-sigma histogram, several HS3 chapters and the preprint, timing |
| `03-fitting-api.md` | 3 | Option catalog (Tolerance, Improve, NormRange are not `fitTo` options; BatchMode does not compile; Parallelize throws; GSLMultiMin segfaults); status = Minuit2 fStatus after MIGRAD, a successful HESSE leaves it unchanged; **3 = MIGRAD ended with EDM above 0.01**; a failed HESSE adds 100 x its code (302, 303, 304); stored edm() is the post-HESSE value; log messages mapped to file:line; small tests: k0s (0,0) status 3 is a machine-accuracy limit at NLL -3.7e7, k0s (3,18) is genuinely bad | **The experiments over the real cells** (run stopped, partial TSVs never analysed): no statistics, no verdict on whether status 3 is benign in general, no number-backed recommendations; `Optimize(1)` as a numeric no-op is inferred, not compared over all cells |
| `04-integrals-errors-normalization.md` | 10 | Source read (createIntegral, getPropagatedError, RooAddPdf, RooExtendPdf, IntegrateBins, numeric integrator); audit of `ExtractYieldsAndPurity`; on 30 K0S cells the propagated error matches covariance toys within ~1% when covQual==3 and statistics are high, erratic in 3 cells with covQual 2 and n1/n2 at a limit, 7-22% below the robust toy spread in 4 low-statistics cells; analytic integrals exact (1e-12), forced numeric ~1e-6; the observable keeps 100 bins for a 200-bin histogram so `IntegrateBins` needs `obs->setBins(200)` (verified by an NLL mismatch) | Analysis of the E2 bootstrap output (files exist), summary of E3, snap-to-bin and window-beyond-range tests, RooExtendPdf direct-yield fit, phi-like numeric test, synthetic IntegrateBins demonstration |
| `05-datasets-plotting-gof.md` | 4 | `Import(TH1)` moves the observable's range outward to bin edges and ignores under/overflow; `isNonPoissonWeighted` tests only non-integer content; for ordinary pdfs the NLL equals the extended unbinned likelihood at bin centres (matched to 0); `IntegrateBins` uses the observable's bin count, not the histogram's (agrees with note 04); the "weighted data" warning comes from the OnlineEfficiency phi histograms (non-integer in all 70 cells), offline `h3PhiData` is integer despite stored Sumw2; RooPlot normalisation, pull and chiSquare definitions (ndf excludes empty bins); neither task draws pulls or a chi2, and total and component curves use different normalisation conventions | The 20-real-cell chi2 comparison, the PNG, the low-count toy, RooHistPdf/binnedClone, VisualizeError, paramOn (the harness failed on a missing `RooChi2Var.h` include); no fit run on an online cell |
| `06-backends-performance.md` | 6 | `Optimize` is a no-op on the cpu backend; `roofit_multiprocess` OFF by default, alidist does not set it, ZMQ/MultiProcess libraries absent; `codegen_no_grad` fails on the DSCB model (RooCrystalBall has no codegen translation or analytical integral); ~98% of a fit is MIGRAD (~1000 NLL calls of ~5 us); measured means k0s 5.6-7 ms, xi ~1.9 ms; nothing worth optimising; nearly all real k0s fits end with status 3, one cell had EDM ~2e10 | `codegen` with gradient and whether Clad loads, the thread-safety test, which node triggers the numeric-integrator message, the pi_tpc profile (partial), bin-centre vs integrated-bin numeric check |
| `07-workspace-histfactory-roostats-xroofit.md` | 8 | Workspace round trip on cell (3,10): 18 KB, reload 0.27 s, cold refit bit-identical, S/B/P reproduced exactly, re-windowing ~0.05 ms; 30-cell audit: status 3 in 29/30 (useless as a flag); **the restart test found a lower NLL in 20/30 cells**; **xRooFit `minimize()` reached status 0 in 28/30 against 1/30 for the project's `fitTo`**, plain `Offset(true)` 10/30; HistFactory and RooStats not relevant | Cross-ROOT-version load (6.36.04), HS3 JSON test, profile/RooMCStudy, tutorials and web research, comparison on the real merged analysis bins |
| `08-docs-release-notes-tutorials.md` | 4 | The manual is a single page; the v2.91 PDF was read (outdated on NumCPU, Optimize, Minos default, Minuit); Doxygen fitTo/createNLL text compared with the code; release notes 6.26-6.40 (local plus official, incl. patch-release bug lists); catalog of all 257 tutorials by topic, 12 run with rc=0; messages traced to file:line; the negative-Chebychev "recover" and NaN messages are one cascade; the "(remaining N suppressed)" count is off by two (RooAbsReal.cxx:3562-3573); RooBatchCompute is "generic CPU, no vectorizations" | Developer docs (section 5), GitHub/forum searches (known issues rest on release notes and tutorials only), several [inferred] points to check (e.g. Optimize with the cpu backend), the 6.38/6.40 notes came through a paraphrasing fetch and need verifying |

### What the notes already say across topics (to be checked, not yet reconciled)
- Status 3 is not one thing: a precision limit on very-high-statistics cells (03), a genuinely non-converged fit
  on others (03, 06), and present in ~29/30 cells of one audit (07), so it cannot be used as a flag as it stands.
- The project's `fitTo` stops before the minimum more often than a restart or xRooFit's `minimize()` do (07). How
  much the signal yield, its error and the purity change is **not measured anywhere yet**; that is the missing
  link (03's real-cell experiments and 07's merged-bin comparison).
- `IntegrateBins` and the observable's bin count (04, 05 agree).
- Two ways to replace the hand-written model parsing: factory strings (01, bit-identical) and HS3 JSON (02,
  experimental, silent on defects, missing Chebychev and Voigtian until registered).

### Practical warning for anyone resuming
Concurrent `alienv printenv` calls collide (empty output, `rm`/`cp` errors, `root` not found). Six researchers
hit it. Run the researchers one at a time, or capture the environment once into a file and `source` it; several
notes' `Resume here` sections give a working manual environment.
