# The steps of the fit-model selection that are not runs of the framework (README.md): the configurations the runs
# use, the candidate fit configurations, the selection, and the reports. run.sh calls one subcommand per step.
#
#   runconfig <Purity|Data> <block> <output dir> <config.json> <name>=<fitConfig>...   (prints the binning_name)
#   candidates <reference fitConfig> <scan dir>                                        (prints the run names)
#   select <scan dir> <reference fitConfig> <output fitConfig>
#   reproduce <scan dir> <quality.tsv of the selected configuration>
#   compare <quality A> <quality B> <values A> <values B>
#   quality <quality.tsv>...
#
# The tables are dump.cpp's: one row per fit (section, binning, cell, ..., reasons) and one per value. Whether a fit is
# flagged is the 'reasons' column, FitQualityRecorder::Reasons as dump.cpp wrote it: no rule of its own here.
import copy, csv, json, os, sys
from collections import Counter, defaultdict

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MARGIN = 10.0  # AIC: fits within this of the best are equivalent
START_GAP = 0.5  # NLL: the configured start "lost" when the winner is lower by more than this
# Per scanned section, the (signal, background) models one candidate run each; a section with fewer candidates than
# runs repeats its last.
CANDIDATES = {
    "k0s": [("DSCB", "Chebychev1"), ("DSCB", "Chebychev2"), ("DSCB", "Chebychev3"),
            ("DSCB+Gaussian", "Chebychev1"), ("DSCB+Gaussian", "Chebychev2"), ("DSCB+Gaussian", "Chebychev3")],
    "xi": [("DSCB", "Chebychev1"), ("DSCB", "Chebychev2")],
    "omega": [("DSCB", "Chebychev1"), ("DSCB", "Chebychev2"), ("Gaussian", "Chebychev1"), ("Gaussian", "Chebychev2")],
    "pi_tpc": [("DSCB", "Gaussian")],
    "pi_tof": [("DSCB", "Exponential"), ("DSCB", "Gaussian"), ("DSCB", "Gaussian+Exponential"),
               ("Gaussian", "Exponential"), ("Gaussian", "Gaussian"), ("Gaussian", "Gaussian+Exponential")],
}
RUNS = max(len(v) for v in CANDIDATES.values())


def model(section, run):
    return CANDIDATES[section][min(run, len(CANDIDATES[section]) - 1)]


def scanned(fit_config):
    return [s for s in CANDIDATES if s in fit_config]


# What a per-cell override of a scanned section may set: the models are what the scan replaces and the parameters are
# completed into the default. Anything else (an integration range, a sideband) would be dropped wherever a candidate
# wins, changing the purity with nothing in the NLL to show it.
OVERRIDE_KEYS = {"sig_model", "bkg_model", "params"}


def fail(message):
    sys.exit(f"[FATAL] FitModelSelection: {message}")


def table(path):
    return list(csv.DictReader(open(path), delimiter="\t"))


def by_fit(rows):
    return {(r["section"], r["binning"], r["cell"]): r for r in rows}


def flagged(row):
    return row["reasons"] != "-"


def aic(row):
    return 2 * int(row["numFloating"]) + 2 * float(row["minNll"])


def complete_defaults(fit_config):
    """Into each section's default, the parameters only overrides set: a candidate that makes a model the default
    needs them there. One value per parameter, whichever override it comes from, or the selection stops. The scanned
    sections only: the others are left as the reference has them."""
    for section in scanned(fit_config):
        setup = fit_config[section]["fit_setup"]
        params, found = setup["default"]["params"], {}
        for tag, override in setup.get("bins", {}).items():
            for name, value in override.get("params", {}).items():
                if name in params:
                    continue
                if name in found and found[name][1] != value:
                    fail(f"'{section}': the overrides {found[name][0]} and {tag} set '{name}' differently; declare it in the default.")
                found.setdefault(name, (tag, value))
        params.update({name: value for name, (_, value) in found.items()})


def runconfig(kind, block, output_dir, out_path, *runs):
    """One configuration the framework runs by path (Codes/runAnalysis.sh <file>): the base and the master as they are,
    the master's workflow with only these runs active, and one block per run, each the production block with its
    fit configuration and its output in the study's directory. WorkflowManager resolves 'inherits' itself.
    The base and the master are put in one file because runAnalysis.sh runs a file given by path without a base. Where
    WorkflowManager would let the master's block win, this stops instead: today no name is in both, and one that came to
    be would be a question for whoever put it there. pp only, as the configurations are."""
    base = json.load(open(f"{REPO}/JSONConfigs/globalConfigBasepp.json"))
    master = json.load(open(f"{REPO}/JSONConfigs/globalConfig{kind}pp.json"))
    if set(base) & set(master):
        fail(f"blocks in both globalConfigBasepp.json and globalConfig{kind}pp.json: {sorted(set(base) & set(master))}")
    config = {**base, **master}
    if block not in config:
        fail(f"no block '{block}' in globalConfig{kind}pp.json or globalConfigBasepp.json")
    directory = {"Purity": "output_dir", "Data": "projections_dir"}[kind]
    blocks = {}
    for run in runs:
        name, fit_config = run.split("=", 1)
        # named after the production block: WorkflowManager knows a task by the prefix of its name
        blocks[f"{block}_fitsel_{name}"] = {"inherits": block, "fit_config_file": os.path.abspath(fit_config),
                                    directory: os.path.abspath(output_dir) + "/", "output_prefix": f"{name}_"}
    if set(blocks) & set(config):
        fail(f"the study's block names are taken: {sorted(set(blocks) & set(config))}")
    config.update(blocks)
    config["workflow"]["active_tasks"] = list(blocks)
    json.dump(config, open(out_path, "w"), indent=4)
    print(config["workflow"]["global_binning"]["binning_name"])


def candidates(reference, scan):
    """cand<i>.json: the reference with every per-cell override removed and each scanned section's default set to its
    i-th candidate (model)."""
    base = json.load(open(reference))
    for section in scanned(base):
        for tag, override in base[section]["fit_setup"].get("bins", {}).items():
            if set(override) - OVERRIDE_KEYS:
                fail(f"'{section}' {tag} in {reference} sets {sorted(set(override) - OVERRIDE_KEYS)}: the scan can only "
                     f"replace {sorted(OVERRIDE_KEYS)}; put the rest in the default or extend the scan.")
    complete_defaults(base)
    for run in range(RUNS):
        c = copy.deepcopy(base)
        for section in scanned(c):
            setup = c[section]["fit_setup"]
            setup.pop("bins", None)
            setup["default"]["sig_model"], setup["default"]["bkg_model"] = model(section, run)
        json.dump(c, open(f"{scan}/cand{run}.json", "w"), indent=4)
        print(f"cand{run}")


def select(scan, reference_path, out_path):
    """Per section and cell, the candidates within MARGIN of the lowest AIC, and among them the unflagged first, then the
    fewest floating parameters. A flag weighs only between equivalent fits: a flagged fit much better than every
    unflagged one is kept, its flag reported. The reference, run as it is (quality_reference.tsv), is a candidate too,
    and where it wins the cell keeps its setting, parameters included. The section's default is the model chosen in
    most cells, every other cell an override. Writes the fitConfig and cells.tsv, whose 'origin' is the run each chosen
    fit comes from."""
    reference = json.load(open(reference_path))
    sections = scanned(reference)
    reference_fits = by_fit(table(f"{scan}/quality_reference.tsv"))

    # (section, binning, cell) -> (kind, sig, bkg) -> (origin, row), the models as each fit recorded them; a model two
    # runs share (a section with fewer candidates) is taken from the first. The reference's models are a distinct
    # candidate: they come with its parameters.
    # Every run must have fitted the same cells as the reference: a cell missing from one would be chosen among fewer
    # candidates without a word.
    expected_fits = {fit for fit in reference_fits if fit[0] in sections}
    fits = defaultdict(dict)
    for run in range(RUNS):
        rows = [r for r in table(f"{scan}/quality_cand{run}.tsv") if r["section"] in sections]
        if {(r["section"], r["binning"], r["cell"]) for r in rows} != expected_fits:
            fail(f"cand{run} and the reference did not fit the same cells: re-run the scan.")
        for r in rows:
            expected = model(r["section"], run)
            if (r["sigModel"], r["bkgModel"]) != expected:
                fail(f"cand{run} fitted {r['section']} {r['cell']} with {r['sigModel']}/{r['bkgModel']}, not its candidate {expected}")
            fits[(r["section"], r["binning"], r["cell"])].setdefault(("candidate",) + expected, (f"cand{run}", r))
    for fit in expected_fits:
        r = reference_fits[fit]
        fits[fit][("reference", r["sigModel"], r["bkgModel"])] = ("reference", r)

    chosen = {}  # (section, binning, cell) -> ((kind, sig, bkg), origin, flagged)
    for fit, models in fits.items():
        rows = {m: r for m, (_, r) in models.items()}
        if all(float(r["counts"]) <= 0 for r in rows.values()):  # nothing to choose between: the cell keeps the default
            continue
        best = min(aic(r) for r in rows.values())
        near = [m for m, r in rows.items() if aic(r) <= best + MARGIN]
        pick = min(near, key=lambda m: (flagged(rows[m]), int(rows[m]["numFloating"]), aic(rows[m])))
        chosen[fit] = (pick, models[pick][0], flagged(rows[pick]))

    out = json.load(open(reference_path))
    complete_defaults(out)
    rows = ["section\tbinning\tcell\torigin\tsig\tbkg\tflagged\treference_sig\treference_bkg"]
    for section in sections:
        # The override key is the cell's tag, which an analysis and a source cell with the same interval share: one
        # setting per tag, so the two choices must agree (the same fit on the same data).
        per_tag = {}
        for (s, binning, cell), (pick, _, _) in chosen.items():
            if s == section:
                if per_tag.setdefault(cell, pick) != pick:
                    fail(f"'{section}' {cell}: the analysis and source fits chose {per_tag[cell]} and {pick}")
        default = Counter(m[1:] for m in per_tag.values()).most_common(1)[0][0]
        setup = out[section]["fit_setup"]
        setup["default"]["sig_model"], setup["default"]["bkg_model"] = default
        setup["bins"] = {}
        for tag, pick in sorted(per_tag.items()):
            if pick[0] == "reference":
                # The reference's setting of the cell as it was: its override whole, or nothing. An override that sets
                # parameters only relied on the reference's default models, which are written out when they differ.
                override = copy.deepcopy(reference[section]["fit_setup"].get("bins", {}).get(tag, {}))
                if pick[1:] != default:
                    override["sig_model"], override["bkg_model"] = pick[1:]
                if override:
                    setup["bins"][tag] = override
            elif pick[1:] != default:
                setup["bins"][tag] = {"sig_model": pick[1], "bkg_model": pick[2]}
        if not setup["bins"]:
            del setup["bins"]

        mine = sorted((fit, c) for fit, c in chosen.items() if fit[0] == section)
        for (s, binning, cell), (pick, origin, is_flagged) in mine:
            ref = reference_fits[(s, binning, cell)]
            rows.append("\t".join([s, binning, cell, origin, *pick[1:], str(is_flagged), ref["sigModel"], ref["bkgModel"]]))
        kept = Counter((fit[1], pick[0] == "reference") for fit, (pick, _, _) in mine)
        print(f"{section:7s} default {default[0]} + {default[1]}; chosen: "
              + ", ".join(f"{'+'.join(m if m[0] == 'reference' else m[1:])} {n}" for m, n in Counter(m for _, (m, _, _) in mine).most_common()))
        print(f"        overrides {len(setup.get('bins', {}))}; reference's setting kept in "
              + ", ".join(f"{b} {kept[(b, True)]}/{kept[(b, True)] + kept[(b, False)]}" for b in ("analysis", "source"))
              + f"; chosen fit flagged in {sum(f for _, (_, _, f) in mine)} fits")
    json.dump(out, open(out_path, "w"), indent=4)
    open(f"{scan}/cells.tsv", "w").write("\n".join(rows) + "\n")


def reproduce(scan, selected_path):
    """The run of the selected configuration against the fits it chose: each must reach the NLL of the run it was
    taken from (the origin in cells.tsv)."""
    selected = by_fit(table(selected_path))
    cells = table(f"{scan}/cells.tsv")
    sources = {o: by_fit(table(f"{scan}/quality_{o}.tsv")) for o in {r["origin"] for r in cells}}
    bad = []
    for r in cells:
        fit = (r["section"], r["binning"], r["cell"])
        if abs(float(selected[fit]["minNll"]) - float(sources[r["origin"]][fit]["minNll"])) > 1e-3:
            bad.append(fit)
    print(f"the selected configuration reproduces the chosen fit in {len(cells) - len(bad)} fits, differs in {len(bad)}")
    for fit in bad[:10]:
        print("   ", *fit)
    if bad:
        sys.exit(1)


def compare(quality_a, quality_b, values_a, values_b):
    """Two runs of the same fits on the same data, A and B: per section and binning the fits where B's NLL is higher
    or lower by more than START_GAP and the flagged ones; per quantity the values B / A."""
    a, b = by_fit(table(quality_a)), by_fit(table(quality_b))
    groups = defaultdict(list)
    for fit in a.keys() & b.keys():
        groups[fit[:2]].append((a[fit], b[fit]))
    print(f"NLL per fit, B - A (B worse when positive); fits in A only {len(a.keys() - b.keys())}, in B only {len(b.keys() - a.keys())}:")
    for (section, binning), pairs in sorted(groups.items()):
        d = [float(rb["minNll"]) - float(ra["minNll"]) for ra, rb in pairs]
        print(f"  {section:7s} [{binning:8s}] {len(pairs):4d} fits: B worse in {sum(x > START_GAP for x in d):3d}, better in "
              f"{sum(x < -START_GAP for x in d):3d}; flagged A {sum(flagged(ra) for ra, _ in pairs):3d}, B {sum(flagged(rb) for _, rb in pairs):3d}")
    va = {(r["quantity"], r["binning"], r["cell"]): float(r["value"]) for r in table(values_a)}
    vb = {(r["quantity"], r["binning"], r["cell"]): float(r["value"]) for r in table(values_b)}
    ratios, zero = defaultdict(list), []
    for key in sorted(va.keys() & vb.keys()):
        if va[key] == 0:
            zero.append(key)
        else:
            ratios[key[:2]].append((vb[key] / va[key], key[2]))
    print(f"Values B / A; values in A only {len(va.keys() - vb.keys())}, in B only {len(vb.keys() - va.keys())}, A = 0 in {len(zero)}:")
    for quantity, binning, cell in zero:
        print(f"  {quantity:7s} [{binning:8s}] {cell:24s} A = 0, B = {vb[(quantity, binning, cell)]:.4g}")
    for (quantity, binning), l in sorted(ratios.items()):
        s = sorted(x for x, _ in l)
        big = sorted((x for x in l if abs(x[0] - 1) > 0.005), key=lambda x: -abs(x[0] - 1))
        print(f"  {quantity:7s} [{binning:8s}] {len(l):4d} cells: median {s[len(s) // 2]:.5f}, min {s[0]:.4f}, max {s[-1]:.4f}; |B/A-1| > 0.5% in {len(big)}")
        for x, cell in big[:4]:
            print(f"      {cell:24s} B/A {x:.4f}")


def quality(*paths):
    """Per file, section and binning: the fits, the flagged ones and why, the start that won, and the fits where the
    configured start lost by more than START_GAP."""
    for path in paths:
        groups = defaultdict(list)
        for r in table(path):
            groups[(r["section"], r["binning"])].append(r)
        for (section, binning), rows in sorted(groups.items()):
            reasons = Counter(reason for r in rows if flagged(r) for reason in r["reasons"].split("; "))
            lost = sum(r["start"] != "config" and float(r["dNLLconfig"]) > START_GAP for r in rows)
            print(f"== {os.path.basename(path)} {section} [{binning}]: {len(rows)} fits, {sum(map(flagged, rows))} flagged; "
                  f"starts {dict(Counter(r['start'] for r in rows))}; the configured start lost by > {START_GAP} in {lost}")
            if reasons:
                print("   reasons: " + ", ".join(f"{k} {v}" for k, v in reasons.most_common(12)))


if __name__ == "__main__":
    commands = {"runconfig": runconfig, "candidates": candidates, "select": select, "reproduce": reproduce,
                "compare": compare, "quality": quality}
    if len(sys.argv) < 2 or sys.argv[1] not in commands:
        sys.exit(f"Usage: selection.py {{{'|'.join(commands)}}} ... (see the head of this file)")
    commands[sys.argv[1]](*sys.argv[2:])
