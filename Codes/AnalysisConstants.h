#pragma once

#include <format>
#include <map>
#include <stdexcept>
#include <string>

// PDG masses, and nothing else. Anything that depends on how a production was
// binned, on how a fitter integrates, or on what the analysis chooses to do is
// configuration - it belongs to the file it describes or to the class that uses
// it, not here.
namespace AnalysisConstants
{
inline const std::map<std::string, double> kParticleMass{
  {"Phi", 1.019461},    // PDG mass of Phi meson in GeV/c^2
  {"K", 0.493677},      // PDG mass of charged kaon in GeV/c^2
  {"K0S", 0.497611},    // PDG mass of K0S in GeV/c^2
  {"Lambda", 1.115683}, // PDG mass of Lambda baryon in GeV/c^2
  {"Xi", 1.32171},      // PDG mass of Xi baryon in GeV/c^2
  {"Omega", 1.67245},   // PDG mass of Omega baryon in GeV/c^2
  {"Pi", 0.139570},     // PDG mass of charged pion in GeV/c^2
};

// An antiparticle has the mass of its particle (CPT): "AntiLambda" is looked up as "Lambda".
inline double GetMass(const std::string& name)
{
  const std::string particle = name.starts_with("Anti") ? name.substr(4) : name;
  auto it = kParticleMass.find(particle);
  if (it == kParticleMass.end())
    throw std::runtime_error(std::format("[FATAL] AnalysisConstants: Unknown particle '{}'", name));
  return it->second;
}

} // namespace AnalysisConstants
