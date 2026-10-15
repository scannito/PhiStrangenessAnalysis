# RooFit knowledge base

Notes written on 14-15 October 2026 from the RooFit documentation of ROOT 6.36.10 (ALICE build) and
from experiments on this project's own fits. One file per topic, numbered. Each claim says where it comes
from: **[doc]** a documentation page or Doxygen comment, **[source]** the code itself, **[verified]**
something that was run here, **[inferred]** a conclusion drawn from the above and not tested.

Written by separate passes over the material; they can disagree at the edges, and where they do the
experiment wins.

## Where to start
- `00-summary.md`: what was found, what is shaky, what is left to decide (in Italian).
- `ROADMAP.md`: what to do in the project, in phases (in Italian).
- `13-not-studied.md`: what of RooFit was not studied, with the approaches that could help highlighted.
- Each note ends with `## Resume here`, which says how to rerun its experiments.

## What was asked, and how it was done
The original request (14 October 2026): "A full immersion in all the existing RooFit documentation, really all of it.
Maybe some things are already automated - for instance there should be native functions to parse models from JSON.
I want you to know everything about that library." The project it serves is `DynamicRooFitter`
(`Codes/DynamicRooFitter.h`), which builds its models by hand from `JSONConfigs/fitConfig.json` and fits ~1000 small
binned extended 1D fits.

The first pass split the library into eight topics, one note and one researcher each (notes 01-08). Later passes
added the questions that turned out to matter for the project: fit quality and yields (09), the model-parsing
decision (10), the purity step end to end (11), purity bias with toys (12). The research was closed on
15 October 2026 with no open item; the history of the passes is in sections 1.7-1.9 of `00-summary.md`.

A number cited by notes 04 and 09: over all 960 cells of the older `AnalysisResults_136_2026_PtMod_23luglio.root`
(k0s 340, xi 260, pi_tpc 360) the fit status was 3 for 310, 134 and 338 of them. That file is not a purity input
(see `00-summary.md` section 2); the later notes use the 2024 (`5agosto`) and 2026 (`6agosto`) files.

## Rerunning an experiment
- Scripts and outputs are in `scratch/` (git-ignored; see its README). Executables, `*.root`, `*.png` and logs are
  not all kept: rebuild with the `build.sh` of each directory.
- Load ROOT with `source scratch/rooenv.sh` (a captured environment). Do not run `alienv printenv` in parallel
  commands: concurrent calls collide and return an empty environment.
- Do not run RooFit in more than one thread of one process (note 06, note 10). Use separate processes.
- New researchers: use the `roofit-researcher` agent (`.claude/agents/roofit-researcher.md`), which carries these rules.
