# Fit-model selection per data period

How each `JSONConfigs/fitConfig_<dataset>.json` was chosen, repeatable on a new period. The method and the outcome of
9 November 2026 are in `DESIGN_HISTORY.md`, *One fit configuration per period, selected on its own data*.

## What it does

For one period, with a reference fit configuration (the previous period's, or the one in force):

1. **Candidates.** `selection.py candidates` writes six configurations: the reference with every per-cell override
   removed and, in run *i*, each scanned section's default set to its *i*-th candidate (`CANDIDATES` in
   `selection.py`). The φ section is left as it is: its model is a decision, not part of the scan.
2. **Purity runs.** The period's purity task, run by the framework once per candidate and once with the reference.
3. **Selection.** Per section and cell, from the `FitQuality` table of each run: the fits within 10 of the lowest AIC
   (2 `numFloating` + 2 `minNll`), and among them the unflagged first, then the fewest floating parameters. The
   reference is a candidate too, and where it wins the cell keeps its setting, parameters included. The section's
   default is the model chosen most often; every other cell gets an override. Whether a fit is flagged is
   `FitQualityRecorder::Reasons`, the framework's rule. The union of the multiplicity classes (`mult0-100`), which
   the purity task fits besides them, is a cell like the others: it votes for the default and can get an override.
   The configurations selected up to 9 November 2026 (`ff309b9`, the ones in use) were chosen without it: their
   union cells run with the section's default.
4. **Check.** The selected configuration run as written: it must reproduce every chosen fit (`reproduce.txt`); then
   compared with the reference run (`selected_vs_reference.txt`: NLL, flags, purity ratios).
5. **φ.** The period's three φ fits (offline, online, online with the strangeness-injected MC) run with the selected
   configuration, and their fit quality summarised (`phi_quality.txt`). They do not enter the selection: they check
   that the φ model holds on the new period.

## How to run it

The period's purity block and three φ blocks must exist in `JSONConfigs/`. From the repository, with ROOT in the
environment:

```bash
FitModelSelection/run.sh 2026_LowIRag purity_task_2026_lowIRag_updated \
  phi_fit_task_offline_efficiency_2026_lowIRag_updated phi_fit_task_online_efficiency_2026_lowIRag_updated \
  phi_fit_task_online_efficiency_2026_strange_injected_lowIRag_updated JSONConfigs/fitConfig_2026_HighIRac.json
```

About 7 minutes per period. The results go to `output/<period>/` (not tracked, ~100 MB), which is emptied first; outside
it only the framework's by-products change (a log per run in `Logs/`, `build/` if stale), and the tracked
configurations and `DataFile/` are not touched. pp only, as the configurations are. Each run of the framework gets a configuration generated
there (`fitsel_<period>_*.json`: the base and the master as they are, plus one block per run that inherits the
production block and overrides its fit configuration and output directory), and its output in `*.out` next to it.

If the selection is to be kept: copy `output/<period>/fitConfig_selected.json` to
`JSONConfigs/fitConfig_<dataset>.json` and set it as `fit_config_file` of the period's purity and φ blocks, after
`inherits`.

## Files

| | |
|---|---|
| `run.sh` | the steps above, for one period |
| `selection.py` | everything that is not a run of the framework: the generated configurations, the candidates, the selection, the reports |
| `dump.cpp` | built against `Codes/` by `run.sh`: one output file's fit quality and values as two tables, the names from `OutputNames` |

## What it does not do

- It does not choose the φ model, nor the candidates: both are written down (`CANDIDATES`), changed by hand.
- An equivalent model (within 10 of AIC) can give a different purity where the purity is low: Omega at pT 4.2-6.5 differs
  by 10-30 % between the models the rule considers equivalent. That spread is a systematic, which this does not
  estimate.
