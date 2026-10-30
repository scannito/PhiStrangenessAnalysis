# Roadmap: cosa resta da fare nel progetto dopo la ricerca RooFit

Scritta il 15 ottobre 2026 dalle conclusioni di `00-summary.md` (sezioni 1-3) e delle note finali 09, 11, 12; ogni voce rimanda
alla nota con le prove. **Rivista voce per voce il 27 ottobre 2026** sul codice di `main` (bf3437a), sulle configurazioni e sui
file prodotti: le voci fatte sono state tolte (sotto, dove sono raccontate), le altre divise in due gruppi, secondo che servano
codice o scelte.

**Tolte perché fatte**, e dove ne resta il ragionamento:

- correzioni lette come `TH1` (era 0.1): `DESIGN_HISTORY.md`, *Corrections read as TH1, not TH1F*;
- stato del fit per cella e record per cella (1.1, 1.5): *Fit quality recorded, ...*, e README *Fit quality*;
- override per indice del K0S che colpivano celle diverse nei due binning (2.4): risolto alla radice da *Objects and overrides
  named by interval*;
- `Offset("bin")`, refit, partenze `config`/`yields`/`staged`, esito per cella (3.1, 3.2, 3.6 e parte della 3.4): *Several starts
  per fit*; il resto della fase 3 è in A10-A11;
- estrapolazione dei pioni (4.3): *A second fit for the high side, and the blast-wave for the π* (blast-wave su 0.2-3.0 GeV/c);
- parser dei modelli tenuto a mano, polinomi per ordine, `Bernstein<n>` (fase 5): *Fit quality: which limit; polynomials by order*;
- range dell'osservabile del Xi (2.2): tenuto [1.29, 1.36], anche in `fitConfig_modelStudy.json`.

---------------------------------------------------------------------------------------------------------------------------------

## A. Serve codice (o CMake)

Nessuna di queste cambia la fisica, salvo dove detto (A9, A10-A11).

### Bug e visibilità

**A1. Conteggio di `RooFitMessages`** (era 0.2): il "(remaining N suppressed)" di RooFit è sbagliato di 2 (nota 08, 8.12).
`RooFitMessages.h` esiste solo sul ramo `wip/roofit-messages`, non su `main`.

**A2. Il sommario di fine run non conta i messaggi di ROOT e RooFit** (era 0.3): dice "No warnings" con decine di migliaia di
righe RooFit nel log (nota 11). È anche in `DESIGN_NOTES.md` sezione 5, *ROOT's own warnings are not in the end-of-run summary*.

**A3. Una cella di purezza vuota riceve un valore inventato** (era 0.4, riaperta il 27 ottobre). Il NaN di ottobre non c'è più, ma
al suo posto c'è un numero che non viene dai dati: nella purezza del 2026 (`Data_2026_HighIR_updated_Purity.root`, produzione del
22 ottobre) le tre celle `pi_tof` vuote del binning di analisi (pT 0.2-0.3 GeV/c, molteplicità 0-1, 40-50, 50-70) valgono tutte
0.967232 ± 0, e lo stesso valore con errore zero è in 14 celle del binning sorgente. Il fit è segnalato (`empty` nel `FitQuality`
e nel sommario), il valore no. Oggi non entra in nessun risultato: le correlazioni usano `pi_tpc` (`purity_sources`) e
`uploadToCCDB.C` non carica purezze. Da fare: scrivere per una cella vuota un valore che non si possa scambiare per una purezza,
e che chi la legge rifiuti, non quello che il minimizzatore lascia senza dati.

**A4. Controllo di finitezza a valle** (era 0.6). Un errore di purezza NaN finiva come errore di bin NaN negli spettri dei pioni
e né il fit di ROOT né `SpectrumExtrapolator` lo scartavano: era la causa principale dell'"amplificazione" dei pioni (nota 11,
sezioni 8-9). Il 27 ottobre nessun file di purezza o di risultati finali (2024 e 2026) ha un bin non finito, ma solo perché i NaN
sono spariti a monte: in `CorrelationTaskBase` e `SpectrumExtrapolator` non c'è nessun controllo, e il prossimo NaN passerebbe
come il primo.

**A5. Path e stderr** (era 0.5). I path delle configurazioni sono relativi alla directory corrente: `runAnalysis.sh` fa `cd`
nella propria, quindi dallo script funziona, il binario lanciato da un'altra directory no. Gli errori di `TGaxis` su stderr non
sono stati riverificati.

**A6. Validatore del modello prima di `fitTo`** (era 1.2): limiti, valore iniziale dentro i limiti, pdf finita e non negativa ai
valori iniziali. Circa 30 righe, codice in nota 10 sezione 6. Oggi ci sono solo due controlli particolari: la soglia dei modelli
con radici o potenze di (M - soglia) e i coefficienti del Bernstein.

**A7. Bontà del fit** (era 1.3): chi2 di Pearson su tutte le bin, ndf = bin - parametri liberi (calibrato sui toy, nota 05).
Oggi nei file non c'è nessun chi2 dei fit di massa.

**A8. Asse dei pull** (resto della 1.4): fisso a ±5 (`DynamicRooFitter.h`, `FormatBottomPadAxes`). La normalizzazione delle
componenti uguale a quella del totale è fatta (`RelativeExpected`).

### Procedura di fit

**A9. `snap_to_bin` tollerante** — *superata il 30 ottobre 2026: l'opzione è stata tolta* (`DESIGN_HISTORY.md`, *snap_to_bin
removed*): gli integrali del segnale e del fondo sono integrali delle pdf e non hanno bisogno di bordi di bin, e allargare la
finestra al bin la allontana da quella del treno. Il testo che segue è com'era. (era 2.6, nota 04): `FindFixBin` senza tolleranza. Cambia la fisica: con il codice attuale
pi_tpc [-3, 3] diventerebbe [-3, 3.2], quindi va fatto insieme alla scelta delle finestre.

**A10-A11. Partenze casuali, partenza dalle celle vicine, test di accettazione, escalation a xRooFit** (resto delle 3.3, 3.4,
3.5). **Lasciate fuori di proposito il 23 ottobre** (`DESIGN_HISTORY.md`, *Several starts per fit*, "Left out, on purpose"):
rispetto ai minimi migliori dello studio dei modelli le tre partenze attuali bastavano (sopra il migliore di più di 1 in 13 fit K0S
su 653, 36 Xi su 844, 110 Omega su 2155, 9 pi_tpc su 460, 21 pi_tof su 698), e ognuna delle altre porta un costo: le casuali un
seme e un numero per famiglia da dichiarare, quella dalle celle vicine una dipendenza dall'ordine del ciclo, xRooFit una libreria
(`RooFitXRooFit` in CMake, oggi assente) e il `gDirectory` nullo attorno alla chiamata (altrimenti restituisce il fit di un'altra
cella, nota 11). Tornano **se una misura le chiede**. Quello che potrebbe chiederle:
- i toy della nota 12 (sezioni 6-7, 40 celle delle famiglie a rischio, 200 toy ciascuna), dove una partenza sola resta in un
  minimo sbagliato: K0S pT 3.0-4.2 5.2%, K0S pT 4.2-6.4 38.8% (fino all'84% in una cella), Xi pT 1.2-1.6 25%, Xi pT 1.6-2.0 78%;
  ne venivano 2-4 partenze casuali per K0S, pi_tpc e pi_tof, 4 per Xi pT 1.2-1.6, e per Xi pT 1.6-2.0 (e Xi 2026 pT 2.0-3.0) la
  mediana delle altre classi di molteplicità più 8 casuali. Misurati con la procedura di allora, non con le tre partenze di oggi:
  rifarli con queste è il modo di decidere;
- sulle celle reali, 20 partenze lasciavano 17 celle sorgente su 2620 sopra il migliore di 50 (Xi 2026 pT alto, S -1.1/-1.4%,
  nota 09 9.11).
Il test `covQual()==3 && edm()<0.01` (nota 09 sezione 2; lo status 3 da solo non è un criterio) serve a decidere quando
scalare: senza escalation non ha un uso. Costo misurato: il passo di purezza da 6-13 s a circa 18 s con le tre partenze di oggi;
con 2 casuali ovunque e 8 per Xi pT 1.2-2.0 la procedura del 15 ottobre andava da 11.7 a 48.9 s (`scratch/coord-timing/`).

**A12. `IntegrateBins` come variazione sistematica, non come default.** Sui toy elimina il bias del centro bin (Xi alta
statistica da +2.6σ a +0.07σ, pi_tpc da +11.7σ a +0.05σ). Un solo fit IB senza refit converge nel 91-92% dei toy con errori
giusti, a 14-30 volte la CPU; con il refit finisce con status ≠ 0 nel 77-88% dei toy. Sulle celle reali S si sposta di poco
(mediana +0.012%, p90 0.43%, oltre l'1% solo in 2 celle Xi 2026 a pT alto, nota 09 9.11.4). `obs->setBins(n)` uguale alle bin
dell'istogramma nel range. Aspetta il driver delle sistematiche.

**A13. Range di fit distinto dal range del grafico** (`DESIGN_NOTES.md` sezione 3): oggi coincidono.

---------------------------------------------------------------------------------------------------------------------------------

## B. Servono scelte di fisica, o portare nella repo ciò che è nel sandbox

**B1. Portare `fitConfig_modelStudy.json` nella repo.** È nel sandbox (`~/alice/claude-sandbox/sb-fitcfg/JSONConfigs/`) e
contiene le scelte delle vecchie 2.1 e 2.3, che nella repo non ci sono:
- limiti delle rese a 1e11 (nella repo `nsig` fino a 1e8 e `nbkg` fino a 2.5e7 per K0S e Xi, 1e10 e 2e9 per i pioni; quelli
  di `nsig` tagliavano la resa in 18 celle K0S e 2 pi_tpc, nota 09 sezioni 1.3 e 9);
- forme: `sigma_sig` del Xi da 0.5 MeV (era 2 MeV, al limite in 439 celle sorgente su 520), `n` delle DSCB fino a 1e3 (era 150),
  K0S DSCB + Gaussiana con Chebychev3, pendenza di pi_tof libera in [-5, 5], gli override per cella dello studio dei modelli.

Sul 2026 funziona (K0S nessun fit segnalato su 100 nel binning di analisi). Sul 2024 meno: un quarto delle celle K0S e pi_tof
segnalate (25 su 100 ciascuna), soprattutto parametri al limite, perché gli override sono stati scelti sulle celle del 2026.
L'effetto netto della purezza sul K0S/π però coincide fra i due anni entro lo 0.4% (B2). Da decidere: portarlo così, o prima
sistemarlo sul 2024.

**B2. `apply_purity`.** Nella repo è `false`, quindi i risultati non sono corretti per la purezza. Misurato il 26 ottobre nel
sandbox (B1, efficienza applicata): la purezza abbassa il 2K0S/π estrapolato del 6% a molteplicità 0-1% e del 4% a 70-100%, uguale
nel 2024 e nel 2026 entro lo 0.4% (procedura offline), e nel 2024 uguale nelle tre procedure. Il confronto con la preliminare 2024 si fa con la purezza (le vecchie
rese venivano da fit).

**B3. Errori del φ con efficienza online** (era 2.5, nota 05). Misurato il 24 ottobre 2026 sul φ del wagon online (produzione del
22 ottobre, mult 15-20%): errori²/contenuti 56, 44, 14 a pT 0.4-0.8, 1.4-2, 6-10, quindi errori attesi circa 7.5, 6.7, 3.8 volte
quelli attuali. `SumW2Error(true)` dà x45.6, x7.9, x3.9, con covQual 2 in 41 celle su 70, fuori proprio a basso pT;
`AsymptoticError(true)` dà x7.0, x6.5, x3.8 con covQual 3, ma RooFit avverte che non è pensato per dati binnati. Da scegliere: uno
dei due, o fittare i conteggi non pesati e applicare l'efficienza dopo, come il wagon offline.

**B4. Rigenerare le purezze nella repo** (era 4.1): fatto nel sandbox (2026 e 2024, con B1 e le partenze multiple), non nel
`DataFile` della repo. Da confrontare cella per cella con l'archivio (l'harness della nota 11 lo fa, in `scratch/study11/`).

**B5. Guardare a mano le celle che restano segnalate** (era 4.4, 00-summary sezione 3, punto 3). Fatto per il 2026 nello studio
dei modelli; nel 2024 con B1 sono un quarto fra K0S e pi_tof, non ancora guardate.

**B6. Approcci non ancora studiati** (nota 13, sezione A): fit simultanei o vincoli sui parametri di forma tra classi di
molteplicità (per le celle Xi multimodali e i limiti che decidono il risultato), sPlot, fit 2D delle masse della coppia, fondo del
φ da template. Ognuno va provato in una nota di ricerca prima di entrare qui. Il Bernstein è già nel parser, come alternativa
per le sistematiche.

---------------------------------------------------------------------------------------------------------------------------------

## Da non fare (provato che non serve o non funziona)

- Fit in più thread (crash, `RooLinkedList::_pool`), `NumCPU` con il backend `cpu`, `Parallelize` (lancia), codegen con gradiente
  (non regge `RooCrystalBall`).
- `Strategy(2)`, `RecoverFromUndefinedRegions`, `Optimize`, `Hesse(true)`: non servono.
- Usare lo status 3 come criterio, da solo.
- Fidarsi di "converged": il test di accettazione passa anche nei minimi sbagliati (nota 12).
- Adottare HS3 come ingresso dei modelli (nota 10).
