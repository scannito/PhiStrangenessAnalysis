# Plots: what was built, and how to use it

This document explains, in detail, the plotting layer added on the branch `plots` between 3 and 4 November 2026
(phases A to G of the plan "plots from deterministic components"), and how to use every part of it. The decision
behind it, and the history of the earlier attempt (`phiplot`), are in `DESIGN_HISTORY.md` (*Plots: deterministic
components, composed*; *phiplot*), the rule that keeps it in `CLAUDE.md` (*Plots*), what is left open in
`DESIGN_NOTES.md` section 15.

Contents:

1. [Why](#1-why)
2. [The pieces at a glance](#2-the-pieces-at-a-glance)
3. [What each phase did](#3-what-each-phase-did)
4. [PlotSeries: finding and combining series](#4-plotseries-finding-and-combining-series)
5. [PlotDesigns: the six layouts and the look](#5-plotdesigns-the-six-layouts-and-the-look)
6. [PlotSummaries: the tasks' summaries, drawn from the files](#6-plotsummaries-the-tasks-summaries-drawn-from-the-files)
7. [What the files record about themselves](#7-what-the-files-record-about-themselves)
8. [PlotSpec: a plot as data](#8-plotspec-a-plot-as-data)
9. [The command `phistrangeplot`](#9-the-command-phistrangeplot)
10. [C++ compositions and `runplot.sh`](#10-c-compositions-and-runplotsh)
11. [Plots asked in words: `plot_ask.py` and a local model](#11-plots-asked-in-words-plot_askpy-and-a-local-model)
12. [The Claude Code skill](#12-the-claude-code-skill)
13. [How it was verified, and how to verify it again](#13-how-it-was-verified-and-how-to-verify-it-again)
14. [When something stops](#14-when-something-stops)
15. [What is not done](#15-what-is-not-done)

---

## 1. Why

Before this work, a plot asked for in words was a new ROOT macro written by Claude each time: 45 of them in
`~/alice/claude-sandbox/plots`, 100-150 lines each. None read a binning stamp, used `OutputNames` or checked an axis
before dividing; a missing object was skipped and a non-finite bin was drawn as 0. Several drove decisions without a
review. The summaries the tasks drew were another path again: canvases created empty in `Init`, found again by name
while computing, filled one class at a time, written at the end.

The rule now: **deterministic components decide what is read, what may be compared and how anything is computed;
whoever asks for a plot (a person, Claude or a local model) only composes them and chooses the cosmetics** (colours,
markers, ranges, legend places, text). A request that needs something the components do not do (a new kind of series,
an operation, a layout) adds it to them first, with its check, review and verification.

## 2. The pieces at a glance

| Piece | File | What it is |
|---|---|---|
| Names | `Codes/OutputNames.h` | every object, canvas, directory and record name a plot looks up (phase A) |
| Series | `Codes/PlotSeries.h` | a histogram found by meaning, with its origins and facts; the comparability rule; the closed list of operations; drawing a set of series; saving a plot (phases B, C) |
| Layouts | `Codes/PlotDesigns.h` | the six recurring layouts, the looks taken from facts, legends of the look (phases D, G4) |
| Summaries | `Codes/PlotSummaries.h` | what the tasks draw at the end of `Terminate`, redrawn from any file of their kind (phases E, F) |
| Records | `RootIO::WriteTable`, `OutputNames::k*` | what a file holds, written by the task that made it (phases C, F) |
| Specification | `Codes/PlotSpec.h` | a plot as JSON, one schema that both validates and is given to a model (phase G) |
| Command | `Codes/phistrangeplot.cpp`, target `phistrangeplot` | `summaries`, `list`, `schema`, `describe`, `render` |
| Compositions | `Macros/runplot.sh` | compiles and runs a plot written in C++ on the components |
| Front end | `Macros/plot_ask.py`, `PlotSpecs/` | a local model (Ollama or llama.cpp) writes the specification |
| Test of the model | `Macros/plot_ask_test.py` | requests with what each must give |
| Skill | `.claude/skills/plots/SKILL.md` | the steps Claude Code follows for a plot |

Layering: `PlotSeries` uses `OutputNames`, `RootIO`, `BinningUtils`, `HistogramUtils`, `CanvasManager`;
`PlotDesigns` uses `PlotSeries`; `PlotSummaries` uses `PlotSeries` (and is the one plot header the tasks include);
`PlotSpec` uses `PlotDesigns` and `PlotSeries`; `phistrangeplot` uses all of them. Nothing in the chain's calculation
depends on any of them: they only read files.

## 3. What each phase did

Commits on the branch `plots` (oldest first). Every phase was reviewed by the two review agents, and every finding was
fixed or answered (the "Review of ..." commits).

**Preparations** (`6f6d1f0`, `d80bf63`, `902126f`). `MCTask` writes the signal loss over the event loss as data
(`SigLoss/OverEvLoss`, `EfficiencyCalculator::EventLossOfClass` and `DivideByEventLoss`): it existed only inside a
canvas. Style left the written objects: a histogram that is written carries no colour or marker, what is drawn is a
styled copy (`CanvasManager::DrawStyledCopy`, a rule now in `CLAUDE.md`). The name builders take the binning (analysis
or source).

**A, every name from `OutputNames`** (`a6bde6e`, `172c4f9`). The names and paths of the fit canvases
(`cFit_Phi_<cell>`, `cFit_<key>_<cell>`), of the correction and purity summaries, of the trend and ratio canvases,
and the correction maps as one table (`OutputNames::Detail::NamesOf`). The strings did not change; the chain gave
identical data and identical canvases.

**B, one comparability rule** (`0a323fe`). `RootIO::RequireSameOptionalStamp` moved out of `ComparisonTask`, so the
plots and the comparison task compared two files' stamps by the same function. On 7 November 2026 the comparison task
was retired, its role being the plots' (`DESIGN_HISTORY.md`, *comparison_task retired*), and the function with it: the
plots call `RootIO::CompareBinningStamps`.

**C, the series** (`44b93a2`, `a0b2cf7`, `3f33c03`, `c4549d1`). The palettes moved to `CanvasManager`
(`ClassColor`, `VariantColor`; colours generated past the listed ones instead of repeating every ten). The correlation
task records the |dy| limits of each section (`deltaY_limits`). `PlotSeries.h`: finders, facts, origins, the
comparison, the operations, `Draw`, `Describe`, `AutoLabels`, `Save`, the catalog. `CanvasManager::CreateJoinedPads`.

**D, the layouts** (`2499336`, `a5e639e`). `PlotDesigns.h` with `Overlay`, `Grid`, `CompareWithRatio`, `JoinedPads`,
`WithReference`, `FitCanvasGrid`, `Table`; `Run` (exit codes); `Macros/runplot.sh`; the `CLAUDE.md` rule. Validated
on three real compositions: the values drawn equal what the old macros read, bin by bin (18/18 and 100/100 series),
and the owner's K0S/π plot of 26 October was reproduced in its look.

**E, the summaries drawn from the files** (`9b99c4c`, `58bc0ad`, `b3dfb94`, `ea306af`, `c8a80f9`). `MCTask`,
`PurityTask` and `CorrelationTaskBase` no longer create canvases in `Init` and fill them by name: at the end of
`Terminate` they close their file, draw the summaries from it with `PlotSummaries`, and write them into it
(`PlotSummaries::WriteInto`). Identical canvases (content, style, pads, statistics box), except two intended changes:
non-finite bins left empty (the pi_tof purities) and the species-ratio Y range covering every |dy| (it was the first
pair's, clipping the others).

**F, the summaries without a task** (`5236ad1`, `743ce63`). The files record what they hold (particles, purity keys,
species ratios: section 7), so the summaries need nothing from the task; `phistrangeplot summaries <file>` redraws
them from any such file.

**G, plots as data and a local model** (`9e2a7f6` to `0c5f32e`). G0: the components refuse what a specification could
get wrong, for every caller (one quantity on X, ambiguous directories, |dy| with more than two decimals). G1: the
specification and `schema`/`describe`/`render`. G2: `list`, the catalog of every kind of file. G3: the front end with
Ollama and llama.cpp, measured. G4: looks by fact, sets of series, every layout in the specification. G5: an example drawn on
other files (`--file`), the skill, the documentation.

## 4. PlotSeries: finding and combining series

### A series

```cpp
struct Series {
  std::shared_ptr<TH1> hist;   // detached, as read: a non-finite bin stays non-finite
  std::vector<Origin> origins; // where it was read (file, path, scheme and species directories); both operands' after a ratio
  std::string label;           // the legend's text when given; AutoLabels' otherwise
  int nonFiniteBins;           // counted when read, warned about
  std::vector<std::string> notes; // the operations done to it
  Facts facts;                 // kind, species, dy, mult, binning, in words
  Look look;                   // colour, marker, marker size, when chosen
  std::optional<OnX> x;        // pT or multiplicity: what its X axis is
};
```

### The finders

Each builds the object's name forward with `OutputNames`, locates the scheme and section (`Where{scheme, section}`, an
empty one meaning "the only one in the file"), checks what can be checked, and reads the histogram detached.

| Finder | Reads | Its facts |
|---|---|---|
| `Spectrum(file, species, dy, mult, where)` | the pT spectrum of one class | spectrum, species, \|dy\|, class |
| `SpectrumExtended(file, species, dy, mult, where)` | the same extended to pT = 0, its fitted curve attached | |
| `Trend(file, species, dy, kind, where)` | the yield against multiplicity: `Measured`, `Extrapolated`, `ExtrapOverMeas` | |
| `SpeciesRatio(file, num, den, dy, kind, where)` | the ratio of two species' yields, as the task wrote it | |
| `Purity(file, key, mult, binning, scheme)` | the purity of a purity key in one class, or in the union of the classes (its interval; a file with no sections: a scheme, no `Where`) | |
| `CorrectionMap(file, particle, map, mult, binning, scheme)` | efficiency, signal loss or signal loss over event loss, in one class or in the union of the classes (its interval) | |
| `Fit(file, purityKey, mult, pt, binning, scheme)` | a mass-fit canvas, the phi's without a purity key (its cell checked against the stamps); not a series: a `FitCanvas{canvas, origin, tag}` | |
| `Object(file, path)` | anything in a file the framework did not write (a preliminary); no stamp, no facts | |

What they refuse: an object that is not there (`NotInFiles`, exit code 3), an object of another class, a multiplicity
interval that is not one of the file's (`MultIndex` by the stamp's edges), a |dy| the section does not record, a |dy|
with more than two decimals (`OutputNames::DyTag`: the names hold two), and an ambiguous scheme, section or species
(more than one and none named: exit code 1, listing them, because naming one fixes it).

### What a file holds

`Intervals(file, scheme)`, `DeltaYLimits(file, where)`, `SpeciesIn(file, where)`, `Sections(file, scheme)`,
`CorrectionParticles(file, scheme)`, `PurityKeys(file, scheme)`, `SpeciesRatios(file, where)`,
`SpeciesRatioLabel(...)` (the label of a ratio is the Y title of the ratio), `KindOf(file, scheme)` (corrections,
purity or spectra, by the record the scheme holds), `Resolve` / `SchemeName` (the empty names made explicit), and
`ReadCatalog(file)`, all of these together.

### Comparing and combining

- `Compare(a, b)`: the stamps of the directories the two were read from (mult and ptPhi of the scheme, ptAssoc of each
  species they share, `RootIO::CompareBinningStamps`) and their X axes. Two series of one directory share its stamp.
  It only says: what differs, and what could not be checked.
- `RequireOneQuantityOnX(series)`: a spectrum (pT) and a trend (multiplicity) on one plot, or in one ratio, stop.
- `RequireCombinable(a, b)`: the X quantity, then `Compare`: a difference stops, a stamp not checked is warned.
- The operations, a closed list: `Ratio(a, b, divideOption)` (the option stated, never guessed: `""` for independent
  samples, `"B"` when the numerator is a subset of the denominator), `Sum(a, b)`, `Scale(a, factor)`, `Relative(a)`
  (each bin 1, its error the relative one: a systematic band on a ratio pad; a bin of content 0 becomes non-finite).
  A ratio keeps both origins and its notes say what was divided by what; it is labelled "numerator / denominator", a
  relative series "<label>, relative", when the operands have labels (what `Table` and `Describe` name them by).

### Drawing and keeping

- `Draw(series, option)`: each series as a styled copy, the first without SAME; what they disagree on warned once;
  a non-finite bin left empty in the copy (the series keeps it), and refused with an option that would draw it at 0
  (allowed: `E`, `E1`, `P`, and no option on a histogram with errors; measured); no statistics box; a curve attached
  to a series (the fit of an extended spectrum) drawn in the series' colour.
- `Describe(series, inputs)`: each input that is not a series (a fit canvas), each series, where it comes from, what
  was done to it, what the series disagree on. `PlotDesigns::Describe(drawn)` and `PlotDesigns::Save(drawn, ...)` pass
  a design's own.
- `AutoLabels(series)`: the legend from the facts: what the series share as a header, what tells each apart as its
  label, file names cut to the words that differ.
- `Save(canvas, outBase, series, inputs, recipe, facts)`: `outBase.pdf`, `.png` and `.root` with the canvas, a copy of
  each series as computed (`Objects/series<i>`) and the provenance: the recipe, when, the commit and the header
  fingerprint of the code that drew it, whatever directory the plot is made from, the input files' provenance per
  origin of each series and per input that is not a series (a fit canvas), and any facts given (a note). `OpenPlotFile`
  refuses to replace a file the plots did not write, or one open as an input (by the file, however its path is spelled).

## 5. PlotDesigns: the six layouts, `Table` and the look

### The parameters

- `Frame`: width, height, font (pixel fonts, 43), text size in pixels, draw option, the legend's corners
  (`Corners{x1, y1, x2, y2, columns}`), and the ALICE label (status and collision system together, or none: the
  system is not in the files).
- `Panel`: the series; title, X and Y titles; Y range or the rule for the automatic one (`zeroBaseline`, `headroom`);
  log Y; the automatic legend on or off, and its header (`legendHeader`, in place of what the series share);
  `keys` (legends of the look); `notes` (lines under the title); a `band` (a systematic uncertainty drawn behind the
  series, with an entry of its own in the legend when it has a label).
- `Key{fact, shows, where, text}`: a legend that says what the colour (`Shows::Color`) or the filling
  (`Shows::Fill`) means, by one fact. Its entries are derived from the series as drawn: a value drawn in two colours,
  or a colour standing for two values, stops the plot instead of mislabelling it.

### The layouts

The six layouts return `Drawn{canvas, series, inputs}`: every series drawn included (the ratios too), for `Save` and
`Table`, and where what is not a series was read (the fit canvases), so that `Save` records it too. `Table` returns the
text.

| Design | What it draws |
|---|---|
| `Overlay(canvases, name, panel, frame)` | every series on one pad |
| `Grid(canvases, name, panels, nCols, frame)` | panels in a grid, row by row |
| `CompareWithRatio(canvases, name, panel, reference, ratioPad, frame, ratioBand)` | the series on top, the ratio of each to the reference below, each drawn as its numerator is; `RatioPad{divideOption, range, title, grid}` says how the pad divides and looks; a band behind the ratios on request |
| `JoinedPads(canvases, name, panels, yRange, frame)` | panels side by side, joined, one Y axis and one range (given, or the union of the pads' own) |
| `WithReference(canvases, name, panel, reference, systematic, ratioPad, frame)` | the series against a foreign reference (listed and drawn after them), its systematic as a band (checked against the reference), the ratios below with the systematic again, relative, around 1 |
| `FitCanvasGrid(canvases, name, file, purityKey, cells, nCols, binning, scheme, frame)` | the mass-fit canvases of some cells (`FitCell{mult, pt, label}`), already in the file, each found by `PlotSeries::Fit` |
| `Table(series)` | the values bin by bin, the series on one X axis |

`Run(body)` wraps a composition's `main`: batch mode, and exit code 0 done, 1 refused or failed, 3 not in the files.

### Looks from facts

`ColourBy`, `ShapeBy` and `FillBy` give the series that share the value of a fact the same colour, shape or filling.
Given the panels of a design, they number the values over all of them, so one value has one look in every pad and a
key drawn in one pad holds for the others. They do not depend on the order they are called in (the shape keeps the
filling and the other way round), refuse a look given by hand (a colour; a marker outside the shapes, `PlotSeries::kShapes`,
whose open or filled form a shape keeps), refuse more values than they can say (a palette with fewer colours than
values, more than seven shapes, more than two fillings), and draw diamonds larger (2.0 against 1.4, the owner's ratio
plots). A specification follows the same rule: it is theirs, not repeated by the reader. `FactValues(series, fact)` is the one rule for "the values of a fact, in the order the series
bring them", used by the looks, by the keys and by a specification's key labels.

## 6. PlotSummaries: the tasks' summaries, drawn from the files

The canvases each task writes into its own file, drawn by library functions from that file:

| Function | Task | Canvases |
|---|---|---|
| `Corrections(canvases, file, scheme)` | `MCTask` | per particle: efficiency, signal loss, signal loss over event loss, at the analysis and (with `rebinning_pt`) the source binning; the classes, then their union in black |
| `Purities(canvases, file, scheme)` | `PurityTask` | per purity key and binning: the classes, then their union in black |
| `Correlations(canvases, file, scheme, section)` | the correlation tasks | per species: spectra per |dy| (log scale), extended spectra with their curves, measured and extrapolated trends and their ratio; the species ratios with their legend |

What must be in the file is said per family: a family the task always writes is required, so a wrong name stops; one
written only when asked (the source binning, the extrapolation) is looked up once and then required whole; the signal
loss over event loss alone may lack a class. The look is the one the canvases always had (ROOT's default option, the
Y range applied when the manager writes). Each task calls, at the end of `Terminate`, after closing its file:

```cpp
PlotSummaries::WriteInto(fileName, GetName(), {globalCfgs.binningName});   // {scheme, section} for a correlation task
```

`OfFile(canvases, file, where)` draws whichever summaries a file has (by `KindOf`); `Save` keeps them for a reader.

## 7. What the files record about themselves

So that a file can be read with no configuration (by the summaries and by `list`), the tasks write what it holds, as
small tables (`RootIO::WriteTable`: a detached `TTree`, one row per entry and one typed branch per column, which
`CompareRootFiles` compares; an empty table means "none", an absent one "written before the record existed"):

| Record | Where | Written by | Columns |
|---|---|---|---|
| `particles` | `{scheme}` of the corrections file | `MCTask` | particle |
| `purity_keys` | `{scheme}` of the purity file | `PurityTask` | purity_key |
| `deltaY_limits` | `{scheme}/{section}` of a spectra file | the correlation tasks | dy (a number) |
| `species_ratios` | `{scheme}/{section}` of a spectra file | the correlation tasks | numerator, denominator |

Each table is one value in `OutputNames`, its descriptor carrying the types of its columns (`Table<std::string>`,
`Table<double>`, `Table<std::string, std::string>`; a row is the value itself for one column) with its name, its columns
and the task to re-run when it is missing; `RootIO::WriteTable` and `ReadTable` take the descriptor. The phi fit file has no table: it is told
by its `Fits` directory, and its binnings are stamped (`binning_mult`, `binning_ptPhi`), which gives `list` the cells of
its fits. A file produced before these records
is refused by `list` and `summaries` with exit code 3 and the task to re-run; nothing falls back to parsing names.

## 8. PlotSpec: a plot as data

A specification is a JSON object: one design and what it draws, in the same vocabulary as a C++ composition. The
schema (`phistrangeplot schema`) is the one description of what it may say; every specification is validated against
it before it is read, and the same text is given to a model.

### The designs

```json
{"design": "overlay", "series": [...], ...panel keys..., ...looks..., ...frame keys...}
{"design": "compare_with_ratio", "series": [two or more], "divide_option": "", "reference": 0, "ratio_range": [lo, hi], "ratio_title": "...", "ratio_grid": true}
{"design": "with_reference", "series": [...], "reference_object": {"file": "...", "path": "..."}, "systematic": {...}, "divide_option": "", "ratio_title": "...", "ratio_grid": true}
{"design": "grid", "n_cols": 3, "panels": [{"series": [...], ...panel keys...}, ...]}
{"design": "joined_pads", "panels": [two or more], "y_range": [lo, hi]}
{"design": "fit_canvas_grid", "file": "...", "purity_key": "k0s", "n_cols": 3, "cells": [{"mult": [15, 20], "pt": [0.8, 1.2]}]}
```

Panel keys: `title`, `x_title`, `y_title`, `y_range` (not in a joined panel), `log_y`, `zero_baseline`, `headroom`,
`legend`, `legend_header`, `keys`, `notes`. Looks (at the top of every design but the fit grid, over all its panels): `color_by`,
`shape_by`, `fill_by`, `colors`. Frame: `width`, `height`, `text_px`, `draw_option`, `legend_box`, `legend_columns`.

### The series

```json
{"of": "spectrum", "file": "...", "species": "K0S", "dy": 1.0, "mult": [0, 1], "extended": false}
{"of": "trend", "file": "...", "species": "K0S", "dy": "all", "trend": "measured"}
{"of": "species_ratio", "file": "...", "numerator": "K0S", "denominator": "Pi", "dy": [1.0, 0.5], "trend": "extrapolated"}
{"of": "purity", "file": "...", "purity_key": "omega", "mult": "all", "binning": "analysis"}
{"of": "correction_map", "file": "...", "particle": "K0S", "map": "efficiency", "mult": [0, 100]}
```

The union of the classes, which purity and corrections files hold, is asked for by its interval (`mult_union` in
the catalog), as the files name it; `"all"` is the classes.

Every series may also take `scheme`, `section` (spectra files), `label`, `color`, `marker`, `marker_size`. `dy` and
`mult` take one value, a list, or `"all"` (every value the file records): one entry is then a set of series, classes
outer and |dy| inner, and a label given to a set is followed by what tells each series apart.

### Looks and keys in a specification

```json
"color_by": "dy", "colors": [633, 601, 418], "shape_by": "dy", "fill_by": "trend",
"keys": [{"fact": "dy", "shows": "color", "box": [0.03, 0.79, 0.97, 0.86], "columns": 3, "labels": ["|#Deltay| < 1", "|#Deltay| < 0.5", "|#Deltay| < 0.1"]}]
```

A fact is named as `PlotSeries` names it (`kind`, `species`, `dy`, `mult`, `binning`) or by the series key that gives
it (`trend`, `map`, `purity_key`, `particle`). `labels` go by position: one per value, in the order the pad's series
bring them; a wrong count stops. `colors` needs `color_by`; a colour or a marker given to a series with the look that
sets it is refused.

### When a specification is refused

The message names the place and the reason: `/series/0: unknown key 'colour'; the object takes ...`,
`/: missing divide_option`, `/series/0/trend: not one of [...]`, `/: 'colors' needs 'color_by'`, and so on. What the
schema cannot say, the components refuse when the series are read and drawn (an object not in the file, a mix of pT
and multiplicity, a filling of three values).

### `WithFiles`

`PlotSpec::WithFiles(text, origin, files)` replaces the files a specification names (in the order they first appear)
one for one, and leaves out the words written for the files it named: the series' `label`, `title`, `ratio_title`
and `notes`, which would describe the new files wrongly ("offline" on a file that is not); the legend then comes from
`AutoLabels`. What `--file` does to an example.

## 9. The command `phistrangeplot`

Built with the chain: `cmake -B build -S . && cmake --build build` (or `--target phistrangeplot`). Run from the
repository root, so that relative paths in specifications and examples resolve.

```bash
# the standard summaries of a file, redrawn with no task (into summaries/<file stem>_<scheme>[_<section>]/)
build/phistrangeplot summaries DataFile/pp/DeltaY/Purities/Data_2026_HighIR_updated_Purity.root --out summaries

# what files hold (JSON, in the specification's keys)
build/phistrangeplot list DataFile/pp/DeltaY/FinalResults/Data_OfflineEff_2026_HighIR_updated_PhiAssocSpectra.root

# what a specification may say
build/phistrangeplot schema

# what a specification resolves to, without drawing
build/phistrangeplot describe my.spec.json

# an example on other files: described first, then drawn (the files it names, in the order they first appear,
# replaced one for one; the specification so made is the plot's recipe). With the two _two_files_ratio examples this
# is how two productions are compared (README, Comparing two productions): the comparison task was retired for it.
build/phistrangeplot describe PlotSpecs/examples/trend_two_files_ratio.json --file A.root --file B.root
build/phistrangeplot render PlotSpecs/examples/trend_two_files_ratio.json --file A.root --file B.root --out Plots/a_vs_b

# the plot: my_plot.pdf, .png and .root, the specification as its recipe
build/phistrangeplot render my.spec.json --out Plots/my_plot --alice "Work in progress" --system "pp, #sqrt{#it{s}} = 13.6 TeV"

# and the values it drew, bin by bin, the ratios included (the series must share their X axis)
build/phistrangeplot render my.spec.json --out Plots/my_plot --table Plots/my_plot.txt

```

Exit codes: 0 done, 1 refused or failed (the message says why), 2 wrong use, 3 not in the files.

## 10. C++ compositions and `runplot.sh`

What a specification cannot say is written as a short C++ program on the components, outside the repository unless
the design recurs:

```cpp
#include "PlotDesigns.h"
using namespace PlotSeries;
int main()
{
  return PlotDesigns::Run([] {
    const std::string off = "DataFile/pp/DeltaY/FinalResults/Data_OfflineEff_2026_HighIR_updated_PhiAssocSpectra.root";
    std::vector<Series> s = {Trend(off, "K0S", 1.0), Trend(off, "K0S", 0.5), Trend(off, "K0S", 0.1)};
    PlotDesigns::ColourBy(s, &Facts::dy);
    CanvasManager canvases;
    PlotDesigns::Save(PlotDesigns::Overlay(canvases, "plot", {.series = s, .title = "K^{0}_{S} yield"}), "Plots/k0s_dy", "k0s_dy.cpp");
  });
}
```

`Macros/runplot.sh k0s_dy.cpp [arguments]` compiles it against `Codes/` and ROOT (ROOT's own C++ standard, C++20 at
least) in a temporary directory and runs it. Compiled, not interpreted: the interpreted mode does not start in this
environment, and a `throw` from a check may not reach a `catch` under Cling. The owner's `ratio3modes` plot is such a
composition (`~/alice/claude-sandbox/plots/v2/ratio3modes.cpp`).

## 11. Plots asked in words: `plot_ask.py` and a local model

```bash
python3 Macros/plot_ask.py "K0S trend offline against online, with the ratio" \
    --file DataFile/pp/DeltaY/FinalResults/Data_OfflineEff_2026_HighIR_updated_PhiAssocSpectra.root \
    --file DataFile/pp/DeltaY/FinalResults/Data_OnlineEff_2026_HighIR_updated_PhiAssocSpectra.root \
    --out Plots/k0s_off_on
```

What it does: it gives the model the instructions (`PlotSpecs/instructions.txt`), the schema, the examples
(`PlotSpecs/examples/`) and the catalog of the files; it passes the answer to `phistrangeplot describe`; a refusal is
handed back to the model with its reason (`--retries`, 2 by default); "cannot" is accepted only at the first answer or
after a "not in the files", never after a malformed specification; the description is shown and confirmed before
`render` (`--yes` skips the question); the specification is kept as `<out>.spec.json` and in the plot's recipe, with
the request and the model as a note.

Engines (standard library only, no dependency):

| `--api` | Engine | Context |
|---|---|---|
| `ollama` (default) | Ollama, `http://localhost:11434` | sized from the prompt (Ollama's default 4096 silently cuts it), with a warning if filled |
| `llama` | llama.cpp's `llama-server`, `http://localhost:8080` | read from the server (`/props`); a prompt that does not fit is refused |
| `openai` | any OpenAI-compatible endpoint (`--endpoint`, key in `$PHISTRANGEPLOT_API_KEY`) | the catalog and the request leave the machine |

To serve the measured model with llama.cpp: `llama-server -m ~/.ollama/models/blobs/sha256-60e05f21... -c 16384`.

Other options: `--constraint json` (default: the answer is any JSON object, the schema in the prompt and the validation
after) or `schema` (the schema as the answer's grammar; measured worse with Ollama, as good with llama.cpp);
`--allow-binomial` (without it, the model is not offered `divide_option "B"` and a `"B"` is refused: a binomial division
is right only for a closure, which the files cannot tell); `--alice` with `--system`. The model actually served is
recorded (Ollama's name and digest, llama-server's model file) and a warning is printed when it is not the one measured.

**Measured** (`Macros/plot_ask_test.py`, qwen2.5-coder:7b, the GGUF Ollama keeps as `sha256-60e05f21...`):

| Requests | Ollama | llama-server |
|---|---|---|
| the 10 of phiplot and 3 held out (G3) | 12 of 13, and 12 again after G4 | 12 of 13 (before G4) |
| one per design added in G4 (grid, joined pads, foreign reference, fit grid) | 1 of 4 | not run |

The one of the 13 it misses is the multiplicity-integrated correction map, left out. Before the guard on
`divide_option "B"` (G3), each engine gave 10 of 13 with any JSON and llama-server 10 of 13 with the schema as its
grammar too; Ollama with the schema as grammar gave 5 of the first 10 (its grammar makes the model write the series
before the title, and it then picks a trend).

Every miss ends in a refusal or a "cannot", none in a plot drawn wrong without a word: the components check what the
model writes. The instructions and examples were tuned on the first ten requests; tuning stopped with G4 (the owner: a
larger model, not more tuning). Running the test: `python3 Macros/plot_ask_test.py --out /tmp/ask [--only 2,5] [-- --api llama]`.

## 12. The Claude Code skill

`.claude/skills/plots/SKILL.md`: the steps Claude follows when asked for a plot (list, summaries or a specification
described before rendering, a composition when a specification cannot say it, a component added first when even the
composition cannot). The rules it applies are those of `CLAUDE.md` and of the schema; the syntax and the exit codes
are in section 9.

## 13. How it was verified, and how to verify it again

- **The chain's data**: the blocks of each kind (`mc`, `purity`, correlation offline, WPDG) compared object by object
  with the outputs of `main` before the branch (`compareRootFiles`): identical, apart from the new objects
  (`SigLoss/OverEvLoss`, `deltaY_limits`, `particles`, `purity_keys`, `species_ratios`).
- **The canvases**: `~/alice/claude-sandbox/tools/compareCanvases` compares the canvases of two files histogram by
  histogram (content, errors, marker and line attributes, marker size, the statistics box) and their pads (size,
  title, log scales, axis ranges and titles). The summaries drawn by the tasks matched the old ones, apart from the
  two intended changes; `phistrangeplot summaries` gives the canvases the tasks wrote (0 differences on the four kinds).
- **Specification against C++**: six plots written both ways give identical canvases
  (`~/alice/claude-sandbox/plot-tests/spec/`).
- **Refusals**: `~/alice/claude-sandbox/plot-tests/design_test.cpp` and `series_test.cpp` (run with `runplot.sh`),
  and every refusal of the specification tried by hand with its message and exit code.
- **The model**: `Macros/plot_ask_test.py`, after any change to the prompt, the schema, the examples or the model.

## 14. When something stops

Every refusal starts with `[FATAL]` and names what it refused; the exit code says which kind (section 9). The ones met
most often, and the warnings that change what a result means:

| What you see | Why, and what to do |
|---|---|
| `... is not a multiplicity interval of '...'. Available: ...` (exit 3) | `mult` must be one of the file's classes, as `list` shows them. |
| `\|dy\| < ... is not a limit of section '...' of '...'. Available: ...` (exit 3) | the same for `dy`. |
| `... holds 2 binning schemes (...); name the one to read.` (exit 1) | the file has more than one binning scheme or section: give `scheme` (or `section`). |
| `... does not record 'particles' / 'purity_keys' / 'species_ratios' ...` or `... does not record its \|dy\| limits` (exit 3) | the file was written before the records of section 7: re-run the task the message names. |
| `... is against multiplicity and '...' against pT` | a spectrum and a trend on one plot, or in one ratio: two plots. |
| `... cannot be combined, their binnings differ:` | a ratio or a sum of series whose stamps or X axes differ; the lines after it say which. Drawn side by side they are allowed, with a warning. |
| `... has N non-finite bin(s), which option '...' would draw at 0` | draw with `E`, `E1` or `P` (`draw_option`), which leave the bin empty. |
| `... exists and is not a plot written here; it is not overwritten.` | `--out` points at a file the plots did not write (a data file, perhaps): choose another. |
| `/series/0: unknown key '...'; the object takes ...` and the like | the specification does not follow the schema; the path and the keys allowed are in the message. |
| `'...' names 2 file(s) (...); 1 given.` | `--file` once per file the example names, in the order they first appear. |
| `[FATAL] plot_ask: cannot reach the model at ...` | the engine is not running: `ollama serve` (or `brew services start ollama`), or `llama-server -m ... -c 16384`. |
| `[plot_ask] WARNING: the prompt filled the whole context ...` | Ollama cut the prompt and the model did not see the catalog: fewer files in one request, or a larger `--num-ctx`. |
| `[FATAL] plot_ask: the prompt needs about N tokens and llama-server has M` | restart llama-server with the `-c` the message gives. |
| `[plot_ask] WARNING: ... is not the model measured` | the results of section 11 do not apply to it: run `plot_ask_test.py` before relying on it. |
| `[plot_ask] attempt N refused:` and the reason | the model's specification was refused and is handed back to it with the reason (a binomial division without `--allow-binomial` among them); it stops only as below. |
| `[FATAL] plot_ask: no acceptable specification after N attempts; the last is in ...` | the model did not get it right within `--retries`: correct the last one by hand and `render` it, or ask in other words. |
| `[FATAL] runplot: ROOT is not in the environment (enterO2P)` | `enterO2P` first. |
| `[FATAL] phistrangeplot: Codes/... changed after this binary was built` | the binary would draw with code the tree no longer holds, and record the tree's commit as if it did not: `cmake --build build --target phistrangeplot`. |

## 15. What is not done

`DESIGN_NOTES.md` section 15, *Plots: what is left open*.
