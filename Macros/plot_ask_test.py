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
# A file the framework did not write, standing for a preliminary: the offline K0S trend copied under a path of its own,
# made by the test in its output directory (make_preliminary).
PRELIM_PATH = "Preliminary/hK0SYieldPreliminary"


def make_preliminary(outdir):
    path = Path(outdir) / "preliminary_K0S.root"
    code = (f'TFile f("{OFF}"); auto* h = dynamic_cast<TH1*>(f.Get("MoreFullDifferential/Extract1D/K0S/Trends/h1MultTrend_K0S_dy1_00")); '
            f'if (!h) exit(1); h->SetDirectory(0); TFile o("{path}", "RECREATE"); o.mkdir("Preliminary")->cd(); h->Write("hK0SYieldPreliminary");')
    # The file, not ROOT's exit code, says it worked: with -e ROOT exits with what the last expression returned.
    subprocess.run(["root", "-l", "-b", "-q", "-e", code], capture_output=True)
    if not path.exists():
        sys.exit("[FATAL] plot_ask_test: could not make the stand-in preliminary (ROOT in the environment?)")
    return str(path)


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


def tests(prelim):
  return [
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
    # One per design added in G4 (4 November 2026), written before the model was run on them.
    ("purezze di k0s, xi e omega nella classe 0-1%, un pannello per particella", [PUR],
     lambda s: s.get("design") == "grid" and sorted(p["series"][0].get("purity_key") for p in s.get("panels", [])) == ["k0s", "omega", "xi"]
               and all(x.get("mult") == [0, 1] for p in s["panels"] for x in p["series"])),
    ("K0S/pi measured ratio for all three |dy|, offline and online side by side with one common y axis", [OFF, ON],
     lambda s: s.get("design") == "joined_pads" and len(s.get("panels", [])) == 2
               and sorted(p["series"][0].get("file") for p in s["panels"]) == sorted([OFF, ON])
               and all(x.get("of") == "species_ratio" and x.get("numerator") == "K0S" and x.get("trend") == "measured"
                       and x.get("dy") in ("all", [1.0, 0.5, 0.1], [0.1, 0.5, 1.0]) for p in s["panels"] for x in p["series"])),
    (f"the online K0S trend at |dy| < 1 against the preliminary {PRELIM_PATH} in {prelim}, with its ratio", [ON],
     lambda s: s.get("design") == "with_reference" and s.get("reference_object", {}).get("file") == prelim
               and s["reference_object"].get("path") == PRELIM_PATH
               and all_(s, of="trend", species="K0S", dy=1.0, file=ON)),
    ("i fit di massa dei k0s nella classe 15-20%, per pT 0.8-1.2 e 1.2-1.6", [PUR],
     lambda s: s.get("design") == "fit_canvas_grid" and s.get("purity_key") == "k0s"
               and sorted(map(str, ((c.get("mult"), c.get("pt")) for c in s.get("cells", [])))) == sorted(map(str, [([15, 20], [0.8, 1.2]), ([15, 20], [1.2, 1.6])]))),
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
    for i, (request, fs, check) in enumerate(tests(make_preliminary(outdir))):
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
