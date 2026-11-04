---
name: plots
description: Make a plot of this framework's outputs on request (spectra, trends, species ratios, purities, correction maps, fit canvases; overlaid, compared with a ratio, in grids or joined pads, against a preliminary), or redraw a task's summaries. Use when the user asks for a plot, an overlay or a comparison of results.
---

# Plots on request

The rule is in CLAUDE.md, *Plots*: a plot is composed from the components, never written from scratch. What is read,
compared and computed is the components'; you choose the series, the layout and the look. Run from the repository root.

1. **Build** if needed: `cmake -B build -S . && cmake --build build --target phistrangeplot` (ROOT in the environment).
2. **What the files hold:** `build/phistrangeplot list <file>...`. Ask for nothing it does not list. Something asked
   that is not there (a species, a |dy|, a class): draw no substitute, say what is missing and what there is.
3. **A standard summary of a task** (the canvases a task draws): `build/phistrangeplot summaries <file> --out <dir>`.
4. **Anything else, as a specification** (`build/phistrangeplot schema` says every key and when it applies;
   `PlotSpecs/examples/` shows the forms): write `<out>.spec.json`, then `build/phistrangeplot describe <spec>`. If it
   refuses, the message names the key and what is allowed; correct and describe again. Show the user the description
   before drawing: it says what each series is and where it comes from. Then
   `build/phistrangeplot render <spec> --out <base>`, and show the PNG. A division's option is stated, never guessed:
   "" for independent samples, "B" only for a closure; ask if unsure. The ALICE label only with the status and the
   system the user gives (`--alice`, `--system`).
5. **What a specification cannot say** (a layout, an operation, a legend the components lack): a C++ composition on
   `PlotSeries.h` and `PlotDesigns.h`, run with `Macros/runplot.sh <composition.cpp>`, out of the repository unless the
   design recurs. If even the components lack it, propose adding it to them first (CLAUDE.md, *Plots*).
6. **An example on other files:** `build/phistrangeplot quick PlotSpecs/examples/<x>.json --file <f>... --out <base>`.

Exit codes of phistrangeplot: 0 done, 1 refused (the message says why), 2 wrong use, 3 not in the files.
