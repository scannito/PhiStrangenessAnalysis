#include "../Codes/CanvasManager.h"

void esporta_fit(const char* rootFilePath = "../DataFile/pp/DeltaY/Purities/Data_K0SPurity.root",
                 const char* pdfOutPath = "tutti_i_fit.pdf")
{
  std::unique_ptr<TFile> f(TFile::Open(rootFilePath));
  if (!f || f->IsZombie()) {
    std::cerr << "Errore: impossibile aprire " << rootFilePath << std::endl;
    return;
  }
  CanvasManager::ExportFileCanvasesToPdf(f.get(), pdfOutPath);
}
