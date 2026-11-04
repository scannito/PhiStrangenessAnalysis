#!/usr/bin/env python3
"""The requests Macros/plot_ask.py is measured on: a regression test of the model, the prompt and the schema.

Each request goes through plot_ask.py with --yes, and the specification it ends with is checked against
what the request meant: the right design, kind of series, species, |dy|, classes, files and reference.
Two requests ask for what the files do not hold, and are right only when the model answers "cannot".
Before them, every example in PlotSpecs/examples is described by phistrangeplot: an example the
schema or the files no longer accept is shown to the model as a model answer, and fails here first.

Run from the repository root, with ROOT in the environment, build/phistrangeplot built and the model
served (Ollama by default):

  python3 Macros/plot_ask_test.py --out /tmp/plot_ask_test
  python3 Macros/plot_ask_test.py --out /tmp/plot_ask_test --only 2,5
  python3 Macros/plot_ask_test.py --out /tmp/plot_ask_test -- --api llama

It reads the 2026 offline and online pp spectra files under DataFile/, which are not in git and must
have been written with the records of what they hold (phistrangeplot list): the results depend on
those productions. A model, a prompt or a schema that changes is re-measured with all of it; the bar
set by phiplot on 19 October 2026 was 9 of 10 with qwen2.5-coder:7b.
"""
import argparse
import json
import re
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "Macros"))  # run with -I, which leaves the script's directory out
from plot_ask import EXIT_NOT_IN_FILES  # noqa: E402  the model answered that the request cannot be drawn
F = "DataFile/pp/DeltaY/FinalResults/"
OFF = F + "Data_OfflineEff_2026_HighIR_updated_PhiAssocSpectra.root"
ON = F + "Data_OnlineEff_2026_HighIR_updated_PhiAssocSpectra.root"
PUR = "DataFile/pp/DeltaY/Purities/Data_2026_HighIR_updated_Purity.root"
COR = "DataFile/pp/DeltaY/Corrections/2026_HighIR_updated_Corrections.root"


def series(spec):
    return spec.get("series", [])


def all_(spec, **kv):
    return bool(series(spec)) and all(all(s.get(k) == v for k, v in kv.items()) for s in series(spec))


def dys(spec):
    return sorted(s.get("dy") for s in series(spec))


def files(spec):
    return sorted(s.get("file") for s in series(spec))


def one_dy(spec):
    return len({s.get("dy") for s in series(spec)}) == 1


def ratio(spec, reference_file=None):
    if spec.get("design") != "compare_with_ratio" or spec.get("divide_option") != "":
        return False
    return reference_file is None or series(spec)[spec.get("reference", 0)].get("file") == reference_file


TESTS = [
    ("trend dei K0S per |Δy| < 1, offline contro online, con il rapporto", [OFF, ON],
     lambda s: all_(s, of="trend", species="K0S", dy=1.0, trend="measured") and files(s) == sorted([OFF, ON]) and ratio(s)),
    ("spettro della Xi in molteplicità 0-1%, per le tre finestre in Δy", [OFF],
     lambda s: all_(s, of="spectrum", species="Xi", mult=[0, 1]) and dys(s) == [0.1, 0.5, 1.0] and s.get("design") == "overlay"),
    ("K0S/pi measured versus extrapolated, dy 0.5", [OFF],
     lambda s: len(series(s)) == 2 and {x.get("trend") for x in series(s)} == {"measured", "extrapolated"}
               and all_(s, of="species_ratio", numerator="K0S", denominator="Pi", dy=0.5)),
    ("Xi over Pi, online against offline efficiency, with a ratio panel", [OFF, ON],
     lambda s: all_(s, of="species_ratio", numerator="Xi", denominator="Pi") and one_dy(s) and files(s) == sorted([OFF, ON]) and ratio(s)),
    ("spettro dei pioni a molteplicità 10-15%, offline contro online", [OFF, ON],
     lambda s: all_(s, of="spectrum", species="Pi", mult=[10, 15]) and one_dy(s) and files(s) == sorted([OFF, ON])),
    ("rapporto estrapolato su misurato per la Xi, tutte e tre le finestre in Δy", [OFF],
     lambda s: all_(s, of="trend", species="Xi", trend="extrap_over_meas") and dys(s) == [0.1, 0.5, 1.0]),
    ("K0S spectrum at 0-1% and at 70-100% multiplicity, log scale, |dy| < 1", [OFF],
     lambda s: all_(s, of="spectrum", species="K0S", dy=1.0) and s.get("log_y") is True
               and sorted(x.get("mult") for x in series(s)) == [[0, 1], [70, 100]]),
    ("Compare the measured K0S yield against multiplicity for online and offline efficiency, |dy|<1, dividing online by offline", [OFF, ON],
     lambda s: all_(s, of="trend", species="K0S", dy=1.0, trend="measured") and files(s) == sorted([OFF, ON]) and ratio(s, OFF)),
    # Impossible on purpose: what is asked does not exist.
    ("trend dei K0S per |Δy| < 0.3", [OFF], None),
    ("spettro delle Lambda a 0-1%", [OFF], None),
    # Added after the instructions and the examples were tuned on the ten above (4 November 2026), and
    # measured once before any further change: the check that the tuning did not just learn the ten.
    ("purezza degli omega nelle classi 10-15% e 40-50%", [PUR],
     lambda s: all_(s, of="purity", purity_key="omega") and sorted(x.get("mult") for x in series(s)) == [[10, 15], [40, 50]]),
    ("efficiency of the K0S integrated over multiplicity, and in the 0-1% class", [COR],
     lambda s: all_(s, of="correction_map", particle="K0S", map="efficiency") and sorted(str(x.get("mult")) for x in series(s)) == ["None", "[0, 1]"]),
    ("Omega spectrum at 70-100%, |dy| < 0.5, offline compared with online, divide online by offline", [OFF, ON],
     lambda s: all_(s, of="spectrum", species="Omega", dy=0.5, mult=[70, 100]) and files(s) == sorted([OFF, ON]) and ratio(s, OFF)),
]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", required=True, help="directory for the specifications, plots and logs")
    ap.add_argument("--only", default=None, help="comma-separated indices of the requests to run")
    ap.add_argument("ask_args", nargs="*", help="passed on to plot_ask.py after --, e.g. -- --api llama")
    args = ap.parse_args()
    outdir = Path(args.out)
    outdir.mkdir(parents=True, exist_ok=True)
    missing = [f for f in (OFF, ON, PUR, COR) if not Path(f).exists()]
    if missing:
        sys.exit(f"[FATAL] plot_ask_test: run from the repository root; missing {', '.join(missing)}")
    tool = str(REPO / "build" / "phistrangeplot")

    stale = [p.name for p in sorted((REPO / "PlotSpecs" / "examples").glob("*.json"))
             if subprocess.run([tool, "describe", str(p)], capture_output=True).returncode != 0]
    if stale:
        sys.exit(f"[FATAL] plot_ask_test: examples that phistrangeplot no longer accepts: {', '.join(stale)}")

    results = []
    only = {int(x) for x in args.only.split(",")} if args.only else None
    for i, (request, fs, check) in enumerate(TESTS):
        if only is not None and i not in only:
            continue
        out = outdir / f"t{i}"
        for p in outdir.glob(f"t{i}.*"):
            p.unlink()
        cmd = [sys.executable, "-I", str(REPO / "Macros" / "plot_ask.py"), request, "--out", str(out), "--yes", *args.ask_args]
        for f in fs:
            cmd += ["--file", f]
        t0 = time.time()
        p = subprocess.run(cmd, capture_output=True, text=True)
        dt = time.time() - t0
        refusals = len(re.findall(r"attempt \d+ refused", p.stderr))
        spec_path = Path(f"{out}.spec.json")
        spec = json.loads(spec_path.read_text()) if spec_path.exists() else {}
        # Drawn: the run succeeded and wrote the .root.
        rendered = p.returncode == 0 and Path(f"{out}.root").exists()
        if check is None:
            # Right only when the model said it cannot be done; any other failure (no endpoint, no
            # phistrangeplot, retries exhausted) is not the model saying no.
            verdict = ("refused (right)" if p.returncode == EXIT_NOT_IN_FILES
                       else "DRAWN SOMETHING ELSE" if rendered else f"FAILED (exit {p.returncode})")
        else:
            verdict = ("right" if check(spec) else "WRONG PLOT") if rendered else f"FAILED (exit {p.returncode})"
        results.append((i, verdict, refusals, dt))
        print(f"[{i}] {verdict:22s} refusals={refusals} {dt:5.1f}s  {request}", flush=True)
        (outdir / f"t{i}.log").write_text(p.stdout + "\n--- stderr ---\n" + p.stderr)
    if not results:
        sys.exit("[FATAL] plot_ask_test: --only selected no request")
    ok = sum(1 for r in results if r[1] in ("right", "refused (right)"))
    first = sum(1 for r in results if r[1] == "right" and r[2] == 0)
    print(f"\n{ok}/{len(results)} right; {first} right at the first attempt; mean {sum(r[3] for r in results) / len(results):.1f}s per request")


if __name__ == "__main__":
    main()
