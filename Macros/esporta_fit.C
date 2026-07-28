void esporta_fit()
{
  // Apri il tuo file ROOT
  TFile* f = TFile::Open("../DataFile/pp/DeltaY/Purities/Data_K0SPurity.root");

  // Crea una canvas fittizia per aprire il file PDF multipagina
  TCanvas* dummy = new TCanvas("dummy");
  dummy->Print("tutti_i_fit.pdf["); // La parentesi quadra aperta INIZIA il PDF

  // Itera su tutte le chiavi nel file
  TIter next(f->GetListOfKeys());
  TKey* key;
  while ((key = (TKey*)next())) {
    TClass* cl = gROOT->GetClass(key->GetClassName());

    // Controlla se l'oggetto è una TCanvas
    if (!cl->InheritsFrom("TCanvas"))
      continue;

    // Leggi e stampa la canvas nel PDF
    TCanvas* c = (TCanvas*)key->ReadObj();
    c->Draw();
    c->Print("tutti_i_fit.pdf");
  }

  // Chiudi il file PDF
  dummy->Print("tutti_i_fit.pdf]"); // La parentesi quadra chiusa TERMINA il PDF

  f->Close();
}
