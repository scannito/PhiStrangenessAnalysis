#!/bin/bash
# Compiles a plot composition (a .cpp written on PlotSeries.h and PlotDesigns.h) against Codes/ and ROOT, and runs it
# with the rest of the arguments: runplot.sh <composition.cpp> [arguments...]. Compiled, not interpreted: the
# interpreted mode does not start in this environment, and a throw from a check may not reach a catch under Cling
# (CLAUDE.md, Cling constraints). ROOT must be in the environment (enterO2P). The exit code is the composition's
# (PlotDesigns::Run: 0 done, 1 refused or failed, 3 not in the files); 2 is a wrong use of this script.
set -e
[ $# -ge 1 ] || { echo "usage: runplot.sh <composition.cpp> [arguments...]" >&2; exit 2; }
src=$1; shift
codes="$(cd "$(dirname "$0")/../Codes" && pwd)"
command -v root-config >/dev/null || { echo "[FATAL] runplot: ROOT is not in the environment (enterO2P)" >&2; exit 2; }
# The standard is ROOT's, as in CMakeLists.txt: headers and ABI must agree. Below C++20 the headers do not compile.
std=$(root-config --cflags | grep -o -- '-std=[^ ]*' || true)
case "$std" in
  *c++2[0-9]|*c++2[a-z]|*gnu++2[0-9]|*gnu++2[a-z]) ;;
  *) echo "[FATAL] runplot: ROOT's standard is '${std:-none}'; the plot components need C++20 or later" >&2; exit 2 ;;
esac
work=$(mktemp -d "${TMPDIR:-/tmp}/runplot.XXXXXX")
trap 'rm -rf "$work"' EXIT
c++ -O1 -Wall -Wextra -I"$codes" $(root-config --cflags) "$src" -o "$work/plot" $(root-config --libs)
"$work/plot" "$@"
