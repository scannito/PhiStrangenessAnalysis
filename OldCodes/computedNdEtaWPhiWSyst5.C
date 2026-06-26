#include <utility>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <array>
#include <vector>
#include <string>
#include <map>

#include "Riostream.h"
#include "TFile.h"
#include "TLegend.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TH3F.h"
#include "THnSparse.h"
#include "TMath.h"
#include "TCanvas.h"
#include "TLine.h"
#include "TStyle.h"
#include "TF1.h"
#include "TF2.h"
#include "TFitResult.h"
#include "TGraphAsymmErrors.h"
#include "TMultiGraph.h"

constexpr float PI = 3.14159274101257324e+00f;
constexpr float TwoPI = 2.f * PI;
constexpr float PIHalf = 0.5f * PI;
constexpr float PIQuarter = 0.25f * PI;

enum {
  kGlobalplusITSonly = 0,
  kGlobalonly,
  kITSonly
};

enum {
  kSpAll = 0,
  kSpPion,
  kSpKaon,
  kSpProton,
  kSpOther,
  kSpStrangeDecay,
  kSpNotPrimary
};

enum {
  kNoGenpTVar = 0,
  kGenpTup,
  kGenpTdown
};

constexpr Float_t PVzCoordNom = 10.0;
const std::vector<Float_t> PVzCoordVar = {7.0, 5.0};
const std::vector<std::pair<Float_t, Float_t>> phiRanges = {{0.0, PIHalf}, {PIHalf, PI}, {PI, 3*PIHalf}, {3*PIHalf, TwoPI}, {PIQuarter, 3*PIQuarter}, {5*PIQuarter, 7*PIQuarter}};
const std::vector<std::vector<std::pair<Int_t, Float_t>>> partToChange = {{{kSpPion, 1.0}, {kSpKaon, 0.7}, {kSpProton, 1.0}, {kSpOther, 1.0}},
                                                                          {{kSpPion, 1.0}, {kSpKaon, 1.3}, {kSpProton, 1.0}, {kSpOther, 1.0}},
                                                                          {{kSpPion, 1.0}, {kSpKaon, 1.0}, {kSpProton, 0.7}, {kSpOther, 1.0}},
                                                                          {{kSpPion, 1.0}, {kSpKaon, 1.0}, {kSpProton, 1.3}, {kSpOther, 1.0}},
                                                                          {{kSpPion, 1.0}, {kSpKaon, 1.0}, {kSpProton, 1.0}, {kSpOther, 0.7}},
                                                                          {{kSpPion, 1.0}, {kSpKaon, 1.0}, {kSpProton, 1.0}, {kSpOther, 1.3}},
                                                                          {{kSpPion, 1.0}, {kSpKaon, 0.7}, {kSpProton, 0.7}, {kSpOther, 1.0}},
                                                                          {{kSpPion, 1.0}, {kSpKaon, 1.3}, {kSpProton, 1.3}, {kSpOther, 1.0}},
                                                                          {{kSpPion, 1.0}, {kSpKaon, 0.7}, {kSpProton, 1.3}, {kSpOther, 1.0}},
                                                                          {{kSpPion, 1.0}, {kSpKaon, 1.3}, {kSpProton, 0.7}, {kSpOther, 1.0}}};

const std::vector<Int_t> Colors = {634, 628, 807, 797, kOrange - 4, 815, 418, 429, 867, 856, 601, kViolet, kPink + 9, kPink + 1, 1};
const std::vector<Int_t> ColorsFinal = {kBlack, kBlue, kGreen+3, 797, kRed+1};
const std::vector<Int_t> FullMarkers  = {20, 21, 33, 34, 29, 41, 47, 43};
const std::vector<Int_t> EmptyMarkers = {53, 56, 57, 58, 64, 67, 54, 65};
const std::vector<Int_t> Markers = {20, 21, 33, 34, 29, 41, 47, 43, 53, 56, 57, 58, 64, 67, 54, 65};

constexpr Int_t nbin_mult = 10;
const std::array<Double_t, nbin_mult+1> mult_axis = {0.0, 1.0, 5.0, 10.0, 15.0, 20.0, 30.0, 40.0, 50.0, 70.0, 100.0};

constexpr Double_t topPadHeight = 0.7; 
constexpr Double_t bottomPadHeight = 1.0 - topPadHeight;
constexpr Double_t scaleFactor = bottomPadHeight / topPadHeight;

const std::vector<const char*> legendPtExtrapolation = {"no #it{p}_{T}^{gen} variation (nominal)", "-10 * #it{p}_{T}^{gen} + 2", "5 * #it{p}_{T}^{gen} + 0.5"};
const std::vector<const char*> legendVariationVertexCut = {"|v_{z}| < 10 cm (nominal)", "|v_{z}| < 7 cm", "|v_{z}| < 5 cm"};
const std::vector<const char*> legendVariationPhiRange = {"0 < #phi < 2#pi (nominal)", "0  < #phi < #pi/2", "#pi/2 < #phi < #pi", "#pi < #phi < 3#pi/2", "3#pi/2 < #phi < 2#pi", "#pi/4 < #phi < 3#pi/4", "5#pi/4 < #phi < 7#pi/4"};
const std::vector<const char*> legendVariationPartSpecies = {"nominal", "K #times 0.7, p #times 1, other #times 1", "K #times 1.3, p #times 1, other #times 1", "K #times 1, p #times 0.7, other #times 1", "K #times 1, p #times 1.3, other #times 1",
                                                             "K #times 1, p #times 1, other #times 0.7", "K #times 1, p #times 1, other #times 1.3", "K #times 0.7, p #times 0.7, other #times 1", "K #times 1.3, p #times 1.3, other #times 1",
                                                             "K #times 0.7, p #times 1.3, other #times 1", "K #times 1.3, p #times 0.7, other #times 1"};
const std::vector<const char*> legendVariationTrackType = {"ITSibAny (nominal)", "ITSallAny", "ITSall7Layers"};
const std::vector<const char*> legendAllVariation = {"#it{p}_{T}^{gen}", "z-vtx", "#phi range", "particle composition", "track-type"};

const std::vector<std::string> systFileList = {"cptExtrapolationMult", "cvariationVertexCutMult", "cvariationPhiRangeMult", "cvariationPartSpeciesMult", "cvariationTrackTypeMult"};

TH1F* Project1D(THnSparseF* hn, Int_t axistocut1, Int_t binlow1, Int_t binup1, Int_t axistocut2, Int_t binlow2, Int_t binup2, Int_t axistocut3, Int_t binlow3, Int_t binup3, Int_t axistoproj, std::string hname = "") 
{ 
    if (!hn || hn->IsZombie()) {
        std::cout << "Error: THnSparseF is null or zombie!" << std::endl;
        return nullptr;
    }
    
    hn->GetAxis(axistocut1)->SetRange(binlow1, binup1);
    hn->GetAxis(axistocut2)->SetRange(binlow2, binup2);
    hn->GetAxis(axistocut3)->SetRange(binlow3, binup3);
    TH1F* h1 = (TH1F*)hn->Projection(axistoproj);
    h1->SetName(hname.c_str());
    h1->SetDirectory(0);
    return h1;
}

TH1F* Project1D(THnSparseF* hn, Int_t axistocut1, Int_t binlow1, Int_t binup1, Int_t axistocut2, Int_t binlow2, Int_t binup2, Int_t axistocut3, Int_t binlow3, Int_t binup3, Int_t axistocut4, Int_t binlow4, Int_t binup4, Int_t axistoproj, std::string hname = "") 
{ 
    if (!hn || hn->IsZombie()) {
        std::cout << "Error: THnSparseF is null or zombie!" << std::endl;
        return nullptr;
    }

    hn->GetAxis(axistocut1)->SetRange(binlow1, binup1);
    hn->GetAxis(axistocut2)->SetRange(binlow2, binup2);
    hn->GetAxis(axistocut3)->SetRange(binlow3, binup3);
    hn->GetAxis(axistocut4)->SetRange(binlow4, binup4);
    TH1F* h1 = (TH1F*)hn->Projection(axistoproj);
    h1->SetName(hname.c_str());
    h1->SetDirectory(0);
    return h1;
}

TH1F* Project1D(THnSparseF* hn, Int_t axistocut1, Int_t binlow1, Int_t binup1, Int_t axistocut2, Int_t binlow2, Int_t binup2, Int_t axistocut3, Int_t binlow3, Int_t binup3, Int_t axistocut4, 
                Int_t binlow4, Int_t binup4, Int_t axistocut5, Int_t binlow5, Int_t binup5, Int_t axistoproj, std::string hname = "") 
{ 
    if (!hn || hn->IsZombie()) {
        std::cout << "Error: THnSparseF is null or zombie!" << std::endl;
        return nullptr;
    }

    hn->GetAxis(axistocut1)->SetRange(binlow1, binup1);
    hn->GetAxis(axistocut2)->SetRange(binlow2, binup2);
    hn->GetAxis(axistocut3)->SetRange(binlow3, binup3);
    hn->GetAxis(axistocut4)->SetRange(binlow4, binup4);
    hn->GetAxis(axistocut5)->SetRange(binlow5, binup5);
    TH1F* h1 = (TH1F*)hn->Projection(axistoproj);
    h1->SetName(hname.c_str());
    h1->SetDirectory(0);
    return h1;
}

TH2F* Project2D(THnSparseF* hn, Int_t axistocut1, Int_t binlow1, Int_t binup1, Int_t axistocut2, Int_t binlow2, Int_t binup2, Int_t axistocut3, Int_t binlow3, Int_t binup3, Int_t axistoproj1, Int_t axistoproj2, std::string hname = "") 
{ 
    if (!hn) return 0;
    hn->GetAxis(axistocut1)->SetRange(binlow1, binup1);
    hn->GetAxis(axistocut2)->SetRange(binlow2, binup2);
    hn->GetAxis(axistocut3)->SetRange(binlow3, binup3);
    TH2F* h2 = (TH2F*)hn->Projection(axistoproj1, axistoproj2);
    h2->SetName(hname.c_str());
    h2->SetDirectory(0);
    return h2;
}

void PlotSpectra(const std::vector<std::array<TH1*, nbin_mult>>& h1SpectraSet) 
{
    std::string cName = "c" + std::string(h1SpectraSet[0][0]->GetName());
    TCanvas* cSpectra = new TCanvas(cName.c_str(), cName.c_str(), 800, 800);
    cSpectra->cd();
    gPad->SetMargin(0.16,0.03,0.13,0.06);
    gStyle->SetOptStat(0);

    TLegend* leg1 = new TLegend(0.5, 0.82, 0.8, 0.85);
    leg1->SetHeader("#bf{This work}");
    leg1->SetTextSize(0.05);
    leg1->SetLineWidth(0);

    TLegend* leg2 = new TLegend(0.5, 0.62, 0.9, 0.82);
    leg2->SetHeader("pp, #sqrt{#it{s}} = 13.6 TeV");
    leg2->SetTextSize(0.035);
    leg2->SetLineWidth(0);
    leg2->SetNColumns(2);

    for (size_t i = 0; i < h1SpectraSet.size(); i++) {
        auto &h1Spectra = h1SpectraSet[i];

        for (int j = 0; j < nbin_mult; j++) {
            auto &h1Spectrum = h1Spectra[j];
            auto & Color = Colors[j];

            h1Spectrum->SetMarkerStyle(20);
            h1Spectrum->SetMarkerColor(Color);
            h1Spectrum->SetMarkerSize(1.5);
            h1Spectrum->SetLineColor(Color);
            h1Spectrum->SetLineWidth(2);
            //h1Spectrum->SetFillStyle(3001);
            //h1Spectrum->SetFillColor(Color);
            if (i == 1) {
                h1Spectrum->SetFillColorAlpha(Color, 0.3); // colore semi-trasparente
                h1Spectrum->SetMarkerSize(0);
                h1Spectrum->SetLineWidth(0);
            }
            h1Spectrum->GetYaxis()->SetTitleSize(0.045);
            h1Spectrum->GetYaxis()->SetTitleOffset(1.0);
            h1Spectrum->GetYaxis()->SetLabelSize(0.045);

            if (i == 0) {
                // primo set: punti + barre
                if (j == 0) h1Spectrum->Draw("E1");  // primo hist apre il canvas
                else h1Spectrum->Draw("E1 same");
            }
            else if (i == 1) h1Spectrum->Draw("E2 same");

            if (i == 0)
                leg2->AddEntry(h1Spectrum, Form("%i-%i %%", (int)mult_axis[j], (int)mult_axis[j+1]), "p");

            leg1->Draw("same");
            leg2->Draw("same");
        }
    }

    std::filesystem::path outDir = "dNdEtaCanvasOutput5";
    std::filesystem::create_directories(outDir);

    cSpectra->SaveAs((outDir / (std::string(cSpectra->GetName()) + ".pdf")).c_str());
    cSpectra->SaveAs((outDir / (std::string(cSpectra->GetName()) + ".root")).c_str());
    
    delete leg1;
    delete leg2;
    delete cSpectra;
}

void PlotSpectraPerMult(const std::vector<std::array<TH1*, nbin_mult>>& h1SpectraSet, const std::vector<const char*>& legend, const std::string& name, std::vector<TH1*>& hSystematic)
{
    std::filesystem::path outDir = "dNdEtaCanvasOutput5";
    std::filesystem::create_directories(outDir);

    for (int j = 0; j < nbin_mult; j++) {
        // Nome canvas
        std::string cName = std::string("c") + name + Form("Mult_%i_%i", (int)mult_axis[j], (int)mult_axis[j+1]);
        TCanvas* c = new TCanvas(cName.c_str(), cName.c_str(), 800, 800);
        gStyle->SetOptStat(0);
        
        TPad* topPad = new TPad("topPad", "Top Pad", 0, bottomPadHeight, 1, 1);
        topPad->SetBottomMargin(0);
        //topPad->SetLogy();
        topPad->Draw();

        TPad* bottomPad = new TPad("bottomPad", "Bottom Pad", 0, 0, 1, bottomPadHeight);
        bottomPad->SetTopMargin(0);
        bottomPad->SetBottomMargin(0.3);
        //bottomPad->SetLogy();
        bottomPad->Draw();

        // --- Pad superiore ---
        topPad->cd();
        gStyle->SetOptStat(0);

        TLegend* leg1 = new TLegend(0.5, 0.82, 0.8, 0.85);
        leg1->SetHeader("#bf{This work}");
        leg1->SetTextSize(0.05);
        leg1->SetLineWidth(0);

        TLegend* leg2 = new TLegend(0.5, 0.62, 0.8, 0.82);
        leg2->SetHeader(Form("pp, #sqrt{#it{s}} = 13.6 TeV, %i-%i %%", (int)mult_axis[j], (int)mult_axis[j+1]));
        leg2->SetTextSize(0.035);
        leg2->SetLineWidth(0);
        //leg2->SetNColumns(2);

        bool firstDraw = true;
        for (size_t i = 0; i < h1SpectraSet.size(); i++) {
            auto h1 = (TH1*) h1SpectraSet[i][j]->Clone(Form("%s_clone_%zu", h1SpectraSet[i][j]->GetName(), i));
            Color_t col = Colors[i];
            h1->SetMarkerStyle(20 + i);
            h1->SetMarkerColor(col);
            h1->SetMarkerSize(1.5);
            h1->SetLineColor(col);
            h1->SetLineWidth(2);
            h1->GetYaxis()->SetTitleSize(0.045);
            h1->GetYaxis()->SetTitleOffset(1.0);
            h1->GetYaxis()->SetLabelSize(0.045);

            if (firstDraw) {
                h1->Draw("E");
                firstDraw = false;
            } else {
                h1->Draw("E same");
            }

            leg2->AddEntry(h1, legend[i], "p");
        }

        leg1->Draw();
        leg2->Draw();

        // --- Pad inferiore: rapporti ---
        bottomPad->cd();
        gStyle->SetOptStat(0);

        // Primo spettro come riferimento
        auto hRef = (TH1*) h1SpectraSet[0][j]->Clone("ref");
        hRef->SetDirectory(0);

        // Calcolo tutti i ratio prima
        std::vector<TH1*> ratios;
        for (size_t i = 1; i < h1SpectraSet.size(); i++) {
            auto hRatio = (TH1*) h1SpectraSet[i][j]->Clone(Form("ratio_%zu", i));
            hRatio->Divide(hRef);
            ratios.push_back(hRatio);
        }

        // Costruisco la banda massima
        auto hBand = (TH1*)ratios[0]->Clone("ratio_band");
        hBand->SetFillColorAlpha(kGray + 1, 0.3); // colore semi-trasparente
        hBand->SetLineColor(kGray + 1);
        hBand->SetMarkerSize(0);
        hBand->SetLineWidth(0);

        for (int bin = 1; bin <= hBand->GetNbinsX(); ++bin) {
            double maxDev = 0.0;
            for (auto h : ratios) {
                double dev = std::abs(h->GetBinContent(bin) - 1.0);
                if (dev > maxDev) maxDev = dev;
            }
            // Banda centrata a 1
            hBand->SetBinContent(bin, 1.0);
            hBand->SetBinError(bin, maxDev);
        }

        // Disegno
        bool firstRatio = true;
        for (size_t idx = 0; idx < ratios.size(); idx++) {
            Color_t col = Colors[idx + 1];
            ratios[idx]->SetTitle("; #it{#eta}; Ratio to nominal");
            ratios[idx]->SetMarkerStyle(20 + idx + 1);
            ratios[idx]->SetMarkerColor(col);
            ratios[idx]->SetLineColor(col);
            ratios[idx]->SetMarkerSize(1.2);

            ratios[idx]->GetXaxis()->SetLabelOffset(0.03);
            ratios[idx]->GetXaxis()->SetLabelSize(0.045 / scaleFactor);
            ratios[idx]->GetXaxis()->SetTitleSize(0.045 / scaleFactor);
            ratios[idx]->GetXaxis()->SetTitleOffset(1.2);
            ratios[idx]->GetYaxis()->SetTitleSize(0.045 / scaleFactor - 0.01);
            ratios[idx]->GetYaxis()->SetTitleOffset(0.45);
            ratios[idx]->GetYaxis()->SetLabelSize(0.045 / scaleFactor);
            ratios[idx]->GetYaxis()->SetNdivisions(505);
            ratios[idx]->GetYaxis()->SetRangeUser(0.93, 1.07);

            if (firstRatio) {
                ratios[idx]->Draw("E same");
                firstRatio = false;
            } else {
                ratios[idx]->Draw("E same");
            }
        }

        hBand->Draw("E2 same"); // sfondo

        // Nuovo TH1 per sistematici
        auto hBandSystematic = (TH1*) hBand->Clone(Form("systematic_mult_%i", j));
        hBandSystematic->Reset();
        for (int bin = 1; bin <= hBandSystematic->GetNbinsX(); ++bin) {
            hBandSystematic->SetBinContent(bin, hBand->GetBinError(bin) * 100);
            hBandSystematic->SetBinError(bin, 0.0);
        }
        hBandSystematic->SetFillColor(0);
        hBandSystematic->SetFillStyle(0);

        hSystematic[j] = hBandSystematic;

        gStyle->SetOptStat(0);

        c->SaveAs((outDir / (std::string(c->GetName()) + ".pdf")).c_str());
        c->SaveAs((outDir / (std::string(c->GetName()) + ".root")).c_str());

        delete leg1;
        delete leg2;
        delete topPad;
        delete bottomPad;
        delete c;
    }
}

void SumSystematicsPerMult(const std::vector<std::vector<TH1*>>& hSystematics) 
{
    std::filesystem::path outDir = "dNdEtaCanvasOutput5";
    std::filesystem::create_directories(outDir);

    for (int j = 0; j < nbin_mult; j++) {
        if (!hSystematics[0][j] || hSystematics[0][j]->IsZombie()) {
            std::cerr << "Error: Invalid histogram for multiplicity bin: " << j << "\n";
            continue;
        }
        TH1* hSum = (TH1*)hSystematics[0][j]->Clone(Form("hSumSystematics_%i", j));
        hSum->Reset();

        std::string cName = std::string("cSystematics") + Form("Mult_%i_%i", (int)mult_axis[j], (int)mult_axis[j+1]);
        TCanvas* c = new TCanvas(cName.c_str(), cName.c_str(), 800, 800);
        gStyle->SetOptStat(0);

        TLegend* leg1 = new TLegend(0.3, 0.82, 0.8, 0.85);
        leg1->SetHeader("#bf{This work}");
        leg1->SetTextSize(0.05);
        leg1->SetLineWidth(0);

        TLegend* leg2 = new TLegend(0.3, 0.62, 0.85, 0.82);
        leg2->SetHeader(Form("pp, #sqrt{#it{s}} = 13.6 TeV, %i-%i %%", (int)mult_axis[j], (int)mult_axis[j+1]));
        leg2->SetTextSize(0.035);
        leg2->SetLineWidth(0);
        leg2->SetNColumns(2);

        bool firstDraw = false;
        for (size_t i = 0; i < hSystematics.size(); i++) {
            if (!hSystematics[i][j] || hSystematics[i][j]->IsZombie()) {
                std::cerr << "Error: Invalid histogram for multiplicity bin: " << j << "\n";
                continue;
            }
            auto h = (TH1*)hSystematics[i][j]->Clone(Form("%s_clone_%zu", hSystematics[i][j]->GetName(), i));
            Color_t col = Colors[i];
            h->SetMarkerStyle(20 + i);
            h->SetMarkerColor(col);
            h->SetMarkerSize(1.5);
            h->SetLineColor(col);
            h->SetLineWidth(2);
            h->GetYaxis()->SetTitleSize(0.045);
            h->GetYaxis()->SetTitleOffset(1.0);
            h->GetYaxis()->SetLabelSize(0.045);
            h->GetXaxis()->SetLabelOffset(0.03);
            h->GetXaxis()->SetLabelSize(0.045);
            h->GetXaxis()->SetTitleSize(0.045);
            h->GetXaxis()->SetTitleOffset(1.2);
            h->SetTitle("; #it{#eta}; Systematic uncertainty (%)");

            if (!firstDraw) {
                h->Draw("HIST");
                firstDraw = true;
            } else {
                h->Draw("HIST same");
            }

            leg2->AddEntry(h, legendAllVariation[i], "l");

            for (int bin = 1; bin <= hSum->GetNbinsX(); ++bin) {
                hSum->SetBinContent(bin, hSum->GetBinContent(bin) + std::pow(h->GetBinContent(bin), 2));
            }
        }
        for (int bin = 1; bin <= hSum->GetNbinsX(); ++bin) {
            hSum->SetBinContent(bin, std::sqrt(hSum->GetBinContent(bin)));
        }
        hSum->SetMarkerStyle(20);
        hSum->SetMarkerColor(kBlack);
        hSum->SetMarkerSize(1.5);
        hSum->SetLineColor(kBlack);
        hSum->SetLineWidth(2);
        hSum->GetYaxis()->SetTitleSize(0.045);
        hSum->GetYaxis()->SetTitleOffset(1.0);
        hSum->GetYaxis()->SetLabelSize(0.045);
        hSum->GetXaxis()->SetLabelOffset(0.03);
        hSum->GetXaxis()->SetLabelSize(0.045);
        hSum->GetXaxis()->SetTitleSize(0.045);
        hSum->GetXaxis()->SetTitleOffset(1.2);
        hSum->SetTitle("; #it{#eta}; Systematic uncertainty (%)");

        hSum->Draw("HIST same");

        leg2->AddEntry(hSum, "total", "l");

        leg1->Draw("same");
        leg2->Draw("same");

        gStyle->SetOptStat(0);

        c->SaveAs((outDir / (std::string(c->GetName()) + ".pdf")).c_str());
        c->SaveAs((outDir / (std::string(c->GetName()) + ".root")).c_str());

        delete leg1;
        delete leg2;
        delete c;
    }
}

std::array<double, nbin_mult> writeSystematicsTxt(const std::string& name,
                         const std::vector<const char*>& rowLabels,
                         const std::vector<std::vector<TH1*>>& histSets)
{
    if (histSets.size() != rowLabels.size()) {
        std::cerr << "Error: number of sources does not match number of labels!" << std::endl;
        return {};
    }

    std::filesystem::path outDir = "dNdEtaCanvasOutput5";
    std::filesystem::create_directory(outDir);

    std::string filename = outDir / name;

    std::ofstream out(filename);
    if (!out.is_open()) {
        std::cerr << "Cannot open file " << filename << std::endl;
        return {};
    }

    std::array<double, nbin_mult> totSyst{};

    for (size_t i = 0; i < histSets.size(); ++i) {
        out << rowLabels[i] << "\n";

        for (size_t j = 0; j < histSets[i].size(); ++j) {
            TH1* h = histSets[i][j];
            if (!h) continue;

            // Calcola la media dei bin
            double sum = 0;
            int nBins = h->GetNbinsX();
            for (int b = 1; b <= nBins; ++b) sum += h->GetBinContent(b);
            double avg = nBins > 0 ? sum / nBins : 0;

            out << std::fixed << std::setprecision(1) << avg;
            if (j < histSets[i].size() - 1) out << " - ";

            totSyst[j] += std::pow(avg, 2);
        }
        out << "\n\n";
    }

    out << "total" << "\n";

    for (int j = 0; j < nbin_mult; ++j) {
        out << std::fixed << std::setprecision(1) << std::sqrt(totSyst[j]);
        if (j < nbin_mult - 1) out << " - ";
    }

    out.close();

    return totSyst;
}

void GetHistosFromCanvasFile(const std::vector<std::string>& names,
                             std::vector<std::vector<TH1*>>& allHistos,
                             const char* padName = "bottomPad",
                             const char* histoName = "ratio_band")
{
    std::cout << "Getting histograms from canvas files...\n";
    for (int i = 0; i < names.size(); i++) {
        std::cout << "Processing: " << names[i] << "\n";
        std::vector<TH1*> histos;
        histos.reserve(nbin_mult);
        auto& name = names[i];
        //if (name == "cvariationVertexCutMult")
            //continue;

        for (int j = 0; j < nbin_mult; j++) {
            std::string canvasName = name + Form("_%i_%i", (int)mult_axis[j], (int)mult_axis[j+1]);
            std::string fileName = std::string("dNdEtaCanvasOutput5/") + name + Form("_%i_%i", (int)mult_axis[j], (int)mult_axis[j+1]) + std::string(".root");

            TFile f(fileName.c_str());
            if (f.IsZombie()) {
                std::cerr << "Errore: impossibile aprire " << fileName << "\n";
                continue;
            }

            // recupera il canvas
            TCanvas* c = (TCanvas*)f.Get(canvasName.c_str());
            if (!c) {
                std::cerr << "Canvas '" << canvasName << "' non trovato\n";
                histos.push_back(nullptr);
                continue;
            }

            // recupera il pad
            TPad* pad = (TPad*)c->GetPrimitive(padName);
            if (!pad) {
                std::cerr << "Pad '" << padName << "' non trovato nel canvas\n";
                histos.push_back(nullptr);
                continue;
            }

            // recupera direttamente l'oggetto per nome
            TObject* obj = pad->FindObject(histoName);
            if (!obj) {
                std::cerr << "Oggetto '" << histoName << "' non trovato nel pad\n";
                histos.push_back(nullptr);
                continue;
            }

            if (!obj->InheritsFrom(TH1::Class())) {
                std::cerr << "Oggetto '" << histoName << "' non è un TH1\n";
                histos.push_back(nullptr);
                continue;
            }

            auto histoName = name + Form("_%i_%i", (int)mult_axis[j], (int)mult_axis[j+1]);
            auto h = (TH1*)obj->Clone(histoName.c_str());
            h->SetDirectory(0); // Assicurati che l'istogramma non sia legato al file
            h->Reset();
            for (int bin = 1; bin <= h->GetNbinsX(); ++bin) {
                h->SetBinContent(bin, ((TH1*)obj)->GetBinError(bin) * 100);
                h->SetBinError(bin, 0.0);
            }
            h->SetFillColor(0);
            h->SetFillStyle(0);

            histos.push_back(h);
        }

        allHistos.push_back(histos);
    }

    std::cout << "Finished processing canvas files.\n";
}

std::optional<std::array<TH1*, nbin_mult>> fullAnalysis(bool giveArray = false, std::pair<const char*, const char*> files = {"AnalysisResultsdNdEtaDataWSyst.root", "AnalysisResultsdNdEtaMCWSyst_4.root"}, 
                                                        Int_t nGenpT = kNoGenpTVar, Float_t PVzCoord = PVzCoordNom, 
                                                        std::optional<std::pair<Float_t, Float_t>> phiRange = std::nullopt,
                                                        std::optional<std::vector<std::pair<Int_t, Float_t>>> partToChange = std::nullopt,
                                                        std::optional<std::array<double, nbin_mult>> totSyst = std::nullopt) 
{
    TFile* fileData = TFile::Open(files.first, "READ");
    TFile* fileMC = TFile::Open(files.second, "READ");

    std::string dirData = "phik0shortanalysis/dataEventHist/";
    std::string dirMC = "phik0shortanalysis/mcEventHist/";
    if (std::string(files.second) == "AnalysisResultsdNdEtaMCWSyst_4.root") dirMC = "phik0shortanalysis_id35775/mcEventHist/";

    TH2F* h2VertexZMultData = (TH2F*)fileData->Get((dirData + std::string("h2VertexZvsMult")).c_str());
    h2VertexZMultData->SetDirectory(0);

    THnSparseF* h5dNdEtaData = (THnSparseF*)fileData->Get((dirData + std::string("h5EtaDistribution")).c_str());

    TH2F* h2GenMCRecoVertexZvsMult = (TH2F*)fileMC->Get((dirMC + std::string("h2GenMCRecoVertexZvsMult")).c_str());
    h2GenMCRecoVertexZvsMult->SetDirectory(0);
    TH2F* h2GenMCAssocRecoVertexZvsMult = (TH2F*)fileMC->Get((dirMC + std::string("h2GenMCAssocRecoVertexZvsMult")).c_str());
    h2GenMCAssocRecoVertexZvsMult->SetDirectory(0);
    TH1F* h1MultMCGen = (TH1F*)fileMC->Get((dirMC + std::string("hGenMCMultiplicityPercent")).c_str());
    h1MultMCGen->SetDirectory(0);

    THnSparseF* h6dNdEtaMCReco = (THnSparseF*)fileMC->Get((dirMC + std::string("h6RecoCheckMCEtaDistribution")).c_str());
    THnSparseF* h6dNdEtaMCGenAssocReco = (THnSparseF*)fileMC->Get((dirMC + std::string("h6GenMCEtaDistributionAssocReco")).c_str());
    THnSparseF* h5dNdEtaMCGen = (THnSparseF*)fileMC->Get((dirMC + std::string("h5GenMCEtaDistribution")).c_str());

    fileData->Close();
    fileMC->Close();

    TH1F* h1MultMCReco = (TH1F*)h2GenMCRecoVertexZvsMult->ProjectionY("h1MultMCReco", h2GenMCRecoVertexZvsMult->GetXaxis()->FindBin(-PVzCoord), h2GenMCRecoVertexZvsMult->GetXaxis()->FindBin(PVzCoord) - 1);
    h1MultMCReco->SetDirectory(0);

    TH1F* h1MultMCGenAssocReco = (TH1F*)h2GenMCAssocRecoVertexZvsMult->ProjectionY("h1MultMCGenAssocReco", h2GenMCAssocRecoVertexZvsMult->GetXaxis()->FindBin(-PVzCoord), h2GenMCAssocRecoVertexZvsMult->GetXaxis()->FindBin(PVzCoord) - 1);
    h1MultMCGenAssocReco->SetDirectory(0);

    TH1F* h1dNdEtaEvSplitTmp = (TH1F*)h1MultMCGenAssocReco->Clone("h1dNdEtaEvSplitTmp");
    h1dNdEtaEvSplitTmp->Divide(h1MultMCGenAssocReco, h1MultMCReco, 1.0, 1.0, "B");

    TH1F* h1dNdEtaEvLossTmp = (TH1F*)h1MultMCGenAssocReco->Clone("h1dNdEtaEvLossTmp");
    h1dNdEtaEvLossTmp->Divide(h1MultMCGenAssocReco, h1MultMCGen, 1.0, 1.0, "B");

    //*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//
    
    if (!giveArray) {
        TH1F* h1dNdEtaDataMultIntGlobalPlusITSonly = Project1D(h5dNdEtaData, 0, h5dNdEtaData->GetAxis(0)->FindBin(-PVzCoord), h5dNdEtaData->GetAxis(0)->FindBin(PVzCoord) - 1, 1, 1, nbin_mult,
                                                                            4, h5dNdEtaData->GetAxis(4)->FindBin(kGlobalplusITSonly), h5dNdEtaData->GetAxis(4)->FindBin(kGlobalplusITSonly), 2, "h1dNdEtaDataMultIntGlobalPlusITSonly");
        h1dNdEtaDataMultIntGlobalPlusITSonly->SetDirectory(0);
        h1dNdEtaDataMultIntGlobalPlusITSonly->Scale(1.0 / h2VertexZMultData->Integral(h2VertexZMultData->GetXaxis()->FindBin(-PVzCoord), h2VertexZMultData->GetXaxis()->FindBin(PVzCoord) - 1, 1, nbin_mult));
        h1dNdEtaDataMultIntGlobalPlusITSonly->Scale(1.0 / h1dNdEtaDataMultIntGlobalPlusITSonly->GetBinWidth(1));
        h1dNdEtaDataMultIntGlobalPlusITSonly->SetMarkerStyle(20);
        h1dNdEtaDataMultIntGlobalPlusITSonly->SetMarkerColor(kBlack);
        h1dNdEtaDataMultIntGlobalPlusITSonly->SetMarkerSize(1.5);
        h1dNdEtaDataMultIntGlobalPlusITSonly->SetLineColor(kBlack);
        h1dNdEtaDataMultIntGlobalPlusITSonly->SetLineWidth(2);

        TH1F* h1dNdEtaDataMultIntGlobalOnly = Project1D(h5dNdEtaData, 0, h5dNdEtaData->GetAxis(0)->FindBin(-PVzCoord), h5dNdEtaData->GetAxis(0)->FindBin(PVzCoord) - 1, 1, 1, nbin_mult,
                                                                    4, h5dNdEtaData->GetAxis(4)->FindBin(kGlobalonly), h5dNdEtaData->GetAxis(4)->FindBin(kGlobalonly), 2, "h1dNdEtaDataMultIntGlobalOnly");
        h1dNdEtaDataMultIntGlobalOnly->SetDirectory(0);
        h1dNdEtaDataMultIntGlobalOnly->Scale(1.0 / h2VertexZMultData->Integral(h2VertexZMultData->GetXaxis()->FindBin(-PVzCoord), h2VertexZMultData->GetXaxis()->FindBin(PVzCoord) - 1, 1, nbin_mult));
        h1dNdEtaDataMultIntGlobalOnly->Scale(1.0 / h1dNdEtaDataMultIntGlobalOnly->GetBinWidth(1));
        h1dNdEtaDataMultIntGlobalOnly->SetMarkerStyle(20);
        h1dNdEtaDataMultIntGlobalOnly->SetMarkerColor(kBlue);
        h1dNdEtaDataMultIntGlobalOnly->SetMarkerSize(1.5);
        h1dNdEtaDataMultIntGlobalOnly->SetLineColor(kBlue);
        h1dNdEtaDataMultIntGlobalOnly->SetLineWidth(2);

        TH1F* h1dNdEtaDataMultIntITSonly = Project1D(h5dNdEtaData, 0, h5dNdEtaData->GetAxis(0)->FindBin(-PVzCoord), h5dNdEtaData->GetAxis(0)->FindBin(PVzCoord) - 1, 1, 1, nbin_mult,
                                                                4, h5dNdEtaData->GetAxis(4)->FindBin(kITSonly), h5dNdEtaData->GetAxis(4)->FindBin(kITSonly), 2, "h1dNdEtaDataMultIntITSonly");
        h1dNdEtaDataMultIntITSonly->SetDirectory(0);
        h1dNdEtaDataMultIntITSonly->Scale(1.0 / h2VertexZMultData->Integral(h2VertexZMultData->GetXaxis()->FindBin(-PVzCoord), h2VertexZMultData->GetXaxis()->FindBin(PVzCoord) - 1, 1, nbin_mult));
        h1dNdEtaDataMultIntITSonly->Scale(1.0 / h1dNdEtaDataMultIntITSonly->GetBinWidth(1));
        h1dNdEtaDataMultIntITSonly->SetMarkerStyle(20);
        h1dNdEtaDataMultIntITSonly->SetMarkerColor(kRed);
        h1dNdEtaDataMultIntITSonly->SetMarkerSize(1.5);
        h1dNdEtaDataMultIntITSonly->SetLineColor(kRed);
        h1dNdEtaDataMultIntITSonly->SetLineWidth(2);

        TCanvas* c1dNdEtaTrackType = new TCanvas("c1dNdEtaTrackType", "dN/d#eta vs #eta", 800, 800);
        h1dNdEtaDataMultIntGlobalPlusITSonly->GetXaxis()->SetTitle("#it{#eta}");
        h1dNdEtaDataMultIntGlobalPlusITSonly->GetYaxis()->SetTitle("(1/#it{N}_{evt})(d#it{N}_{ch}/d#it{#eta})");
        h1dNdEtaDataMultIntGlobalPlusITSonly->GetYaxis()->SetRangeUser(0.0, 1.2 * h1dNdEtaDataMultIntGlobalPlusITSonly->GetMaximum());
        h1dNdEtaDataMultIntGlobalPlusITSonly->Draw();
        h1dNdEtaDataMultIntGlobalOnly->Draw("same");
        h1dNdEtaDataMultIntITSonly->Draw("same");
        gStyle->SetOptStat(0);

        TLegend* leg1 = new TLegend(0.5, 0.82, 0.8, 0.85);
        leg1->SetHeader("#bf{This work}");
        leg1->SetTextSize(0.05);
        leg1->SetLineWidth(0);
        leg1->Draw("same");

        TLegend* leg2 = new TLegend(0.5, 0.62, 0.8, 0.82);
        leg2->SetHeader(Form("pp, #sqrt{#it{s}} = 13.6 TeV"));
        leg2->SetTextSize(0.035);
        leg2->SetLineWidth(0);
        leg2->AddEntry(h1dNdEtaDataMultIntGlobalPlusITSonly, "Global + ITS-only", "p");
        leg2->AddEntry(h1dNdEtaDataMultIntGlobalOnly, "Global", "p");
        leg2->AddEntry(h1dNdEtaDataMultIntITSonly, "ITS-only", "p");
        leg2->Draw("same");

        c1dNdEtaTrackType->SaveAs((std::string("dNdEtaCanvasOutput5/") + (std::string(c1dNdEtaTrackType->GetName()) + ".pdf")).c_str());
        c1dNdEtaTrackType->SaveAs((std::string("dNdEtaCanvasOutput5/") + (std::string(c1dNdEtaTrackType->GetName()) + ".root")).c_str());

        delete leg1;
        delete leg2;
        delete c1dNdEtaTrackType;

        delete h1dNdEtaDataMultIntGlobalPlusITSonly;
        delete h1dNdEtaDataMultIntGlobalOnly;
        delete h1dNdEtaDataMultIntITSonly;

        TH2F* h2EtaVsPhiData = Project2D(h5dNdEtaData, 0, h5dNdEtaData->GetAxis(0)->FindBin(-PVzCoord), h5dNdEtaData->GetAxis(0)->FindBin(PVzCoord) - 1, 1, 1, nbin_mult,
                                                       4, h5dNdEtaData->GetAxis(4)->FindBin(kGlobalplusITSonly), h5dNdEtaData->GetAxis(4)->FindBin(kGlobalplusITSonly), 2, 3, "h2EtaVsPhiData");

        TCanvas* c2EtaVsPhiData = new TCanvas("c2EtaVsPhiData", "#eta vs #phi", 1400, 800);
        gStyle->SetOptStat(0);
        gPad->SetLeftMargin(0.1);
        gPad->SetRightMargin(0.15);
        h2EtaVsPhiData->SetTitle("");
        h2EtaVsPhiData->GetXaxis()->SetTitle("#it{#phi}");
        h2EtaVsPhiData->GetYaxis()->SetTitle("#it{#eta}");
        h2EtaVsPhiData->Draw("COLZ");

        c2EtaVsPhiData->SaveAs((std::string("dNdEtaCanvasOutput5/") + (std::string(c2EtaVsPhiData->GetName()) + ".pdf")).c_str());
        c2EtaVsPhiData->SaveAs((std::string("dNdEtaCanvasOutput5/") + (std::string(c2EtaVsPhiData->GetName()) + ".root")).c_str());

        delete c2EtaVsPhiData;

        delete h2EtaVsPhiData;
    }

    //*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//*//

    std::array<TH1*, nbin_mult> h1dNdEtaData = {nullptr};
    std::array<TH1*, nbin_mult> h1dNdEtaMCReco = {nullptr};
    std::array<TH1*, nbin_mult> h1dNdEtaMCGenAssocReco = {nullptr};
    std::array<TH1*, nbin_mult> h1dNdEtaMCGen = {nullptr};
    std::array<TH1*, nbin_mult> h1dNdEtaEff = {nullptr};
    std::array<TH1*, nbin_mult> h1dNdEtaSigLoss = {nullptr};
    std::array<TH1*, nbin_mult> h1dNdEtaEvSplit = {nullptr};
    std::array<TH1*, nbin_mult> h1dNdEtaEvLoss = {nullptr};
    std::array<TH1*, nbin_mult> h1dNdEtaDataCorr = {nullptr};
    std::array<TH1*, nbin_mult> h1dNdEtaDataCorrSyst = {nullptr};

    for (int i = 0; i < nbin_mult; ++i) {
        if (phiRange) {
            h1dNdEtaData[i] = Project1D(h5dNdEtaData, 0, h5dNdEtaData->GetAxis(0)->FindBin(-PVzCoord), h5dNdEtaData->GetAxis(0)->FindBin(PVzCoord) - 1, 1, i+1, i+1,
                                                      3, h5dNdEtaData->GetAxis(3)->FindBin(phiRange->first), h5dNdEtaData->GetAxis(3)->FindBin(phiRange->second),
                                                      4, h5dNdEtaData->GetAxis(4)->FindBin(kGlobalplusITSonly), h5dNdEtaData->GetAxis(4)->FindBin(kGlobalplusITSonly), 2, Form("h1dNdEtaData_%d", i));
        } else {
            h1dNdEtaData[i] = Project1D(h5dNdEtaData, 0, h5dNdEtaData->GetAxis(0)->FindBin(-PVzCoord), h5dNdEtaData->GetAxis(0)->FindBin(PVzCoord) - 1, 1, i+1, i+1,
                                                      4, h5dNdEtaData->GetAxis(4)->FindBin(kGlobalplusITSonly), h5dNdEtaData->GetAxis(4)->FindBin(kGlobalplusITSonly), 2, Form("h1dNdEtaData_%d", i));
        }
        h1dNdEtaData[i]->SetDirectory(0);
        h1dNdEtaData[i]->Scale(1.0 / h2VertexZMultData->Integral(h2VertexZMultData->GetXaxis()->FindBin(-PVzCoord), h2VertexZMultData->GetXaxis()->FindBin(PVzCoord) - 1, i+1, i+1));
        h1dNdEtaData[i]->Scale(1.0 / h1dNdEtaData[i]->GetBinWidth(1));
        if (phiRange) {
            Float_t lowerPhi = h5dNdEtaData->GetAxis(3)->GetBinLowEdge(h5dNdEtaData->GetAxis(3)->FindBin(phiRange->first));
            Float_t upperPhi = h5dNdEtaData->GetAxis(3)->GetBinUpEdge(h5dNdEtaData->GetAxis(3)->FindBin(phiRange->second));
            h1dNdEtaData[i]->Scale(1.0 / ((upperPhi - lowerPhi) / TwoPI));
        }
        h1dNdEtaData[i]->SetTitle("");
        h1dNdEtaData[i]->GetXaxis()->SetTitle("#it{#eta}");
        h1dNdEtaData[i]->GetYaxis()->SetTitle("(1/#it{N}_{evt})(d#it{N}_{ch}/d#it{#eta})");
        h1dNdEtaData[i]->GetYaxis()->SetRangeUser(0.1, 1.2 * h1dNdEtaData[i]->GetMaximum());

        if (phiRange) {
            h1dNdEtaMCReco[i] = Project1D(h6dNdEtaMCReco, 0, h6dNdEtaMCReco->GetAxis(0)->FindBin(-PVzCoord), h6dNdEtaMCReco->GetAxis(0)->FindBin(PVzCoord) - 1, 1, i+1, i+1,
                                                          3, h6dNdEtaMCReco->GetAxis(3)->FindBin(phiRange->first), h6dNdEtaMCReco->GetAxis(3)->FindBin(phiRange->second),
                                                          4, h6dNdEtaMCReco->GetAxis(4)->FindBin(kSpAll), h6dNdEtaMCReco->GetAxis(4)->FindBin(kSpAll),
                                                          5, h6dNdEtaMCReco->GetAxis(5)->FindBin(kGlobalplusITSonly), h6dNdEtaMCReco->GetAxis(5)->FindBin(kGlobalplusITSonly), 2, Form("h1dNdEtaMCReco_%d", i));
        } else if (partToChange) {
            bool first = true;
            for (const auto& [species, weight] : *partToChange) {
                std::cout << "Processing species: " << species << " with weight: " << weight << " reco for mult:" << i << std::endl;
                TH1* htmp = Project1D(h6dNdEtaMCReco, 0, h6dNdEtaMCReco->GetAxis(0)->FindBin(-PVzCoord), h6dNdEtaMCReco->GetAxis(0)->FindBin(PVzCoord) - 1, 1, i+1, i+1,
                                                      4, h6dNdEtaMCReco->GetAxis(4)->FindBin(species), h6dNdEtaMCReco->GetAxis(4)->FindBin(species),
                                                      5, h6dNdEtaMCReco->GetAxis(5)->FindBin(kGlobalplusITSonly), h6dNdEtaMCReco->GetAxis(5)->FindBin(kGlobalplusITSonly), 2, Form("h1dNdEtaMCRecotmp_%d", i));
                if (first) {
                    h1dNdEtaMCReco[i] = (TH1*) htmp->Clone(Form("h1dNdEtaMCReco_%d", i));
                    h1dNdEtaMCReco[i]->Scale(weight);
                    first = false;
                } else {
                    h1dNdEtaMCReco[i]->Add(htmp, weight);
                }
                delete htmp;
            }
        } else {
            h1dNdEtaMCReco[i] = Project1D(h6dNdEtaMCReco, 0, h6dNdEtaMCReco->GetAxis(0)->FindBin(-PVzCoord), h6dNdEtaMCReco->GetAxis(0)->FindBin(PVzCoord) - 1, 1, i+1, i+1,
                                                          4, h6dNdEtaMCReco->GetAxis(4)->FindBin(kSpAll), h6dNdEtaMCReco->GetAxis(4)->FindBin(kSpAll),
                                                          5, h6dNdEtaMCReco->GetAxis(5)->FindBin(kGlobalplusITSonly), h6dNdEtaMCReco->GetAxis(5)->FindBin(kGlobalplusITSonly), 2, Form("h1dNdEtaMCReco_%d", i));
        }
        h1dNdEtaMCReco[i]->SetDirectory(0);
        h1dNdEtaMCReco[i]->Scale(1.0 / h1dNdEtaMCReco[i]->GetBinWidth(1));
        if (phiRange) {
            Float_t lowerPhi = h6dNdEtaMCReco->GetAxis(3)->GetBinLowEdge(h6dNdEtaMCReco->GetAxis(3)->FindBin(phiRange->first));
            Float_t upperPhi = h6dNdEtaMCReco->GetAxis(3)->GetBinUpEdge(h6dNdEtaMCReco->GetAxis(3)->FindBin(phiRange->second));
            h1dNdEtaMCReco[i]->Scale(1.0 / ((upperPhi - lowerPhi) / TwoPI));
        }
        h1dNdEtaMCReco[i]->SetTitle("");
        h1dNdEtaMCReco[i]->GetXaxis()->SetTitle("#it{#eta}");
        h1dNdEtaMCReco[i]->GetYaxis()->SetTitle("(1/#it{N}_{evt})(d#it{N}_{ch}/d#it{#eta})");
        h1dNdEtaMCReco[i]->GetYaxis()->SetRangeUser(0.1, 1.2 * h1dNdEtaMCReco[i]->GetMaximum());

        if (phiRange) {
            h1dNdEtaMCGenAssocReco[i] = Project1D(h6dNdEtaMCGenAssocReco, 0, h6dNdEtaMCGenAssocReco->GetAxis(0)->FindBin(-PVzCoord), h6dNdEtaMCGenAssocReco->GetAxis(0)->FindBin(PVzCoord) - 1, 1, i+1, i+1,
                                                                          3, h6dNdEtaMCGenAssocReco->GetAxis(3)->FindBin(phiRange->first), h6dNdEtaMCGenAssocReco->GetAxis(3)->FindBin(phiRange->second),
                                                                          4, h6dNdEtaMCGenAssocReco->GetAxis(4)->FindBin(kSpAll), h6dNdEtaMCGenAssocReco->GetAxis(4)->FindBin(kSpAll),
                                                                          5, h6dNdEtaMCGenAssocReco->GetAxis(5)->FindBin(nGenpT), h6dNdEtaMCGenAssocReco->GetAxis(5)->FindBin(nGenpT), 2, Form("h1dNdEtaMCGenAssocReco_%d", i));
        } else if (partToChange) {
            bool first = true;
            for (const auto& [species, weight] : *partToChange) {
                std::cout << "Processing species: " << species << " with weight: " << weight << " gen for mult: " << i << std::endl;
                TH1* htmp = Project1D(h6dNdEtaMCGenAssocReco, 0, h6dNdEtaMCGenAssocReco->GetAxis(0)->FindBin(-PVzCoord), h6dNdEtaMCGenAssocReco->GetAxis(0)->FindBin(PVzCoord) - 1, 1, i+1, i+1,
                                                              4, h6dNdEtaMCGenAssocReco->GetAxis(4)->FindBin(species), h6dNdEtaMCGenAssocReco->GetAxis(4)->FindBin(species),
                                                              5, h6dNdEtaMCGenAssocReco->GetAxis(5)->FindBin(nGenpT), h6dNdEtaMCGenAssocReco->GetAxis(5)->FindBin(nGenpT), 2, Form("h1dNdEtaMCGenAssocRecoTmp_%d", i));
                if (first) {
                    h1dNdEtaMCGenAssocReco[i] = (TH1*) htmp->Clone(Form("h1dNdEtaMCGenAssocReco_%d", i));
                    h1dNdEtaMCGenAssocReco[i]->Scale(weight);
                    first = false;
                } else {
                    h1dNdEtaMCGenAssocReco[i]->Add(htmp, weight);
                }
                delete htmp;
            }
        } else {
            h1dNdEtaMCGenAssocReco[i] = Project1D(h6dNdEtaMCGenAssocReco, 0, h6dNdEtaMCGenAssocReco->GetAxis(0)->FindBin(-PVzCoord), h6dNdEtaMCGenAssocReco->GetAxis(0)->FindBin(PVzCoord) - 1, 1, i+1, i+1,
                                                                          4, h6dNdEtaMCGenAssocReco->GetAxis(4)->FindBin(kSpAll), h6dNdEtaMCGenAssocReco->GetAxis(4)->FindBin(kSpAll),
                                                                          5, h6dNdEtaMCGenAssocReco->GetAxis(5)->FindBin(nGenpT), h6dNdEtaMCGenAssocReco->GetAxis(5)->FindBin(nGenpT), 2, Form("h1dNdEtaMCGenAssocReco_%d", i));
        }
        h1dNdEtaMCGenAssocReco[i]->SetDirectory(0);
        h1dNdEtaMCGenAssocReco[i]->Scale(1.0 / h1dNdEtaMCGenAssocReco[i]->GetBinWidth(1));
        if (phiRange) {
            Float_t lowerPhi = h6dNdEtaMCGenAssocReco->GetAxis(3)->GetBinLowEdge(h6dNdEtaMCGenAssocReco->GetAxis(3)->FindBin(phiRange->first));
            Float_t upperPhi = h6dNdEtaMCGenAssocReco->GetAxis(3)->GetBinUpEdge(h6dNdEtaMCGenAssocReco->GetAxis(3)->FindBin(phiRange->second));
            h1dNdEtaMCGenAssocReco[i]->Scale(1.0 / ((upperPhi - lowerPhi) / TwoPI));
        }
        h1dNdEtaMCGenAssocReco[i]->SetTitle("");
        h1dNdEtaMCGenAssocReco[i]->GetXaxis()->SetTitle("#it{#eta}");
        h1dNdEtaMCGenAssocReco[i]->GetYaxis()->SetTitle("(1/#it{N}_{evt})(d#it{N}_{ch}/d#it{#eta})");
        h1dNdEtaMCGenAssocReco[i]->GetYaxis()->SetRangeUser(0.1, 1.2 * h1dNdEtaMCGenAssocReco[i]->GetMaximum());

        if (phiRange) {
            h1dNdEtaMCGen[i] = Project1D(h5dNdEtaMCGen, 0, i+1, i+1, 2, h5dNdEtaMCGen->GetAxis(2)->FindBin(phiRange->first), h5dNdEtaMCGen->GetAxis(2)->FindBin(phiRange->second),
                                                        3, h5dNdEtaMCGen->GetAxis(3)->FindBin(kSpAll), h5dNdEtaMCGen->GetAxis(3)->FindBin(kSpAll),
                                                        4, h5dNdEtaMCGen->GetAxis(4)->FindBin(nGenpT), h5dNdEtaMCGen->GetAxis(4)->FindBin(nGenpT), 1, Form("h1dNdEtaMCGen_%d", i));
        } else if (partToChange) {
            bool first = true;
            for (const auto& [species, weight] : *partToChange) {
                std::cout << "Processing species: " << species << " with weight: " << weight << " gen for mult: " << i << std::endl;
                TH1* htmp = Project1D(h5dNdEtaMCGen, 0, i+1, i+1, 3, h5dNdEtaMCGen->GetAxis(3)->FindBin(species), h5dNdEtaMCGen->GetAxis(3)->FindBin(species),
                                                     4, h5dNdEtaMCGen->GetAxis(4)->FindBin(nGenpT), h5dNdEtaMCGen->GetAxis(4)->FindBin(nGenpT), 1, Form("h1dNdEtaMCGenTmp_%d", i));
                if (first) {
                    h1dNdEtaMCGen[i] = (TH1*) htmp->Clone(Form("h1dNdEtaMCGen_%d", i));
                    h1dNdEtaMCGen[i]->Scale(weight);
                    first = false;
                } else {
                    h1dNdEtaMCGen[i]->Add(htmp, weight);
                }
                delete htmp;
            }
        } else {
            h1dNdEtaMCGen[i] = Project1D(h5dNdEtaMCGen, 0, i+1, i+1, 3, h5dNdEtaMCGen->GetAxis(3)->FindBin(kSpAll), h5dNdEtaMCGen->GetAxis(3)->FindBin(kSpAll),
                                                        4, h5dNdEtaMCGen->GetAxis(4)->FindBin(nGenpT), h5dNdEtaMCGen->GetAxis(4)->FindBin(nGenpT), 1, Form("h1dNdEtaMCGen_%d", i));
        }
        h1dNdEtaMCGen[i]->SetDirectory(0);
        h1dNdEtaMCGen[i]->Scale(1.0 / h1dNdEtaMCGen[i]->GetBinWidth(1));
        if (phiRange) {
            Float_t lowerPhi = h5dNdEtaMCGen->GetAxis(2)->GetBinLowEdge(h5dNdEtaMCGen->GetAxis(2)->FindBin(phiRange->first));
            Float_t upperPhi = h5dNdEtaMCGen->GetAxis(2)->GetBinUpEdge(h5dNdEtaMCGen->GetAxis(2)->FindBin(phiRange->second));
            h1dNdEtaMCGen[i]->Scale(1.0 / ((upperPhi - lowerPhi) / TwoPI));
        }
        h1dNdEtaMCGen[i]->SetTitle("");
        h1dNdEtaMCGen[i]->GetXaxis()->SetTitle("#it{#eta}");
        h1dNdEtaMCGen[i]->GetYaxis()->SetTitle("(1/#it{N}_{evt})(d#it{N}_{ch}/d#it{#eta})");
        h1dNdEtaMCGen[i]->GetYaxis()->SetRangeUser(0.1, 1.2 * h1dNdEtaMCGen[i]->GetMaximum());

        h1dNdEtaEff[i] = (TH1F*)h1dNdEtaMCReco[i]->Clone(Form("h1dNdEtaEff_%d", i));
        h1dNdEtaEff[i]->Divide(h1dNdEtaMCReco[i], h1dNdEtaMCGenAssocReco[i], 1.0, 1.0, "B");
        h1dNdEtaEff[i]->SetDirectory(0);
        h1dNdEtaEff[i]->SetTitle("");
        h1dNdEtaEff[i]->GetXaxis()->SetTitle("#it{#eta}");
        h1dNdEtaEff[i]->GetYaxis()->SetTitle("Acc #times #varepsilon");
        h1dNdEtaEff[i]->GetYaxis()->SetRangeUser(0.85, 0.88);

        h1dNdEtaSigLoss[i] = (TH1F*)h1dNdEtaMCGenAssocReco[i]->Clone(Form("h1dNdEtaSigLoss_%d", i));
        h1dNdEtaSigLoss[i]->Divide(h1dNdEtaMCGenAssocReco[i], h1dNdEtaMCGen[i], 1.0, 1.0, "B");
        h1dNdEtaSigLoss[i]->SetDirectory(0);
        h1dNdEtaSigLoss[i]->SetTitle("");
        h1dNdEtaSigLoss[i]->GetXaxis()->SetTitle("#it{#eta}");
        h1dNdEtaSigLoss[i]->GetYaxis()->SetTitle("Signal loss");
        h1dNdEtaSigLoss[i]->GetYaxis()->SetRangeUser(0.55, 0.8);

        h1dNdEtaEvSplit[i] = (TH1F*)h1dNdEtaSigLoss[i]->Clone(Form("h1dNdEtaEvSplit_%d", i));
        for (int j = 1; j <= h1dNdEtaEvSplit[i]->GetNbinsX(); ++j) {
            h1dNdEtaEvSplit[i]->SetBinContent(j, h1dNdEtaEvSplitTmp->GetBinContent(i+1));
            h1dNdEtaEvSplit[i]->SetBinError(j, h1dNdEtaEvSplitTmp->GetBinError(i+1));
        }
        h1dNdEtaEvSplit[i]->GetYaxis()->SetRangeUser(0.8, 1.1);
        h1dNdEtaEvSplit[i]->GetYaxis()->SetTitle("Event split");

        h1dNdEtaEvLoss[i] = (TH1F*)h1dNdEtaSigLoss[i]->Clone(Form("h1dNdEtaEvLoss_%d", i));
        for (int j = 1; j <= h1dNdEtaEvLoss[i]->GetNbinsX(); ++j) {
            h1dNdEtaEvLoss[i]->SetBinContent(j, h1dNdEtaEvLossTmp->GetBinContent(i+1));
            h1dNdEtaEvLoss[i]->SetBinError(j, h1dNdEtaEvLossTmp->GetBinError(i+1));
        }
        h1dNdEtaEvLoss[i]->GetYaxis()->SetTitle("Event loss");

        h1dNdEtaDataCorr[i] = (TH1F*)h1dNdEtaData[i]->Clone(Form("h1dNdEtaDataCorr_%d", i));
        h1dNdEtaDataCorr[i]->Divide(h1dNdEtaEff[i]);
        h1dNdEtaDataCorr[i]->Divide(h1dNdEtaSigLoss[i]);
        h1dNdEtaDataCorr[i]->Multiply(h1dNdEtaEvLoss[i]);
        h1dNdEtaDataCorr[i]->Divide(h1dNdEtaEvSplit[i]);
        h1dNdEtaDataCorr[i]->SetDirectory(0);
        h1dNdEtaDataCorr[i]->SetTitle("");
        h1dNdEtaDataCorr[i]->GetXaxis()->SetTitle("#it{#eta}");
        h1dNdEtaDataCorr[i]->GetYaxis()->SetTitle("(1/#it{N}_{evt})(d#it{N}_{ch}/d#it{#eta})");
        h1dNdEtaDataCorr[i]->GetYaxis()->SetRangeUser(0.1, 1.1 * h1dNdEtaDataCorr[i]->GetMaximum());

        if (totSyst) {
            h1dNdEtaDataCorrSyst[i] = (TH1F*)h1dNdEtaDataCorr[i]->Clone(Form("h1dNdEtaDataCorrSyst_%d", i));
            for (int bin = 1; bin <= h1dNdEtaDataCorrSyst[i]->GetNbinsX(); ++bin) {
                h1dNdEtaDataCorrSyst[i]->SetBinError(bin, h1dNdEtaDataCorr[i]->GetBinContent(bin) * (totSyst->at(i) / 100.0));
            }
            h1dNdEtaDataCorrSyst[i]->SetDirectory(0);
            h1dNdEtaDataCorrSyst[i]->SetTitle("");
            h1dNdEtaDataCorrSyst[i]->GetXaxis()->SetTitle("#it{#eta}");
            h1dNdEtaDataCorrSyst[i]->GetYaxis()->SetTitle("(1/#it{N}_{evt})(d#it{N}_{ch}/d#it{#eta})");
            h1dNdEtaDataCorrSyst[i]->GetYaxis()->SetRangeUser(0.1, 1.1 * h1dNdEtaDataCorrSyst[i]->GetMaximum());
        }

        std::cout << "Analysis done for mult: " << i << std::endl;
    }

    delete h2VertexZMultData;
    delete h5dNdEtaData;
    delete h2GenMCRecoVertexZvsMult;
    delete h2GenMCAssocRecoVertexZvsMult;
    delete h1MultMCGen;
    delete h6dNdEtaMCReco;
    delete h6dNdEtaMCGenAssocReco;
    delete h5dNdEtaMCGen;
    delete h1MultMCReco;
    delete h1MultMCGenAssocReco;
    delete h1dNdEtaEvSplitTmp;
    delete h1dNdEtaEvLossTmp;

    if (giveArray) {
        std::cout << "Analysis done" << std::endl;

        for (int i = 0; i < nbin_mult; ++i) {
            delete h1dNdEtaData[i];
            delete h1dNdEtaMCGen[i];
            delete h1dNdEtaMCGenAssocReco[i];
            delete h1dNdEtaMCReco[i];
            delete h1dNdEtaEff[i];
            delete h1dNdEtaSigLoss[i];
            delete h1dNdEtaEvLoss[i];
            delete h1dNdEtaEvSplit[i];
            delete h1dNdEtaDataCorrSyst[i];
        }

        return h1dNdEtaDataCorr;
    } else {
        PlotSpectra({h1dNdEtaData});
        /*PlotSpectra({h1dNdEtaMCReco});
        PlotSpectra({h1dNdEtaMCGenAssocReco});
        PlotSpectra({h1dNdEtaMCGen});*/
        PlotSpectra({h1dNdEtaEff});
        PlotSpectra({h1dNdEtaSigLoss});
        PlotSpectra({h1dNdEtaEvLoss});
        PlotSpectra({h1dNdEtaEvSplit});
        if (totSyst)
            PlotSpectra({h1dNdEtaDataCorr, h1dNdEtaDataCorrSyst});
        else
            PlotSpectra({h1dNdEtaDataCorr});

        Float_t meanNch[nbin_mult]{}, meanNchErrStat[nbin_mult]{}, meanNchErrSyst[nbin_mult]{};
        for (int i = 0; i < nbin_mult; ++i) {
            int nbins = 0;
            for(int j = h1dNdEtaDataCorr[i]->GetXaxis()->FindBin(-0.5); j <= h1dNdEtaDataCorr[i]->GetXaxis()->FindBin(0.5)-1; ++j) {
                meanNch[i] += h1dNdEtaDataCorr[i]->GetBinContent(j);
                meanNchErrStat[i] += TMath::Power(h1dNdEtaDataCorr[i]->GetBinError(j),2);
                if (totSyst) meanNchErrSyst[i] += TMath::Power(h1dNdEtaDataCorrSyst[i]->GetBinError(j),2);
                nbins++;
            }
            meanNch[i] /= nbins;
            meanNchErrStat[i] = TMath::Sqrt(meanNchErrStat[i]) / nbins;
            if (totSyst) meanNchErrSyst[i] = TMath::Sqrt(meanNchErrSyst[i]) / nbins;

            if (totSyst)
                std::cout << "Mult bin number: " << i << "\t" << meanNch[i] << "\t +- \t" << meanNchErrStat[i] << "\t +- \t" << meanNchErrSyst[i] << std::endl;
            else
                std::cout << "Mult bin number: " << i << "\t" << meanNch[i] << "\t +- \t" << meanNchErrStat[i] << std::endl;
        }

        for (int i = 0; i < nbin_mult; ++i) {
            delete h1dNdEtaData[i];
            delete h1dNdEtaMCGen[i];
            delete h1dNdEtaMCGenAssocReco[i];
            delete h1dNdEtaMCReco[i];
            delete h1dNdEtaEff[i];
            delete h1dNdEtaSigLoss[i];
            delete h1dNdEtaEvLoss[i];
            delete h1dNdEtaEvSplit[i];
            delete h1dNdEtaDataCorr[i];
            delete h1dNdEtaDataCorrSyst[i];
        }
    }

    return std::nullopt;
}

void nominalAnalysis(std::optional<std::array<double, nbin_mult>> totSyst)
{
    auto c1dNdEtaDataCorr = fullAnalysis(false, {"AnalysisResultsdNdEtaDataWSyst.root", "AnalysisResultsdNdEtaMCWSyst_4.root"}, kNoGenpTVar, PVzCoordNom, std::nullopt, std::nullopt, totSyst);
}

void ptExtrapolation(const std::string& name, std::vector<TH1*>& hSystematic) 
{
    auto c1dNdEtaDataCorrNoGenpTVar = fullAnalysis(true, {"AnalysisResultsdNdEtaDataWSyst.root", "AnalysisResultsdNdEtaMCWSyst_4.root"}, kNoGenpTVar, PVzCoordNom, std::nullopt, std::nullopt, std::nullopt);
    auto c1dNdEtaDataCorrGenpTUp = fullAnalysis(true, {"AnalysisResultsdNdEtaDataWSyst.root", "AnalysisResultsdNdEtaMCWSyst_4.root"}, kGenpTup, PVzCoordNom, std::nullopt, std::nullopt, std::nullopt);
    auto c1dNdEtaDataCorrGenpTDown = fullAnalysis(true, {"AnalysisResultsdNdEtaDataWSyst.root", "AnalysisResultsdNdEtaMCWSyst_4.root"}, kGenpTdown, PVzCoordNom, std::nullopt, std::nullopt, std::nullopt);

    if (c1dNdEtaDataCorrNoGenpTVar && c1dNdEtaDataCorrGenpTUp && c1dNdEtaDataCorrGenpTDown)
        PlotSpectraPerMult({ *c1dNdEtaDataCorrNoGenpTVar, *c1dNdEtaDataCorrGenpTUp, *c1dNdEtaDataCorrGenpTDown }, legendPtExtrapolation, name, hSystematic);
}

void variationVertexCut(const std::string& name, std::vector<TH1*>& hSystematic) {
    auto c1dNdEtaDataCorrZ10cm = fullAnalysis(true, {"AnalysisResultsdNdEtaDataWSyst.root", "AnalysisResultsdNdEtaMCWSyst_4.root"}, kNoGenpTVar, PVzCoordNom, std::nullopt, std::nullopt, std::nullopt);
    auto c1dNdEtaDataCorrZ7cm = fullAnalysis(true, {"AnalysisResultsdNdEtaDataWSyst.root", "AnalysisResultsdNdEtaMCWSyst_4.root"}, kNoGenpTVar, PVzCoordVar[0], std::nullopt, std::nullopt, std::nullopt);
    auto c1dNdEtaDataCorrZ5cm = fullAnalysis(true, {"AnalysisResultsdNdEtaDataWSyst.root", "AnalysisResultsdNdEtaMCWSyst_4.root"}, kNoGenpTVar, PVzCoordVar[1], std::nullopt, std::nullopt, std::nullopt);

    if (c1dNdEtaDataCorrZ10cm && c1dNdEtaDataCorrZ7cm && c1dNdEtaDataCorrZ5cm)
        PlotSpectraPerMult({ *c1dNdEtaDataCorrZ10cm, *c1dNdEtaDataCorrZ7cm, *c1dNdEtaDataCorrZ5cm }, legendVariationVertexCut, name, hSystematic);
}

void variationPhiRange(const std::string& name, std::vector<TH1*>& hSystematic) 
{
    auto c1dNdEtaDataCorrFullPhiRange = fullAnalysis(true, {"AnalysisResultsdNdEtaDataWSyst.root", "AnalysisResultsdNdEtaMCWSyst_4.root"}, kNoGenpTVar, PVzCoordNom, std::nullopt, std::nullopt, std::nullopt);
    auto c1dNdEtaCorr0PiHalf = fullAnalysis(true, {"AnalysisResultsdNdEtaDataWSyst.root", "AnalysisResultsdNdEtaMCWSyst_4.root"}, kNoGenpTVar, PVzCoordNom, phiRanges[0], std::nullopt, std::nullopt);
    auto c1dNdEtaCorrPiHalfPi = fullAnalysis(true, {"AnalysisResultsdNdEtaDataWSyst.root", "AnalysisResultsdNdEtaMCWSyst_4.root"}, kNoGenpTVar, PVzCoordNom, phiRanges[1], std::nullopt, std::nullopt);
    auto c1dNdEtaCorrPi3PiHalf = fullAnalysis(true, {"AnalysisResultsdNdEtaDataWSyst.root", "AnalysisResultsdNdEtaMCWSyst_4.root"}, kNoGenpTVar, PVzCoordNom, phiRanges[2], std::nullopt, std::nullopt);
    auto c1dNdEtaCorr3PiHalf2Pi = fullAnalysis(true, {"AnalysisResultsdNdEtaDataWSyst.root", "AnalysisResultsdNdEtaMCWSyst_4.root"}, kNoGenpTVar, PVzCoordNom, phiRanges[3], std::nullopt, std::nullopt);
    auto c1dNdEtaCorrPiQuarter3PiQuarter = fullAnalysis(true, {"AnalysisResultsdNdEtaDataWSyst.root", "AnalysisResultsdNdEtaMCWSyst_4.root"}, kNoGenpTVar, PVzCoordNom, phiRanges[4], std::nullopt, std::nullopt);
    auto c1dNdEtaCorr5PiQuarter7PiQuarter = fullAnalysis(true, {"AnalysisResultsdNdEtaDataWSyst.root", "AnalysisResultsdNdEtaMCWSyst_4.root"}, kNoGenpTVar, PVzCoordNom, phiRanges[5], std::nullopt, std::nullopt);

    if (c1dNdEtaDataCorrFullPhiRange && c1dNdEtaCorr0PiHalf && c1dNdEtaCorrPiHalfPi && c1dNdEtaCorrPi3PiHalf && c1dNdEtaCorr3PiHalf2Pi && c1dNdEtaCorrPiQuarter3PiQuarter && c1dNdEtaCorr5PiQuarter7PiQuarter)
        PlotSpectraPerMult({ *c1dNdEtaDataCorrFullPhiRange, *c1dNdEtaCorr0PiHalf, *c1dNdEtaCorrPiHalfPi, *c1dNdEtaCorrPi3PiHalf, *c1dNdEtaCorr3PiHalf2Pi, 
                             *c1dNdEtaCorrPiQuarter3PiQuarter, *c1dNdEtaCorr5PiQuarter7PiQuarter }, legendVariationPhiRange, name, hSystematic);
}

void variationPartSpecies(const std::string& name, std::vector<TH1*>& hSystematic) 
{
    auto c1dNdEtaDataCorrAllPart = fullAnalysis(true, {"AnalysisResultsdNdEtaDataWSyst.root", "AnalysisResultsdNdEtaMCWSyst_4.root"}, kNoGenpTVar, PVzCoordNom, std::nullopt, std::nullopt, std::nullopt);
    auto c1dNdEtaDataCorrPartSpecies1 = fullAnalysis(true, {"AnalysisResultsdNdEtaDataWSyst.root", "AnalysisResultsdNdEtaMCWSyst_4.root"}, kNoGenpTVar, PVzCoordNom, std::nullopt, partToChange[0], std::nullopt);
    auto c1dNdEtaDataCorrPartSpecies2 = fullAnalysis(true, {"AnalysisResultsdNdEtaDataWSyst.root", "AnalysisResultsdNdEtaMCWSyst_4.root"}, kNoGenpTVar, PVzCoordNom, std::nullopt, partToChange[1], std::nullopt);
    auto c1dNdEtaDataCorrPartSpecies3 = fullAnalysis(true, {"AnalysisResultsdNdEtaDataWSyst.root", "AnalysisResultsdNdEtaMCWSyst_4.root"}, kNoGenpTVar, PVzCoordNom, std::nullopt, partToChange[2], std::nullopt);
    auto c1dNdEtaDataCorrPartSpecies4 = fullAnalysis(true, {"AnalysisResultsdNdEtaDataWSyst.root", "AnalysisResultsdNdEtaMCWSyst_4.root"}, kNoGenpTVar, PVzCoordNom, std::nullopt, partToChange[3], std::nullopt);
    auto c1dNdEtaDataCorrPartSpecies5 = fullAnalysis(true, {"AnalysisResultsdNdEtaDataWSyst.root", "AnalysisResultsdNdEtaMCWSyst_4.root"}, kNoGenpTVar, PVzCoordNom, std::nullopt, partToChange[4], std::nullopt);
    auto c1dNdEtaDataCorrPartSpecies6 = fullAnalysis(true, {"AnalysisResultsdNdEtaDataWSyst.root", "AnalysisResultsdNdEtaMCWSyst_4.root"}, kNoGenpTVar, PVzCoordNom, std::nullopt, partToChange[5], std::nullopt);
    auto c1dNdEtaDataCorrPartSpecies7 = fullAnalysis(true, {"AnalysisResultsdNdEtaDataWSyst.root", "AnalysisResultsdNdEtaMCWSyst_4.root"}, kNoGenpTVar, PVzCoordNom, std::nullopt, partToChange[6], std::nullopt);
    auto c1dNdEtaDataCorrPartSpecies8 = fullAnalysis(true, {"AnalysisResultsdNdEtaDataWSyst.root", "AnalysisResultsdNdEtaMCWSyst_4.root"}, kNoGenpTVar, PVzCoordNom, std::nullopt, partToChange[7], std::nullopt);
    auto c1dNdEtaDataCorrPartSpecies9 = fullAnalysis(true, {"AnalysisResultsdNdEtaDataWSyst.root", "AnalysisResultsdNdEtaMCWSyst_4.root"}, kNoGenpTVar, PVzCoordNom, std::nullopt, partToChange[8], std::nullopt);
    auto c1dNdEtaDataCorrPartSpecies10 = fullAnalysis(true, {"AnalysisResultsdNdEtaDataWSyst.root", "AnalysisResultsdNdEtaMCWSyst_4.root"}, kNoGenpTVar, PVzCoordNom, std::nullopt, partToChange[9], std::nullopt);

    if (c1dNdEtaDataCorrAllPart && c1dNdEtaDataCorrPartSpecies1 && c1dNdEtaDataCorrPartSpecies2 && c1dNdEtaDataCorrPartSpecies3 && c1dNdEtaDataCorrPartSpecies4 && c1dNdEtaDataCorrPartSpecies5 && 
        c1dNdEtaDataCorrPartSpecies6 && c1dNdEtaDataCorrPartSpecies7 && c1dNdEtaDataCorrPartSpecies8 && c1dNdEtaDataCorrPartSpecies9 && c1dNdEtaDataCorrPartSpecies10)
        PlotSpectraPerMult({ *c1dNdEtaDataCorrAllPart, *c1dNdEtaDataCorrPartSpecies1, *c1dNdEtaDataCorrPartSpecies2, *c1dNdEtaDataCorrPartSpecies3, *c1dNdEtaDataCorrPartSpecies4, 
                             *c1dNdEtaDataCorrPartSpecies5, *c1dNdEtaDataCorrPartSpecies6, *c1dNdEtaDataCorrPartSpecies7, *c1dNdEtaDataCorrPartSpecies8, 
                             *c1dNdEtaDataCorrPartSpecies9, *c1dNdEtaDataCorrPartSpecies10 }, legendVariationPartSpecies, name, hSystematic);
}

void variationTrackType(const std::string& name, std::vector<TH1*>& hSystematic)
{
    auto c1dNdEtaDataCorrITSibAny = fullAnalysis(true, {"AnalysisResultsdNdEtaDataWSyst.root", "AnalysisResultsdNdEtaMCWSyst_4.root"}, kNoGenpTVar, PVzCoordNom, std::nullopt, std::nullopt, std::nullopt);
    auto c1dNdEtaDataCorrITSallAny = fullAnalysis(true, {"AnalysisResultsdNdEtaDataWSyst2.root", "AnalysisResultsdNdEtaMCWSyst2_4.root"}, kNoGenpTVar, PVzCoordNom, std::nullopt, std::nullopt, std::nullopt);
    auto c1dNdEtaDataCorrITSall7Layers = fullAnalysis(true, {"AnalysisResultsdNdEtaDataWSyst3.root", "AnalysisResultsdNdEtaMCWSyst3_4.root"}, kNoGenpTVar, PVzCoordNom, std::nullopt, std::nullopt, std::nullopt);

    if (c1dNdEtaDataCorrITSibAny && c1dNdEtaDataCorrITSallAny && c1dNdEtaDataCorrITSall7Layers)
        PlotSpectraPerMult({ *c1dNdEtaDataCorrITSibAny, *c1dNdEtaDataCorrITSallAny, *c1dNdEtaDataCorrITSall7Layers }, legendVariationTrackType, name, hSystematic);
}

void computedNdEtaWPhiWSyst5(Int_t mode = 0) 
{
    std::vector<std::pair<std::string, std::function<void(const std::string& name, std::vector<TH1*>&)>>> sourceFunctions {
        {"ptExtrapolation", ptExtrapolation}, 
        {"variationVertexCut", variationVertexCut}, 
        {"variationPhiRange", variationPhiRange}, 
        {"variationPartSpecies", variationPartSpecies},
        {"variationTrackType", variationTrackType}
    };

    std::vector<std::vector<TH1*>> hSystematics;

    std::array<Double_t, nbin_mult> totSyst;

    switch (mode) {
        case 0:
            nominalAnalysis(std::nullopt);
            break;
        case 1:
            hSystematics.push_back(std::vector<TH1*>(nbin_mult, nullptr));
            ptExtrapolation("ptExtrapolation", hSystematics.back());
            break;
        case 2:
            hSystematics.push_back(std::vector<TH1*>(nbin_mult, nullptr));
            variationVertexCut("variationVertexCut", hSystematics.back());
            break;
        case 3:
            hSystematics.push_back(std::vector<TH1*>(nbin_mult, nullptr));
            variationPhiRange("variationPhiRange", hSystematics.back());
            break;
        case 4:
            hSystematics.push_back(std::vector<TH1*>(nbin_mult, nullptr));
            variationPartSpecies("variationPartSpecies", hSystematics.back());
            break;
        case 5:
            hSystematics.push_back(std::vector<TH1*>(nbin_mult, nullptr));
            variationTrackType("variationTrackType", hSystematics.back());
            break;
        case 6:
            for (auto& [name, func] : sourceFunctions) {
                std::cout << "Processing source: " << name << std::endl;
                hSystematics.push_back(std::vector<TH1*>(nbin_mult, nullptr));
                std::cout << "Calling function for source: " << name << std::endl;
                func(name, hSystematics.back());
                std::cout << "Finished processing source: " << name << std::endl;
            }
            SumSystematicsPerMult(hSystematics);
            //writeSystematicsTxt("dNdEtaSystematics.txt", {"track-type variation"}, hSystematics);
            writeSystematicsTxt("dNdEtaSystematics.txt", legendAllVariation, hSystematics);
            break;
        case 7:
            GetHistosFromCanvasFile(systFileList, hSystematics);
            SumSystematicsPerMult(hSystematics);
            totSyst = writeSystematicsTxt("dNdEtaSystematics.txt", legendAllVariation, hSystematics);
            nominalAnalysis(totSyst);
            break;
        default:
            std::cerr << "Invalid mode selected." << std::endl;
            break;
    }
}

