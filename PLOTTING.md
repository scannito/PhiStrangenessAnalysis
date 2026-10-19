# Plots on demand: `phiplot`

`build/phiplot` draws what the correlation tasks wrote into `PhiAssocSpectra` files: spectra,
yields against multiplicity, species ratios, compared across files with an optional ratio pad.
A plot is asked for by **meaning** - kind, species, |Δy| limit, multiplicity interval - and
never by object name. It works with no language model at all; a model, local or remote, or
Claude Code, can be put in front of it to ask in words.

What is not done yet, and the choices still open, are in `DESIGN_NOTES.md`, section 14. Why it
is built this way, and how it was checked, is in `DESIGN_HISTORY.md`, *Plots on demand, and the
object names in one place*.

---

## Why a tool and not macros

The objects in the files are named by multiplicity **index**: `h1SpectrumPhiK0S_dy1_00_multBin3`
is the fourth interval of whatever binning the producer had (CLAUDE.md, *The rule the whole
framework is built on*). A macro that builds that name by hand - written by a person or by a
model - finds the object and draws it, and nothing says whether bin 3 is the interval it meant.
`phiplot` never builds a name from a request: it reads the `binning_mult` stamp to find which
index *is* the interval asked for, and refuses to divide series whose binnings disagree.

So there is one interface, the **plot specification** (a JSON file), and everything that asks
for a plot - the command line, a hand-written file, a language model - produces one and goes
through the same parser and the same checks. A model is a front end that writes
specifications, never one that writes code.

---

## Building

```bash
enterO2P                                    # or any environment with ROOT
cmake -B build -S . && cmake --build build --target phiplot
```

It is built with the rest of the project (`cmake --build build`), compiled only (it is not
part of the analysis chain, and Cling is not asked to load it). Run every command from the
repository root: the file paths in a specification are relative to where `phiplot` runs.

---

## Commands

| Command | |
|---|---|
| `phiplot list [--json] <file>...` | What the files hold: per scheme and section, the kinds of object for each species, the \|Δy\| limits and multiplicity intervals present, the binnings of the stamps, when and from which commit the file was produced. Histograms whose names it does not recognise are listed as such. |
| `phiplot schema` | The JSON schema of a specification, with a description of every key. |
| `phiplot describe <spec.json>` | Finds every object and runs every check, then says in plain words what would be drawn: each series, the object and file it comes from, what was checked and what could not be. Draws nothing. |
| `phiplot render <spec.json>` | The same, then draws. |
| `phiplot quick <kind> ...` | A specification from command-line options, saved as `<out>.spec.json` and rendered from that file. |

```bash
build/phiplot list DataFile/pp/DeltaY/FinalResults/Data_OfflineEff_2024_PhiAssocSpectra.root

build/phiplot quick trend --species K0S --dy 1 \
    --file DataFile/pp/DeltaY/FinalResults/Data_OfflineEff_2024_PhiAssocSpectra.root:2024 \
    --file DataFile/pp/DeltaY/FinalResults/Data_OfflineEff_2026_PhiAssocSpectra.root:2026 \
    --ratio --out Plots/k0s_years

build/phiplot render PlotSpecs/examples/spectrum_three_dy.json
```

`build/phiplot` with no arguments prints the options of `quick`. Two worth knowing: `--ratio [B]`
divides by the first `--file`, and `--alice STATUS` needs `--system TEXT` (the label's two lines:
the collision system is not in the files, so it is never assumed).

**Exit codes**:
- 0: done.
- 1: refused or failed; the message says why. This includes a malformed `quick` option, such as an
  unknown option, a `--mult` that is not LO-HI, or a value that is not a number.
- 2: usage. That means a wrong command, a wrong number of arguments, or a `quick` missing
  `--out`, `--file`, `--species` or `--dy`.
- **3: what was asked is not in the files**: a species, a |Δy| limit, an interval, a scheme, a
  section, or the `binning_mult` stamp a spectrum needs.

A front end tells 3 from a malformed specification by the code, not by the wording.

---

## The specification

Examples in `PlotSpecs/examples/`; every key, with what it means, in `phiplot schema`.

```json
{
  "output": "Plots/k0s_trend_2024_vs_2026",
  "title": "K^{0}_{S} yield, 2024 against 2026",
  "series": [
    {"file": "DataFile/pp/DeltaY/FinalResults/Data_OfflineEff_2024_PhiAssocSpectra.root", "kind": "trend", "species": "K0S", "dy": 1.0, "label": "2024"},
    {"file": "DataFile/pp/DeltaY/FinalResults/Data_OfflineEff_2026_PhiAssocSpectra.root", "kind": "trend", "species": "K0S", "dy": 1.0, "label": "2026"}
  ],
  "ratio": {"reference": 0, "divide_option": ""}
}
```

The kinds, as `OutputNames::kKinds` defines them (`phiplot schema` prints the same):

| Kind | What it is | Needs |
|---|---|---|
| `spectrum` | pT spectrum of one species in one multiplicity interval | `mult` |
| `spectrum_extended` | the same, with the extrapolated bin down to pT = 0 in front | `mult` |
| `trend` | measured yield dN/dy of one species against multiplicity | |
| `trend_extrapolated` | the yield extrapolated to pT = 0, against multiplicity | |
| `trend_extrap_over_meas` | extrapolated over measured yield of one species | |
| `species_ratio_measured` | ratio of the measured yields of two species | `species_den` |
| `species_ratio_extrapolated` | ratio of the extrapolated yields of two species | `species_den` |

A key that means
nothing for a kind (`mult` on a trend) is refused, not ignored. `dy` is a limit the files were
produced with, to at most two decimals. `mult` is an interval of the file's multiplicity binning
(`list` shows them), never an index. Spectra (against pT) and the other kinds (against
multiplicity) cannot share a plot.

`binning_name` and `section` are needed only when a file holds more than one. `label`, `color`
and `marker` are optional: by default the legend shows what differs between the series (the
file names are shortened to the part that tells them apart) and puts what they share in its
header. Top-level options: `formats` (pdf and png by default; also svg, eps), `title`,
`x_title`, `y_title`, `log_y` (on by default for spectra), `x_range`, `y_range`, `alice_label`
(`status` and `system`), `ratio` (`reference`, `divide_option`, optional `y_range`, `y_title`).

**`divide_option`** is required in a ratio and never guessed, as in `comparison_task`: `""` for
independent samples (two years, two productions), `"B"` when each numerator is a subset of the
reference (a closure: corrected against generated).

`output` is a path without extension. It is never written over a `.root` that phiplot did not
write itself - an input, above all.

---

## What is checked

- A multiplicity interval is looked up in the file's `binning_mult` stamp. A file without the stamp
  cannot have a spectrum asked of it.
- Dividing two series (a ratio pad) requires:
  - the same X axis, edge by edge;
  - the same trigger-pT binning, which everything here is summed over;
  - for a trend or a species ratio, the same associated-pT binning of each species the two series
    share, because a trend is an integral over it.

  A mismatch stops the run. The multiplicity axes of two trends can agree perfectly while their
  contents are integrals over different pT ranges, and only the stamps say so.
- Drawing such series together without dividing is allowed. The disagreement goes through
  `Logger::Warn`, into the end-of-run summary, and is listed under `DIFFERS` in the description.
- A stamp missing on one side means a check that could not be made. It is warned about in both
  modes and listed under `NOT checked`.

---

## Output

For `"output": "Plots/x"`: `Plots/x.pdf`, `Plots/x.png` (or the `formats` asked for), and
`Plots/x.root`:

| Path in `x.root` | |
|---|---|
| the canvas | what was drawn |
| `Objects/series<i>/<name>` | each input as it was read, under its original name |
| `Objects/phiplot_ratio<i>` | the ratios of the ratio pad |
| `Provenance/plot_spec` | the specification, as read |
| `Provenance/produced_at` | when |
| `Provenance/git_commit`, `Provenance/sources_fingerprint` | the plotting code's commit and the fingerprint of its headers, from the source directory phiplot was built from, whatever the working directory; absent if that tree has moved since the build |
| `Provenance_series<i>/...` | each input's own provenance, its file and the path of the object |

`quick` also leaves `Plots/x.spec.json`, so the plot can be made again with `render`.

---

## Asking in words: `Macros/phiplot_ask.py`

```bash
python3 Macros/phiplot_ask.py "trend dei K0S, 2024 contro 2026, con il rapporto" \
    --file DataFile/pp/DeltaY/FinalResults/Data_OfflineEff_2024_PhiAssocSpectra.root \
    --file DataFile/pp/DeltaY/FinalResults/Data_OfflineEff_2026_PhiAssocSpectra.root \
    --out Plots/prova
```

What it does:
1. **Builds the prompt.** It holds the schema, with what each key and kind means; the catalog of
   the files (`list --json`); and the examples. All are compact JSON.
2. **Asks the model.** The answer is constrained to the schema: one form per group of kinds, so
   `mult` and `species_den` are present exactly where they belong. The answer may also be
   `{"cannot": "..."}`.
3. **Checks the answer.** It saves the specification as `<out>.spec.json` and runs `phiplot describe`.
4. **Handles a refusal.** If phiplot refuses, the message goes back to the model to correct, at
   most `--retries` times (2 by default). "Cannot" is offered again only when phiplot exits with
   code 3, meaning the object is not in the files. For a malformed specification the model must
   correct, not give up. When the model answers "cannot", the script prints why and exits with 3.
5. **Asks for confirmation.** It prints phiplot's description of what it resolved and asks before
   drawing (`--yes` skips the question).

The `output` is always the caller's: whatever the model wrote there is replaced. Standard library
only; no dependency to install.

**What you confirm is phiplot's description, not the model's words.** A specification can be
valid and still not be what you meant (the right file, the wrong kind): the checks catch what
cannot be, the description shows what was understood.

### With a local model (Ollama)

Nothing leaves the machine.

```bash
brew install ollama
brew services start ollama          # a background service; or `ollama serve` in a terminal
ollama pull qwen2.5-coder:7b        # 4.7 GB, Q4_K_M; Apache 2.0
```

The service loads the model into memory on the first request (a few seconds) and unloads it after
a few minutes idle. It is stopped with `brew services stop ollama`, and removed with
`brew uninstall ollama` and `~/.ollama`. Options: `--model` (default `qwen2.5-coder:7b`),
`--endpoint` (default `http://localhost:11434`), `--seed` (temperature is always 0), `--num-ctx`
(the context, by default sized from the prompt; see below). Exit codes: 0 drawn or declined at
the confirmation, 1 failed, 3 the model said the request cannot be drawn from these files.

**Context.** Ollama gives a model 4096 tokens of context unless told otherwise, and cuts a longer
prompt **without an error**. The prompt here is longer: the schema, a catalog of two files and the
examples. The script sizes the context from the prompt (`num_ctx`), and warns if the prompt filled
it. The warning is best effort: Ollama does not count a prefix it reused from its cache.

### With a remote model

```bash
export PHIPLOT_API_KEY=...
python3 Macros/phiplot_ask.py "..." --file ... --out ... --api openai --endpoint https://<host> --model <name>
```

`--endpoint` is the base URL, **without** `/v1`: the script appends `/v1/chat/completions`. It can be
any OpenAI-compatible endpoint, including `llama-server` from llama.cpp run locally (`--endpoint
http://localhost:8080`). The
schema is sent as `response_format`. Not tried against a real remote endpoint: whether a given
server enforces it is to be checked.

### With Claude Code

The skill `.claude/skills/phiplot` follows the same steps: list, write the specification, `describe`,
`render`. With an impossible request it says what exists instead of drawing a substitute. It is
loaded where the branch with it is checked out.

---

## Testing the model front end

```bash
python3 Macros/phiplot_ask_test.py --model qwen2.5-coder:7b --out /tmp/phiplot_test
python3 Macros/phiplot_ask_test.py --model qwen2.5-coder:7b --out /tmp/phiplot_test --only 2,5
```

There are ten requests, in Italian and English:
- eight possible: trends, spectra at given intervals, species ratios, extrapolated over measured,
  years and online/offline compared, with and without a ratio pad;
- two impossible: a |Δy| and a species that the files do not hold.

For each request the script checks the specification it ends with against what was meant: the
kind, species, |Δy| (and that the series share one where they are compared), intervals, files, and
the ratio's reference and `divide_option`. It reports the number of refusals and the time. An
impossible request is right only when the model answered that it cannot be drawn (exit 3). Any
other failure is reported as FAILED with its exit code, for example no endpoint or the retries
used up. It uses the 2024 and 2026 offline and the 2024 online pp files under `DataFile/`. They are
not in git, so the numbers belong to those productions. Rerun what failed first, then everything
as a regression check whenever the prompt, the schema or the model changes: a change made for one
request can move the others.

What was measured, with which model, and what each first run taught, is in `DESIGN_NOTES.md`,
section 14. It is kept in one place, so a new measurement updates one place.

---

## Troubleshooting

| What you see | Why, and what to do |
|---|---|
| `cannot reach the model at http://localhost:11434` | The Ollama service is not running: `brew services start ollama` or `ollama serve`. |
| The model invents files, or ignores the catalog | The prompt was cut (look for `WARNING: the prompt filled the whole context`). Fewer files in one request, or a larger `--num-ctx`. |
| `no binning_name to choose from: the file has no <binning_name>/Extract1D or Extract2D directory` | The file predates the current layout (or is not a PhiAssocSpectra file). Re-run the correlation task that wrote it. |
| `... is not a multiplicity bin of this file. Available: ...` | `mult` must be one of the intervals listed; `list` shows them. |
| `exists and was not written by phiplot ... it is not replaced` | `output` points at a file that is not a previous plot: choose another. |
| `some histograms ... have names OutputNames does not recognise` | Objects written by an older producer, or a kind not yet in `OutputNames`: listed by `list`, not plottable. |
| A warning about different binnings in an overlay | The series were summed or integrated over different binnings: drawn, but not comparable as they are. The description says which. |
