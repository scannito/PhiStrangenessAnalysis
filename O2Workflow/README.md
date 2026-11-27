# O2Workflow — running the O2Physics side locally

How the O2Physics part of the analysis is run on a local AO2D: the producer `phiStrangeCorrelator`
(`PWGLF/TableProducer/Strangeness/phiStrangeCorrelator.cxx`) and the task `phiStrangeCorrelation`
(`PWGLF/Tasks/Strangeness/phiStrangeCorrelation.cxx`), after the services they need. It is for testing a change of
the O2Physics code before it goes to Hyperloop. The output has the format the framework reads, but the services
are configured here (below), not as in the trains: a difference from a train output may come from them.

## Running

```bash
alienv enter O2Physics/latest                       # or the environment of the build under test
cd <a directory for the output>
<repo>/O2Workflow/runPhiStrangeCorr.sh data ~/alice/Test/data/data1.root
<repo>/O2Workflow/runPhiStrangeCorr.sh mc ~/alice/Test/dataMC/dataMC.root cutsOn    # 'cutsOn' labels the run
```

Each run writes into a new directory `<mode>[-<label>]` of the current one: `AnalysisResults.root`,
`configuration.json` (what was passed), `dpl-config.json` (what DPL used, every option) and `log.txt`, plus
`QAResults.root` if a service writes QA (none does with these configurations). A directory that already exists stops the script: nothing is overwritten. The input can be a
file or `@list`, a file list whose entries are absolute paths.

The local inputs are kept outside the repository, in `~/alice/Test/data/data1.root` (data) and
`~/alice/Test/dataMC/dataMC.root` (MC): AO2Ds of several GB, which a `git clean -x` would delete with no warning
if they sat here among the ignored files.

## The configurations

`configs/base.json` is everything that is the same in data and MC: every cut, binning and service setting. The
mode files hold only what runs, and the script merges the two (`jq`, key by key):

| | producer | task |
|---|---|---|
| `configs/data.json` | `processData` | same event (`processPhiAssocSEDataLike`) and mixed event for every species (`processPhi<species>MEDataLike`) |
| `configs/mc.json` | `processMCReco`; event selection also `processMCGen` | same and mixed event with the PDG code (`...MCWithPDG`), efficiencies (`processParticleEfficiency`), closure on generated particles (`processMCGenClosure`) |

`mc.json` also sets up the MC devices (`mc-centrality`, `mc-collision-converter`, the MC process switches of the
services), and the script adds those devices to the chain when the configuration has the `mc-centrality` block.
One base for both modes is what keeps the efficiencies and closures computed with the selection of the data.

Same and mixed event are one run: the histograms have different names. In MC the efficiency and the two closures
are one run for the same reason (`...MCReco`/`...MCGen`, `...ClosureMCGen`, the reconstructed correlations).
Online efficiencies are off (`applyEfficiency`, `applyPhiEfficiency`).

Every option is written out, as DPL dumps it. An option left out would run at the default of whatever build runs
it, and a default that changes in O2Physics would change the result with nothing in the configuration to show it.
DPL also ignores a key that no device declares, so a renamed option would do both at once, silently: after every
run the script compares the options passed with those DPL used and stops (exit 2, the output kept) if they differ.

How they were made (27 November 2026, O2Physics `phistrange-ccdb-ownership` at 47a83c41f), from the
`dpl-config.json` of one run per mode, the `internal-dpl-*` blocks removed (they hold the input path):

- **Our devices** (`phi-meson-cand-producer`, `v0-reduced-cand-producer`, `cascade-reduced-cand-producer`,
  `pion-track-producer`, `event-selection-producer`, `phi-strange-correlation`): the defaults of that build, except
  the process switches above. Every process switch is set, since several default to true.
- **The services**: the values of the previous local configurations (March-April 2026), keys that no longer exist
  dropped, labelled arrays merged by row label into the current ones. Two changes were needed for the current
  services, both in `mc.json`:
  - `mult-cent-table.processMonteCarlo` true: `mc-centrality` joins the MC collisions with `MultMCExtras`, which
    that process writes; without it the run stops at the first DF.
  - `mc-centrality.path` without the `ccdb://` prefix: the path is now handed to the CCDB as it is, and with the
    prefix the calibration is not found and the generated centrality is filled uncalibrated, with no error.
    The log of a good run says `loaded TList with this many objects` and `Fully configured for run`.
- The previous configurations stopped reading after 100 s (`time-limit` of the AOD reader): these have no limit.

When the script reports a mismatch after an O2Physics change: remove the options marked `<` from `configs/`, and
add those marked `>` with the value in the run's `dpl-config.json` (the new default), to `base.json` unless it is a
process switch or differs between the modes.

A message expected in every log, not an error of ours: the event-selection service does not find the RCT flags
of the input's pass (`Requested resource does not exist: .../RCT/Flags/RunFlags/...`). The producer's event
selection does not use them (`cfgRCTLabel` empty).
