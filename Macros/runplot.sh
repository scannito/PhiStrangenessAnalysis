#!/bin/bash
# Compiles a plot composition (a .cpp written on PlotSeries.h and PlotDesigns.h) against Codes/ and ROOT, and runs it
# with the rest of the arguments: runplot.sh <composition.cpp> [arguments...]. Compiled, not interpreted: the
# interpreted mode does not start in this environment, and a throw from a check may not reach a catch under Cling
# (CLAUDE.md, Cling constraints). ROOT must be in the environment (enterO2P).
set -e
[ $# -ge 1 ] || { echo "usage: runplot.sh <composition.cpp> [arguments...]"; exit 2; }
src=$1; shift
codes="$(cd "$(dirname "$0")/../Codes" && pwd)"
command -v root-config >/dev/null || { echo "runplot: ROOT is not in the environment (enterO2P)"; exit 2; }
bin="${TMPDIR:-/tmp}/runplot_$(basename "${src%.*}")"
c++ -std=c++20 -O1 -Wall -Wextra -I"$codes" $(root-config --cflags | sed 's/-std=[^ ]*//') "$src" -o "$bin" $(root-config --libs)
exec "$bin" "$@"
