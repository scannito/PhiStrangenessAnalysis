#!/bin/bash
# Remakes the old K0S/pi ratio from its own inputs and compares it with the framework's 2024 one
# (OldComparison/README.md). Needs ROOT in the environment and the chain run on data pp.
set -e -o pipefail
here="$(cd "$(dirname "$0")" && pwd)"
repo="$(dirname "$here")"
config="$here/globalConfigComparisonOld.json"

command -v root-config > /dev/null || { echo "[ERROR] ROOT is not in the environment (enterO2P)."; exit 1; }
[ -f "$here/data/ResultsData2.root" ] || { echo "[ERROR] the old inputs are not in $here/data (README.md)."; exit 1; }

# The numerator as the comparison will divide it: from the configuration, the one place it is
# written (paths there are relative to Codes/, where the chain runs).
read -r label newFile newObject < <(python3 -I -c '
import json, sys
c = json.load(open(sys.argv[1]))["comparison_task_old_k0spi"]["comparisons"][0]
print(c["label"], c["numerator"], c["objects"][0]["numerator"])' "$config")
newFile="$repo/Codes/$newFile"
[ -f "$newFile" ] || { echo "[ERROR] $newFile is missing: run 'cd Codes && ./runAnalysis.sh data pp --compile' first."; exit 1; }

fail() { grep -h "FATAL" "$1" || true; echo "[ERROR] $2 failed: see $1"; exit 1; }
mkdir -p "$here/output"

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

echo "== comparison_task (output/comparison.log)"
# Through runAnalysis.sh, which rebuilds the binary when the sources are newer than it.
(cd "$repo/Codes" && ./runAnalysis.sh ../OldComparison/globalConfigComparisonOld.json --compile) > "$here/output/comparison.log" 2>&1 \
  || fail "$here/output/comparison.log" "comparison_task"
grep -E "divided|SUMMARY" "$here/output/comparison.log"

echo "== the table, and output/overlay.png"
root -l -b -q "$here/macro/show.C(\"$newFile\",\"$newObject\",\"$here/output/oldK0SPiRatio.root\",\"$here/output/Old_Comparisons.root\",\"$label\",\"$here/output/overlay.png\")" 2>&1 \
  | grep -v "^Info in\|^Processing\|^$"
