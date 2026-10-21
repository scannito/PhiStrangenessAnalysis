#!/bin/bash
# The extrapolation study and its checks (README.md here): builds each program against the framework's
# headers, runs it on a 2024 output of the chain, and writes the tables, the figures and the logs to
# output/. Needs ROOT in the environment and the chain run on data pp (any configuration: the spectra the
# study fits do not depend on the extrapolation; the checks of the saved curves read the current output).
#
# Usage: run.sh [PhiAssocSpectra.root]
# The default input is the 2024 result that the comparison divides, and the binning is the comparison's, both read
# from the same place as ../run.sh reads them (globalConfigComparisonOld.json, paths relative to Codes/), so that the
# two cannot drift apart. The extrapolation configuration is the one the input records in its provenance.
set -e -o pipefail
here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
out="$here/output"

command -v root-config > /dev/null || { echo "[ERROR] ROOT is not in the environment (enterO2P)."; exit 1; }
read -r binning numerator < <(python3 -I -c '
import json, sys
c = json.load(open(sys.argv[1]))
print(c["workflow"]["global_binning"]["binning_name"], c["comparison_task_old_k0spi"]["comparisons"][0]["numerator"])' \
  "$here/../globalConfigComparisonOld.json")
input="${1:-$repo/Codes/$numerator}"
[ -f "$input" ] || { echo "[ERROR] $input is missing: run 'cd Codes && ./runAnalysis.sh data pp --compile' first."; exit 1; }
mkdir -p "$out/bin"

# With the chain's flags (CMakeLists.txt: Release, -Wall -Wextra -Wpedantic): checks/asTheChain compares numbers.
build() { # source, binary
  c++ -std=c++20 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -I"$repo/Codes" "$here/$1" $(root-config --cflags --libs) -o "$out/bin/$2" 2> "$out/build_$2.log" \
    || { cat "$out/build_$2.log"; echo "[ERROR] $1 does not build"; exit 1; }
}
# The framework's progress lines ([INFO], [DEBUG]) are left out of the tables. Its warnings and ROOT's errors go
# to <binary>.err with the rest of stderr, and are counted on the screen, so that none goes unseen.
run() { # binary, table, arguments...
  local binary="$1" table="$2"
  shift 2
  set +e
  "$out/bin/$binary" "$@" 2> "$out/$binary.err" | grep -v '^\[INFO\]\|^\[DEBUG\]' > "$out/$table"
  local status=${PIPESTATUS[0]}
  set -e
  [ "$status" -eq 0 ] || { cat "$out/$table" "$out/$binary.err"; echo "[ERROR] $binary exited with $status"; exit 1; }
  local warnings
  warnings=$(cat "$out/$binary.err" "$out/$table" | grep -c '^\[WARNING\]\|^Error in\|^Warning in' || true)
  [ "$warnings" -eq 0 ] || echo "   $binary: $warnings warning/error lines, in $out/$binary.err"
}
# A macro fails on its own Error() (a missing object); ROOT's other messages are counted like the programs' ones
# (integrating the read-back curves, which are piecewise linear, makes GSL report round-off).
draw() { # macro, input, figure
  root -l -b -q "$here/$1.C(\"$out/$2\",\"$out/$3\")" > "$out/$1.log" 2>&1 || { cat "$out/$1.log"; exit 1; }
  ! grep -q "^Error in <$1>" "$out/$1.log" || { grep "^Error in <$1>" "$out/$1.log"; echo "[ERROR] $1 failed"; exit 1; }
  local warnings
  warnings=$(grep -c '^Error in\|^Warning in' "$out/$1.log" || true)
  [ "$warnings" -eq 0 ] || echo "   $1: $warnings warning/error lines from ROOT, in $out/$1.log"
}

echo "== input: $input, binning $binning"
for p in modelScan twoFits checks/asTheChain checks/toyGaus checks/savedCurves checks/configHigh; do build "$p.cpp" "$(basename "$p")"; done

echo "== the models (output/modelScan.txt, output/pi_models.png)"
run modelScan modelScan.txt "$input" "$binning" "$out/modelScan.root"
draw drawModels modelScan.root pi_models.png

echo "== one fit or two (output/twoFits.txt, output/pi_two_fits.png)"
run twoFits twoFits.txt "$input" "$binning" "$out/twoFits.root"
draw drawTwoFits twoFits.root pi_two_fits.png

echo "== checks (output/checks_*.txt)"
run asTheChain checks_asTheChain.txt "$input" "$binning"
run toyGaus checks_toyGaus.txt "$input" "$binning"
run savedCurves checks_savedCurves.txt "$input" "$binning" "$out/savedCurves_scratch.root"
run configHigh checks_configHigh.txt "$out"
grep -h "PASS\|FAIL" "$out"/checks_*.txt | grep -v "^ok\|^     "
