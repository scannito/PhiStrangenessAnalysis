// Built with the analysis (CMakeLists.txt) and linked into nothing: no task includes the plot components, so without
// this a change to CanvasManager, RootIO or OutputNames that breaks them would pass the build and stop the next plot
// (CLAUDE.md, Cling constraints: a header nobody builds hides its errors until it is used).
#include "PlotDesigns.h"
#include "PlotSummaries.h"
