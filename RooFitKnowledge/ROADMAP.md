# Roadmap: cosa fare nel progetto dopo la ricerca RooFit

15 ottobre 2026. Scritta dalle conclusioni di `00-summary.md` (sezioni 1-3) e delle note finali 09, 11, 12. Ogni voce rimanda alla nota con le prove. Implementata finora: la 1.1 (21 ottobre 2026, `FitQualityRecorder`, README *Fit quality*); della fase 3, in forma ridotta, le partenze
multiple di `DoFit` (23 ottobre 2026: config, rese scalate, a stadi; senza partenze casuali né xRooFit; `DESIGN_HISTORY.md`, *Several
starts per fit*).

Ordine: prima ciò che non cambia la fisica (bug, visibilità), poi le tue decisioni di fisica, poi la nuova procedura di fit, infine la
rigenerazione e il confronto a valle. Le fasi 0 e 1 si possono fare subito e in qualsiasi ordine.

---------------------------------------------------------------------------------------------------------------------------------

## Fase 0 - Bug, nessun effetto sulla fisica

| # | cosa | dove | prova | costo |
|---|---|---|---|---|
| 0.1 | Caricare le correzioni come `TH1` (o `TH1D`): oggi `GetUniqueOrThrow<TH1F>` lancia su 198 TH1D di `Corrections.root` appena si accende l'efficienza | `Codes/CorrelationTaskBase.h:578` | 00-summary 1.8, nota 11 | 1 riga |
| 0.2 | Correggere il tally di `RooFitMessages.h`: il "(remaining N suppressed)" di RooFit è sbagliato di 2 | `Codes/RooFitMessages.h` | nota 08 (8.12) | poche righe |
| 0.3 | Sommario del logger: "No warnings" anche con decine di migliaia di righe RooFit | `Codes/Logger.h` | nota 11 | poche righe |
| 0.4 | Purezza NaN sulle celle pi_tof vuote: segnalare e scrivere un valore definito | `Codes/PurityTask.h` | nota 11 | piccolo |
| 0.5 | Path relativi alla directory corrente; errori `TGaxis` su stderr | vari | nota 11 | piccolo |
| 0.6 | **Errori NaN a valle:** un errore di purezza NaN finisce come errore di bin NaN negli spettri dei pioni, e né il fit di ROOT né `SpectrumExtrapolator` lo scartano. È la causa principale dell'"amplificazione" dei pioni. Controllare la finitezza di purezza ed errore prima di usarli e scartare o segnalare i punti NaN | `PurityTask`, `SpectrumExtrapolator` | nota 11 sezioni 8-9 | piccolo |

## Fase 1 - Rendere visibile la qualità dei fit (i risultati non cambiano)

1. **Leggere e registrare lo stato del fit per cella:** `status`, `covQual`, `edm`, `minNll`, parametri al limite. **Fatto il 21 ottobre 2026**: un `TTree` `{binning}/FitQuality` per file, e un warning per sezione con i segnali affidabili (cella vuota, `covQual<3`, valutazioni NaN, parametri al limite); lo status non è segnalato.
   Lo status 3 da solo non è un indicatore (nota 09 sezione 2): segnalare con il test `covQual==3 && edm<0.01` dopo `Offset(true)` (vedi fase 3).
2. **Validatore del modello prima di `fitTo`:** limiti, start dentro i limiti, pdf finita e non negativa ai valori iniziali. Circa 30 righe,
   codice in nota 10 sezione 6. Indipendente da parser, factory o HS3.
3. **Bontà del fit:** chi2 di Pearson su tutte le bin, ndf = bin - parametri liberi (calibrato sui toy, nota 05), al posto di `RooPlot::chiSquare`.
4. **Grafici:** asse dei pull non fissato a ±5; curva totale e componenti con la stessa normalizzazione (nota 05).
5. **Opzionale:** record di provenienza per cella (blocco nostro, JSON-lines o oggetto nel file ROOT; HS3 solo come contenitore del modello),
   agganciato dopo `ExtractYieldsAndPurity` (nota 02 sezione 10, nota 10 sezione 8). In alternativa il minimo: una tabella per cella con lo stato del punto 1.

## Fase 2 - Decisioni di fisica (tue)

Ognuna è un cambio di configurazione, non di codice. Tutte cambiano i numeri di purezza.

1. **Limiti `nsig`/`nbkg` del K0S (1e8) e di pi_tpc (1e10):** tagliano la resa in 18 celle K0S e 2 pi_tpc; alzandoli S si sposta di una mediana
   del 22% (fino al 78%). Raccomandato: alzare a 1e11. (nota 09 sezione 1.3 e 9). Il taglio riguarda solo le celle del binning di analisi
   (bin uniti, più conteggi): nessuna delle 2620 celle sorgente è tagliata, quindi la mappa CCDB non cambia (nota 09 sezione 9.8).
2. **Range dell'osservabile di Xi:** [1.29, 1.36] (oggi, purezza più bassa di una mediana 0.074) o [1.2, 1.4] (archivio del 7 agosto). Ricorda che
   RooFit allarga il range ai bordi delle bin dell'istogramma ([1.289, 1.361], nota 08 sezione 3b).
3. **Forme:** `sigma_sig` di Xi al limite inferiore in quasi tutte le celle (439 su 520 celle sorgente; rilassato S -0.49% di mediana,
   fino a -3%; 1.8 MeV è fisico?); `n>=1` del K0S lega in 47 celle sorgente a pT alto (S +1.1%); `n>=1` di Xi va tenuto (rilassato S +8.6%); `c1_bkg` / Chebychev2 del K0S (pdf negativa nel
   27.5% della scatola dei parametri); segno della pendenza di pi_tof.
4. **Override per bin del K0S (`mult*_pt4`, `mult*_pt5`):** colpiscono bin diversi nei fit di analisi e in quelli sorgente. Voluto? (nota 09 1.1)
5. **Errori dell'efficienza online del phi:** `SumW2Error(true)` (errori di default 4-10 volte troppo piccoli, nota 05).
6. **`snap_to_bin` tollerante** (tolleranza 1e-3 bin, nota 04): attenzione, pi_tpc [-3,3] diventerebbe [-3,3.2] con il codice attuale.

## Fase 3 - Nuova procedura di `DoFit()`

Dopo la fase 2, perché le due cose si sommano. Procedura (nota 09 sezione 4, nota 11, nota 12):

1. fit con **`Offset("bin")`** (meglio di `Offset(true)`: stesse rese, ma il test del punto 3 accetta il 96% delle celle invece dell'88%,
   quindi l'escalation del punto 5 gira 3 volte meno; nota 09 sezione 9.9);
2. refit dal risultato;
3. accettare se `covQual()==3 && edm()<0.01`;
4. **partenze multiple, tenere l'NLL più basso** (nota 12 sezioni 6-7, toy su 40 celle delle famiglie a rischio, 200 toy ciascuna).
   Senza, la procedura resta in un minimo sbagliato e il test del punto 3 non se ne accorge: K0S pT 3.0-4.2 5.2%, K0S pT 4.2-6.4 38.8%
   (fino all'84% in una cella; P -0.013/-0.016 per toy sbagliato), Xi pT 1.2-1.6 25%, Xi pT 1.6-2.0 78%. Regola che ne viene:
   - K0S, pi_tpc, pi_tof: **partenza scalata sulla resa** (rese iniziali proporzionali ai conteggi della cella; da sola toglie i minimi
     sbagliati sui toy e costa meno della partenza dalla config) **più 2-4 partenze casuali** come sicurezza (partendo dalla config,
     2 partenze lasciano 2 toy sbagliati su 4000 nel K0S pT 4.2-6.4, 4 nessuno);
   - Xi pT 1.2-1.6: 4 partenze casuali (0.35%);
   - Xi pT 1.6-2.0 (e Xi 2026 pT 2.0-3.0, multimodale anch'esso, nota 11): **partenza dalla mediana dei fit delle altre classi di
     molteplicità dello stesso pT, più 8 partenze casuali** (2 toy sbagliati su 4000). Per Xi la partenza scalata sulla resa peggiora le
     cose (fino al 99.8%) e `sigma_sig` al limite aiuta poco (57%).
   Nella catena completa (nota 11 sezione 8, con k=2 ovunque e 8 per Xi pT 1.2-2.0): la purezza di analisi si sposta di più di 0.001 in 89
   celle su 737, di più di 0.01 in 9, mai oltre 0.05; i minimi sbagliati scendono da 180 a 55. Tempo 2.5 volte (indicativo). Le celle
   segnalate salgono da 38 a 80, quasi tutte pi_tpc con `nbkg` al limite 0: va rivisto il criterio di segnalazione per pi_tpc, dove
   `nbkg`=0 è il risultato atteso (nota 09 9.2). Sulle celle reali 20 partenze lasciano ancora 17 celle sorgente su 2620 sopra il
   migliore di 50 (Xi 2026 pT alto, S -1.1/-1.4%, nota 09 9.11): un caso in cui solo 1-4 partenze su 50 trovano il minimo;
5. altrimenti escalation a `xRooFit::minimize`, partendo dal fit con offset (non dai valori della config: è ciò che spiega 16 contro 35 celle
   segnalate, nota 09 9.11), **con `gDirectory` nullo attorno alla chiamata** (altrimenti restituisce il fit di un'altra cella, nota 11);
6. registrare l'esito per cella (fase 1.1).

Requisiti: `RooFitXRooFit` in CMake (oggi manca). **Tempi misurati a macchina libera** (15 ottobre 2026, coordinatore, un processo, due ripetizioni
concordi entro il 2%; passo di purezza intero, sei file 2024 e 2026, copia in `scratch/coord-timing/`, `times.txt`): `DoFit` attuale 11.7 s,
procedura raccomandata senza partenze 19.2 s (1.6 volte), con 2 partenze casuali ovunque e 8 per Xi pT 1.2-2.0 48.9 s (4.2 volte). La regola
della fase 3.4 (partenza scalata sulla resa, mediana delle molteplicità per Xi) non è ancora codificata: il suo tempo andrà rimisurato.
**`IntegrateBins`: non come default, utile come sistematica.** Sui toy elimina il bias del centro bin (Xi alta statistica da +2.6σ a +0.07σ,
pi_tpc da +11.7σ a +0.05σ). Con il refit della procedura il fit ad alta statistica finisce con status ≠ 0 nel 77-88% dei toy e gli errori
sono 1.4-1.5 volte troppo grandi; **un solo fit IB senza refit** converge nel 91-92% dei toy con errori giusti (0.93-1.14σ), a 14-30 volte la
CPU. La precisione (1e-4, 1e-5, 1e-6) non cambia nulla. L'errore del fit al centro bin copre il valore IB (copertura su P 0.645-0.73), e sulle
celle reali, con entrambi i fit al proprio minimo migliore, lo spostamento di S è piccolo: mediana +0.012%, p90 0.43%, oltre l'1% solo in
2 celle Xi 2026 a pT alto su 60 (nota 09 9.11.4; gli spostamenti del 2% della 9.10 erano differenze di minimo). Quindi: fit al centro bin come
nominale, un fit IB singolo come variazione sistematica nelle celle ad alta statistica. `obs->setBins(n)` uguale alle bin dell'istogramma nel range.

Non servono: `Strategy(2)`, `RecoverFromUndefinedRegions`, `Optimize`, `Hesse(true)`.

## Fase 4 - Rigenerare e confrontare

1. Rigenerare i file di purezza (K0S, Xi, pi_tpc, pi_tof) con fasi 2 e 3; confrontarli cella per cella con l'archivio
   (l'harness della nota 11 lo fa già, in `scratch/study11/`).
2. Rieseguire le correlazioni con `apply_purity` vero ed efficienza accesa (dopo 0.1) e misurare lo spostamento finale.
   Con efficienza accesa (fix 0.1 in una copia) le variazioni massime sono: resa dei pioni estrapolata 3.35%, K0S/π estrapolato 3.1%, trend K0S
   misurato 1.07% (nota 11 sezione 7).
3. **Estrapolazione dei pioni (Levy-Tsallis):** l'amplificazione viene soprattutto dagli errori NaN (0.6): riparati quelli, la sensibilità
   alla purezza scende da 3.35% a 0.62% (efficienza accesa) e da 9.4% a 2.4% (spenta). Un range di fit [0.2,1.5] o [0.2,3.0] GeV/c con un
   errore minimo del 2% la porta a ≤0.9% e ≤0.5%. La scelta del range sposta la resa dei pioni dell'1-6%: va trattata come sistematica
   (gli input non hanno errori sistematici). La curva sotto 0.2 GeV/c vale circa il 25% della resa (nota 11 sezione 9).
4. Guardare a mano le celle che restano segnalate (00-summary sezione 3, punto 3).

## Fase 5 - Struttura, quando serve

- **Parser dei modelli:** tenere quello a mano finché la lista dei modelli è corta; passare alle stringhe `factory` quando cresce (identiche al bit,
  nota 10). **Non** adottare HS3 come ingresso.
- **Range di fit distinto dal range del grafico** (DESIGN_NOTES sezione 3): oggi coincidono.
- **Approcci non ancora studiati** (nota 13, sezione A): fit simultanei o vincoli sui parametri di forma tra bin di molteplicità
  (per le celle Xi multimodali e i limiti che decidono il risultato), `RooBernstein` al posto del Chebychev (positivo per costruzione),
  sPlot, fit 2D delle masse della coppia, fondo del phi da template. Ognuno va provato in una nota di ricerca prima di entrare qui.

## Da non fare (provato che non serve o non funziona)

- Fit in più thread (crash, `RooLinkedList::_pool`), `NumCPU` con il backend `cpu`, `Parallelize` (lancia), codegen con gradiente (non regge `RooCrystalBall`).
- Usare lo status 3 come criterio, da solo.
- Fidarsi di "converged": il test di accettazione passa anche nei minimi sbagliati (nota 12).
