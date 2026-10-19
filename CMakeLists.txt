cmake_minimum_required(VERSION 3.20)
project(PhiStrangenessAnalysis CXX)

# Default to Release build type if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose build type (Debug, Release, RelWithDebInfo)" FORCE)
endif()

# If running inside an alienv environment, prioritize ROOTSYS
if(DEFINED ENV{ROOTSYS})
  list(PREPEND CMAKE_PREFIX_PATH "$ENV{ROOTSYS}")
endif()

# Find ROOT with required components
find_package(ROOT QUIET COMPONENTS Core RIO Hist Tree Physics Matrix Graf Gpad RooFit RooFitCore)
if(NOT ROOT_FOUND)
  message(FATAL_ERROR
    "ROOT not found. Load an environment providing it (e.g. `alienv enter ROOT/latest`) "
    "or pass -DROOT_DIR=<ROOT>/cmake. ROOTSYS is "
    "'$ENV{ROOTSYS}'.")
endif()

# Use the same C++ standard ROOT was built with: headers and ABI must agree.
# Fall back to C++20 for ROOT versions that do not export it.
if(DEFINED ROOT_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD ${ROOT_CXX_STANDARD})
else()
  set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Executable target
add_executable(phistrangeanalysis Codes/main.cpp)

target_include_directories(phistrangeanalysis PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Codes
)

# Imported ROOT targets carry their own include directories (as SYSTEM, so ROOT
# headers do not flood the log with warnings) and their transitive dependencies.
target_link_libraries(phistrangeanalysis PRIVATE
  ROOT::Core ROOT::RIO ROOT::Hist ROOT::Tree ROOT::Physics ROOT::Matrix
  ROOT::Graf ROOT::Gpad ROOT::RooFit ROOT::RooFitCore
)

# Plots on demand from the correlation outputs (PLOTTING.md). Not part of
# the analysis chain: no RooFit, and runAnalysis.sh builds it only because it builds all.
add_executable(phiplot Codes/phiplot.cpp)
target_include_directories(phiplot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Codes)
# Where the sources are, for the commit and fingerprint in a plot's provenance (phiplot.cpp, CodeFacts).
target_compile_definitions(phiplot PRIVATE PHIPLOT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(phiplot PRIVATE ROOT::Core ROOT::RIO ROOT::Hist ROOT::Graf ROOT::Gpad)
