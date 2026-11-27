#!/bin/bash
# Runs the O2Physics phi-strangeness chain (services, phiStrangeCorrelator, phiStrangeCorrelation) on a local AO2D.
# The configuration is configs/base.json, the same in every mode (cuts, binnings, services), merged with
# configs/<mode>.json (what runs: the process switches and, in MC, the MC devices). Needs the O2Physics environment.

if [ $# -lt 2 ]; then
  cat << 'EOF'
usage: runPhiStrangeCorr.sh <mode> <AO2D file | @file list> [label]
  <mode>   data or mc: configs/<mode>.json, merged with configs/base.json
  [label]  added to the name of the output directory, to tell apart runs of one mode
Writes into a new directory <mode>[-<label>] of the current one: AnalysisResults.root, the configuration
passed (configuration.json), the one DPL used (dpl-config.json) and log.txt.
The entries of a file list must be absolute paths.
EOF
  exit 1
fi

HERE=$(cd "$(dirname "$0")" && pwd)
MODE=$1
NAME=$MODE${3:+-$3}
OVERLAY="$HERE/configs/$MODE.json"
if [ "$MODE" = base ] || [ ! -f "$OVERLAY" ]; then
  echo "No mode $MODE: configs/ has $(cd "$HERE/configs" && ls | grep -v '^base.json$' | sed 's/\.json$//' | tr '\n' ' ')"
  exit 1
fi
if ! command -v o2-analysis-lf-phi-strange-correlation > /dev/null; then
  echo "The O2Physics environment is not loaded"
  exit 1
fi

# The input made absolute: the run goes into a directory of its own
INPUT=$2
LIST=${INPUT#@}
if [ ! -f "$LIST" ]; then
  echo "No input $LIST"
  exit 1
fi
LIST="$(cd "$(dirname "$LIST")" && pwd)/$(basename "$LIST")"
if [ "${INPUT:0:1}" = @ ]; then
  INPUT="@$LIST"
else
  INPUT=$LIST
fi

if ! mkdir "$NAME" 2> /dev/null; then
  echo "$NAME already exists here: move it, or give another label"
  exit 1
fi
cd "$NAME" || exit 1
jq -s '.[0] * .[1]' "$HERE/configs/base.json" "$OVERLAY" > configuration.json || exit 1

# The two converters bring the AO2D's tables to the data model the tasks read (TracksExtra v001 -> v002, and in MC
# McCollisions): an input of another version may need other converters
DEVICES="o2-analysis-event-selection-service o2-analysis-propagationservice o2-analysis-ft0-corrected-table
o2-analysis-trackselection o2-analysis-tracks-extra-v002-converter o2-analysis-pid-tpc-service
o2-analysis-pid-tof-merge o2-analysis-multcenttable"
# The MC devices run when the configuration sets them up
if jq -e 'has("mc-centrality")' configuration.json > /dev/null; then
  DEVICES="$DEVICES o2-analysis-mccollision-converter o2-analysis-lf-mc-centrality"
fi
DEVICES="$DEVICES o2-analysis-lf-phi-strange-correlator o2-analysis-lf-phi-strange-correlation"

printf -v CFG %q "json://$PWD/configuration.json"
printf -v AOD %q "$INPUT"
OPTION="-b --configuration $CFG --aod-memory-rate-limit 2147483648 --shm-segment-size 8589934592"
PIPELINE=""
for device in $DEVICES; do
  PIPELINE="$PIPELINE${PIPELINE:+ | }$device $OPTION"
done
PIPELINE="$PIPELINE --aod-file $AOD"

echo "Running $MODE on $INPUT, output in $NAME/"
eval "$PIPELINE" > log.txt 2>&1
rc=$?
if [ $rc -ne 0 ]; then
  echo "Failed with exit code $rc: see $NAME/log.txt"
  exit $rc
fi

# DPL ignores a key that no device declares, and runs an option the configuration does not name at its default:
# after a rename in O2Physics both happen with no message. The options of every device, as passed and as used.
optionPaths() {
  jq -r 'del(.[keys[] | select(startswith("internal-dpl"))])
         | paths(type != "object" or has("values"))
         | select(all(.[]; type == "string")) | select(.[-1] | IN("values", "labels_rows", "labels_cols") | not)
         | join(".")' "$1" | sort
}
MISMATCH=$(diff <(optionPaths configuration.json) <(optionPaths dpl-config.json) | grep '^[<>]')
if [ -n "$MISMATCH" ]; then
  echo "The run is done, but its configuration does not match the options of this build"
  echo "('<' only in the configuration, ignored; '>' only in the build, run at its default):"
  echo "$MISMATCH"
  echo "Update configs/ (O2Workflow/README.md) before using this output."
  exit 2
fi
echo "Done: $NAME/AnalysisResults.root"
