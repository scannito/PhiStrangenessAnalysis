# 13 - Cosa di RooFit non è stato studiato

15 ottobre 2026. Scritta dal coordinatore a partire dal catalogo delle note 01 (classi) e 08 (tutorial, release notes, documentazione
per sviluppatori). **Nessuna delle voci qui sotto è stata provata sui dati del progetto.** Dove una voce dice che cosa risolverebbe,
è un'ipotesi [inferred] da verificare; i problemi a cui si aggancia sono invece misurati, nella nota citata.

La ricerca ha coperto a fondo tutto ciò che il progetto usa oggi: costruzione dei modelli, `factory`, JSON/HS3, `fitTo` e minimizzatore,
integrali ed errori, dataset binnati, grafici e bontà del fit, backend e thread, workspace, xRooFit, profilo e toy, la catena di purezza
(note 01-12). Questa nota elenca il resto.

| | voce | pertinenza | problema del progetto a cui si aggancia |
|---|---|---|---|
| A1 | Fit simultanei con parametri di forma condivisi (`RooSimultaneous`) | **alta** | Xi multimodali, `sigma_sig` al limite, celle a bassa statistica (09, 12) |
| A2 | Vincoli esterni sui parametri di forma (`ExternalConstraints`) | **alta** | come A1, con meno cambiamenti (09, 12) |
| A3 | `RooBernstein` al posto di `RooChebychev` | **alta** | Chebychev2 del K0S negativa nel 27.5% della scatola dei parametri (01, 08) |
| A4 | `sPlot` (pesi di segnale) | **media** | sottrazione del fondo nelle correlazioni |
| A5 | Fit 2D delle due masse della coppia | **media** | la purezza di coppia come prodotto di purezze |
| A6 | Fondo da template (`RooHistPdf`, Barlow-Beeston) | **media** | fondo del phi descritto da polinomi |
| A7 | Profilo della likelihood e MINOS nelle celle a bassa statistica | media, già studiato in parte | errori nelle celle piccole (07) |
| A8 | Bontà del fit basata sulla devianza | bassa | sostituto di `RooPlot::chiSquare` (05) |
| A9 | Fit delle efficienze (`RooEfficiency`) | bassa | solo se le mappe di efficienza venissero fittate |
| B | RooStats, HistFactory, morphing, convoluzioni, fit non binnati, reti neurali, multiprocesso, ... | nessuna | vedi sezione B |

---------------------------------------------------------------------------------------------------------------------------------

## A. Possibili approcci ulteriori (pertinenti)

### A1. Fit simultanei con parametri di forma condivisi - **pertinenza alta**

**Cos'è.** `RooSimultaneous` (in `factory`: `SIMUL::m(cat, a=A, b=B)`, nota 01) somma le likelihood di più istogrammi; i parametri si possono
dichiarare comuni a tutti i canali o propri di ciascuno. `RooSimWSTool` costruisce le copie per canale da un modello unico. Tutorial di partenza:
`rf501_simultaneouspdf`, `rf504_simwstool`. Nella nota 02 (sezione 11.2) è stato fatto un solo giro di prova, ma solo come contenitore HS3, non come metodo.

**Che cosa affronterebbe.** Oggi ogni cella (molteplicità × pT) è fittata da sola. Le celle Xi a pT 1.2-2.0 sono multimodali (nota 12: minimo
sbagliato nel 91% dei toy senza partenze multiple), `sigma_sig` di Xi sta al limite inferiore in 439 celle sorgente su 520 (nota 09, 9.8),
le celle pi_tof a bassa statistica danno purezze instabili (nota 11). Se la forma del picco (larghezza, code) dipende dal pT ma poco dalla
molteplicità, un fit simultaneo sui bin di molteplicità dello stesso pT, con la forma comune e le rese libere, userebbe tutta la statistica
per la forma e lascerebbe a ogni cella solo rese e fondo [inferred].

**Indizio a favore** [verified, nota 12 sezione 7]: per le celle Xi pT 1.6-2.0, far partire il fit dalla mediana dei fit delle altre classi di
molteplicità dello stesso pT toglie i minimi sbagliati in 19 celle su 20. L'informazione condivisa tra molteplicità aiuta già come punto di partenza;
A1 e A2 la userebbero dentro la likelihood.

**Da verificare prima.** Che la forma non dipenda davvero dalla molteplicità (si vede già dai fit separati: dispersione di `sigma_sig`,
`alpha`, `n` lungo la molteplicità nelle celle buone). Le purezze delle celle diventano correlate tra loro: va tenuto conto negli errori.
Il fit è più grande (10 canali), quindi più lento e con più parametri: le partenze multiple restano necessarie.

**Costo per provarlo.** Una nota di ricerca: un modello simultaneo per una famiglia (Xi, un pT), confronto con i fit separati sui dati
e sui toy della nota 12 (minimi sbagliati, bias, copertura).

### A2. Vincoli esterni sui parametri di forma - **pertinenza alta**

**Cos'è.** Un termine gaussiano (o altro) moltiplicato sulla likelihood per un parametro: `ExternalConstraints(set)` in `fitTo`, oppure dentro
il modello con `RooProdPdf` (nota 03, tabella delle opzioni: "not used"; tutorial `rf604_constraints`).

**Che cosa affronterebbe.** Gli stessi problemi di A1 con un cambiamento più piccolo: si fitta la forma su un istogramma ad alta statistica
(per esempio tutte le molteplicità sommate per quel pT) e la si impone come vincolo, con la sua incertezza, ai fit delle singole celle.
Il vincolo sostituirebbe i limiti duri che oggi decidono il risultato (`sigma_sig` di Xi, `n` del K0S a pT alto, nota 09 9.8) [inferred].

**Da verificare prima.** Come A1. In più: con `Offset` attivo e vincoli, la documentazione per sviluppatori segnala che il nuovo
`RooFit::TestStatistics` dà risultati diversi dalle classi vecchie solo per pdf simultanee con termini di vincolo (nota 08, 5.2): da controllare se
si combinano A1 e A2.

### A3. `RooBernstein` al posto di `RooChebychev` - **pertinenza alta**

**Cos'è.** Polinomio nella base di Bernstein (`Bernstein::b(x,{c0..cn})`, nota 01): **non negativo per costruzione se tutti i coefficienti
sono ≥ 0** [doc]. Lo stesso grado dà la stessa famiglia di polinomi del Chebychev; il vincolo c_i ≥ 0 è sufficiente, non necessario, per la
positività, quindi esclude alcune forme positive (poche, per gradi bassi) [doc, nota 01 tabella].

**Che cosa affronterebbe.** Il Chebychev2 del K0S è negativo nel 27.5% della scatola dei parametri ammessa (nota 08, 9.2); da lì vengono la
cascata di messaggi "p.d.f value is less than zero" e i casi in cui il fit esplora regioni non fisiche. Il limite `c1>=-1` del Chebychev1 è
esattamente la condizione di positività (nota 09, 9.8). Con Bernstein e c_i ≥ 0 il problema sparisce per costruzione [inferred].

**Da verificare prima.** Che i fit con Bernstein diano lo stesso NLL dei Chebychev nelle celle dove il Chebychev è positivo, e che
`RooBernstein` funzioni con il backend `cpu` in questa build (6.32.02 aveva un segfault, #15690, corretto; nota 08). Costo basso: è un cambio
di tipo nella configurazione e poche righe nel parser.

### A4. `sPlot` - **pertinenza media**

**Cos'è.** `RooStats::SPlot` (tutorial `rs301_splot`): dai fit in massa calcola per ogni evento (o per ogni bin di massa) un peso di segnale;
gli istogrammi di un'altra variabile pesati così riproducono la distribuzione del solo segnale, senza sideband.

**Che cosa affronterebbe.** Le correlazioni oggi tolgono il fondo con le sideband o con il fattore di purezza (`apply_purity`). Con i pesi
sPlot si otterrebbe la correlazione del solo segnale direttamente [inferred].

**Da verificare prima.** Requisito di sPlot: la variabile discriminante (la massa) deve essere indipendente dalla variabile di controllo
(Δφ, Δy) separatamente per segnale e fondo. Va controllato sui dati. Servono istogrammi con l'asse di massa insieme alle variabili di correlazione
(dipende da cosa produce O2Physics: da chiedere). Cambia il metodo, non solo il fit.

### A5. Fit 2D delle due masse della coppia - **pertinenza media**

**Cos'è.** Un modello prodotto (`RooProdPdf`, tutorial `rf3xx`) sulle masse dei due candidati (phi e K0S/Xi) con quattro componenti:
segnale×segnale, segnale×fondo, fondo×segnale, fondo×fondo.

**Che cosa affronterebbe.** Fattorizzare la purezza di coppia nel prodotto delle purezze singole presuppone che segnale e fondo dei due candidati
siano indipendenti nella coppia. Un fit 2D lo misura invece di presupporlo [inferred].

**Da verificare prima.** Servono istogrammi di massa × massa per coppia da O2Physics (non so se esistono: da chiedere). La statistica per cella
2D è molto più bassa.

### A6. Fondo da template - **pertinenza media**

**Cos'è.** `RooHistPdf` (un istogramma usato come pdf, tutorial `rf706_histpdf`; catalogato nella nota 01) e il trattamento Barlow-Beeston della
statistica del template (`RooParamHistFunc` con vincoli di Poisson, tutorial `rf709_BarlowBeeston`, nota 08 6b).

**Che cosa affronterebbe.** Il fondo del phi è oggi un polinomio o una funzione ad hoc (BkgSourav, BkgMattia). Un template da like-sign o
da mixed event avrebbe una forma motivata fisicamente [inferred].

**Da verificare prima.** Che O2Physics produca quegli istogrammi. Bug noto: integrale di `RooHistPdf` con ordine di interpolazione 1 (#20116,
nota 08).

### A7. Profilo della likelihood e MINOS nelle celle a bassa statistica - già studiato in parte

Nota 07: nelle celle buone il profilo coincide con Hesse (entro il 3%, entro lo 0.3% con `Offset("bin")`); MINOS è fallito nella prova;
`Minos` non si può combinare con `SumW2Error`/`AsymptoticError` (nota 03). Non studiato: le celle a bassa statistica (pi_tof con poche decine di
conteggi, Xi a pT basso), dove gli errori asimmetrici potrebbero contare. Pertinente solo se quelle celle restano nell'analisi.

### A8. Bontà del fit basata sulla devianza - pertinenza bassa

Il rapporto di likelihood con il modello saturato (la direzione presa dagli sviluppatori in #21697 e PR #21703, nota 08). La nota 05 ha
già mostrato che il chi2 di Pearson su tutte le bin è calibrato sui toy, quindi basta quello; la devianza sarebbe un'alternativa equivalente
ad alta statistica.

### A9. Fit delle efficienze - pertinenza bassa

`RooEfficiency` (tutorial `rf701_efficiencyfit`, `rf702_efficiencyfit_2D`) fitta un'efficienza da eventi passati/non passati. Oggi le efficienze
arrivano già come istogrammi (CCDB, `Corrections.root`): pertinente solo se si decidesse di parametrizzarle o lisciarle.

---------------------------------------------------------------------------------------------------------------------------------

## B. Non studiato e non pertinente (con il motivo)

| area | perché non serve al progetto |
|---|---|
| RooStats: limiti, CLs, test di ipotesi, intervalli bayesiani e MCMC, Feldman-Cousins, `ModelConfig` | l'analisi misura rese e purezze, non cerca segnali né mette limiti. Eccezione: sPlot (A4) |
| HistFactory | modelli a template multi-canale con sistematiche come parametri di disturbo; il tutorial `hf001` è stato solo eseguito (nota 08). Diventerebbe rilevante solo con A1+A6 su larga scala |
| Morphing (`RooLagrangianMorphFunc`, `RooMomentMorph`, `rf705_linearmorph`, `rf616_morphing`) | interpolazione di forme tra punti di un parametro fisico; non c'è un parametro del genere |
| Convoluzioni (`RooFFTConvPdf`, `RooNumConvPdf`, `rf208`) | il Voigtian del phi è già una convoluzione Breit-Wigner × Gauss analitica; nessun altro modello la richiede |
| Pdf condizionali ed errori per evento (`rf303`-`rf306`) | richiedono dati non binnati |
| Fit non binnati, `RooDataSet`, categorie, manipolazione dei dataset (`rf4xx`) | i dati arrivano da O2Physics come istogrammi |
| `RooKeysPdf`, `RooNDKeysPdf` | stime di densità a kernel da dati non binnati |
| Blinding (`RooUnblindPrecision` e simili) | nessuna misura cieca |
| Fit di ampiezze (`rf704`) | non ci sono ampiezze |
| Inferenza basata su simulazione, classificatori neurali (`rf615`, `rf617`), `RooONNXFunc` (6.40) | nessun uso nel progetto. È anche la risposta alla domanda sull'AI che guida i fit: la convergenza si ottiene con offset, refit e partenze multiple (note 09, 12), non serve un modello addestrato |
| `RooFit::TestStatistics` e `MultiProcess` (gradiente in parallelo) | non compilati nella build ALICE (nota 06); un fit dura millisecondi |
| Differenziazione automatica (Clad, codegen con gradiente) | provata solo per vedere che non regge `RooCrystalBall` (nota 06); i fit sono già veloci |
| Pythonizzazioni, I/O con numpy/pandas | il progetto è in C++; PyROOT provato solo per HS3 (nota 02) |
| Configurazione dei generatori numerici (`rf902`), `RooStudentT` (6.40) | non usati |

## C. Studiato solo come documentazione

- Tutorial: 257 file catalogati per argomento, **12 eseguiti**, una decina letti nel codice (nota 08, sezioni 6 e 6b); gli altri solo dall'intestazione.
- La Quick Start Guide in PDF non ha testo estraibile (nota 08).
- Forum e GitHub: solo le issue legate ai problemi trovati (lista di controllo nella nota 08, sezione 9), non una ricerca completa.

## D. Impossibile qui

- Test tra versioni di ROOT: la 6.36.04 non parte su questo sistema (errore di modulemap), la 6.38 e la 6.40 non sono installate (note 07, 08).
  Le differenze di 6.38 e 6.40 sono note solo dalle release notes, verificate sui file originali.
