# 00 - Sintesi della ricerca RooFit (14-15 ottobre 2026)

Per il responsabile del progetto. Riassume le note 01-10, scritte da quattro ricercatori (due passate). **Non ho rieseguito nulla io:** i numeri
sono quelli riportati nelle note, che a loro volta citano script e output nella cartella `scratch/` (copiata dentro `RooFitKnowledge/`, vedi il suo README). Le etichette di provenienza
(`[verified]`, `[source]`, `[doc]`, `[inferred]`) sono nelle note; qui le richiamo solo dove contano. Ambiente: ROOT 6.36.10 (build ALICE), macOS arm64.

**Stato finale (15 ottobre 2026, sera): la ricerca è chiusa.** L'ultima passata (sezione 1.9) ha completato tutti i punti aperti; le note 01-12 sono
finali e non hanno più `NOT DONE`, salvo voci impossibili qui (ognuna con la sua ragione). Cosa di RooFit non è stato studiato: nota 13.
Cosa fare nel progetto: `ROADMAP.md`.

---------------------------------------------------------------------------------------------------------------------------------

## 1. Le conclusioni, in ordine di importanza

### 1.1 Il problema di qualità dei fit cambia i risultati fisici (note 03, 09)

Sulle celle che `PurityTask` fa davvero (734 celle con binning di analisi, dai due file di purezza 2024 e 2026), `DoFit()` dà una resa del segnale che
differisce dal miglior fit trovato di:

| | celle | resa S >0.1% | >1% | >5% | purezza \|dP\| >0.001 | >0.01 |
|---|---|---|---|---|---|---|
| tutte | 734 | 29% | 12% | 4% | 21% | 9% |
| K0S | 200 | 35% | 31% | 8% | | 28% |

Mediana di |dS|/S 1e-4, ma p90 1.9% e p99 25%. L'errore propagato di S è sbagliato di oltre un fattore 2 nel 24% delle celle (pi_tpc 58%).
Il danno sul K0S è concentrato nei bin di pT 4-9 (1.2-6.4 GeV/c) del file 2026.

**Lo status 3 non è un indicatore affidabile.** Delle 523 celle di analisi con status 3: 28% limite di precisione innocuo, 52% davvero non convergenti,
20% celle cattive (covQual<3, NaN, parametri ai limiti). "EDM sotto il limite di macchina" non basta come test: nel 54% di quelle celle un restart abbassa comunque l'NLL
di più di 0.5. Il quadro è coerente con la letteratura: Moneta indica un limite di macchina di circa 1e-8 × |NLL| e raccomanda `Offset(true)` o `Offset("bin")`
(forum 54379 e 47302; rf614). Per il K0S la frazione di status 3 cresce con la statistica (39% a 1e5-3e5 entries, 83% a 3e5-1e6, 100% sopra 1e6).

### 1.2 Procedura proposta per `DoFit()` (nota 09, sezione 4)

1. fit con `Offset(true)`;
2. un refit dal risultato, ancora con `Offset(true)`;
3. accettare se `covQual()==3 && edm()<0.01` (con l'offset l'EDM è risolvibile e il test ha senso);
4. altrimenti scalare a xRooFit `minimize()`, tenere l'NLL più basso;
5. registrare status/covQual/edm per cella e segnalare quelle ancora non accettate.

Effetto misurato: celle con S fuori di oltre l'1% scendono dal 12.3% all'1.4%, oltre il 5% dal 3.8% allo 0.8%. 88% delle celle accettate al passo 3, 33 celle (4.5%) restano segnalate.
Costo: circa 2 volte la CPU per cella (3.0 → 4.9 ms senza escalation); un intero file di purezza passa da circa 5 s a circa 10 s.
**Non risolvono il problema:** `Offset(true)` da solo, `Strategy(2)`, `RecoverFromUndefinedRegions`. **Danno risultati identici al bit:** `Optimize`, `Hesse(true)`, `Strategy(1)`.

### 1.3 Due problemi che non riguardano la convergenza (nota 09)

- **`nsig` max = 1e8 taglia la resa del K0S.** 19 delle 100 celle K0S del file 2026 hanno più di 1e8 conteggi; il miglior fit sta al limite in 18 celle.
  Alzando `nsig`/`nbkg` a 1e11 la resa si sposta di una mediana del 22% (fino al 78%) e la purezza fino a 0.06 (esempio K0S mult 8, pT 3: P da 0.942 a 0.984). Due celle pi_tpc sono tagliate allo stesso modo (`nsig` max 1e10).
- **Minimi multipli.** Una ricerca da 15 punti di partenza trova un NLL più basso di più di 0.5 nel 10% delle celle (73/734; Xi 30, pi_tpc 32, K0S 11), con rese diverse fino all'1.7%.
  `sigma_sig` di Xi è al limite inferiore (0.002) in 125 celle su 140 (nota 06: 237 su 260 celle sorgente). Anche un fit perfettamente convergente lascia quindi un'ambiguità fino a circa 1.7% su S in quelle celle.

### 1.4 Altri difetti reali trovati (note 03-06)

| difetto | effetto | dove |
|---|---|---|
| `covQual` 2: il restart sposta S del 4-13% in 5 celle K0S su 16 | l'errore calcolato non ha significato | 04 |
| `snap_to_bin` allarga di una bin una finestra già allineata ai bordi (rumore in virgola mobile) | nessuna configurazione lo attiva oggi | 04 |
| finestre oltre il range estrapolate (fondo negativo, purezza NaN); finestre invertite danno zeri silenziosi | nessun avviso | 04 |
| range di fit di Xi è in silenzio [1.289, 1.361] invece di [1.29, 1.36] | da guardare | 05 |
| `RooPlot::chiSquare` segue la statistica di Neyman; per K0S chi2/ndf ≈ 1 + 3e-6·N | modello "rifiutato" da circa 1e5 entries anche con fit buono | 05 |
| asse dei pull a ±5 lascia fuori scala l'85-96% dei punti nei fit cattivi; curva totale e componenti normalizzate diversamente (circa 2%) | il grafico inganna | 05 |
| fit sull'efficienza online del phi (contenuti non interi): errori di default 4-8 volte troppo piccoli, il warning "weighted data" compare | `SumW2Error(true)` li corregge; `AsymptoticError(true)` entro il 2% | 05, 06 |
| `nbkg` di pi_tpc vale 0 in 165 celle su 360 | fondo a zero o al limite in quasi metà dei casi | 06 |
| bias del centro del bin su `nsig`: al massimo 0.08% (-2.55σ nella cella con più statistica); `IntegrateBins` lo elimina | | 04, 06 |
| `RooCrystalBall`: bug sull'integrale vicino a n=1 (#19602) corretto solo in 6.38; qui errore ~1e-6, ma i limiti di `n` partono esattamente da 1 | trascurabile oggi | 08 |

**Chiarito (non è un difetto):** le 100 bin dell'osservabile (su un istogramma da 200) non cambiano nessun fit né resa (identici al bit da 50 a 1000 bin su 10 celle);
cambiano solo il campionamento della curva nei grafici (fino al 2%, 7% per pi_tpc). Se si attiva `IntegrateBins`, `obs->setBins` deve valere esattamente il numero di bin
dell'istogramma nel range (K0S 200, xi 72, phi 71, pi_tpc 100), altrimenti il fit del phi risulta molto distorto.

`getPropagatedError` con `covQual`=3 coincide con il bootstrap entro circa il 10% (12 celle × 500 refit); il "7-22% troppo piccolo" della prima passata era un artefatto.
`RooExtendPdf` concorda con integrale × `nsig` ma è 100-300 volte più lento per il phi.

### 1.5 Sostituire il parsing a mano dei modelli (note 01, 02, 10)

Raccomandazione: **tenere il parser a mano (A), aggiungere subito un livello di validazione, passare alle stringhe `factory` (B) quando la lista dei modelli cresce, non adottare HS3 (C).**

- Equivalenza: B e C danno risultati identici in ogni campo su 1440 celle reali (k0s, xi, pi_tpc, pi_tof, phi), anche con finestra n-sigma, snap e modelli compositi. `fitConfig.json` non cambia con B.
- Il parser attuale gira senza errori su `min>max`, start fuori dai limiti, Chebychev con start negativo, frazione sopra 1. Due controlli (config + modello finito e non negativo ai valori iniziali, circa 30 righe) li rifiutano tutti: il guadagno principale è lì, indipendente dalla strada scelta.
- Costo: B netto +70 righe, C +160; nessuna delle due riduce il codice. Costruzione 3.4-5.4 ms senza cache (contro 0.005 ms), ma 0.02 ms con cache.
- Insidie della `factory`: una formula `EXPR::` che non compila termina il processo; in Cling un `catch` non cattura; valori fuori range e nomi duplicati passano in silenzio; `std::format("{:.17g}")` non compila in Cling.
- HS3 è sperimentale, non può esprimere finestre, purezza, sideband e override per bin, accetta in silenzio 7 difetti su 16, e Chebychev/Voigtian richiedono un importatore/esportatore scritto a mano (65 righe, funziona anche in Cling e PyROOT).

**Terza passata su HS3 (note 02 sezione 10, 10 sezione 8, 07):** la raccomandazione non cambia. HS3 ha però un uso utile come *uscita*: un record di provenienza
per ogni cella fittata. Provato su 1000 celle reali (k0s 340, xi 260, pi_tpc 360, phi 40): l'export nativo porta modello, valori fittati, limiti, flag const e le finestre
`signalRegion`/`sidebandRegion`; errori (che l'esportatore non scrive ma l'importatore legge) e status/edm/covQual/minNll, covarianza, config, rese e identità dell'input (file, path, bin, hash)
vanno in un blocco `misc.dynamic_roofitter` nostro. Il reimport riproduce valori, errori, limiti e NLL in 1000 celle su 1000. 5.5 KB per cella (3.5 KB senza covarianza),
0.125 ms compilato e 0.8-0.9 ms in Cling contro 3.5 ms di fit. Si agganciarebbe dopo `ExtractYieldsAndPurity` (`PhiFitTask.h:129`, `PurityTask.h:327`), opzionale; il blocco nostro da solo non richiederebbe HS3.
Export di BkgSourav1/2, BkgMattia e del Voigtian del phi: NLL e resa identici. #23221 si presenta solo con un `createIntegral` del pdf totale vivo; ricetta: esportare una copia temporanea e richiedere un array `distributions` non vuoto.
`DrawFit` dà PNG identici al byte per A, B e C su 5 celle; HS3 perde il titolo dell'osservabile (una riga di `SetTitle`). 6.38/6.40: nessun cambiamento HS3 nelle release notes, `hs3_version` resta "0.2",
Chebychev nativo dalla 6.38, Voigtian in nessuna versione, `extended` ancora ignorato in 6.40. Lo standard non ha posto per errori e covarianza e non ha un validatore.
Non fatto: record come `TObjString` o JSON-lines, compilazione di `hs3-pair.h` su 6.38/6.40, implementazione nel `DynamicRooFitter`.

### 1.6 Backend, thread, workspace (note 06, 07)

- **Thread:** 2 thread hanno dato crash in 6 esecuzioni su 6 (più un blocco), 4 thread 6 su 6, anche con `EnableThreadSafety()`; causa: `RooLinkedList::_pool` statico senza lock. Nessun numero sbagliato; un mutex attorno a ogni fit equivale al seriale. **Non parallelizzare i fit con thread.**
- **Codegen:** con gradiente lancia un'eccezione su `RooCrystalBall` (e `RooVoigtian` non è tradotto), quindi i modelli DSCB non lo possono usare. Clad invece funziona in questa build.
- **Workspace/HS3:** export senza perdite con Chebychev registrato (due file JSON da una riga); #23221: l'export è silenziosamente vuoto finché esiste un oggetto `createIntegral`.
- **Profilo e toy:** su una cella convergente l'intervallo di profile likelihood combacia con Hesse entro circa il 3%, MINOS fallisce, la RMS dei pull dei toy è 0.98; `RooMCStudy` scarta in silenzio l'8% dei toy in status 3.
- **Messaggio "numeric integrator":** viene solo dai modelli del phi (Voigtian e pdf generica).

### 1.7 Quarta passata: limiti, Xi, purezza end to end (note 09 sezione 9, 11)

Chiusa in anticipo per limite d'uso: i punti non raggiunti sono in sezione 4.

- **Il mismatch di Xi è risolto** [verified]: il commit 3e2878a (27 agosto) ha cambiato il range dell'osservabile di Xi da [1.2, 1.4] a [1.29, 1.36]. Con il vecchio range tutte le 660 celle Xi riproducono l'archivio del 7 agosto. Il nuovo range abbassa la purezza di Xi di una mediana 0.074 (p90 0.092, max 0.111). **Quale range sia giusto è una decisione di fisica tua.**
- **Rilassare i limiti**, una famiglia per volta (con la cautela che parte dei cali di NLL è variabilità della procedura, perché anche il fit base non è sempre al minimo globale; solidi solo i tre primi punti):
  - Xi `sigma_sig`, limite inferiore: lega quasi tutte le celle Xi; rilassato dà 1.1-2.1 MeV (mediana 1.8 MeV) con bin da 1 MeV; S si sposta di una mediana 0.44%, P di 0.0025. Se quella larghezza è fisica lo decidi tu.
  - K0S `c1_bkg`: oltre |c1|=1 la pdf va negativa. È una questione di forma del fondo, tua.
  - pi_tpc `nbkg`: il fit non vuole fondo (|nbkg/nsig| al massimo 0.03); lasciarlo negativo non cambia nulla di rilevante. Tenere il limite a 0.
  - DSCB n e alpha: innocui per S.
  - pi_tof `slope` massimo: il fondo vuole una pendenza crescente; il segno lo scegli tu.
- **`PurityTask` end to end su una copia in scratch** (nota 11) [verified]: il passo non modificato riproduce l'archivio al bit per K0S, pi_tpc e pi_tof, mappe CCDB comprese. La procedura raccomandata (offset + refit + escalation) cambia la purezza di analisi per più di 0.001 in 158 celle, più di 0.01 in 69, più di 0.05 in 7: la previsione offline era 154, 67, 7. Spostamenti concentrati nei bin K0S di pT 4-9 (53 verso l'alto, 16 verso il basso); massimo 0.147 sul K0S, 0.92 su una cella pi_tof con 127 conteggi.
  Tempo del passo 11.9 s contro 17.0 s (indicativo).
  **Due cautele:** l'escalation a xRooFit non riproduce il banco di prova (308 celle escalate su 353 differiscono, causa non trovata), per cui restano segnalate 80 celle di analisi su 737 invece delle 33 previste; la procedura nuova richiede `RooFitXRooFit` in CMake, che oggi manca.
- **Effetto a valle:** il task di correlazione moltiplica lo spettro per la purezza del binning di analisi solo se `apply_purity` è vero, ed è falso nella config di base [source], quindi la catena di default non cambia. La mappa CCDB va a O2: lì dipende da come viene usata. L'effetto sulle correlazioni finali non è stato misurato.
- **Toy per il bias sulla purezza:** il codice esiste (`toys.cpp`) ma è stato provato solo su 3 toy; in uno, a bassa statistica nel K0S, sia la chiamata attuale sia la procedura raccomandata sono rimaste in un minimo sbagliato (P 0.8995 contro la verità 0.914). Nessuna conclusione.
- **Problemi trovati nel progetto** (elencati, non corretti): lo status del fit non viene mai letto; il sommario del logger dice "No warnings" nonostante decine di migliaia di righe RooFit (63k righe in 24 tipi nella baseline); purezza NaN per le celle pi_tof vuote; errori `TGaxis` su stderr; CMake senza xRooFit; path relativi alla directory corrente.

### 1.8 Quinta passata (15 ottobre 2026; note 04, 05, 07, 08, 01, 02, 10, 11, 12; 09/03 in chiusura)

Chiusa per limite d'uso giornaliero. Numeri riportati dai ricercatori, non riverificati dal coordinatore salvo dove detto.

- **Toy sul bias della purezza (nota 12, nuova):** la chiamata del progetto distorce molto P nelle celle segnalate da 09 (K0S 2026 pT 3-6.4: -0.015/-0.026; pi_tpc pT 2-3: -0.003, centinaia di σ; Xi multimodale -17σ; K0S alta statistica copertura 0.35). La procedura raccomandata corregge, ma resta in un minimo sbagliato nel 73% dei toy della cella K0S di prova e nel 91% della Xi multimodale, e il suo test di accettazione non lo vede (534 su 545 passano). Le partenze casuali trovano il minimo giusto in >=96.5% dei toy: **la procedura va estesa con 2-4 partenze multiple nelle celle a rischio** (non provato). Al minimo giusto resta il bias del centro bin (fino a -9.5σ su S ad alta statistica), che `IntegrateBins` elimina (provato solo su Asimov). Al minimo giusto gli errori propagati sono affidabili.
- **Escalation xRooFit (nota 11): bug trovato.** `xRooFit::minimize` tiene una cache in `gDirectory`, indicizzata per nomi e non per dati; dentro `PurityTask` è il file di output, quindi le celle ricevevano il fit di una cella precedente e la cache finiva nei file di purezza. Con `gDirectory` nullo attorno alla chiamata le 353 celle coincidono con lo studio; celle segnalate 35 (previste 33). **Se si adotta xRooFit, questa protezione è obbligatoria.**
- **A valle (nota 11):** con `apply_purity` vero ed efficienza spenta gli spettri si spostano del rapporto delle purezze (K0S p90 2.7%, max 20.6%); l'estrapolazione dei pioni amplifica variazioni del per mille fino al 9.4% (causa non capita). Nessun codice O2Physics legge le mappe di purezza (nota 07: O2 usa RooFit solo in macro, mai in task o CCDB).
- **Bug del progetto, verificato dal coordinatore:** `CorrelationTaskBase.h:578` carica le correzioni con `GetUniqueOrThrow<TH1F>`, ma `Corrections.root` (in `globalConfigBasepp.json`) contiene 198 TH1D e 3 TH1F: con l'efficienza accesa il task lancia.
- **Bontà del fit (nota 05):** su toy da 1e3 a 1e5 entries il chi2 di Pearson su tutte le bin con ndf = bin - parametri è calibrato (rifiuta circa 5% a p<0.05); `RooPlot::chiSquare` legge 0.56-0.96 e non rifiuta quasi mai. Sui dati reali invece rifiutava da circa 1e5 entries: lì il modello non è perfetto, sui toy lo è per costruzione.
- **Phi online (nota 05):** con i pesi veri per bin (variano 1-6%) gli errori di default sono 4-10 volte troppo piccoli; `SumW2Error` giusto entro 0.87-1.10 sui toy convergenti; con covQual 2 (40-80% dei toy) nessuno dei due è affidabile.
- **`snap_to_bin` tollerante (nota 04):** ricetta con tolleranza 1e-3 bin, codice nella nota; con il codice attuale pi_tpc [-3,3] diventerebbe [-3,3.2] (S spostato di 14 dS). Non applicato.
- **Profilo e toy ad alta statistica (nota 07):** con il fit del progetto profilo sbagliato fino al 50% a 1σ, `nsig` 3.6% (96σ) sotto il migliore in una cella; dopo `Offset("bin")` Hesse = profilo entro 0.3%. Toy alla verità: 199/200 in status 3 con le opzioni del progetto, 0 con `Offset("bin")`. `RooMCStudy` tiene solo i fit in status 0; i toy binned usano le bin dell'osservabile (serve `setBins(200)`).
- **Riguarda il nostro codice (nota 08):** il conteggio "(remaining N messages suppressed)" di RooFit è sbagliato di 2, quindi `RooFitMessages.h` sbaglia il tally di 2 a ogni soppressione; `PrintLevel(-1)` non nasconde la riga INFO dell'ottimizzazione costante. Chebychev2 del K0S: il 27.5% della scatola dei parametri dà una pdf negativa.
- **Thread (nota 10):** anche la sola costruzione dei modelli va in crash su 2 thread senza lock; con un mutex 18/18. Niente RooFit in più thread: processi separati.
- **Record di provenienza (nota 02):** JSON-lines con flush per record (3.22 MB per 600 record contro 1.43 MB in un file ROOT, che però sopravvive meglio a un abort).
- **`factory` (nota 01):** `$REMOVE()` con parentesi vuote non fa nulla in silenzio; un workspace con classe compilata personalizzata va in crash alla lettura se la libreria non è caricata prima. Grafici dal modello `factory` identici al byte.

---------------------------------------------------------------------------------------------------------------------------------

### 1.9 Passata finale (15 ottobre 2026, sera; note 01, 07, 08, 09, 11, 12, nuova 13)

Numeri riportati dai ricercatori; il coordinatore ha controllato l'integrità delle note sul disco (titoli, coda, diff contro git per la 09).

- **Partenze multiple (nota 12 sezione 7, 40 celle × 200 toy).** Con la procedura raccomandata i minimi sbagliati sono il 5.2% (K0S pT 3.0-4.2),
  38.8% (K0S pT 4.2-6.4, fino all'84%), 25% (Xi pT 1.2-1.6), 78% (Xi pT 1.6-2.0), e il test di accettazione non li vede. Regola: partenza scalata
  sulla resa più 2-4 partenze casuali per K0S e pioni; 4 partenze per Xi pT 1.2-1.6; per Xi pT 1.6-2.0 partenza dalla mediana dei fit delle altre
  molteplicità dello stesso pT più 8 partenze (2 toy sbagliati su 4000). Per Xi la partenza scalata sulla resa peggiora (99.8%).
- **Catena completa con le partenze (nota 11 sezione 8):** purezza di analisi spostata oltre 0.001 in 89 celle su 737, oltre 0.01 in 9, mai oltre
  0.05, rispetto alla procedura senza partenze; minimi sbagliati da 180 a 55; tempo 2.5 volte. Anche Xi 2026 pT 2.0-3.0 è multimodale.
  Celle segnalate da 38 a 80, quasi tutte pi_tpc con `nbkg`=0, che lì è il risultato atteso: va rivisto il criterio.
- **Riferimento a 50 partenze su tutte le 2620 celle sorgente (nota 09, 9.11):** 20 partenze restano sopra il migliore in 17 celle (Xi 2026 pT 11-14
  quasi tutte; S -1.1/-1.4%, P -0.011). Dispersione di S tra soluzioni con lo stesso NLL trascurabile (mediana 7e-6), tranne 3 celle Xi 2024 a pT alto (1.3-2.2%).
- **`Offset("bin")` (nota 09, 9.9):** stesse rese di `Offset(true)` ma accettazione 96% contro 88%, escalation 3 volte meno. Diventa il primo passo.
- **Limiti sulle celle sorgente (nota 09, 9.8):** nessuna cella sorgente è tagliata da `nsig`/`nbkg` (la mappa CCDB non cambia); `sigma_sig` di Xi
  lega in 439 celle su 520 (S -0.49% mediana, fino a -3%); `n>=1` del K0S in 47 celle a pT alto (S +1.1%); `n>=1` di Xi va tenuto (+8.6%).
- **`IntegrateBins`:** sui dati reali, con entrambi i fit al proprio minimo migliore, S si sposta di +0.012% di mediana (p90 0.43%; oltre l'1% solo in 2
  celle Xi 2026 a pT alto su 60; gli spostamenti del 2% visti prima erano differenze di minimo, nota 09 9.11.4). Sui toy elimina il bias,
  ma con il refit ad alta statistica non converge (77-88%); un fit IB singolo converge nel 91-92% con errori giusti, a 14-30 volte la CPU; la precisione
  non conta. Uso: variazione sistematica, non nominale.
- **Perché il K0S cade nel minimo sbagliato sui toy e non sui dati (nota 12):** sui dati reali quel bacino non è un minimo locale; esiste solo se il
  modello descrive perfettamente i dati (i dati reali lo mancano con χ² 2884 su 200 bin). Le rese della config, 30 volte sotto N, spingono il fit lì.
- **A valle con efficienza (nota 11 sezioni 7-9).** Il bug `TH1F`/`TH1D` è confermato (corretto solo in scratch). **L'"amplificazione" dei pioni era
  soprattutto un errore NaN:** la procedura nuova scrive errori di purezza NaN in 11 celle (pi_tpc con P≈1 e `nbkg` al limite); finiscono in 63 dei 180
  spettri dei pioni e né il fit di ROOT né `SpectrumExtrapolator` li scartano (testano `<= 0`, falso per NaN). Riparati: sensibilità da 3.35% a 0.62%
  (efficienza accesa), da 9.4% a 2.4% (spenta). Range di fit [0.2,1.5] o [0.2,3.0] con errore minimo 2%: ≤0.9% e ≤0.5%. La scelta del range sposta la
  resa dei pioni dell'1-6% (sistematica). Effetto finale con efficienza accesa e partenze: trend K0S misurato fino a 1.16%, rapporti fino a 1.13%.
- **Note 01, 07, 08:** `$REMOVE(nome)` non funziona per un errore di nome in RooFit (`RooAbsArg.cxx:1052` legge `ORIGNAME`, `RooCustomizer` scrive
  `origName`); helper HS3 provato su celle reali K0S e phi (NLL identico; HS3 non salva gli errori dei parametri); importare un istogramma allarga il
  range dell'osservabile ai bordi delle bin (da qui [1.289, 1.361] di Xi); PR #21703 depreca i fit chi2 x-y, non `RooPlot::chiSquare` (correzione).
- **Nota 13 (nuova):** tutto ciò che di RooFit non è stato studiato, con in rilievo gli approcci pertinenti (fit simultanei o vincoli tra molteplicità,
  `RooBernstein` al posto del Chebychev, sPlot, fit 2D delle masse, fondo da template).

---------------------------------------------------------------------------------------------------------------------------------

## 2. Cosa non è solido

- **Xi non riproduceva i file di purezza archiviati** (660 celle, es. P 0.954 archiviata contro 0.877 ricalcolata nella cella (0,0) del 2024): **causa trovata**, il cambio del range dell'osservabile del 27 agosto (sezione 1.7). K0S, pi_tpc e pi_tof riproducevano già esattamente le 2720 celle archiviate: buona validazione dell'harness. I numeri di Xi nello studio 09 descrivono la configurazione attuale.
- **`23luglio` non è un input della purezza.** Il suo asse di pT del K0S ha un bordo a 6.5 dove `rebinning_pt` ha 6.4, quindi `MapToSourceBins` lancerebbe. Le note 03-07 più vecchie usavano quel file; lo studio 09 usa i file 2024 (`5agosto`) e 2026 (`6agosto`).
- Il miglior NLL trovato non è dimostrato globale, ma con 50 partenze su tutte le 2620 celle sorgente il riferimento a 20 partenze cede solo in 17 celle (sezione 1.9). `PurityTask` è stato eseguito end to end su una copia, fino alle correlazioni con efficienza accesa (nota 11).
- Toy: il modello genera dati che descrive perfettamente; i dati reali no (χ² 2884 su 200 bin nella cella K0S di prova). Tassi di minimi sbagliati e bias dei toy sono quindi un limite superiore per i minimi e non dicono nulla sull'errore di modello (nota 12).
- I tempi sono misurati con altri job concorrenti: nessuna conclusione sulle prestazioni. I risultati di correttezza restano validi.
- 6.38 e 6.40 sono verificati solo sulle release notes (controllate sui file primari), non eseguendo quelle versioni. La 6.36.04 non parte su questo sistema (errore modulemap), quindi niente test di caricamento tra versioni.
- La nota 09 è stata troncata a metà di una modifica dall'agente e le sezioni 1-8 riscritte dal suo stesso testo precedente; ho controllato che i numeri chiave (734 celle, 29/12/4%, 523 celle con status 3, 2720 celle riprodotte) coincidano con la versione precedente, ma il resto non l'ho riletto. Nella passata finale è stata troncata di nuovo e ricostruita da git più una copia; il diff contro git mostra solo le aggiunte volute.
- La nota 01 è stata troncata da un heredoc mal quotato e ricostruita a memoria dall'agente: può mancare qualche dettaglio.
- Lo studio 09 vale per la config corrente e per le celle fittate come oggi; sul rilassamento dei limiti vale solo per i tre punti marcati solidi (sezione 1.7).

## 3. Decisioni che spettano a te

In ordine di valore:

1. **Alzare i limiti `nsig`/`nbkg` del K0S (e pi_tpc).** Cambio di configurazione, non di codice, con effetto fisico dimostrato (fino al 78% su S nelle celle tagliate). È un cambio di fisica: lo devi approvare tu.
2. **Adottare o no la procedura di refit per `DoFit()`** (sezione 1.2): decide anche se i risultati di purezza già prodotti vanno rigenerati.
3. **Guardare a mano le celle segnalate.** Nel file 2026, 125 celle su 366 hanno almeno un flag duro. Il dettaglio è nella sezione 5 della nota 09 e in `cells-master.tsv` in `scratch/study09/`. Tra queste, K0S (5,2) (6,2) (8,2) (9,2), (1..9,3), (2,4) (5,4) (6,4) (8,4), (5,5), K0S pT 4-9 in generale, tutte le celle Xi con pT 1-4 (multimodali), pi_tpc pT 0, 3, 4, 5, 9 e pi_tof pT 0 (vuoto o meno di 10 conteggi).
4. **Il range dell'osservabile di Xi:** [1.29, 1.36] (dal 27 agosto, purezza più bassa di una mediana 0.074) o [1.2, 1.4] (archivio del 7 agosto)? Per fisica.
5. **Limiti di forma:** `sigma_sig` di Xi (la larghezza di 1.8 MeV è fisica?), `c1_bkg` del K0S, segno della pendenza di pi_tof.
6. **Chiarire se è voluto che gli override per bin del K0S (`mult*_pt4`, `mult*_pt5`, Chebychev2) colpiscano i bin di analisi 4 e 5 (1.2-2.0 GeV/c) ma i bin sorgente 4 e 5 (0.8-1.0 GeV/c)** nei fit sorgente (nota 09, sezione 1.1, trovato leggendo il codice).
7. **Il validatore del modello** (controlli su limiti e finitezza prima di `fitTo`): piccolo, indipendente dal resto, nota 10 sezione 6 ha il codice.
8. Per gli errori dell'efficienza online del phi: `SumW2Error(true)`.
9. **Limiti `n` delle code:** `n>=1` del K0S lega in 47 celle sorgente a pT alto (S +1.1%); `n>=1` di Xi va tenuto (rilassato S +8.6%) (nota 09 9.8).
10. **Criterio di segnalazione per pi_tpc:** `nbkg`=0 è il risultato atteso, non un difetto; oggi fa salire le celle segnalate da 38 a 80 (nota 11).
11. **Range del fit di estrapolazione dei pioni:** sposta la resa dell'1-6%; [0.2,1.5] o [0.2,3.0] GeV/c con errore minimo del 2% lo stabilizzano (nota 11 sezione 9).
12. Decisioni rimandate: factory contro parser a mano; record di provenienza HS3; aggiungere xRooFit a CMake se si adotta la procedura di refit; gli approcci della nota 13.

## 4. Cosa resta da fare

**Nella ricerca: niente.** Tutti i punti delle passate precedenti sono chiusi (sezione 1.9). Restano solo:

- **Impossibile qui:** test tra versioni di ROOT (la 6.36.04 non parte, errore modulemap; la 6.38 e la 6.40 non sono installate); errori sistematici
  veri nel fit di estrapolazione (gli input non ne hanno).
- **Opzionali, giudicati senza effetto sulle decisioni dai ricercatori:** `IntegrateBins` su tutta la catena (non è raccomandato come nominale);
  come Minuit tratta un punto con errore NaN (la correzione è scartarlo comunque, roadmap 0.6).
- **Non studiato:** nota 13.
- **Tempi a macchina libera** (misurati dal coordinatore a fine ricerca, passo di purezza intero): `DoFit` attuale 11.7 s, procedura raccomandata
  19.2 s, con partenze casuali 48.9 s (`ROADMAP.md` fase 3, `scratch/coord-timing/times.txt`).

**Nel progetto:** `ROADMAP.md`.

Gli script e i risultati sono in `RooFitKnowledge/scratch/` (git-ignorata); i percorsi nelle note puntano già lì. Per un'eventuale ripresa, ogni nota ha
la sua sezione `## Resume here` con i comandi per rieseguire.

---------------------------------------------------------------------------------------------------------------------------------

## 5. Indice delle note

| nota | contenuto | stato |
|---|---|---|
| 01 | costruzione dei modelli, `factory` | finale |
| 02 | JSON e HS3 (con record di provenienza, sezioni 10-11) | finale |
| 03 | API del fit, status, opzioni | finale |
| 04 | integrali, errori, normalizzazione | finale |
| 05 | dataset, grafici, bontà del fit | finale |
| 06 | backend e prestazioni | finale |
| 07 | workspace, HistFactory, RooStats, xRooFit, helper HS3 | finale |
| 08 | documentazione, manuale, release notes, tutorial, problemi noti | finale |
| 09 | qualità dei fit e effetto sulle rese (sezione 9: Xi, limiti, 50 partenze, offset, IntegrateBins) | finale |
| 10 | raccomandazione sul parsing dei modelli | finale |
| 11 | `PurityTask` end to end su copia, procedura raccomandata e partenze multiple, effetto a valle con efficienza, estrapolazione dei pioni | finale |
| 12 | toy: bias e copertura, minimi sbagliati per famiglia, regola delle partenze, `IntegrateBins` | finale |
| 13 | cosa di RooFit non è stato studiato; approcci pertinenti in rilievo | finale |
| `ROADMAP.md` | cosa fare nel progetto, in fasi | finale |

Il materiale scratch (`study09/`, `study0102/`, `study0405/`, `study060708/`, `study11/`, `study12/`, le cartelle `study-final-*`, `study09-final`, `study-last-*` e quelle della prima passata) è in `RooFitKnowledge/scratch/`: senza eseguibili, `*.root`, `*.png` e log, che si ricostruiscono con i `build.sh`. Le build di `study11` sono state eliminate (cache CMake con i vecchi percorsi): rigenerarle.
