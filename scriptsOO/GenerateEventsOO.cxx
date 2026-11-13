#include "loadTheFist.h"

using namespace thermalfist;

constexpr int kNumberOfEvents = 4;
constexpr double kVolVsMult = 2.4;
constexpr double kVolOffset = 0.;

constexpr double kMultCharged[] = {101.04226, 73.40364, 55.57322, 41.29956, 29.80835, 20.78430, 14.07475, 8.35581};
constexpr double kCentClasses[] = {0., 10., 20., 30., 40., 50., 60., 70., 80.};

constexpr int kNSample = 1;

constexpr int kPrPDG = 2212;

// NEW: eta cuts definition
constexpr int kNEtaCuts = 20;
double kEtaCuts[kNEtaCuts];

constexpr double kPrPtRange[] = {0.5, 1.5};

void GenerateEventsOO(const int kMultClass = 3, const double kCorrVolume = 3., const int kSample = 0)
{

  // initialize eta cuts: 0.05 → 1.00
  for (int i = 0; i < kNEtaCuts; ++i)
    kEtaCuts[i] = 0.05 * (i + 1);

  TH1D hRap("hRap", ";#it{y};Counts", 200, -10, 10);

  gRandom->SetSeed(kSample);
  RandomGenerators::SetSeed(gRandom->Integer(10000));

  TStopwatch clock;
  clock.Start();

  ThermalParticleSystem parts("Thermal-FIST/input/list/PDG2020/list.dat");

  //  ThermalModelCanonical *model = new ThermalModelCanonical(&parts);
  ThermalModelParameters params;

  params.muB = 0.0;
  params.muQ = 0.0;
  params.muS = 0.0;
  params.gammaS = 1. - 0.25 * exp(-kMultCharged[kMultClass] / 59.);
  params.gammaq = 1.0;
  params.T = 0.176 - 0.0026 * log(kMultCharged[kMultClass]);

  params.B = params.Q = params.S = 0;
  //  model->SetParameters(params);

  double volume = kCorrVolume * (kVolVsMult * kMultCharged[kMultClass] + kVolOffset);
  //  model->SetVolume(volume);
  //  model->SetCanonicalVolume(volume);

  //  model->ConserveBaryonCharge(true);
  //  model->ConserveElectricCharge(false);
  //  model->ConserveStrangeness(false);
  // test BQS for paper
  // model->ConserveElectricCharge(true);
  //  model->ConserveStrangeness(true);

  //  model->SetStatistics(1);
  //  model->CalculateQuantumNumbersRange(true);
  //  model->SetUseWidth(ThermalParticle::eBW);
  //  model->FillChemicalPotentials();

  EventGeneratorConfiguration configMC;
  configMC.fModelType = EventGeneratorConfiguration::PointParticle;
  configMC.fEnsemble = EventGeneratorConfiguration::CE;
  // configMC.B = //model->Parameters().B;
  // configMC.Q = //model->Parameters().Q;
  // configMC.S = //model->Parameters().S;
  configMC.CFOParameters.V = volume; // model->Parameters();
  // configMC.CFOParameters.V = configMC.CFOParameters.SVc = volume
  configMC.CFOParameters.T = params.T;
  configMC.CFOParameters.gammaS = params.gammaS;
  configMC.CFOParameters.gammaq = params.gammaq;
  configMC.CFOParameters.muB = 0.000;
  configMC.CFOParameters.muQ = 0.000;
  configMC.CFOParameters.muS = 0.000;
  configMC.CFOParameters.muC = 0.000;

  configMC.CanonicalB = true;
  configMC.CanonicalQ = false;
  configMC.CanonicalS = false;

  double beta_avg_BW = 0.68072 - 35.1217 / (kMultCharged[kMultClass] + 99.9306);
  double T_kin_BW = 0.0872556 + 7.98355 / (kMultCharged[kMultClass] + 91.4804);
  double n_BW = 0.681412 + 79.1691 / (kMultCharged[kMultClass] + 35.6361);

  for (int ip = 0; ip < parts.ComponentsNumber(); ++ip) {
    auto& part = parts.Particle(ip);

    // Boltzmann statistics throughout
    part.UseStatistics(false);

    // Strong and electromagnetic
    if (part.DecayType() == thermalfist::ParticleDecayType::Strong || part.DecayType() == thermalfist::ParticleDecayType::Electromagnetic)
      part.SetStable(false);
    else
      part.SetStable(true);

    // In any case, do not decay charged pions and kaons
    if (part.PdgId() == 211 || part.PdgId() == -211 || part.PdgId() == 321 || part.PdgId() == -321)
      part.SetStable(true);
  }

  double betaS = (2. + n_BW) / 2. * beta_avg_BW;
  CylindricalBlastWaveEventGenerator* generator =
    new CylindricalBlastWaveEventGenerator(&parts, configMC, T_kin_BW, betaS, kCorrVolume * 0.5, n_BW, 1.331);

  //  generator->RecalculateTotalConservedNumbers();

  //  double betaS = (2. + n_BW) / 2. * beta_avg_BW;
  //  generator->SetParameters(T_kin_BW, betaS, kCorrVolume * 0.5, n_BW);

  std::cout << beta_avg_BW << "\t" << T_kin_BW << "\t" << n_BW << "\t" << betaS << std::endl;

  for (int iS = 0; iS < kNSample; ++iS) {

    TFile out_file(Form("/data/mciacco/out/out_file_%d_s_%d.root", kMultClass, kSample), "recreate");

    // NEW: dynamic TNtuple definition
    std::string varlist = "mult";
    for (int i = 0; i < kNEtaCuts; ++i) {
      char buf[64];
      sprintf(buf, ":q1pp_%02d:q1pn_%02d", int(kEtaCuts[i] * 100), int(kEtaCuts[i] * 100));
      varlist += buf;
    }

    TNtupleD tuple("evts", "evts", varlist.c_str());

    for (int i = 0; i < kNumberOfEvents; ++i) {

      if (i % 1000 == 0)
        std::cout << "generated " << i << " events..." << std::endl;

      double q1pp[kNEtaCuts] = {0.};
      double q1pn[kNEtaCuts] = {0.};

      auto ev = generator->GetEvent(true);
      std::vector<SimpleParticle> part = ev.Particles;

      int nInAcc = 0;

      for (auto p : part) {

        if (p.GetPt() > 0.05 && std::abs(p.GetEta()) < 1 &&
            (std::abs(p.PDGID) == kPrPDG || std::abs(p.PDGID) == 321 ||
             std::abs(p.PDGID) == 211 || std::abs(p.PDGID) == 11 ||
             std::abs(p.PDGID) == 13))
          nInAcc++;

        if (std::abs(p.PDGID) == kPrPDG)
          hRap.Fill(p.GetY());

        if (std::abs(p.PDGID) == kPrPDG &&
            (p.GetPt() < kPrPtRange[0] || p.GetPt() > kPrPtRange[1]))
          continue;

        double absEta = std::abs(p.GetEta());

        switch (p.PDGID) {

          case kPrPDG: // proton
          {
            for (int iCut = 0; iCut < kNEtaCuts; ++iCut)
              if (absEta < kEtaCuts[iCut])
                q1pp[iCut] += 1;
            break;
          }

          case -kPrPDG: // anti-proton
          {
            for (int iCut = 0; iCut < kNEtaCuts; ++iCut)
              if (absEta < kEtaCuts[iCut])
                q1pn[iCut] += 1;
            break;
          }

          default:
            break;
        }
      }

      if (nInAcc > 0) {
        std::vector<double> values;
        values.push_back(kCentClasses[kMultClass]);

        for (int iCut = 0; iCut < kNEtaCuts; ++iCut) {
          values.push_back(q1pp[iCut]);
          values.push_back(q1pn[iCut]);
        }

        tuple.Fill(values.data());
      }
    }

    out_file.cd();
    tuple.Write();
    out_file.Close();
  }

  clock.Stop();
  clock.Print();

  TFile foutRap(Form("/data/mciacco/outOO/rapOut_%d_%d.root", kMultClass, kSample), "recreate");
  foutRap.cd();
  hRap.Write();
  foutRap.Close();
}
