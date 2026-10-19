---
name: phiplot
description: Make a plot from the correlation outputs (PhiAssocSpectra files) on request - spectra, multiplicity trends, species ratios, compared across files with an optional ratio pad. Use when the user asks for a plot, an overlay or a comparison of results of this framework.
---

# Plots on demand with phiplot

The plot is made by `build/phiplot` from a **plot specification** (JSON). You write the
specification; phiplot finds the objects, runs the checks and draws. Never write a ROOT
macro that opens the files and builds object names: the names carry multiplicity
*indices*, and only phiplot reads them through the binning stamps (PLOTTING.md; CLAUDE.md,
the rule the framework is built on).

Run every command from the repository root: the file paths in a specification, and the
examples', are relative to it.

1. Build it if needed: `cmake -B build -S . && cmake --build build --target phiplot`
   (ROOT in the environment).
2. See what the files hold: `build/phiplot list <file>...`. Ask for nothing the listing
   does not show. If the user asks for something absent (a species, a |Δy| limit, an
   interval), write no specification and draw no substitute: say what is missing and what
   there is, and let them choose.
3. Write the specification to `<output>.spec.json`. The keys, and when each applies:
   the descriptions in `build/phiplot schema`, which also say what each kind is ("the trend"
   is `trend`, the measured yield, unless extrapolated is said). Examples:
   `PlotSpecs/examples/`. Use the user's words for the series labels. The ratio's
   `reference` is the series the user divides by; if they only say "A against B", take the
   first named and say so. If you cannot tell which `divide_option` a ratio needs, ask:
   phiplot will not guess.
4. `build/phiplot describe <spec>`: it resolves and checks without drawing. If it refuses,
   fix the specification from its message (it lists what exists). Before drawing anything
   that compares different productions, show the user the description as phiplot printed
   it (the series, the objects and the checks), not a summary of your own.
5. `build/phiplot render <spec>`, then show the PNG (Read it) and give the paths of the
   images, of `<output>.root` (canvas, objects, provenance) and of the specification.

Warnings in phiplot's summary (for example, two trends of the same species integrated over
different pT binnings, drawn together) go into your answer: they say the plot compares
things that do not mean the same.
