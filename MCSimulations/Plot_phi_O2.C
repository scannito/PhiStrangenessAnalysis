#include <vector>

#include <Riostream.h>
#include <TH1.h>
#include <TH2.h>
#include <TH3.h>
#include <TGraphErrors.h>
#include <TMultiGraph.h>
#include <TFile.h>
#include <TCanvas.h>
#include <TLegend.h>
#include <TMath.h>
#include <TROOT.h>

using namespace std;

class ParticleType {
public:
   enum Type {
      K0S=1,
      Lambda,
      Xi,
      Omega,
      Phi,
      Proton,
      Pion,
      Unknown
    };

   static const char* ToString(Type type) {
      switch (type) {
         case K0S:      return "K0S";
         case Lambda:   return "Lambda";
         case Xi:       return "Xi";
         case Omega:    return "Omega";
         case Phi:      return "Phi";
         case Proton:   return "Proton";
         case Pion:     return "Pion";
         default:       return "Unknown";
      }
   }

   static const char* ToSymbol(Type type) {
      switch (type) {
         case K0S:      return "2K^{0}_{S}";
         case Lambda:   return "(#Lambda + #bar{#Lambda})";
         case Xi:       return "(#Xi^{#minus} + #bar{#Xi}^{+})";
         case Omega:    return "(#Omega^{#minus} + #bar{#Omega}^{+})";
         case Phi:      return "2#Phi";
         case Proton:   return "(p + #bar{p})";
         case Pion:     return "(#pi^{+} + #pi^{#minus})";
         default:       return "Unknown";
      }
   }

   static Type FromPDG(int pdgCode) {
      static const std::map<int, Type> pdgToType = {
         {310, K0S},
         {3122, Lambda},
         {3312, Xi},
         {3334, Omega},
         {333, Phi},
         {2212, Proton},
         {211, Pion}     
      };

      int absCode = std::abs(pdgCode); 
      auto it = pdgToType.find(absCode);
      return (it != pdgToType.end()) ? it->second : Unknown;
   }
};

vector<Int_t> PDGCodes = {310, 3122, 3312, 3334, 333, 2212, 211};

TH2D* ProjectToPart(TH3D* h3, ParticleType::Type type) {
    TH3D* h3Part = (TH3D*)h3->Clone(Form("h3Part%d",(int)type));
    h3Part->GetZaxis()->SetRange((int)type, (int)type);
    TH2D* h2Part = (TH2D*)h3Part->Project3D("yx");
    return h2Part;
}

TGraphErrors* FromHistoToGraph(TH2D* h2Mult, TH3D* h3Part, Int_t deltaYclass, ParticleType::Type typeNum, ParticleType::Type typeDen) {
    vector<Double_t> mult, errmult;
    for (int i = 1; i <= h2Mult->GetNbinsX(); i++) {
        TH1D* h1Mult = h2Mult->ProjectionY(Form("h%d",i), i, i);
        mult.emplace_back(h1Mult->GetMean());
        errmult.emplace_back(h1Mult->GetMeanError());
        delete h1Mult;
    }

    TH2D* h2NPartNum = ProjectToPart(h3Part, typeNum);
    TH2D* h2NPartDen = ProjectToPart(h3Part, typeDen);

    TH2D* h2Ratio = (TH2D*)h2NPartNum->Clone("h2Ratio");
    if (typeNum == ParticleType::K0S || typeNum == ParticleType::Phi) h2Ratio->Divide(h2NPartNum, h2NPartDen, 2, 1, "B");
    else h2Ratio->Divide(h2NPartNum, h2NPartDen, 1, 1, "B");

    if (!h2Ratio) {
        cout << "Error: the ratio histogram is null." << endl;
        return nullptr;
    } else if (h2Ratio->GetNbinsX() != h2Mult->GetNbinsX()) {
        cout << "Error: the number of bins in the ratio histogram is different from the number of bins in the multiplicity histogram." << endl;
        return nullptr;
    } else {};

    vector<Double_t> ratio, errratio;
    for (int i = 1; i <= h2Ratio->GetNbinsX(); i++) {
        ratio.emplace_back(h2Ratio->GetBinContent(i, deltaYclass));
        errratio.emplace_back(h2Ratio->GetBinError(i, deltaYclass));
    }

    TGraphErrors* g = new TGraphErrors(h2Mult->GetNbinsX(), mult.data(), ratio.data(), errmult.data(), errratio.data());
    return g;
}

void GraphFeatures(TGraphErrors* graph, Style_t markstyle, Color_t markcolor, Size_t marksize, Style_t linestyle, Color_t linecolor, Width_t linewidth, Style_t fillstyle, Color_t fillcolor, Float_t alpha, TMultiGraph* mg) 
{															
    graph->SetMarkerStyle(markstyle);
    graph->SetMarkerColor(markcolor);
    graph->SetMarkerSize(marksize);
    graph->SetLineStyle(linestyle);
    graph->SetLineColor(linecolor);
    graph->SetLineWidth(linewidth);
    graph->SetFillStyle(fillstyle);
    graph->SetFillColorAlpha(fillcolor,alpha);

    mg->Add(graph);
}

TMultiGraph* FromHistoToMultiGraph(vector<TH2D*>& h2Mult, vector<TH3D*>& h3Part, ParticleType::Type typeNum, ParticleType::Type typeDen = ParticleType::Pion) {
    TMultiGraph* mg = new TMultiGraph();

    for (int i = 0; i < nDeltaYClasses; i++) {
        for (int j = 0; j < h2Mult.size(); j++) {
            TGraphErrors* g = FromHistoToGraph(h2Mult[j], h3Part[j], i+1, typeNum, typeDen);
            GraphFeatures(g, Markers[j], Colors[i], 2, LineStyles[j], Colors[i], 2, FillStyles[j], Colors[i], 0.40, mg);
        }
        /*TGraphErrors* gPythiaMonash = FromHistoToGraph(h2Mult[0], h3Part[0], i, typeNum, typeDen);
        TGraphErrors* gPythiaRopes = FromHistoToGraph(h2Mult[1], h3Part[1], i, typeNum, typeDen);
        TGraphErrors* gEpos4 = FromHistoToGraph(h2Mult[2], h3Part[2], i, typeNum, typeDen);
        if (i == 1) {
            GraphFeatures(gPythiaMonash, 34, kBlack, 2, 3, kBlack, 2, 3001, kBlack, 0.40, mg);
            GraphFeatures(gPythiaRopes, 33, kBlack, 2, 1, kBlack, 2, 3002, kBlack, 0.40, mg);
            GraphFeatures(gEpos4, 29, kBlack, 2 ,7, kBlack, 2, 3003, kBlack, 0.40, mg);
        }
        else {
            GraphFeatures(gPythiaMonash, 34, kRed+1, 2, 3, kRed+1, 2, 3001, kRed+1, 0.40, mg);
            GraphFeatures(gPythiaRopes, 33, kRed+1, 2, 1, kRed+1, 2, 3002, kRed+1, 0.40, mg);
            GraphFeatures(gEpos4, 29, kRed+1, 2 ,7, kRed+1, 2, 3003, kRed+1, 0.40, mg);
        }*/
    }
    
    return mg;
}

void PlotFeatures(TCanvas* c, TMultiGraph* mg, ParticleType::Type typeNum, ParticleType::Type typeDen = ParticleType::Pion) {
    c->cd();
    c->SetMargin(0.16,0.03,0.15,0.06);

    mg->GetXaxis()->SetTitleSize(0.05);
    mg->GetXaxis()->SetTitleOffset(1.3);
    mg->GetXaxis()->SetLabelOffset(0.);
    mg->GetXaxis()->SetLabelSize(0.045);
    mg->GetXaxis()->SetTitle("#LTdN_{ch}/d#eta#GT_{|#eta|<0.5, #phi}");
    mg->GetYaxis()->SetTitle(Form("%s / %s", ParticleType::ToSymbol(typeNum), ParticleType::ToSymbol(typeDen)));
    mg->GetYaxis()->SetTitleOffset(1.5);
    mg->GetYaxis()->SetTitleSize(0.05);
    mg->GetYaxis()->SetLabelSize(0.045);
    mg->GetYaxis()->SetMaxDigits(4);
}

void AddLegend(TMultiGraph* mg) {
    TLegend* leg = new TLegend(0.6, 0.6, 0.9, 0.9);
    leg->SetHeader("pp, #sqrt{s} = 13 TeV, |#it{y}|<0.5");
    leg->AddEntry(mg->GetListOfGraphs()->At(0), "Pythia 8.3 Monash", "lpf");
    leg->AddEntry(mg->GetListOfGraphs()->At(1), "Pythia 8.3 Ropes", "lpf");
    leg->AddEntry(mg->GetListOfGraphs()->At(2), "EPOS4", "lpf");
    leg->SetLineWidth(0);
    leg->Draw("same");
}

void Plot_phi_O2() {
    vector<TFile*> files;
    files.emplace_back(new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/Analysis_phi_O2/analysis_pythiaMonash.root"));
    files.emplace_back(new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/Analysis_phi_O2/analysis_pythiaRopes_nophicalib.root"));
    files.emplace_back(new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/Analysis_phi_O2/analysis_epos4_nophicalib.root"));

    vector<TH2D*> h2NCharged;
    vector<TH3D*> h3NPart;
    vector<TH3D*> h3NPairs;
    for (TFile* file : files) {
        h2NCharged.emplace_back((TH2D*)file->Get("hNCharged"));
        h3NPart.emplace_back((TH3D*)file->Get("hNPart"));
        h3NPairs.emplace_back((TH3D*)file->Get("hNPairs"));
    }
    
    files.clear();
    files.shrink_to_fit();

    TFile* outputfile = new TFile("analysisOutput.root", "RECREATE");

    for (Int_t code : PDGCodes) {
        ParticleType::Type type = ParticleType::FromPDG(code);
        if (type == ParticleType::Pion) continue;

        TMultiGraph* mgPart = FromHistoToMultiGraph(h2NCharged, h3NPart, type);
        TCanvas* cPart = new TCanvas(Form("c%sPart", ParticleType::ToString(type)), Form("c%sPart", ParticleType::ToString(type)), 850, 850);
        cPart->cd();
        mgPart->Draw("A3L");
        PlotFeatures(cPart, mgPart, type);
        AddLegend(mgPart);
        cPart->Write();
        delete cPart;

        TMultiGraph* mgPairs = FromHistoToMultiGraph(h2NCharged, h3NPairs, type);
        TCanvas* cPairs = new TCanvas(Form("c%sPairs", ParticleType::ToString(type)), Form("c%sPairs", ParticleType::ToString(type)), 850, 850);
        cPairs->cd();
        mgPairs->Draw("A3L");
        PlotFeatures(cPairs, mgPairs, type);
        AddLegend(mgPairs);
        cPairs->Write();
        delete cPairs;
    }

    outputfile->Close();

    return;
}