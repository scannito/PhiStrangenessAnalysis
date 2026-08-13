#!/bin/bash

# Without pipefail the exit status of a pipeline is that of its LAST command,
# so piping ROOT into tee would always report success, even on a crash.
set -o pipefail

# Every path below - and every path inside the JSON configurations - is relative
# to this directory. Without this, they would be resolved against whatever
# directory the caller happens to be in, and the same command would work or fail
# depending on where it was typed.
cd "$(dirname "${BASH_SOURCE[0]}")" || exit 1

# 1. Check if at least one argument is provided
if [ "$#" -eq 0 ]; then
    echo "Error: No configuration keyword provided."
    echo "Usage: $0 <keyword>"
    echo "Available keywords: data, purity, mc, mcclosure, mcclosurewpdg, mcclosuregen"
    exit 1
fi

KEYWORD=$1
COLL_SYS=$2
JSON_PATH=""

# The base configuration is the same whatever is being run: every named mode
# inherits its shared blocks from it. It used to be repeated inside each branch
# below, and three of the six forgot it - which is why those three carried their own
# copies of blocks that already existed here, free to drift apart.
#
# Set here and cleared only by the fallback: a JSON handed to this script by path is
# meant to stand alone.
JSON_PATH_BASE="../JSONConfigs/globalConfigBase${COLL_SYS}.json"

# 2. Map the keyword to the specific JSON configuration file
case "$KEYWORD" in
    "data")
        JSON_PATH="../JSONConfigs/globalConfigData${COLL_SYS}.json"
        ;;
    "purity")
        JSON_PATH="../JSONConfigs/globalConfigPurity${COLL_SYS}.json"
        ;;
    "mc")
        JSON_PATH="../JSONConfigs/globalConfigMC${COLL_SYS}.json"
        ;;
    "mcclosure")
        JSON_PATH="../JSONConfigs/globalConfigMCClosure${COLL_SYS}.json"
        ;;
    "mcclosurewpdg")
        JSON_PATH="../JSONConfigs/globalConfigMCClosureWPDG${COLL_SYS}.json"
        ;;
    "mcclosuregen")
        JSON_PATH="../JSONConfigs/globalConfigMCClosureGen${COLL_SYS}.json"
        ;;
    *)
        # Fallback: if the keyword is not recognized, try to use it directly as a
        # filename/path. No base: a configuration passed this way is self-contained,
        # which is the point of being able to pass one - a quick test inheriting from
        # nothing. WorkflowManager runs single-file when the base is empty.
        JSON_PATH_BASE=""
        if [[ "$KEYWORD" == *".json" ]]; then
            JSON_PATH="$KEYWORD"
        else
            JSON_PATH="../JSONConfigs/${KEYWORD}${COLL_SYS}.json"
        fi
        ;;
esac

# 3. Check that the mapped JSON files actually exist
if [ ! -f "$JSON_PATH" ]; then
    echo "Error: The JSON file '$JSON_PATH' does not exist!"
    exit 1
fi

# Only when one is expected. An empty base means single-file mode, which is a
# legitimate way to run - the fallback branch above uses it - so what is checked is
# that a base which was ASKED for is there, not that there must be one. Failing here
# names the missing file before ROOT has started.
if [ -n "$JSON_PATH_BASE" ] && [ ! -f "$JSON_PATH_BASE" ]; then
    echo "Error: The base JSON file '$JSON_PATH_BASE' does not exist!"
    echo "The named run modes inherit their shared blocks from it."
    exit 1
fi

# 4. Extract a clean name for the log file and set it up
# This removes the path and the .json extension
CONFIG_NAME=$(basename "$JSON_PATH" .json)
LOG_FILE="../Logs/log_output_${CONFIG_NAME}.log"
mkdir -p "$(dirname "$LOG_FILE")"

echo "=========================================================="
echo "Starting ROOT analysis..."
echo "Keyword mapped : $KEYWORD"
echo "System         : $COLL_SYS"
echo "Configuration  : $JSON_PATH"
echo "Base config    : ${JSON_PATH_BASE:-<none, single-file mode>}"
echo "Log file       : $LOG_FILE"
echo "=========================================================="

# Print the exact command that will be executed
echo "Executing: root -l -b -q 'PhiStrangenessCorrelation.C(\"$JSON_PATH\", \"$JSON_PATH_BASE\")' 2>&1 | tee \"$LOG_FILE\""
echo "=========================================================="

# 5. Execute the ROOT macro and pipe both stdout and stderr to tee
root -l -b -q "PhiStrangenessCorrelation.C(\"$JSON_PATH\", \"$JSON_PATH_BASE\")" 2>&1 | tee "$LOG_FILE"
STATUS=${PIPESTATUS[0]}

echo "=========================================================="
if [ "$STATUS" -ne 0 ]; then
    echo "Analysis FAILED (exit code $STATUS). Log saved in: $LOG_FILE"
else
    echo "Analysis completed. Log saved in: $LOG_FILE"
fi

exit "$STATUS"