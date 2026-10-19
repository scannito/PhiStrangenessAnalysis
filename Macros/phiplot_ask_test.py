#!/usr/bin/env python3
"""The requests phiplot_ask.py was measured on (PLOTTING.md, Testing the model front end).

Each request goes through Macros/phiplot_ask.py with --yes, and the specification it ends with is
checked against what the request meant: the right kind, species, |dy|, intervals, files and ratio.
Two requests ask for what the files do not hold, and are right only when nothing is drawn. A
model that changes, a prompt that changes or a schema that changes is re-measured with this.

Run from the repository root, with ROOT in the environment, build/phiplot built and the model
served (Ollama by default):

  python3 Macros/phiplot_ask_test.py --model qwen2.5-coder:7b --out /tmp/phiplot_test
  python3 Macros/phiplot_ask_test.py --model qwen2.5-coder:7b --out /tmp/phiplot_test --only 2,5

It reads the 2024 and 2026 offline and the 2024 online pp files under DataFile/; they are not in git,
so the results depend on those productions. Rerun what failed first (--only), then everything, as a
regression check, when the change touches what all requests share.
"""
import argparse
import json
import re
import subprocess
import sys
import time
from pathlib import Path

F = "DataFile/pp/DeltaY/FinalResults/"
A = F + "Data_OfflineEff_2024_PhiAssocSpectra.root"
B = F + "Data_OfflineEff_2026_PhiAssocSpectra.root"
C = F + "Data_OnlineEff_2024_PhiAssocSpectra.root"


def series(spec):
    return spec.get("series", [])


def all_(spec, **kv):
    return all(all(s.get(k) == v for k, v in kv.items()) for s in series(spec))


def dys(spec):
    return sorted(s.get("dy") for s in series(spec))


def files(spec):
    return [s.get("file") for s in series(spec)]


def one_dy(spec):
    return len({s.get("dy") for s in series(spec)}) == 1


EXIT_CANNOT = 3  # phiplot_ask.py: the model answered that the request cannot be drawn


TESTS = [
    ("trend dei K0S per |Δy| < 1, 2024 contro 2026, con il rapporto", [A, B],
     lambda s: all_(s, kind="trend", species="K0S", dy=1.0) and sorted(files(s)) == sorted([A, B]) and "ratio" in s
               and s["ratio"].get("divide_option") == ""),
    ("spettro della Xi in molteplicità 0-1%, per le tre finestre in Δy", [A],
     lambda s: all_(s, kind="spectrum", species="Xi", mult=[0, 1]) and dys(s) == [0.1, 0.5, 1.0] and "ratio" not in s),
    ("K0S/pi measured versus extrapolated, dy 0.5", [A],
     lambda s: len(series(s)) == 2 and {x.get("kind") for x in series(s)} == {"species_ratio_measured", "species_ratio_extrapolated"}
               and all_(s, species="K0S", species_den="Pi", dy=0.5)),
    ("Xi over Pi, online against offline efficiency, with a ratio panel", [A, C],
     lambda s: all_(s, kind="species_ratio_measured", species="Xi", species_den="Pi") and one_dy(s)
               and sorted(files(s)) == sorted([A, C]) and "ratio" in s and s["ratio"].get("divide_option") == ""),
    ("spettro dei pioni a molteplicità 10-15%, offline 2024 contro 2026", [A, B],
     lambda s: all_(s, kind="spectrum", species="Pi", mult=[10, 15]) and one_dy(s) and sorted(files(s)) == sorted([A, B])),
    ("rapporto estrapolato su misurato per la Xi, tutte e tre le finestre in Δy", [A],
     lambda s: all_(s, kind="trend_extrap_over_meas", species="Xi") and dys(s) == [0.1, 0.5, 1.0]),
    # log_y is on by default for spectra, so leaving it out is right; false is not.
    ("K0S spectrum at 0-1% and at 70-100% multiplicity, log scale, |dy| < 1", [A],
     lambda s: all_(s, kind="spectrum", species="K0S", dy=1.0) and s.get("log_y", True)
               and sorted(x.get("mult") for x in series(s)) == [[0, 1], [70, 100]]),
    ("Compare the measured K0S yield against multiplicity for 2024 and 2026, |dy|<1, dividing 2026 by 2024", [A, B],
     lambda s: all_(s, kind="trend", species="K0S", dy=1.0) and sorted(files(s)) == sorted([A, B]) and "ratio" in s
               and s["ratio"].get("divide_option") == "" and series(s)[s["ratio"]["reference"]].get("file") == A),
    # Impossible on purpose: what is asked does not exist.
    ("trend dei K0S per |Δy| < 0.3", [A], None),
    ("spettro delle Lambda a 0-1%", [A], None),
]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--model", required=True)
    ap.add_argument("--out", required=True, help="directory for the specifications, plots and logs")
    ap.add_argument("--only", default=None, help="comma-separated indices of the requests to run")
    ap.add_argument("ask_args", nargs="*", help="passed on to phiplot_ask.py after --, e.g. -- --api openai --endpoint URL")
    args = ap.parse_args()
    model, outdir = args.model, Path(args.out)
    outdir.mkdir(parents=True, exist_ok=True)
    missing = [f for f in (A, B, C) if not Path(f).exists()]
    if missing:
        sys.exit(f"[FATAL] phiplot_ask_test: run from the repository root; missing {', '.join(missing)}")
    results = []
    only = {int(x) for x in args.only.split(",")} if args.only else None
    for i, (request, fs, check) in enumerate(TESTS):
        if only is not None and i not in only:
            continue
        out = outdir / f"t{i}"
        for p in outdir.glob(f"t{i}.*"):
            p.unlink()
        cmd = [sys.executable, "-I", str(Path(__file__).resolve().parent / "phiplot_ask.py"), request, "--out", str(out),
               "--model", model, "--yes", *args.ask_args]
        for f in fs:
            cmd += ["--file", f]
        t0 = time.time()
        p = subprocess.run(cmd, capture_output=True, text=True)
        dt = time.time() - t0
        refusals = len(re.findall(r"attempt \d+ refused", p.stderr))
        spec_path = Path(f"{out}.spec.json")
        spec = json.loads(spec_path.read_text()) if spec_path.exists() else {}
        # Drawn: the run succeeded and wrote the .root, which every format leaves.
        rendered = p.returncode == 0 and Path(f"{out}.root").exists()
        if check is None:
            # Right only when the model said it cannot be done; any other failure (no endpoint,
            # no phiplot, retries exhausted) is not the model saying no.
            verdict = ("refused (right)" if p.returncode == EXIT_CANNOT
                       else "DRAWN SOMETHING ELSE" if rendered else f"FAILED (exit {p.returncode})")
        else:
            verdict = ("right" if check(spec) else "WRONG PLOT") if rendered else f"FAILED (exit {p.returncode})"
        results.append((i, verdict, refusals, dt))
        print(f"[{i}] {verdict:22s} refusals={refusals} {dt:5.1f}s  {request}", flush=True)
        (outdir / f"t{i}.log").write_text(p.stdout + "\n--- stderr ---\n" + p.stderr)
    if not results:
        sys.exit("[FATAL] phiplot_ask_test: --only selected no request")
    ok = sum(1 for r in results if r[1] in ("right", "refused (right)"))
    first = sum(1 for r in results if r[1] == "right" and r[2] == 0)
    print(f"\n{ok}/{len(results)} right; {first} right at the first attempt; mean {sum(r[3] for r in results) / len(results):.1f}s per request")


if __name__ == "__main__":
    main()
