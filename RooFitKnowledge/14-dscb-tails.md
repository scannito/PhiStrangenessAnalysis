# 14 - DSCB tail parameters (n1, n2, alpha1, alpha2): what the data determine and which limits to keep

Owner of this note: study 14 (21 October 2026). Scratch: `RooFitKnowledge/scratch/study14/`. Builds on note 09 (sections 9.2, 9.5, 9.8:
relaxed limits, fixed tails) and does not repeat it.

## Conclusione per il responsabile

**Risposta breve.** I limiti di n contano poco per la resa; conta come si tratta n dove i dati lo misurano. Su 13 celle reali (2024),
k0s, xi e pi_tpc, con il fit al suo minimo migliore (partenze multiple, `Offset("bin")`):

1. **n non è "difficile da determinare" per rumore.** Con più di circa 1e5 conteggi n è misurato con precisione (l'NLL sale di 1e2-2e4
   tra il valore migliore e il limite). Quando il fit va al limite superiore (150, o 50 per pi_tpc), è perché la coda dentro la finestra
   è esponenziale: per n -> infinito il DSCB diventa gaussiana più coda esponenziale, e in n il likelihood lì è piatto. Il limite superiore
   è un bordo finto di una direzione piatta, non un errore del fit. [source, verified]
2. **Per S quel limite è irrilevante**: portarlo da 150 (50) a 1e4 sposta S al massimo dello 0.07% (k0s e xi <= 0.05%, <= 0.1 sigma_S;
   pi_tpc 0.02% = 3.8 sigma_S solo perché lì sigma_S vale 0.005%). Lo stesso risultato della nota 09, qui cella per cella con i profili. [verified]
3. **Lasciato libero, n pesa poco sulla resa** (correlazione efficace S-n <= 0.44, mediana circa 0.05) **ma di più sulla purezza** (fino a 0.8 sigma_P
   per sigma di n, k0s n1). **Forzarlo a un valore che i dati non vogliono costa molto**: k0s pT 0.8-1.2 con n1 a 150: S -0.8% (25 sigma_S);
   k0s sorgente pT 1.8-1.9 con n2 a 150: S -5.5% (30 sigma_S). Quindi niente n1 = n2 (rifiutato dai dati, S fino a +2.3%), e niente n fisso
   per il K0S ad alta statistica. [verified]
4. **Toy alla statistica osservata** (4 celle, 300 toy, verità = miglior fit dei dati): Xi a 2.7e4 conteggi misura entrambi gli n (mai al limite),
   S senza bias per tutte le scelte (pull <= 0.1). K0S a 1.3e4 conteggi (pT 8-10): n non è identificabile (n1 tra 10 e 150 al 68%, al limite nel
   41% dei toy), ma S ha lo stesso bias (+0.35-0.40%, 0.15-0.2 sigma_S) dentro e al limite: **arrivare al limite non distorce S**. pi_tpc: n2
   finisce al limite nel 100% dei toy anche quando la verità è n2 = 50 (degenere con il fondo gaussiano a destra); S spostato di 0.08% (0.4 sigma_S). [verified]
5. **La riparametrizzazione in 1/n non cambia il minimo** (S identica entro 0.001% con gli stessi limiti, 13 celle su 13) e non migliora la convergenza della chiamata del progetto
   (nessuna scelta delle code la migliora: è il problema di offset/refit della nota 09). Serve solo se si apre il limite: con n <= 1e4 scritto in n
   l'errore propagato di S nelle celle povere diventa 4 volte troppo grande (RMS dei pull 0.26), scritto come 1/n >= 1e-4 resta vicino al giusto (0.87). [verified]

**Raccomandazione per specie** (tua la decisione):
- **k0s**: n1, n2 liberi e separati; limite inferiore 1 come oggi (nota 09); limite superiore: va bene 150. Se lo si vuole togliere dai flag di
  qualità, aprirlo come 1/n in [1e-4, 1], non come n <= 1e4. Non fissare n dal fit sommato in molteplicità: per pT 0.8-1.2 quel fit dà n = 150 e la cella
  vuole 3.5/10.8 (S -0.8%); probabilmente perché il fit sommato è tagliato da `nsig` <= 1e8 [inferred].
- **xi**: liberi e separati, [1,150] come oggi. Qui ogni scelta provata dà la stessa S entro 0.11% (<= 0.21 sigma_S); fissare n dal fit sommato in
  molteplicità sarebbe accettabile e dà i fit più stabili, ma non serve.
- **pi_tpc**: n1 libero (misurato bene, coda sinistra senza fondo). n2 a 50 lega quasi sempre ed è una domanda sulla forma del fondo, non sulla coda:
  per S è indifferente (<= 0.07%). Due scelte equivalenti per S: lasciare [1,50] e **non contare `n2_sig` al limite superiore come fit cattivo** nel
  record `FitQuality`; oppure 1/n2 in [1e-4, 1]. Non stringere a 30: costa fino a 1.3e4 in NLL e -0.12% (2.2 sigma_S) su S in una cella.
- **alpha** [0.5, 5]: tenere. È determinato meglio di n e lega solo in una cella pi_tpc (alpha1 a 0.5, come nella nota 09).

Cosa non è fatto: i toy coprono 4 celle, non le celle multimodali di Xi; le scelte sono state provate su 13 celle del 2024 e non su tutte le 734 celle
di analisi; il valore di n "da statistica sommata" per il K0S andrebbe rifatto con i limiti di resa alzati. Vedi `## Resume here`.

---------------------------------------------------------------------------------------------------------------------------------

## 0. Setup

- Data: `DataFile/pp/DeltaY/Data/AnalysisResults_136_2024_PtMod_5agosto.root` (read only), the input of the 2024 purity file; config
  `JSONConfigs/fitConfig.json` as it is today (k0s, xi: n in [1,150], alpha in [0.5,5]; pi_tpc: n in [1,50], alpha in [0.5,5]; start n=5).
- Cells chosen from the fresh record `MoreFullDifferential/FitQuality` of `claude-sandbox/sb-fitq/.../Data_2024_Purity.root` (1690 rows;
  dumped by `dumpfq.cpp` -> `fq-2024.txt`, parsed to `fq-2024.tsv`). In that record (the project's single `fitTo`), tail parameters at a
  limit: k0s analysis n2 2 rows, k0s source n2 25, xi source n2 1, pi_tpc analysis n2 15 / n1 6 / alpha2 2, pi_tpc source n2 114 /
  n1 18 / alpha1 10 / alpha2 1 [verified, `fq-2024.tsv`]. Most of those rows are status 3 or covQual 2, so they are a record of where
  the project's fit stopped, not of where the minimum is (note 09).
- 13 cells (`cells.txt`): k0s analysis (5,0) pT 0-0.3 [n2 at 150 in the record], (4,3) pT 0.8-1.2 [1.2e7 counts]; k0s source (0,15)
  pT 1.8-1.9 [n2 at a limit], (0,33) pT 8-10 [1.3e4 counts]; xi analysis (4,0) pT 0.8-1.2, (4,1) pT 1.2-1.6 [multimodal region,
  note 12], (4,6) pT 4.2-6.5 [2.7e4 counts]; xi source (7,3) pT 1.1-1.2 [n2 at 150]; pi_tpc analysis (4,2) pT 0.4-0.5 [3.6e8],
  (2,5) pT 0.8-1.0 [n2 at 50], (4,9) pT 2-3; pi_tpc source (0,33) pT 3.4-3.6 [2.9e5, n2 at 50], (1,29) pT 2.6-2.8 [n1, n2, alpha1 at limits].
  (i, k) = (multiplicity bin, pT bin), 0-based.
- Program `dscb.cpp` (built by `build.sh dscb`). It rebuilds the model (DSCB + Chebychev1/2 or Gaussian, extended `RooAddPdf`)
  instead of using `DynamicRooFitter`, so that n can be tied, fixed or written as 1/n; for every cell it first checks that its std build
  gives the same NLL as `DynamicRooFitter` at the config start: **difference 0 in 13/13 cells** [verified, `profA.err`, `profB.err`].
  S and P are computed as in `ExtractYieldsAndPurity` (window integral of the normalised component times its yield; errors by
  `getPropagatedError`).
- "Best" = lowest NLL over 13 starts (config start + 12 random starts inside the std ranges, the same starts for every variant, plus
  the std best for the non-std variants), each start an `Offset("bin")` fit repeated until the NLL moves by less than 0.01 (note 09
  9.9). NLL compared without offset (`createNLL(..., Extended(true))`).
- Profiles: n1, n2 on a log grid 1 ... 150 (23 points) plus 300 and 1000 (beyond today's limit, the range of the profiled variable is
  widened while it is constant); alpha on 17 points in [0.5, 5]. At each point the other parameters are free inside today's limits and
  are fitted from two starts (the previous grid point and the global best), lower NLL kept. Outputs `profA-*.tsv`, `profB-*.tsv`
  (run: `./dscb prof <data> cellsA.txt profA`, same for B); summaries `a1.py` -> `res-a1.txt`, `a2.py` -> `res-a2.txt`.
- CPU: the whole profile run of 13 cells took about 5 s per process [verified, `profA.err`]; noisy, relative only.

## 1. How RooCrystalBall builds the tails [source]

Source: `sw/SOURCES/ROOT/v6-36-10-alice4/.../roofit/roofit/src/RooCrystalBall.cxx`.

- The project builds `RooCrystalBall(name, "DSCB", obs, mean, sigma, alpha1, n1, alpha2, n2)` (`Codes/DynamicRooFitter.h:400-407`), the
  constructor with one sigma for both sides: **alpha1/n1 is the LOW side (left, t < -alpha1), alpha2/n2 the HIGH side.**
- `evaluate()` (lines 203-229): t = (x - x0)/sigma; core `exp(-t^2/2)` for -alpha1 <= t <= alpha2; tails `evaluateCrystalBallTail`
  (lines 163-169): `exp(-alpha^2/2) * pow(r/(b - t), n)` with r = n/alpha, b = r - alpha (t replaced by -t on the right). Rewritten with
  the distance from the junction, u = alpha (|t| - alpha) >= 0:

  f_tail = exp(-alpha^2/2) * (1 + u/n)^(-n)

  Value and first derivative are continuous at |t| = alpha (slope -alpha on both sides). Negative alpha is folded by `std::abs`.
- **n -> infinity**: (1 + u/n)^(-n) -> exp(-u), so f_tail -> exp(alpha^2/2 - alpha |t|): **an exponential tail of slope alpha (in units
  of sigma)**, i.e. Gaussian core plus exponential tail. For finite n the tail is a power law, heavier than that exponential by
  ln(f_n / f_inf) = u - n ln(1 + u/n) = u^2/(2n) - u^3/(3n^2) + ... [inferred, algebra]. So **the shape depends on n only through
  1/n, smoothly, and n = infinity is an ordinary point in 1/n**; in n itself the likelihood becomes flat (d/dn ~ 1/n^2) and a fit that
  wants an exponential tail runs to whatever upper limit it is given. This is the mechanism behind the upper-limit hits of note 09
  (k0s n2 at 150 in 41 analysis cells, pi_tpc n2 at 50 in 171).
- **Where the n-alpha degeneracy comes from**: the local log-slope of the tail is -alpha/(1 + u/n). A heavier tail over a finite
  window can be made either by a smaller alpha (the tail starts closer to the core, with a smaller initial slope) or by a smaller n
  (the slope decreases faster with distance). Only the far tail, where u^2/(2n) is large, separates the two. Measured: corr(alpha, n)
  -0.74 to -0.86 on the same side in every cell where n is not at a limit, about 0 where n sits at a limit (section 2.2).
- **Normalisation and n <= 1**: the tail integral over an infinite range is sigma exp(-alpha^2/2) (n/alpha)/(n - 1) [`integrateTailRegular`,
  lines 191-197, with tmin -> -infinity] and diverges as n -> 1; for n <= 1 it is infinite. RooFit integrates only over the observable
  window (the switch to the log form at |n - 1| < 1e-5 is in `analyticalIntegral`, the #19602 issue of note 08), so the fit can go below
  1, but then the "signal" tail is a slowly falling power law that over the window cannot be told from background: note 09 found S +8.6%
  (Xi) and +1.1% (K0S) there and recommended keeping n >= 1.
- **What part of the data determines n**: only the tail at distances where u^2/(2n) is not small compared with the relative precision
  of the tail counts. The window reaches (std best, t in units of sigma, `res-a1.txt`, tail table):

| section | sigma (best) | alpha | window in t (low / high) | u_max = alpha (|t|max - alpha) | comment |
|---|---|---|---|---|---|
| k0s, pT < 2 | 3.3-4.5 MeV | 1.2-1.8 | -10 to -14 / +12 to +16 | 10-25 | long reach: n up to about u_max^2/2 = 50-300 can be told from an exponential |
| k0s, pT 8-10 | 6.1 MeV | 0.7-1.0 | -7.6 / +8.8 | 6 | short reach, 1.3e4 counts: n is not determined (best at 150 on both sides) |
| xi | 2.0 MeV (at its lower limit) | 1.1-2.1 | -16 / +20 | 17-37 | longest reach in sigma units; the tails sit on a background comparable to them (signal/background in the tails 0.5-2) |
| pi_tpc | 0.5-1.05 | 0.5-3.9 | -9.5 to -21 / +9.5 to +19 | left 5-30, right 15-17 | left tail on NO background (1e-15 to 35 background counts); right tail under the background Gaussian (mean 4-10) |

  `logDev` in the same table is ln(f_n/f_inf) at the window edge for the fitted n: 11-15 for k0s pT < 2 with n = 2-11 (the tail at the
  edge is 1e5-1e6 times above the exponential), 5-27 for Xi with n = 4-17, but 0.1-2.7 wherever n sits at its upper limit (k0s (5,0) n2,
  k0s (0,33), xi (7,3) n2, pi_tpc n2): there the tail inside the window is an exponential within a factor 1-15 at the edge, and n is
  not measurable. [verified, `res-a1.txt`]
- For pi_tpc the two sides are different problems: **n1 is measured on a clean tail** (no background on the left), **n2 competes with
  the background Gaussian** on the right, and in 4 of 5 pi_tpc cells it runs to its limit 50; with the limit opened (1/n down to 1e-4)
  it runs to n = 1e4 (section 4), i.e. the data want a right tail at least as steep as an exponential, a background-shape question
  rather than a tail-limit one [inferred].

## 2. Profiles on 13 real cells [verified, `dscb prof`, `res-a1.txt`]

### 2.1 Is n determined?

In all cells with more than about 1e5 counts, **yes, sharply, at least on one side**: the NLL rises by 1e2-2e4 between the best n and
the far limit (k0s (4,3): n1 = 3.52, NLL +1.8e4 at n1 = 1 and +1.3e4 at 150; pi_tpc (4,9): n1 = 5.74, +3.4e4 at 50; xi (4,1): n1 = 4.09,
+28.6 at 150, +448 at 1). The profiles are so steep that in most cells above 1e5 counts no grid point next to the best lies within dNLL < 0.5 (`res-a1.txt`, "range(dNLL<0.5) none").

Not determined (flat towards infinity, best at the upper limit): k0s (5,0) n2 (dNLL -3.55 from 150 to 1000), k0s (0,33) n1 and n2
(1.3e4 counts: within dNLL < 0.5 for n1 in [13, 150] and n2 in [20, 150]), xi (7,3) n2 ([80, 150] within 0.5), xi (4,0) n2
([50, 150]), all pi_tpc n2 at 50 (dNLL -19.6 to -1.9e4 from 50 to 1000: the limit binds hard). These are the cells where n "is hard to
determine": it is not noise, the tail inside the window is exponential.

At the LOWER limit: k0s source (0,15) n2 = 1 (the case of note 09 9.8, K0S source pT bins 14-16): the profile rises monotonically to
+2.4e3 at 150; the data want n2 <= 1.

### 2.2 Correlations at the best (Hesse, std parametrisation, `res-a1.txt`)

- alpha-n on the same side: -0.74 to -0.86 when n is not at a limit (k0s, xi, pi_tpc alike); with 1/n the sign flips and the size is
  the same (+0.74 to +0.86): **the reparametrisation does not remove the alpha-n correlation**, it only removes the upper boundary.
- n-nsig: up to -0.62 (k0s (0,15) n1), -0.2 to -0.3 for k0s and Xi, about 0 for pi_tpc (P is about 1 there and the window [-3,3]
  contains nearly all the signal).
- n1-n2: -0.00 to +0.21. alpha-nsig: up to -0.57.

### 2.3 Does n matter for S and purity? (`res-a1.txt`, "S and P sensitivity")

rhoS = |S(v) - S0| / sigma_S / sqrt(2 dNLL(v)) at the two grid points next to the best, which is |corr(S, n)| for a parabolic profile:
how many sigma_S the yield moves when n moves by its own 1 sigma.

| cell | N | rhoS n1 / n2 | rhoP n1 / n2 | S shift if n forced to 150 (50 for pi_tpc) | ... to 1000 |
|---|---|---|---|---|---|
| k0s a(4,3) | 1.2e7 | 0.14 / 0.08 | 0.71 / 0.34 | n1: -0.77% (-25 sigma_S), n2: -0.14% (-4.7) | -0.79% / -0.15% |
| k0s a(5,0) | 2.0e6 | 0.16 / 0.01 | 0.81 / 0.02 | n1: -0.23% (-3.0); n2 already there | n2: -0.006% (-0.08) |
| k0s s(0,15) | 6.6e5 | 0.44 / 0.27 | 0.78 / 0.52 | n1: -2.5% (-13.5); **n2: -5.5% (-29.5), P -0.042** | n2 -5.6% |
| k0s s(0,33) | 1.3e4 | 0.03 / 0.00 | 0.10 / 0.05 | already there | -0.02% (-0.02) |
| xi a(4,0) | 1.8e5 | 0.01 / 0.01 | 0.09 / 0.05 | n1: -0.25% (-0.07) | n2 -0.02% |
| xi a(4,1) | 2.9e5 | 0.24 / 0.18 | 0.52 / 0.34 | n1: -0.63% (-2.6), n2: -0.20% (-0.8) | -0.65% / -0.21% |
| xi a(4,6) | 2.7e4 | 0.18 / 0.16 | 0.37 / 0.39 | n1: -0.36% (-0.46), n2: -0.65% (-0.83) | -0.39% / -0.68% |
| xi s(7,3) | 6.5e4 | 0.19 / 0.01 | 0.42 / 0.02 | n1: -0.41% (-0.83); n2 already there | n2 -0.013% |
| pi_tpc a(4,2) | 3.6e8 | 0.00 / 0.00 | 0 / 0.03 | n1: -0.001%; n2 at 50 | n2: -0.020% (-3.7) |
| pi_tpc a(2,5) | 1.4e8 | 0.00 / 0.00 | 0 / 0.05 | n1: -0.002% | n2: +0.011% (+1.3) |
| pi_tpc a(4,9) | 2.4e7 | 0.02 / 0.01 | 0.00 / 0.90 | n1: -0.018% (-0.9), n2: -0.036% (-1.8) | -0.08% / -0.04% |
| pi_tpc s(0,33) | 2.9e5 | 0.16 / 0.01 | 0.06 / 0.05 | n1: +2.8% (+14.5) | n2: -0.06% (-0.3) |
| pi_tpc s(1,29) | 3.6e6 | 0.03 / 0.00 | 0.00 / 0.05 | both at 50 | n1 +0.14% (+2.5) |

Reading:
- **Within the likelihood's own interval n hardly matters for S**: rhoS <= 0.44 (median about 0.05), i.e. letting n float costs at most
  0.44 sigma_S of extra spread and usually nothing. **The purity is more sensitive** (rhoP up to 0.8 for k0s n1, 0.5 for Xi n1): P
  depends on how much of the tail falls outside the integration window. [verified]
- **Forcing n to a value the data do not want moves S by many sigma** where n is determined: -0.8% (25 sigma_S) in k0s (4,3), -5.5%
  (30 sigma_S) in k0s (0,15), +2.8% in pi_tpc (0,33). So n is not a nuisance one can fix freely in high-statistics cells. [verified]
- **Where n sits at the upper limit, the limit does not matter**: moving it from 150 (50) to 1000 shifts S by at most 0.06% (k0s, xi:
  <= 0.08 sigma_S; pi_tpc 0.02% = 3.7 sigma_S in the 3.6e8-count cell, whose sigma_S is 0.005%). Consistent with note 09 (9.2, 9.8:
  "n_hi harmless", S < 0.04%). [verified]
- alpha: the same picture, stronger: forcing alpha to its limits moves S by 1-8% (k0s, xi) and 2-3.5% (pi_tpc), but at the best alpha is
  determined to about the grid step or better (dNLL > 0.5 at the grid neighbours in most cells); only pi_tpc (1,29) has alpha1 at 0.5
  (as in note 09 9.8, pi_tpc source pT bin 29).

## 3. Toys at the observed statistics

[verified, `./dscb toy <data> toyA.txt toyA 300` and `toyB`, outputs `toyA-toys.tsv`, `toyB-toys.tsv`, summary `a3.py` -> `res-a3.txt`]

Four cells, 300 toys each. Truth = the std multi-start best on the data; each bin is drawn as Poisson(N_tot x pdf(bin centre) x bin
width), i.e. exactly the bin-centre likelihood the fit uses, with the data histogram's bins inside the range (K0S 200, Xi 72, pi_tpc 100).
Every toy is fitted with every variant from 4 starts (the truth + 3 random starts inside the std ranges), lowest NLL kept. "At a limit"
uses the project's tolerance (1e-5 of the range), so it is not comparable between n and 1/n (1e-5 of [1, 150] in n is not 1e-5 of
[1/150, 1] in 1/n): compare limit counts only within one parametrisation. A second run of the whole study (data and toys) reproduced
the first bit for bit (`run1/` against the current files: 0 rows differ).

| cell (truth n1 / n2) | variant | fits ok (st 0, cq 3) | n at a limit (hi / lo) | fitted n1 median [16%, 84%] | fitted n2 | S bias | pull S mean (RMS) | P bias | pull P mean |
|---|---|---|---|---|---|---|---|---|---|
| k0s s(0,15) pT 1.8-1.9, 6.6e5 (2.07 / 1) | std | 280 | 0 / 284 | 2.04 [1.99, 2.10] | 1 [1, 1] | -0.019% | -0.10 (1.01) | -0.0002 | -0.19 |
| | fixmerged (2.16 / 1) | 300 | - | fixed | fixed | -0.16% | -0.95 (0.99) | -0.0013 | -2.1 |
| | tie | 300 | 0 / 0 | 1.20 | = n1 | **+0.99%** | **+3.99** (0.93) | +0.0076 | +4.4 |
| | inv / inv_open / nmax1e4 | 288 / 297 / 240 | | 2.04 | 1 | -0.019% | -0.09 / -0.10 / -0.08 | | |
| k0s s(0,33) pT 8-10, 1.3e4 (150 / 150) | std | 299 | 123 / 0 | 52.5 [10, 150] | 80.9 [12, 150] | +0.38% | +0.16 (**0.55**) | +0.0039 | +0.75 |
| | std, toys with n at the limit | | 123 | | | +0.35% | +0.19 (0.62) | | |
| | std, toys with n inside | | 177 | | | +0.40% | +0.14 (0.49) | | |
| | nmax30 | 296 | 208 / 0 | 30 [9.4, 30] | 30 [11.4, 30] | +0.51% | +0.28 (0.68) | +0.0053 | +1.10 |
| | fixmerged (7.68 / 7.21) | 300 | - | fixed | fixed | **+1.19%** | +1.06 (0.93) | +0.0128 | +3.25 |
| | tie | 296 | 72 / 0 | 95.5 [15, 150] | = n1 | +0.28% | +0.19 (0.76) | +0.0028 | +0.52 |
| | inv | 296 | 199 / 0 | 48.6 [10, 150] | 76.5 [12, 150] | +0.38% | +0.31 (0.87) | +0.0039 | +0.60 |
| | inv_open (n <= 1e4) | 296 | 175 / 0 | 51 [10, 1e4] | 84 [12, 1e4] | +0.36% | +0.28 (0.87) | +0.0036 | +0.53 |
| | nmax1e4 (same, in n) | 300 | 12 / 0 | 51 [10, 9.9e3] | 82 [12, 9.8e3] | +0.36% | +0.09 (**0.26**) | +0.0036 | +0.60 |
| xi a(4,6) pT 4.2-6.5, 2.7e4 (14.4 / 7.6) | std | 295 | 0 / 0 | 14.5 [10.8, 20.3] | 7.15 [6.1, 8.6] | +0.04% | +0.07 (1.02) | +0.0004 | +0.18 |
| | nmax30 / fixmerged (11.4 / 8.2) / tie / inv / inv_open / nmax1e4 | 296-298 | nmax30: 16 hi | | | +0.04% to +0.08% | +0.04 to +0.10 (1.03-1.07) | +0.0003 to +0.0005 | +0.17 to +0.25 |
| pi_tpc s(0,33) pT 3.4-3.6, 2.9e5 (1.6 / 50) | std | 300 | **300** / 0 | 1.58 [1.57, 1.59] | 50 [50, 50] | +0.077% | +0.40 (0.93) | +0.0007 | +1.9 |
| | nmax30 | 300 | 299 / 0 | 1.58 | 30 | +0.13% | +0.68 (0.93) | +0.0013 | +4.4 |
| | fixmerged (1.69 / 50) | 300 | - | fixed | fixed | +0.32% | +1.65 (0.93) | +0.0008 | +2.1 |
| | tie | 248 | 0 / 0 | 1.87 | = n1 | +0.27% | +1.57 (1.23) | +0.0029 | (P error 0: meaningless) |
| | inv_open / nmax1e4 | 299 / 299 | n2 at 1e4 in 296 / 195 | 1.58 | **1e4 [1e4, 1e4]** | -0.044% | -0.22 / -0.20 (0.9) | -0.0006 | -1.3 / -1.1 |

What the toys say:
- **Identifiability.** n is identifiable where the tail reaches far enough with enough counts: Xi at 2.7e4 counts recovers both n
  (14.5 vs 14.4, 7.15 vs 7.6, 68% bands 10.8-20.3 and 6.1-8.6) and never hits a limit with the std limits. K0S pT 8-10 at 1.3e4 counts
  (window to 7.6-8.8 sigma, u_max about 6) does not: the 68% band of n1 is [10, 150] with a truth at 150, and 41% of the toys end at the
  upper limit. pi_tpc n2 is not identifiable at all: generated at n2 = 50, fitted at 50 (the limit) in 300/300 toys and at 1e4 in 296/300
  when the limit is open; [inferred] n2 trades with the background Gaussian on the right (whose truth here is nbkg = 0, at its own
  boundary), which is why the data cells also run there. n1 of pi_tpc is recovered to 1%.
- **Is S biased when n ends at a limit?** No, not by the limit: in K0S pT 8-10 the S bias is the same in the toys at the limit
  (+0.35%, pull +0.19) and inside (+0.40%, +0.14); it is a low-statistics bias of the whole fit (0.15-0.2 sigma_S), not of n. In
  k0s s(0,15) (truth n2 at the lower limit 1) S is unbiased (-0.02%, pull -0.10). In pi_tpc the limit-bound fit has pull +0.40
  (+0.08%), the open one -0.22 (-0.04%): the limit costs at most 0.4 sigma_S there.
- **Errors.** Where n is not identifiable the propagated S error is too large: pull RMS 0.55 with n <= 150 (K0S pT 8-10), **0.26 with
  n <= 1e4 written in n**, 0.87 with 1/n in [1e-4, 1] or [1/150, 1]. So if the upper limit is opened it should be opened in 1/n, where
  the flat direction is a finite, well-scaled interval and Hesse sees a sensible curvature [verified for this cell only].
- **Choices that bias S**: fixing n at the value of the multiplicity-merged spectrum, when that value is not the truth (K0S pT 8-10: +1.2%,
  1.1 sigma_S; pi_tpc: +0.32%, 1.6 sigma_S; K0S pT 1.8-1.9: -0.16%, -0.95 sigma_S); tying n1 = n2 (K0S pT 1.8-1.9 +1.0%, 4 sigma_S, P
  +0.0076, 4.4 sigma_P); n <= 30 in pi_tpc (+0.13%, 0.7 sigma_S). Merged-n fixing is harmless only for Xi (+0.045%).
- Fit status in the toys (status 0 and covQual 3, after the multi-start) is 80-100% for every variant; `nmax1e4` is the worst in K0S
  pT 1.8-1.9 (240/300), `fixmerged` the best (300/300 in three cells).

## 4. Candidate choices on the same 13 cells (data) [verified, `res-a2.txt`]

Variants (same starts, same procedure): `std` (today), `nmax30` (n <= 30 on both sides), `fixmerged` (n1, n2 fixed to the best fit of
the same pT range summed over all multiplicity bins, std limits), `tie` (n1 = n2), `inv` (fit 1/n in [1/150, 1] (pi_tpc [1/50, 1]):
the same model and limits as std, reparametrised), `inv_open` (1/n in [1e-4, 1], i.e. n up to 1e4).

Multi-start best of each variant against the std best (dNLL > 0 = worse; S in % and in sigma_S):

| cell | nmax30 | fixmerged | tie | inv | inv_open |
|---|---|---|---|---|---|
| k0s a(4,3) | 0 | **+1.35e4, -0.78% (-26)** (merged n 150/150) | +3.3e3, -0.14% (-4.6) | 0 | 0 |
| k0s a(5,0) | +23.7, +0.012% | +6.0, -0.04% | +102, -0.07% (-0.9) | 0 | -4.0, -0.007% |
| k0s s(0,15) | 0 | +0.86, -0.11% (-0.6) | **+228, +2.3% (+12)** | 0 | 0 |
| k0s s(0,33) | +0.29, +0.19% (+0.18) | +2.7, +1.0% (+0.94) | 0 | 0 | -0.03, -0.05% |
| xi a(4,0) | +1.65, +0.07% | +1.49, +0.11% | +4.5, -0.01% | 0 | 0 |
| xi a(4,1) | 0 | +0.15, -0.02% | +0.13, +0.003% | 0 | 0 |
| xi a(4,6) | 0 | +0.35, +0.05% | +1.6, +0.02% | 0 | 0 |
| xi s(7,3) | +1.45, +0.06% (+0.12) | +1.09, -0.10% (-0.21) | +5.4, -0.07% | 0 | -0.20, -0.015% |
| pi_tpc a(4,2) | **+1.3e4, -0.006% (-1.1)** | +0.01 | +4.6e3, -0.001% | 0 | -2.1e4, -0.020% (-3.8) |
| pi_tpc a(2,5) | +3.8e3, -0.005% | +0.12 | +2.2e3, -0.002% | 0 | -4.7e3, +0.012% (+1.4) |
| pi_tpc a(4,9) | 0 | +4.9, +0.001% | +535, -0.013% (-0.6) | 0 | 0 |
| pi_tpc s(0,33) | +15.9, +0.03% | +31.8, +0.23% (+1.2) | +2.4e3, +0.26% (+1.3) | 0 | -20.4, -0.06% |
| pi_tpc s(1,29) | +842, **-0.12% (-2.2)** | 0 | 0 | 0 | -427, +0.07% (+1.2) |

- `inv` reproduces std to |dS| <= 0.001% in 13/13 cells (it is the same model): the reparametrisation alone changes nothing at
  the minimum. `inv_open` = raising the upper limit to 1e4, which note 09 already measured as harmless; here at most 0.07% (pi_tpc,
  1.2-3.8 sigma_S in cells with sigma_S of 0.005-0.05%), <= 0.05% for k0s and xi.
- `nmax30` costs nothing where n is small, but binds in pi_tpc with dNLL up to 1.3e4 and moves S by 0.12% (2.2 sigma_S) in pi_tpc
  (1,29); for k0s/xi at most 0.19% (0.18 sigma_S).
- `tie` is rejected by the data wherever n is measured (dNLL 1e2-5e3) and moves S by +2.3% (12 sigma_S) in k0s (0,15): the two tails are
  different (n1 = 2.07, n2 = 1 there; n1 = 3.5, n2 = 10.8 in k0s (4,3)).
- `fixmerged` is good for Xi (|dS| <= 0.11%, <= 0.21 sigma_S, dNLL <= 1.5) and in the pi_tpc cells whose merged n equals the cell's
  (but +0.23% in (0,33)). For k0s it fails in (4,3): the merged spectrum of pT 0.8-1.2 (about 1e8 counts) gives n1 = n2 = 150, while the
  cell wants 3.5/10.8 [verified]; [inferred] that merged fit is probably distorted by the `nsig` maximum of 1e8 (note 09 section 3.8:
  merged K0S cells above 1e8 counts are clipped), so the value to fix would have to come from a merged fit with the yield limits raised.
  In the low-count k0s (0,33) it moves S by +1.0% (0.94 sigma_S).

**Fit behaviour** of the project-style single fit (the options of `DoFit()`, config start, no offset), per variant (`res-a2.txt`, second
table): status 0 and covQual 3 in 5 (std), 4 (nmax30), 6 (fixmerged), 8 (tie), 3 (inv), 3 (inv_open) of 13 cells; more than 0.5 above
the variant's own best in 4, 4, 5, 4, 6, 5 cells. **No tail parametrisation fixes the convergence of the project's call**: the cells that
fail (all pi_tpc high-statistics cells, dNLL 1e1-7e5, S off by 0.1-0.3%) fail with every variant; that is the offset/refit problem of
note 09 (section 4), not the tails. `inv` happens to bring pi_tpc a(4,2) from dNLL 5.4e5 to 35 but makes a(2,5) worse (21 -> 2.9e5).
Multi-start stability (fraction of the starts within 0.5 of the best) is similar for all variants: all 14 starts agree in 4 cells for
`fixmerged` and `inv_open`, 3 for `nmax1e4`, 2 for the others (the multimodal Xi a(4,1) stays at 2-5 of 14 for every free-n variant, 14/14 with n fixed).
A seventh variant, `nmax1e4` (n <= 1e4 in n, added after the first run), reaches the same minima as `inv_open` in all 13 cells (S equal to
<= 0.002%) and gives 5 / 5 / 2 in the single-fit counts (status 0 and covQual 3 / dNLL > 0.5 / |dS| > 0.1%), against 3 / 5 / 2 for `inv_open`.

## Resume here

State (21 October 2026): items 1-4 DONE on 13 cells (data) and 4 cells (toys). Everything reruns in about 3 minutes of two processes:

```
cd RooFitKnowledge/scratch/study14 && source ../rooenv.sh && bash build.sh dscb
D=/Users/scannito/alice/Analysis/DataFile/pp/DeltaY/Data/AnalysisResults_136_2024_PtMod_5agosto.root
./dscb prof $D cellsA.txt profA 2> profA.err & ./dscb prof $D cellsB.txt profB 2> profB.err & wait
./dscb toy $D toyA.txt toyA 300 2> toyA.err & ./dscb toy $D toyB.txt toyB 300 2> toyB.err & wait
python3 a1.py > res-a1.txt; python3 a2.py > res-a2.txt; python3 a3.py > res-a3.txt
```

Files: `dscb.cpp` (model rebuilt with the variants `std`, `nmax30`, `fixmerged`, `tie`, `inv`, `inv_open`, `nmax1e4`; cell check against
`DynamicRooFitter`), `common.h`/`parts.h` copied from `study-last-09` (the `RooFitMessages.h` include removed: that header is only on the
wip branch), `dumpfq.cpp` + `fq-2024.txt`/`fq-2024.tsv` (the FitQuality record), `cells.txt`, `cellsA.txt`, `cellsB.txt`, `toyA.txt`,
`toyB.txt`, outputs `prof?-best.tsv`, `prof?-prof.tsv`, `prof?-corr.tsv`, `prof?-tail.tsv`, `toy?-toys.tsv`, summaries `res-a1.txt`
(geometry, correlations, profiles), `res-a2.txt` (variants on data), `res-a3.txt` (toys); `run1/` = first run, identical;
`git-status-before.txt`/`git-status-after.txt` (only this note added).

NOT DONE:
- The variants on ALL analysis and source cells (734 + 2620): `dscb prof` costs about 0.4 s per cell; generate a cells file from
  `fq-2024.tsv` (and the 2026 file) and drop the profile loop if only the variants are wanted. This would give the per-species numbers
  the recommendation rests on with full coverage.
- Toys for the multimodal Xi cells (a(4,1), pT 1.2-1.6; 2026 pT 1.6-2.0, note 12) and for a high-statistics K0S cell where n is measured
  (k0s a(4,3)): `printf 'xi analysis 4 1\nk0s analysis 4 3\n' > toyC.txt; ./dscb toy $D toyC.txt toyC 300`.
- `fixmerged` for K0S with the yield limits raised (nsig, nbkg max 1e11), to check the [inferred] clipping of the merged fit at pT
  0.8-1.2; `MergedN` in `dscb.cpp` uses the std config.
- pi_tpc n2 against the background model: whether a different background shape (or a free second Gaussian) makes n2 measurable.
- Not run: the 2026 file; any change in the project (`fitConfig.json` untouched).
