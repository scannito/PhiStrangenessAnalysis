#include "TCanvas.h"
#include "TFile.h"
#include "TH1D.h"
#include "TH2D.h"
#include "THnSparse.h"
#include "TLegend.h"
#include "TString.h"
#include "TStyle.h"

#include <algorithm>
#include <iostream>

void TestProjections(int mode)
{
  std::cout << "[INFO] Starting TestProjections for Online Efficiency..." << std::endl;

  // =========================================================================
  // 1. Path Configuration
  // =========================================================================
  TString dataFileName;

  if (mode == 0) {
    dataFileName = "../DataFile/pp/DeltaY/Data/AnalysisResults_136_7maggio.root";
  } else if (mode == 1) {
    dataFileName = "../DataFile/pp_ref/DeltaY/Data/AnalysisResults_536_Bpos_7maggio.root";
  } else if (mode == 2) {
    dataFileName = "../DataFile/pp_ref/DeltaY/Data/AnalysisResults_536_Bneg_7maggio.root";
  } else {
    std::cerr << "[ERROR] Invalid mode selected. Please choose 0 for pp, 1 for pp_ref Bpos, or 2 for pp_ref Bneg." << std::endl;
    return;
  }

  // Assi (3 = Delta y, 4 = Delta Phi)
  const int axisDY = 3;
  const int axisDPhi = 4;

  TFile* fIn = TFile::Open(dataFileName, "READ");
  if (!fIn || fIn->IsZombie()) {
    std::cerr << "[ERROR] Cannot open file: " << dataFileName << std::endl;
    return;
  }

  TString outputFileName;
  if (mode == 0) {
    outputFileName = "TestProjectionsOnlineEffOutput_pp.root";
  } else if (mode == 1) {
    outputFileName = "TestProjectionsOnlineEffOutput_pp_ref_Bpos.root";
  } else if (mode == 2) {
    outputFileName = "TestProjectionsOnlineEffOutput_pp_ref_Bneg.root";
  } else {
    return;
  }
  TFile* fOut = new TFile(outputFileName, "RECREATE");

  gStyle->SetOptStat(0); // Disabilita statistiche globali

  // =========================================================================
  // 2. Core Processing Function (Lambda)
  // =========================================================================
  auto processParticle = [&](TString partName, TString dirName) {
    std::cout << "\n=======================================================" << std::endl;
    std::cout << "[INFO] Processing Particle: " << partName << std::endl;
    std::cout << "=======================================================" << std::endl;

    TString basePath = "phi-strangeness-correlation_OnlineEfficiency/phiStrangenessCorrelation/" + dirName + "/";

    auto getSparse = [&](TString name) -> THnSparseF* {
      THnSparseF* h = (THnSparseF*)fIn->Get(basePath + name);
      if (!h)
        std::cerr << "[WARNING] THnSparse not found: " << basePath + name << std::endl;
      return h;
    };

    THnSparseF* h5Sig = getSparse("h5Phi" + partName + "DataSignal");
    THnSparseF* h5Side = getSparse("h5Phi" + partName + "DataSideband");
    THnSparseF* h5MESig = getSparse("h5Phi" + partName + "DataMESignal");
    THnSparseF* h5MESide = getSparse("h5Phi" + partName + "DataMESideband");

    if (!h5Sig || !h5Side || !h5MESig || !h5MESide) {
      std::cerr << "[ERROR] Aborting projection for " << partName << " due to missing data." << std::endl;
      return;
    }

    // --- 1D Projections ---
    std::cout << "[INFO] Projecting 1D..." << std::endl;
    TH1D* h1DPhi_Sig = h5Sig->Projection(axisDPhi);
    h1DPhi_Sig->SetName("h1DPhi_Sig_" + partName);
    TH1D* h1DPhi_Side = h5Side->Projection(axisDPhi);
    h1DPhi_Side->SetName("h1DPhi_Side_" + partName);
    TH1D* h1DPhi_MESig = h5MESig->Projection(axisDPhi);
    h1DPhi_MESig->SetName("h1DPhi_MESig_" + partName);
    TH1D* h1DPhi_MESide = h5MESide->Projection(axisDPhi);
    h1DPhi_MESide->SetName("h1DPhi_MESide_" + partName);

    TH1D* h1DY_Sig = h5Sig->Projection(axisDY);
    h1DY_Sig->SetName("h1DY_Sig_" + partName);
    TH1D* h1DY_Side = h5Side->Projection(axisDY);
    h1DY_Side->SetName("h1DY_Side_" + partName);
    TH1D* h1DY_MESig = h5MESig->Projection(axisDY);
    h1DY_MESig->SetName("h1DY_MESig_" + partName);
    TH1D* h1DY_MESide = h5MESide->Projection(axisDY);
    h1DY_MESide->SetName("h1DY_MESide_" + partName);

    // --- 2D Projections ---
    std::cout << "[INFO] Projecting 2D..." << std::endl;
    TH2D* h2_Sig = h5Sig->Projection(axisDY, axisDPhi);
    h2_Sig->SetName("h2_Sig_" + partName);
    TH2D* h2_Side = h5Side->Projection(axisDY, axisDPhi);
    h2_Side->SetName("h2_Side_" + partName);
    TH2D* h2_MESig = h5MESig->Projection(axisDY, axisDPhi);
    h2_MESig->SetName("h2_MESig_" + partName);
    TH2D* h2_MESide = h5MESide->Projection(axisDY, axisDPhi);
    h2_MESide->SetName("h2_MESide_" + partName);

    // =======================================================================
    // 3. ME Normalization & Ratios
    // =======================================================================
    std::cout << "[INFO] Normalizing ME to (0,0) and computing SE/ME Ratios..." << std::endl;

    // Helper: Trova il valore medio di ME attorno a (0,0) in modo super-sicuro
    auto getZeroZeroAvg = [](TH2D* h) -> double {
      // 1e-6 garantisce di cadere esattamente a destra/sinistra/sopra/sotto lo zero
      int bx_left = h->GetXaxis()->FindBin(-1e-6);
      int bx_right = h->GetXaxis()->FindBin(1e-6);
      int by_down = h->GetYaxis()->FindBin(-1e-6);
      int by_up = h->GetYaxis()->FindBin(1e-6);

      // Controllo di sicurezza se lo zero non cade su un bordo, ma è interno a un bin
      if (bx_left == bx_right && by_down == by_up) {
        return h->GetBinContent(bx_left, by_down);
      }

      double sum = h->GetBinContent(bx_left, by_down) +
                   h->GetBinContent(bx_left, by_up) +
                   h->GetBinContent(bx_right, by_down) +
                   h->GetBinContent(bx_right, by_up);

      return sum / 4.0;
    };

    // Estrai i fattori di normalizzazione
    double normFactor_Sig = getZeroZeroAvg(h2_MESig);
    double normFactor_Side = getZeroZeroAvg(h2_MESide);

    std::cout << "       -> ME Signal (0,0) Avg: " << normFactor_Sig << std::endl;
    std::cout << "       -> ME Sideband (0,0) Avg: " << normFactor_Side << std::endl;

    // Scala i ME per far sì che (0,0) valga 1.0
    if (normFactor_Sig > 0)
      h2_MESig->Scale(1.0 / normFactor_Sig);
    if (normFactor_Side > 0)
      h2_MESide->Scale(1.0 / normFactor_Side);

    // Calcola il Rapporto SE / ME_Norm
    TH2D* h2_RatioSig = (TH2D*)h2_Sig->Clone("h2_RatioSig_" + partName);
    h2_RatioSig->Divide(h2_MESig);

    TH2D* h2_RatioSide = (TH2D*)h2_Side->Clone("h2_RatioSide_" + partName);
    h2_RatioSide->Divide(h2_MESide);

    // --- Styling ---
    auto setStyle1D = [](TH1* h, int color, TString title) {
      h->SetLineColor(color);
      h->SetMarkerColor(color);
      h->SetMarkerStyle(20);
      h->SetTitle(title);
      h->SetDirectory(0);
    };

    auto setStyle2D = [](TH2* h, TString title) {
      h->SetTitle(title);
      h->GetXaxis()->SetTitle("#Delta#varphi");
      h->GetXaxis()->SetTitleOffset(1.5);
      h->GetYaxis()->SetTitle("#Delta y");
      h->GetYaxis()->SetTitleOffset(1.5);
      h->SetDirectory(0);
    };

    setStyle1D(h1DPhi_Sig, kBlack, partName + " #Delta#varphi Proj; #Delta#varphi; Counts");
    setStyle1D(h1DPhi_Side, kBlue, partName + " #Delta#varphi Proj; #Delta#varphi; Counts");
    setStyle1D(h1DPhi_MESig, kRed, partName + " #Delta#varphi Proj; #Delta#varphi; Counts");
    setStyle1D(h1DPhi_MESide, kGreen + 2, partName + " #Delta#varphi Proj; #Delta#varphi; Counts");

    setStyle1D(h1DY_Sig, kBlack, partName + " #Delta y Proj; #Delta y; Counts");
    setStyle1D(h1DY_Side, kBlue, partName + " #Delta y Proj; #Delta y; Counts");
    setStyle1D(h1DY_MESig, kRed, partName + " #Delta y Proj; #Delta y; Counts");
    setStyle1D(h1DY_MESide, kGreen + 2, partName + " #Delta y Proj; #Delta y; Counts");

    setStyle2D(h2_Sig, partName + " SE Signal");
    setStyle2D(h2_MESig, partName + " ME Signal (Norm=1)");
    setStyle2D(h2_RatioSig, partName + " Ratio (SE / ME) Signal");

    setStyle2D(h2_Side, partName + " SE Sideband");
    setStyle2D(h2_MESide, partName + " ME Sideband (Norm=1)");
    setStyle2D(h2_RatioSide, partName + " Ratio (SE / ME) Sideband");

    // --- Canvases ---
    fOut->cd();

    // DPhi 1D Canvas
    TCanvas* cDPhi = new TCanvas("cDPhi_" + partName, "Delta Phi " + partName, 800, 600);
    cDPhi->cd();
    double maxDPhi = std::max({h1DPhi_Sig->GetMaximum(), h1DPhi_MESig->GetMaximum()});
    h1DPhi_Sig->SetMaximum(maxDPhi * 1.2);
    h1DPhi_Sig->Draw("PE");
    h1DPhi_Side->Draw("PE SAME");
    h1DPhi_MESig->Draw("HIST SAME");
    h1DPhi_MESide->Draw("HIST SAME");
    TLegend* legDPhi = new TLegend(0.65, 0.7, 0.88, 0.88);
    legDPhi->SetBorderSize(0);
    legDPhi->AddEntry(h1DPhi_Sig, "SE Signal", "pe");
    legDPhi->AddEntry(h1DPhi_Side, "SE Sideband", "pe");
    legDPhi->AddEntry(h1DPhi_MESig, "ME Signal", "l");
    legDPhi->AddEntry(h1DPhi_MESide, "ME Sideband", "l");
    legDPhi->Draw();
    cDPhi->Write();

    // DY 1D Canvas
    TCanvas* cDY = new TCanvas("cDY_" + partName, "Delta y " + partName, 800, 600);
    cDY->cd();
    double maxDY = std::max({h1DY_Sig->GetMaximum(), h1DY_MESig->GetMaximum()});
    h1DY_Sig->SetMaximum(maxDY * 1.2);
    h1DY_Sig->Draw("PE");
    h1DY_Side->Draw("PE SAME");
    h1DY_MESig->Draw("HIST SAME");
    h1DY_MESide->Draw("HIST SAME");
    TLegend* legDY = new TLegend(0.65, 0.7, 0.88, 0.88);
    legDY->SetBorderSize(0);
    legDY->AddEntry(h1DY_Sig, "SE Signal", "pe");
    legDY->AddEntry(h1DY_Side, "SE Sideband", "pe");
    legDY->AddEntry(h1DY_MESig, "ME Signal", "l");
    legDY->AddEntry(h1DY_MESide, "ME Sideband", "l");
    legDY->Draw();
    cDY->Write();

    // --- 2D Projections Canvas (Grid 3x2) ---
    TCanvas* c2D = new TCanvas("c2D_" + partName, "2D Analysis " + partName, 1800, 1000);
    c2D->Divide(3, 2);

    // Top Row: Signal
    c2D->cd(1);
    h2_Sig->Draw("SURF1");
    c2D->cd(2);
    h2_MESig->Draw("SURF1");
    c2D->cd(3);
    h2_RatioSig->Draw("SURF1");

    // Bottom Row: Sideband
    c2D->cd(4);
    h2_Side->Draw("SURF1");
    c2D->cd(5);
    h2_MESide->Draw("SURF1");
    c2D->cd(6);
    h2_RatioSide->Draw("SURF1");

    c2D->Write();

    // Write all standalone histograms
    h1DPhi_Sig->Write();
    h1DPhi_Side->Write();
    h1DPhi_MESig->Write();
    h1DPhi_MESide->Write();
    h1DY_Sig->Write();
    h1DY_Side->Write();
    h1DY_MESig->Write();
    h1DY_MESide->Write();
    h2_Sig->Write();
    h2_MESig->Write();
    h2_RatioSig->Write();
    h2_Side->Write();
    h2_MESide->Write();
    h2_RatioSide->Write();

    delete cDPhi;
    delete cDY;
    delete c2D;
  };

  // =========================================================================
  // 4. Execution
  // =========================================================================
  processParticle("K0S", "phiK0S");
  processParticle("Pi", "phiPi");

  fOut->Close();
  fIn->Close();
  std::cout << "\n[INFO] All done! Projections successfully saved to " << outputFileName << std::endl;
}
