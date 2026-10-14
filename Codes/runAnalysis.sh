#!/bin/bash

# Without pipefail the exit status of a pipeline is that of its LAST command,
# so piping ROOT into tee would always report success, even on a crash.
set -o pipefail

# Every path below - and every path inside the JSON configurations - is relative
# to this directory. Without this, they would be resolved against whatever
# directory the caller happens to be in, and the same command would work or fail
# depending on where it was typed.
cd "$(dirname "${BASH_SOURCE[0]}")" || exit 1

# 1. Parse execution flags and positional arguments
MODE="interpret"
POSITIONAL_ARGS=()

for arg in "$@"; do
    case "$arg" in
        --compile|-c)
            MODE="compile"
            ;;
        --interpret|-i)
            MODE="interpret"
            ;;
        *)
            POSITIONAL_ARGS+=("$arg")
            ;;
    esac
done

if [ "${#POSITIONAL_ARGS[@]}" -eq 0 ]; then
    echo "Error: No configuration keyword provided."
    echo "Usage: $0 <keyword> [coll_sys] [--compile | --interpret]"
    echo "Available keywords: data, purity, mc, mcclosure, mcclosurewpdg, mcclosuregen, comparison"
    echo "Execution mode: --interpret / -i (default), --compile / -c"
    exit 1
fi

KEYWORD="${POSITIONAL_ARGS[0]}"
COLL_SYS="${POSITIONAL_ARGS[1]:-}"
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
    "comparison")
        JSON_PATH="../JSONConfigs/globalConfigComparison${COLL_SYS}.json"
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
echo "Starting Phi-Strangeness analysis..."
echo "Keyword mapped : $KEYWORD"
echo "System         : $COLL_SYS"
echo "Execution mode : $MODE"
echo "Configuration  : $JSON_PATH"
echo "Base config    : ${JSON_PATH_BASE:-<none, single-file mode>}"
echo "Log file       : $LOG_FILE"
echo "=========================================================="

# 5. Execute analysis (compiled binary or interpreted ROOT macro)
if [ "$MODE" = "compile" ]; then
    BINARY="../build/phistrangeanalysis"
    BUILD_LOG="../Logs/build.log"
    mkdir -p "$(dirname "$BUILD_LOG")"
    : > "$BUILD_LOG"

    # cmake is the staleness detector: it knows the exact dependencies of every
    # file, so the build is a no-op when nothing changed and recompiles when a header
    # was edited. Running the binary only because it exists would silently execute a
    # stale version of the code. The output goes to the log and not to the screen;
    # it is shown only if the build fails, and the analysis is not launched then.
    # cmake does not notice a different ROOT on its own: the one it found is kept in
    # the cache and reused. If the environment now provides another, start from a
    # clean build directory (a full rebuild is a few seconds).
    if [ -f ../build/CMakeCache.txt ] && command -v root-config > /dev/null; then
        CACHED_ROOT=$(sed -n 's/^ROOT_DIR:[A-Z]*=//p' ../build/CMakeCache.txt)
        CURRENT_ROOT="$(root-config --prefix)/cmake"
        if [ "$CACHED_ROOT" != "$CURRENT_ROOT" ]; then
            echo "Build          : ROOT changed ($CACHED_ROOT -> $CURRENT_ROOT), recreating ../build"
            rm -rf ../build
        fi
    fi

    if [ ! -f ../build/CMakeCache.txt ]; then
        cmake -B ../build -S .. >> "$BUILD_LOG" 2>&1 || { cat "$BUILD_LOG"; echo "[ERROR] cmake configuration failed (log: $BUILD_LOG)"; exit 1; }
    fi
    cmake --build ../build >> "$BUILD_LOG" 2>&1 || { cat "$BUILD_LOG"; echo "[ERROR] Build failed (log: $BUILD_LOG)"; exit 1; }

    if grep -q "Building CXX" "$BUILD_LOG"; then
        echo "Build          : sources changed, binary rebuilt ($(grep -c 'warning:' "$BUILD_LOG") warnings, log: $BUILD_LOG)"
    else
        echo "Build          : binary up to date"
    fi

    echo "Executing compiled binary: $BINARY \"$JSON_PATH\" \"$JSON_PATH_BASE\""
    echo "=========================================================="
    "$BINARY" "$JSON_PATH" "$JSON_PATH_BASE" 2>&1 | tee "$LOG_FILE"
    STATUS=${PIPESTATUS[0]}
else
    echo "Executing interpreted ROOT macro: root -l -b -q 'PhiStrangenessCorrelation.C(\"$JSON_PATH\", \"$JSON_PATH_BASE\")'"
    echo "=========================================================="
    root -l -b -q "PhiStrangenessCorrelation.C(\"$JSON_PATH\", \"$JSON_PATH_BASE\")" 2>&1 | tee "$LOG_FILE"
    STATUS=${PIPESTATUS[0]}
fi

echo "=========================================================="
if [ "$STATUS" -ne 0 ]; then
    echo "Analysis FAILED (exit code $STATUS). Log saved in: $LOG_FILE"
else
    echo "Analysis completed. Log saved in: $LOG_FILE"
fi

exit "$STATUS"