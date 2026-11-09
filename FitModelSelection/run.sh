#!/bin/bash
# The fit-model selection of one data period (README.md): the period's purity task run once per candidate model and
# once with the reference configuration, the selection, the selected configuration run and checked, and the period's
# three phi fits run with it. The results go to output/<period>/; outside it only the framework's own by-products
# change: a log per run in Logs/ (log_output_fitsel_<period>_*.log) and build/, rebuilt if stale. The tracked
# configurations and DataFile/ are not touched. Needs ROOT in the environment.
#
# Usage: run.sh <period> <purity block> <phi block offline> <phi block online> <phi block online StrInj> <reference fitConfig>
# e.g.   run.sh 2026_LowIRag purity_task_2026_lowIRag_updated phi_fit_task_offline_efficiency_2026_lowIRag_updated \
#          phi_fit_task_online_efficiency_2026_lowIRag_updated phi_fit_task_online_efficiency_2026_strange_injected_lowIRag_updated \
#          JSONConfigs/fitConfig_2026_HighIRac.json
set -e -o pipefail
here="$(cd "$(dirname "$0")" && pwd)"
repo="$(dirname "$here")"
[ $# -eq 6 ] || { echo "Usage: run.sh <period> <purity block> <phi block offline> <phi block online> <phi block online StrInj> <reference fitConfig>"; exit 2; }
period=$1 purity=$2 phiOffline=$3 phiOnline=$4 phiStrInj=$5
# a name, not a path: output/<period> is emptied below
[[ "$period" =~ ^[A-Za-z0-9_]+$ ]] || { echo "[ERROR] the period '$period' must be letters, digits and _."; exit 2; }
reference="$(cd "$(dirname "$6")" && pwd)/$(basename "$6")"
command -v root-config > /dev/null || { echo "[ERROR] ROOT is not in the environment (enterO2P)."; exit 1; }

out="$here/output/$period"
scan="$out/scan"
runs="$out/runs"
rm -rf "${out:?}"
mkdir -p "$scan" "$runs" "$here/output/bin"
step() { python3 -I "$here/selection.py" "$@"; }

c++ -O2 -Wall -Wextra -Wpedantic -I"$repo/Codes" "$here/dump.cpp" $(root-config --cflags --libs) -o "$here/output/bin/dump" \
  2> "$here/output/bin/build.log" || { cat "$here/output/bin/build.log"; echo "[ERROR] dump.cpp does not build"; exit 1; }

# One configuration per call, named after the period so that its log (Logs/log_output_<name>.log) is its own; the
# binning_name the runs write under is the one of that configuration.
run() { # kind, block, name, runs...
  local kind=$1 block=$2 name="fitsel_${period}_$3"
  shift 3
  binning=$(step runconfig "$kind" "$block" "$runs" "$out/$name.json" "$@")
  (cd "$repo/Codes" && ./runAnalysis.sh "$out/$name.json" --compile) > "$out/$name.out" 2>&1 \
    || { grep -h "FATAL" "$out/$name.out" || true; echo "[ERROR] $name failed: see $out/$name.out"; exit 1; }
  echo "   $name done"
}
dump() { # purity|phi, run name, directory of the tables (quality_<run>.tsv, values_<run>.tsv)
  "$here/output/bin/dump" "$1" "$runs" "$2" "$binning" "$3/quality_$2.tsv" "$3/values_$2.tsv"
}

echo "== $period: the candidates and the reference ($purity)"
candidates=$(step candidates "$reference" "$scan")
run Purity "$purity" scan reference="$reference" $(for c in $candidates; do echo "$c=$scan/$c.json"; done)
for r in reference $candidates; do dump purity "$r" "$scan"; done

echo "== the selection (selection.txt, fitConfig_selected.json)"
step select "$scan" "$reference" "$out/fitConfig_selected.json" | tee "$out/selection.txt"
run Purity "$purity" selected selected="$out/fitConfig_selected.json"
dump purity selected "$scan"
step reproduce "$scan" "$scan/quality_selected.tsv" | tee "$out/reproduce.txt"
step compare "$scan"/{quality_reference,quality_selected,values_reference,values_selected}.tsv | tee "$out/selected_vs_reference.txt"

echo "== the phi fits with the selected configuration (phi_quality.txt)"
run Data "$phiOffline" phi_offline phi_offline="$out/fitConfig_selected.json"
run Data "$phiOnline" phi_online phi_online="$out/fitConfig_selected.json"
run Data "$phiStrInj" phi_online_strinj phi_online_strinj="$out/fitConfig_selected.json"
for p in phi_offline phi_online phi_online_strinj; do dump phi "$p" "$out"; done
step quality "$out"/quality_phi_*.tsv | tee "$out/phi_quality.txt"
