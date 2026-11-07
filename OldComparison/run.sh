#!/bin/bash
# Remakes the old K0S/pi ratio from its own inputs and draws the framework's 2024 ones against it, with the ratios and
# their table (OldComparison/README.md). Needs ROOT in the environment and the 2024 results of the chain.
set -e -o pipefail
here="$(cd "$(dirname "$0")" && pwd)"
repo="$(dirname "$here")"
spec="$here/preliminary.json"
drawn="$here/output/preliminary.drawn.json"

command -v root-config > /dev/null || { echo "[ERROR] ROOT is not in the environment (enterO2P)."; exit 1; }
[ -f "$here/data/ResultsData2.root" ] || { echo "[ERROR] the old inputs are not in $here/data (README.md)."; exit 1; }
mkdir -p "$here/output"

# The framework's files, from the specification (the one place they are written, paths relative to the repository),
# each with its provenance for the scheme the plot reads (Macros/ShowProvenance.C): when and from which commit it was
# produced, since the comparison is only as current as these files, and with which apply_purity. The three must agree
# on it (the old yields come from fits, so like for like is with the purity; README.md), and the setting goes into the
# legend's header of the specification drawn, kept in output/ and, as the plot's recipe, in its .root.
echo "== the 2024 files"
python3 -I - "$spec" "$repo" "$drawn" << 'EOF'
import json, os, subprocess, sys
spec_path, repo, drawn = sys.argv[1:4]
spec = json.load(open(spec_path))
settings = set()
for series in spec["series"]:
    name, path = os.path.basename(series["file"]), os.path.join(repo, series["file"])
    if not os.path.isfile(path):
        sys.exit("[ERROR] %s is missing: run the three 2024 correlation tasks first (README.md)." % series["file"])
    run = subprocess.run(["root", "-l", "-b", "-q", '%s/Macros/ShowProvenance.C("%s","json")' % (repo, path)], capture_output=True, text=True)
    sys.stderr.write(run.stderr)
    lines = run.stdout.splitlines()
    if run.returncode != 0 or "[" not in lines or "]" not in lines:
        sys.exit("[ERROR] ShowProvenance.C failed on %s." % name)
    # The JSON array the macro prints, between its own "[" and "]" lines.
    entries = json.loads("\n".join(lines[lines.index("["):lines.index("]") + 1]))
    mine = [e for e in entries if e["scheme"] == series["scheme"]]
    applied = (mine[0]["config_block"] or {}).get("apply_purity") if len(mine) == 1 else None
    if applied is None:
        sys.exit("[ERROR] %s records no provenance, or no apply_purity, for '%s': re-run the chain." % (name, series["scheme"]))
    settings.add(applied)
    print("%s: produced %s, commit %s, apply_purity %s" % (name, mine[0]["produced_at"], mine[0]["git_commit"] or "unknown", str(applied).lower()))
if len(settings) != 1:
    sys.exit("[ERROR] the files disagree on apply_purity: re-run the three tasks with one setting.")
spec["legend_header"] += ", purity " + ("applied" if settings.pop() else "not applied")
json.dump(spec, open(drawn, "w"), indent=1)
EOF

fail() { grep -h "FATAL" "$1" || true; echo "[ERROR] $2 failed: see $1"; exit 1; }

# The macro writes next to its inputs (and overwrites one systematics file), so it runs on a copy;
# ResultsData2/ is where it saves its per-species canvases.
work="$here/output/work"
rm -rf "${work:?}"
mkdir -p "$work/ResultsData2"
cp -cR "$here/data/." "$work/" 2> /dev/null || cp -R "$here/data/." "$work/"

echo "== the old ratio, remade (output/export.log)"
(cd "$work" && root -l -b -q "$here/macro/ratioK0SPiCorrectedUpdated_export.C(0)") > "$here/output/export.log" 2>&1 \
  || fail "$here/output/export.log" "the old macro"
mv "$work/oldK0SPiRatio.root" "$here/output/"

echo "== the plot and its table (output/overlay.*, output/table.txt)"
(cd "$repo" && cmake --build build --target phistrangeplot > "$here/output/build.log" 2>&1) || fail "$here/output/build.log" "building phistrangeplot"
(cd "$repo" && build/phistrangeplot render "$drawn" --out "$here/output/overlay" --table "$here/output/table.txt") \
  > "$here/output/plot.log" 2>&1 || fail "$here/output/plot.log" "phistrangeplot"
cat "$here/output/table.txt"
