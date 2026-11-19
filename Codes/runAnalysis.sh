#!/bin/bash

# Without pipefail the exit status of a pipeline is that of its LAST command,
# so piping ROOT into tee would always report success, even on a crash.
set -o pipefail

# Every path below - and every path inside the JSON configurations - is relative
# to this directory. Without this, they would be resolved against whatever
# directory the caller happens to be in, and the same command would work or fail
# depending on where it was typed.
cd "$(dirname "${BASH_SOURCE[0]}")" || exit 1

# One run at a time in this checkout: two runs share DataFile (mc writes Corrections/, which data reads) and the build
# directory. On 9 November 2026 several `--compile` runs started together, each rebuilding ../build, and some crashed
# at startup; started from one binary built once, 5 processes in the same second never did (19 November, 15 starts).
# The script runs itself again under lockf(1): the kernel lock goes with the process, so a run that dies leaves none.
LOCK_FILE="$PWD/../Logs/.runAnalysis.lock"
if [ -z "$RUNANALYSIS_HOLDS_LOCK" ]; then
    if ! command -v lockf > /dev/null; then
        echo "Error: lockf(1) not found. It comes with macOS; elsewhere this script cannot take its lock."
        exit 1
    fi
    mkdir -p ../Logs
    RUNANALYSIS_HOLDS_LOCK=1 lockf -s -k -t 0 "$LOCK_FILE" "$PWD/$(basename "${BASH_SOURCE[0]}")" "$@"
    STATUS=$?
    if [ "$STATUS" -eq 75 ]; then
        echo "Error: another runAnalysis.sh is running in this checkout ($(cat "$LOCK_FILE")). Wait for it to finish."
    fi
    exit "$STATUS"
fi
echo "$* (pid $$), started $(date '+%F %T')" > "$LOCK_FILE"

# 1. Parse execution flags and positional arguments
# A missing value would leave the option empty, and the run would do something else than asked: every group, one
# after the other.
need_value() {
    if [ -z "$2" ] || [[ "$2" == -* ]]; then
        echo "Error: $1 needs a value."
        exit 1
    fi
}

MODE=""
POSITIONAL_ARGS=()
GROUP=""
JOBS=""

while [ $# -gt 0 ]; do
    case "$1" in
        --compile|-c)
            MODE="compile"
            ;;
        --interpret|-i)
            MODE="interpret"
            ;;
        --group|-g)
            need_value "$1" "$2"; GROUP="$2"; shift
            ;;
        --jobs|-j)
            need_value "$1" "$2"; JOBS="$2"; shift
            ;;
        *)
            POSITIONAL_ARGS+=("$1")
            ;;
    esac
    shift
done

if [ "${#POSITIONAL_ARGS[@]}" -eq 0 ]; then
    echo "Error: No configuration keyword provided."
    echo "Usage: $0 <keyword> [coll_sys] [--compile | --interpret] [--group <name> | --jobs <N>]"
    echo "Available keywords: data, purity, mc, mcclosure, mcclosurewpdg, mcclosuregen, comparison"
    echo "Execution mode: --interpret / -i (default), --compile / -c"
    echo "Groups of active_tasks: --group / -g <name> runs one; --jobs / -j <N> runs them all, N processes at a time"
    exit 1
fi

# --jobs runs the compiled binary: the groups are separate processes of it.
if [ -n "$JOBS" ]; then
    if [ "$MODE" = "interpret" ] || [ -n "$GROUP" ] || ! [ "$JOBS" -ge 1 ] 2> /dev/null; then
        echo "Error: --jobs takes a number N >= 1, runs the compiled binary and every group: not with --interpret or --group."
        exit 1
    fi
    MODE="compile"
fi
MODE="${MODE:-interpret}"

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
# This removes the path and the .json extension. A group run alone gets a log of its own; the groups of a parallel run
# write theirs into the run directory (run_groups).
CONFIG_NAME=$(basename "$JSON_PATH" .json)
LOG_FILE="../Logs/log_output_${CONFIG_NAME}${GROUP:+_$GROUP}.log"
mkdir -p "$(dirname "$LOG_FILE")"


echo "=========================================================="
echo "Starting Phi-Strangeness analysis..."
echo "Keyword mapped : $KEYWORD"
echo "System         : $COLL_SYS"
echo "Execution mode : $MODE"
echo "Configuration  : $JSON_PATH"
echo "Base config    : ${JSON_PATH_BASE:-<none, single-file mode>}"
if [ -n "$JOBS" ]; then
    echo "Groups         : all, $JOBS at a time, one log each in ../Logs/run_${CONFIG_NAME}_<time>/"
else
    echo "Group          : ${GROUP:-<all groups, in order>}"
    echo "Log file       : $LOG_FILE"
fi
echo "=========================================================="

# Every group of active_tasks, JOBS processes at a time, judged as one run (--jobs). A group is a list of blocks that
# runs in one process, in order. Groups run in parallel must not share files: a group that opens a file another group
# writes gets a version that depends on which process got there first, with the right stamps and no error. Each
# process records the files it opens and its warnings, and the binary judges the records together after the run
# (--check-run), whatever the order the groups ran in; a wrong grouping is found at the end, not prevented.
# Bash 3.2 (macOS): no `wait -n`; it keeps the exit status of a finished job for `wait <pid>` (checked 19 November).
# Everything of one run is in RUN_DIR: the binary, copied so that a build started meanwhile does not give the later
# groups other code, and per group its log, record and summary. The provenance of each group still fingerprints the
# headers on disk when it starts: a header edited during the run makes it disagree with the binary.
run_groups() {
    RUN_DIR="../Logs/run_${CONFIG_NAME}_$(date +%Y%m%d_%H%M%S)"
    mkdir -p "$RUN_DIR" && cp "$BINARY" "$RUN_DIR/" || return 1
    local BIN="$RUN_DIR/phistrangeanalysis"
    local LIST
    if ! LIST=$("$BIN" "$JSON_PATH" "$JSON_PATH_BASE" --list-groups 2>&1); then
        echo "$LIST"
        echo "[ERROR] Cannot read the groups of $JSON_PATH."
        return 1
    fi
    local NAMES g
    NAMES=$(echo "$LIST" | sed -n 's/^GROUP //p')

    # The binaries themselves are the background jobs: a non-interactive shell starts them with SIGINT ignored, so
    # an interruption reaches them only through this trap.
    GROUP_PIDS=""
    trap 'kill $GROUP_PIDS 2> /dev/null; wait; echo "[ERROR] Interrupted: the running groups were stopped."; exit 130' INT TERM
    echo "Run directory  : $RUN_DIR"
    for g in $NAMES; do
        while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do
            sleep 2
        done
        "$BIN" "$JSON_PATH" "$JSON_PATH_BASE" --group "$g" --run-dir "$RUN_DIR" > "$RUN_DIR/$g.log" 2>&1 &
        GROUP_PIDS="$GROUP_PIDS $!"
        echo "  $(date +%H:%M:%S) started $g"
    done

    local FAILED=0 RC
    set -- $GROUP_PIDS
    {
        echo "=========================================================="
        echo "Groups of $CONFIG_NAME (logs: $RUN_DIR/<group>.log)"
        for g in $NAMES; do
            wait "$1"
            RC=$?
            shift
            printf "  %-14s exit %-3s %s\n" "$g" "$RC" "$(grep -m1 -o '\[FATAL\].*' "$RUN_DIR/$g.log" | cut -c1-150)"
            [ "$RC" -eq 0 ] || FAILED=1
        done
        echo "----------------------------------------------------------"
        # Without the lines of reading the configuration again.
        local CHECK
        CHECK=$("$BIN" "$JSON_PATH" "$JSON_PATH_BASE" --check-run "$RUN_DIR" 2>&1) || FAILED=1
        echo "$CHECK" | grep -v '^\[INFO\]'

        echo "=========================================================="
        if [ "$FAILED" -eq 0 ]; then
            echo " [SUCCESS] All groups completed."
        else
            echo " [FAILED] See above."
        fi
        echo "=========================================================="
    } > "$RUN_DIR/summary.txt"
    cat "$RUN_DIR/summary.txt"
    return "$FAILED"
}

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

    if [ -n "$JOBS" ]; then
        run_groups
        exit $?
    fi

    echo "Executing compiled binary: $BINARY \"$JSON_PATH\" \"$JSON_PATH_BASE\" ${GROUP:+--group $GROUP}"
    echo "=========================================================="
    "$BINARY" "$JSON_PATH" "$JSON_PATH_BASE" ${GROUP:+--group "$GROUP"} 2>&1 | tee "$LOG_FILE"
    STATUS=${PIPESTATUS[0]}
else
    echo "Executing interpreted ROOT macro: root -l -b -q 'PhiStrangenessCorrelation.C(\"$JSON_PATH\", \"$JSON_PATH_BASE\", \"$GROUP\")'"
    echo "=========================================================="
    root -l -b -q "PhiStrangenessCorrelation.C(\"$JSON_PATH\", \"$JSON_PATH_BASE\", \"$GROUP\")" 2>&1 | tee "$LOG_FILE"
    STATUS=${PIPESTATUS[0]}
fi

echo "=========================================================="
if [ "$STATUS" -ne 0 ]; then
    echo "Analysis FAILED (exit code $STATUS). Log saved in: $LOG_FILE"
else
    echo "Analysis completed. Log saved in: $LOG_FILE"
fi

exit "$STATUS"