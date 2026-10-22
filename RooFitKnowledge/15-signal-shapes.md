# 15 - Forme del segnale per K0S, Xi e pi_tpc (21 ottobre 2026)

## Conclusione per il responsabile

Domanda: il segnale di k0s, xi e pi_tpc (oggi DSCB, con n1/n2 difficili da determinare) è descritto meglio da un'altra forma?
Provate su 15 celle reali del file 2024 (binning di analisi) sette forme con lo stesso fondo, le stesse rese e la stessa finestra
della config, 5 partenze per forma; poi toy (30 per verità) su una cella per specie. Numeri e script nelle sezioni sotto.

- **K0S: tenere la DSCB.** È la forma migliore in 4 celle su 5, con un NLL più basso di 200-8000 unità rispetto a ogni alternativa;
  le forme a 4 parametri (Johnson, Gauss con code esponenziali) spostano la resa dello 0.2-4.4% e la purezza fino a 0.036. Nei toy la DSCB
  è senza bias (+0.04 σ_S) anche quando n è al limite (metà dei toy) e anche su una verità Johnson; le alternative su una verità DSCB hanno
  un bias di 2-3 σ_S. Il problema di n al limite per il K0S non tocca S né P: le code a potenza servono, n al limite vuol dire solo che
  da quel lato la coda è esponenziale. Solo nella cella con meno conteggi (pT 4.2-6.4, 3.8e5) Johnson è equivalente (ΔNLL -9).
- **Xi: i dati non scelgono tra DSCB, Johnson e Gauss con code esponenziali.** Con il limite inferiore di `sigma_sig` della config (2 MeV)
  la DSCB resta bloccata lì e Johnson vince di 479 in una cella solo perché la sua larghezza non è legata a quel limite: la questione vera
  per Xi è quel limite (note 09, 14), non le code. Con il limite dimezzato e `IntegrateBins` le tre forme stanno entro ΔNLL da -6 a +13.5;
  Johnson ha due parametri in meno ed è preferita dall'AIC in 3 celle su 4, con S entro lo 0.2% della DSCB. Nei toy (pT 1.6-2.0) DSCB e
  Johnson sono senza bias sulla propria verità (+0.5 σ_S, +0.2 σ_S) e si scostano di 0.7-1.3 σ_S l'una sulla verità dell'altra.
  Consiglio: tenere la DSCB (o passare a Johnson, è indifferente per S) e usare l'altra come variazione sistematica della forma.
- **pi_tpc: passare alla Gaussiana con code esponenziali (EGE) è l'opzione più pulita; la resa non cambia.** Nessuna forma descrive
  la distribuzione a 2e7-1e9 conteggi (devianza/ndf da 262 a 14604), ma nella finestra [-3,3] S cambia al massimo dello 0.09% (EGE) e
  0.12% (Johnson), P al massimo di 0.001 (P vale 0.9996-0.99996). Il problema è la salute del fit DSCB: n al limite (50) in 5 celle su 6, e
  nei toy covQual<3 in metà dei fit, minimo migliore trovato da una sola partenza su tre in 19 toy su 30, pull RMS di S 4.9 (errore 5 volte
  troppo piccolo). La EGE (= DSCB con n infinito, 4 parametri) non ha parametri al limite in nessuna cella, pull RMS 1.05 sulla propria
  verità; su una verità DSCB il bias di S è -2.8e-4 relativo. Johnson descrive meglio (devianza fino a 10 volte più bassa) ma ha covQual 2
  in 3 celle su 6 e il suo `mu` sbatte contro il limite [-1,1] di `mean_sig` in 2.
- **Da scartare:** singola Gaussiana (S -2.4/-7.7% sul K0S, -2.8/-4.4% su Xi), doppia Gaussiana (peggio della DSCB ovunque salvo una
  cella pi_tpc), Bukin (parametri ai limiti, normalizzazione numerica inesatta a queste statistiche), Voigtian (code lorentziane che
  mangiano il fondo, una normalizzazione rotta nel pi_tpc).
- **Cosa servirebbe nel parser:** per Johnson un ramo `"Johnson"` in `ParseModelString` con `RooJohnson` (già in ROOT) e parametri
  `mean_sig`, `lambda_sig`, `gamma_sig`, `delta_sig` (mu con un range più largo di `mean_sig`); per la EGE una classe `RooAbsPdf` propria
  (circa 30 righe, integrale analitico, provata qui solo compilata), che però non passa dall'export HS3 né dalla `factory` senza lavoro in
  più (nota 02, nota 10). Nessuna delle due ha `sigma_sig`, per cui una finestra n-sigma non funzionerebbe con loro (oggi queste specie
  usano finestre fisse). Alternativa senza codice nuovo per il pi_tpc: la DSCB con n fisso grande, che è il campo della nota 14.
- **Non fatto:** il file 2026, più celle per la stabilità, Hypatia, la EGE sotto Cling, toy con più di 30 repliche e più partenze.

---------------------------------------------------------------------------------------------------------------------------------

## 1. Setup

- Program: `scratch/study15/shapes.cpp`, built by `build.sh` as `shapes` (first k0s/xi/pi run), `shapes2` (the same plus a Poisson
  deviance column) and `shapes3` (plus the `IB` option for `IntegrateBins`); each binary was built from the source as it was at that
  point and the later additions do not change the earlier outputs (`real-b.tsv` from `shapes2` reproduces `real-b-v1.tsv` from `shapes`
  digit for digit in every common column) [verified].
- Input: `DataFile/pp/DeltaY/Data/AnalysisResults_136_2024_PtMod_5agosto.root` (the 2024 file of the purity run in
  `claude-sandbox/sb-fitq`), analysis binning, histograms as in `scratch/study-last-12/parts.h`. The `RooDataHist` sums equal the
  `counts` of the FitQuality tree (`fq2024.tsv`, dumped by `dumpfq.C`) for every cell [verified: `real-*.log`, `toys-xi.log`; the
  `counts` printed by `LoadCell` for Xi are lower because they use bin centres strictly inside [1.29,1.36]; the observable range becomes
  [1.289,1.361] with 72 bins on import, as note 05 found].
- Same for every shape [source, `JSONConfigs/fitConfig.json`, read 21 Oct]: observable range, background (k0s Chebychev1, Chebychev2
  for pT bins 4-5; xi Chebychev1; pi_tpc Gaussian), yields and their limits, fixed integration window (k0s [0.475,0.515], xi
  [1.305,1.335], pi_tpc [-3,3]), mean and core-width limits. Extended binned NLL, bin centres, unless `IB` is set.
- Fit procedure per shape: config start + 4 random starts (yield fraction uniform, shapes uniform in the inner 90 % of the box), each
  `Offset(true)` fit + `Offset(true)` refit (steps 1-2 of note 09); best = lowest NLL among status 0/1/3. A start is excluded if
  NLL - deviance/2 departs from the cell's median by more than 1e-4 |NLL| + 10 (broken normalisation: RooVoigtian on pi-m1p2 start 1,
  NLL 7.4e6 below every other fit, deviance 50 times larger). Smaller departures are expected: the extended term uses nsig+nbkg, the
  deviance the sum of the bin-centre expectations, and they differ by the quadrature error (up to about 60 on Xi) [verified, `summ.py`].
- "dev" = Poisson deviance over the in-range bins, ndf = bins - free parameters; chi2 = Pearson (explodes where a shape's tail is far
  below the data, so dev is the column to read).

| name | pdf | free signal pars | limits |
|---|---|---|---|
| DSCB | `RooCrystalBall` (as the project) | mean, sigma, alpha1, n1, alpha2, n2 | config |
| G | `RooGaussian` | mean, sigma | config |
| DG | two `RooGaussian`, common mean, sigma2 = r sigma1 | mean, sigma, r, f | r [1,6], f [0,1] |
| EGE | Gaussian core + exponential tails beyond -kL, +kH sigma (Das, arXiv:1603.08591): the DSCB at n = infinity, value and slope continuous. Custom `RooAbsPdf` with analytic integral, `shapes.cpp` | mean, sigma, kL, kH | kL, kH [0.5,5] |
| JOHN | `RooJohnson` (Johnson SU) | mu, lambda, gamma, delta | mu = `mean_sig` limits, lambda [0.5 slo, 30 shi], gamma [-5,5], delta [0.3,50] |
| BUKIN | `RooBukinPdf` | Xp, sigp, xi, rho1, rho2 | xi [-1,1], rho1 [-1,0], rho2 [0,1] |
| VOIGT | `RooVoigtian` (in the parser already) | mean, width, sigma | width [0, shi] |

`RooHypatia2` is also in this ROOT [verified: `$ROOTSYS/include`]; not tried (NOT DONE).

- Cells (15; `cells.tsv`, chosen from `fq2024.tsv`): k0s m5p0, m9p0 (n2 at its limit in the project fit), m0p3 (4.6e6 counts), m0p4
  (Chebychev2, c1 at limit), m9p9 (pT 4.2-6.4, 3.8e5); xi m0p0 (pT 0.8-1.2, 5.4e4), m9p2 (pT 1.6-2.0, sigma at its limit, 2.1e5), m0p6,
  m9p6 (pT 4.2-6.5, 1.5e4 and 1.1e4); pi_tpc m0p0 (pT 0.2-0.3, n1, n2 at limit), m1p2, m9p1 (n2 at limit, 4e8 and 1.1e9), m5p8, m7p7
  (n2 at limit, covQual 2 in the project), m9p9 (pT 2-3, 2e7). Mult bin 0 is 0-1 %, 9 is 70-100 % [verified, `real-*.log`].

## 2. Item 1: the candidates on real cells

ΔNLL = NLL(shape) - NLL(DSCB) at each one's best start; positive = worse than the DSCB. dS = S/S_DSCB - 1 in the window.

### 2.1 K0S [verified: `real-a.tsv`, `summary-a.txt`]

| cell (counts) | DSCB chi2/ndf | G | DG | EGE | JOHN | BUKIN | VOIGT |
|---|---|---|---|---|---|---|---|
| m5p0 (2.0e6) | 7.8 | +20894, dS -2.5% | +10950, -0.9% | +254, -0.25% | +212, -0.20% | +871, -0.14% | +13006, +0.07% |
| m9p0 (2.6e6) | 11.5 | +28928, -2.4% | +15405, -0.9% | +311, -0.22% | +219, -0.17% | +1342, +0.04% | +18396, +0.05% |
| m0p3 (4.6e6) | 30.4 | +53491, -2.6% | +11293, -0.7% | +4725, -0.8% | +3736, -0.6% | +7094, -0.65% | +13000, +0.23% |
| m0p4 (3.8e6) | 44.8 | +37437, -7.7% | +1716, -1.05% | +8076, -4.4% | +5998, -3.6% | +7189, -3.9% | +1910, -1.5% |
| m9p9 (3.8e5) | 1.5 | +3821, -3.4% | +613, -0.27% | +69, -0.52% | **-9**, -0.08% | +30, -0.27% | +528, +2.3% |

P - P_DSCB follows S: from -0.0012 (JOHN, m9p9) to -0.036 (EGE, m0p4); G -0.014 to -0.055.

- **The DSCB is the best shape in 4 of 5 K0S cells, by 200 to 8000 units of NLL**, far more than its 2 extra parameters. In m9p9 Johnson
  is better by 9 with two parameters fewer [verified].
- Where the statistics are high (m0p3, m0p4) every 4-parameter shape loses thousands and moves the yield by 0.6-4.4 %. The DSCB's
  chi2/ndf is 1.5-45, so it too is rejected above 1e6 counts (note 05), but the others are 2-25 times worse.
- n2 at its upper limit 150 in the best DSCB of m5p0 and m9p0, as in the project fits [verified]: that tail wants to be exponential.
  EGE (both tails exponential) is still worse by 254-311: the other side needs the power law (n1 = 10-13 there).
- (`real-a.tsv` comes from `shapes`, without the deviance column; the BUKIN and VOIGT rows of m9p9 and xi took most of the CPU: their
  normalisations are numerical.)

### 2.2 pi_tpc [verified: `real-b.tsv`, `summary-b.txt`]

| cell (counts) | DSCB dev/ndf | DSCB at limit | G | DG | EGE | JOHN | BUKIN | VOIGT |
|---|---|---|---|---|---|---|---|---|
| m0p0 (2.0e8) | 1258 | n1, n2 hi | +204749, -0.22% | +6656, -0.27% | **-7136**, -0.01% | **-43657**, -0.06% (cq 2) | +229096 | +169255 |
| m1p2 (4.2e8) | 6408 | n2 hi | +606351, +0.25% | +460124, +0.14% | **-16958**, -0.02% | **-241303**, +0.04% (cq 2, mu at lo) | +1046088 | +581544 |
| m9p1 (1.1e9) | 14604 | n2 hi | +1548269, +0.26% | +1147094, +0.16% | **-38996**, -0.02% | **-586782**, +0.05% (mu at lo) | +2597776 | +1482488 |
| m5p8 (8.2e7) | 584 | n2 hi | +529080, +0.23% | **-2041**, +0.01% | +21024, -0.03% | +48033, -0.00% (cq 2) | +318816 | +92704 |
| m7p7 (6.6e7) | 534 | n2 hi | +151884, -0.00% | +54058, -0.08% | +2726, +0.00% | **-4486**, +0.03% | +157156 | +102145 |
| m9p9 (2.0e7) | 2803 | - | +905375, +0.92% | +68182, +0.36% | +28497, -0.09% | +6197, -0.12% | +67504 | +90737 |

ndf 89-95. BUKIN and VOIGT dS within 0.55 % and 0.4 %.

- **No shape describes the pi_tpc nσ distribution**: the best dev/ndf is 262-14604 [verified]. The ranking moves from cell to cell:
  Johnson is best in 4 of 6 (dev/ndf 262 vs 1258, 1007 vs 6408, 1456 vs 14604, 416 vs 534), DG in m5p8, the DSCB in m9p9. Part of the
  misfit may be the Gaussian background, the same for every shape [inferred].
- **The yield in [-3,3] does not care**: EGE and JOHN move S by at most 0.09 % and 0.12 %, P by at most 0.0005 and 0.0010. G is off by up
  to 0.9 %, DG 0.36 % [verified].
- The DSCB has n at its upper limit (50) in 5 of 6 cells [verified]. EGE has no parameter at a limit in 6 of 6, 4 signal parameters
  instead of 6, and is better than the DSCB in 3 cells (by 7136-38996), worse in 3 (by 2726-28497): where only one DSCB tail goes to n = 50,
  the other tail's power law still helps.
- Johnson: covQual 2 in 3 of 6 best fits; its `mu` is not the mode when gamma is not zero, so the config's [-1,1] limit stops it in m1p2
  and m9p1 (gamma -1.1, -1.2).
- BUKIN: worst or second worst everywhere, rho1/rho2 at limits in 6 of 6, and its numerical normalisation is inexact at these counts
  (NLL - dev/2 off by 539-8510 in 3 cells). VOIGT: the one broken normalisation of the study (pi-m1p2, excluded), a Lorentzian tail that
  takes the place of the background there (`mean_bkg` at its limit) [verified].

### 2.3 Xi [verified: `real-a.tsv`/`summary-a.txt` (config limits), `real-xi-siglo.tsv`/`summary-xi-siglo.txt` (sigma lower limit
1 MeV instead of 2, env `SIGLO=0.5`), `real-xi-siglo-ib.tsv`/`summary-xi-siglo-ib.txt` (same plus `IB=1e-6`, `obs->setBins(72)`)]

With the config limits the DSCB core width sits at 2 MeV in m9p2, and Johnson, whose width is not tied to `sigma_sig`, wins by 479 with
chi2/ndf 1.93 against 18.2. That is the width limit (note 09 section 9: relaxed, the Xi width is 1.1-2.1 MeV), not the tails, so the
comparison below uses the relaxed limit for every shape (G, DG, EGE, JOHN, DSCB; BUKIN and VOIGT not rerun).

| cell (counts) | DSCB dev/ndf | G | DG | EGE | JOHN |
|---|---|---|---|---|---|
| bin centres | | | | | |
| m0p0 (5.4e4) | 1.43 | +391, -2.8% | +106, -0.75% | +5.2, -0.34% | +8.5, -0.08% |
| m9p2 (2.1e5) | 2.71 | +1541, -2.6% | +287, -0.49% | +75.6, -0.54% | +38.9, -0.32% |
| m0p6 (1.5e4) | 1.30 | +189, -4.4% | +15.6, -1.0% | +5.6, -0.49% | +4.4, +0.03% |
| m9p6 (1.1e4) | 1.07 | +163, -4.3% | +12.0, -0.87% | +7.7, -1.04% | +5.8, -0.11% |
| `IntegrateBins(1e-6)` | | | | | |
| m0p0 | 1.42 | +383, -2.8% | +97.7, -0.71% | +0.3, -0.14% | +0.1, -0.03% |
| m9p2 | 2.72 | +1501, -2.4% | +246, -0.31% | +13.5, -0.29% | **-6.0**, -0.12% |
| m0p6 | 1.29 | +185, -4.3% | +11.7, -0.89% | +3.2, -0.67% | +0.3, +0.19% |
| m9p6 | 1.04 | +162, -4.3% | +10.2, -0.91% | +6.6, -0.91% | +3.9, -0.10% |

- **On Xi the data do not separate DSCB, Johnson and EGE**: with bin integration they are within ΔNLL -6.0 to +13.5, and the bin-centre
  approximation alone moves the Johnson-DSCB difference by up to 45 (m9p2: +38.9 at bin centres, -6.0 integrated). With 2 parameters
  fewer, Johnson is preferred by AIC in 3 of 4 cells (ΔAIC -3.3 to -15.9; +3.9 in m9p6), EGE in 2 of 4 [verified].
- S: Johnson within 0.19 % of the DSCB, EGE within 0.9 %, DG within 0.9 %, G 2.4-4.4 % low. P: Johnson within 0.0014, EGE 0.0054.
- With the relaxed limit the DSCB core width is 1.73-1.97 MeV and n2 is at 150 only in m0p0 (integrated) [verified].

### 2.4 Stability of the parameters [verified: parameter lists at the end of each `summary-*.txt`]

| | K0S (5 cells) | Xi (4 cells, relaxed, integrated) | pi_tpc (6 cells) |
|---|---|---|---|
| DSCB | alpha 1.1-1.7; n1 1.55-12.6; n2 2.3-150 (150 in 2) | alpha 0.97-1.54; n1 5.2-30; n2 10.6-150 (150 in 1) | alpha 1.5-3.9; n1 3.7-50 (50 in 1); n2 3.5-50 (50 in 5) |
| EGE | kL 1.03-1.19, kH 0.93-1.74 | kL 0.94-1.11, kH 0.89-1.41 | kL 1.15-3.32, kH 1.66-2.19 |
| JOHN | delta 1.30-1.75, gamma -0.23-0.55 | delta 1.18-1.62, gamma -0.06-0.33 | delta 2.1-4.7, gamma -1.2-0.85, mu at -1 in 2 |
| DG | | f 0.56-0.73, r 1.93-2.33 | f 0.62-0.94, r 1.34-2.97 |

- The EGE tail parameters are in a narrow band and never at a limit in the 15 cells; the DSCB's n spans two orders of magnitude and sits
  at its upper limit in 8 of 15 best fits (2 K0S, 1 Xi, 5 pi_tpc). Johnson's delta and gamma are smooth on K0S and Xi; on pi_tpc gamma
  grows to -1.2 and drags mu to the limit [verified].
- 15 cells, one file: whether these bands hold across all 270 analysis cells is NOT DONE.

## 3. Item 2: toys at the observed statistics

`MODE=toys`: a generating shape is fitted to the real cell (5 starts), its expected bin contents (bin centre x width) are the truth,
each toy is Poisson of them; every fitted shape uses the config start + 2 random starts, best NLL kept; 30 toys per (cell, truth).
Bias against the generating model's own S and P [verified: `toysumm.py`]. Bias errors are the error of the mean over 30 toys.

### 3.1 K0S m5p0 (2.0e6 counts) [verified: `toys-k0s.tsv`, `toys-k0s-summary.txt`]

| truth | fit | S bias (rel.) | in σ_S | pull RMS S | P bias | in σ_P | n at limit |
|---|---|---|---|---|---|---|---|
| DSCB | DSCB | +0.00003 ± 0.00013 | +0.04 | 0.90 | +0.0001 | +0.5 | 15/30 |
| DSCB | EGE | -0.0025 ± 0.0001 | -3.3 | 3.40 | -0.0025 | -19.5 | |
| DSCB | JOHN | -0.0019 ± 0.0001 | -2.4 | 2.56 | -0.0019 | -13.6 | |
| JOHN | DSCB | +0.0002 ± 0.0001 | +0.23 | 0.84 | +0.00001 | +0.1 | 30/30 |
| JOHN | EGE | -0.0004 ± 0.0003 | -0.5 | 1.95 | -0.0004 | -3.0 | |
| JOHN | JOHN | -0.0001 ± 0.0001 | -0.12 | 0.82 | -0.00004 | -0.3 | |

- **The DSCB is unbiased on its own truth and on the Johnson truth**, with n at a limit in half (own truth) or all (Johnson truth) of the
  toys: n at a limit does not bias S or P here, and its propagated errors are right (pull RMS 0.84-0.90) [verified]. The 4-parameter
  shapes are biased by 2-3 σ_S on a DSCB truth.

### 3.2 Xi m9p2 (2.1e5 counts, sigma lower limit 1 MeV, bin centres) [verified: `toys-xi.tsv`, `toys-xi-summary.txt`]

| truth | fit | S bias (rel.) | in σ_S | pull RMS S | P bias | in σ_P | n at limit | covQual<3 |
|---|---|---|---|---|---|---|---|---|
| DSCB | DSCB | +0.0012 ± 0.0006 | +0.47 | 1.23 | +0.0005 | +0.75 | 0 | 0 |
| DSCB | EGE | -0.0067 ± 0.0005 | -2.6 | 2.84 | -0.0033 | -5.6 | | 0 |
| DSCB | JOHN | -0.0034 ± 0.0005 | -1.3 | 1.74 | -0.0018 | -2.9 | | 12/30 |
| JOHN | DSCB | +0.0018 ± 0.0005 | +0.68 | 1.30 | +0.0008 | +1.2 | 11/30 | 0 |
| JOHN | EGE | +0.0002 ± 0.0006 | +0.07 | 1.34 | -0.00004 | -0.06 | | 0 |
| JOHN | JOHN | +0.0005 ± 0.0005 | +0.21 | 1.12 | +0.0002 | +0.31 | | 6/30 |

- DSCB and Johnson are compatible with no bias on their own truth (+0.5 and +0.2 σ_S) and differ by 0.7-1.3 σ_S on each other's: on Xi
  the choice between them is a systematic of about 0.2-0.3 % on S and 0.001-0.002 on P. EGE is biased by -2.6 σ_S on a DSCB truth [verified].

### 3.3 pi_tpc m5p8 (8.2e7 counts) [verified: `toys-pi.tsv`, `toys-pi-summary.txt`]

| truth | fit | S bias (rel.) | in σ_S | pull RMS S | pull RMS P | at limit | covQual<3 | best reached by 1 start of 3 |
|---|---|---|---|---|---|---|---|---|
| DSCB | DSCB | -0.00022 ± 0.00010 | -2.1 | 4.86 | 6.3e5 | 11/30 | 15/30 | 19/30 |
| DSCB | EGE | -0.00028 ± 0.00002 | -2.5 | 2.66 | 9.6 | 0 | 9/30 | 3/30 |
| DSCB | JOHN | -0.00008 ± 0.00002 | -0.7 | 1.26 | 32.8 | 0 | 17/30 | 0 |
| EGE | DSCB | -0.00012 ± 0.00015 | -1.1 | 49.3 | 22.9 | 30/30 | 12/30 | 13/30 |
| EGE | EGE | -0.00005 ± 0.00002 | -0.5 | 1.05 | 2.6 | 0 | 5/30 | 2/30 |
| EGE | JOHN | +0.00044 ± 0.00002 | +4.0 | 4.14 | 41.5 | 0 | 10/30 | 0 |

- At this statistics the DSCB fit is the worst behaved of the three even on its own truth: half the fits covQual < 3, the best minimum
  reached by only one of 3 starts in 19 of 30 toys, S pull RMS 4.9 (the propagated error 5 times too small). On an EGE truth (n = ∞) n sits
  at 50 in every toy and the S pull RMS is 49 [verified].
- EGE on its own truth: bias -0.5 σ_S, pull RMS 1.05, no limits. Every bias is below 5e-4 relative: for the pion the shape question is
  about the fit's health and its errors, not about S. The P pulls are meaningless where nbkg sits at 0 (P = 1 - O(1e-5)).

## 4. Item 3: recommendation [inferred, from sections 2-3]

| species | recommendation | why | what the parser needs |
|---|---|---|---|
| k0s | **keep DSCB** | best in 4/5 cells by 200-8000 NLL; unbiased in toys with n at a limit; alternatives -0.2 to -4.4 % on S | nothing. n at its limit is harmless for S and P here; whether to fix or constrain it is note 14 |
| xi | **keep DSCB**, Johnson as the shape systematic | DSCB, Johnson, EGE equivalent within ΔNLL 13.5 once the width limit is relaxed; Johnson-DSCB 0.2-0.3 % on S. The decision that matters is the `sigma_sig` lower limit | for the systematic: a `"Johnson"` branch (below) |
| pi_tpc | **EGE** (or DSCB with n fixed large, note 14) | same S within 0.09 %; no parameter at a limit in 6/6 cells; toys: pull RMS 1.05 against 4.9 for the DSCB, fewer covQual<3 and fewer lost minima | an `"ExpGaussExp"` model: a compiled `RooAbsPdf` (below) |

What the parser would need [inferred; source `Codes/DynamicRooFitter.h:395-420` for the existing branches, `:535-550` for the window]:

- `"Johnson"`: `RooJohnson(name, title, obs, mean_sig, lambda_sig, gamma_sig, delta_sig)` (in ROOT 6.36, `RooJohnson.h`). Limits used
  here: `lambda_sig` [0.5 sigma_lo, 30 sigma_hi], `gamma_sig` [-5,5], `delta_sig` [0.3,50]; `mean_sig` is mu, not the mode, so it needs a
  range wider than the peak's (pi_tpc hit -1 in 2 cells). Not checked: HS3 export of `RooJohnson`, `factory` spelling.
- `"ExpGaussExp"`: not in RooFit. The class in `shapes.cpp` (about 30 lines, analytic integral, no `ClassDef`) works in a compiled binary
  [verified]; under Cling and through HS3 or the `factory` it was not tried, and a custom class needs its own importer/exporter for HS3
  (note 02) and its library loaded before a workspace is read (note 01). Parameters `mean_sig`, `sigma_sig`, `kl_sig`, `kh_sig` [0.5,5].
  The DSCB with n1 = n2 fixed at a large value is the same shape without new code [inferred: the EGE is the n → ∞ limit] - note 14's
  subject.
- In n-sigma mode `CalculateIntegrationLimits` reads `mean_sig` and `sigma_sig`: Johnson has no `sigma_sig` (and its `mean_sig` would
  not be the peak), so an n-sigma window would throw; the EGE has both. Today k0s, xi and pi_tpc use fixed windows [source, `fitConfig.json`].

## 5. What was not done

- The 2026 file; more than 15 cells; stability across all analysis cells (NOT DONE).
- `RooHypatia2`; asymmetric hybrids (power-law tail on one side, exponential on the other), which section 2.2 suggests for pi_tpc
  (NOT DONE).
- Toys: 30 per (cell, truth), 3 starts per fit, bin centres; K0S and pi_tpc with the config limits, Xi with the relaxed width only; no
  toys with `IntegrateBins` (NOT DONE).
- The EGE class under Cling (NOT DONE). Timing: Bukin and Voigtian are much slower (numerical normalisation), not measured (other jobs
  were running).

## Resume here

- Items 1-3 done on 15 cells of the 2024 file. Everything is in `scratch/study15/`: `shapes.cpp` (+ `build.sh`; `OUT=shapes3 bash
  build.sh shapes` builds the current source), `cells.tsv`, `summ.py`, `toysumm.py`, outputs `real-*.tsv`, `summary-*.txt`, `toys-*.tsv`,
  `toys-*-summary.txt`; git status before/after: `git-status-start.txt`, `git-status-end.txt` (only the two new notes 14, 15 appeared).
- Rerun: `source ../rooenv.sh; ./shapes3 cells.tsv out.tsv 4` (env `SHAPES=DSCB,EGE,...`, `SIGLO=0.5`, `IB=1e-6`); toys:
  `MODE=toys NTOYS=30 GEN=DSCB,JOHN SHAPES=DSCB,EGE,JOHN ./shapes3 <one-cell.tsv> toys.tsv 2`; then `python3 summ.py out.tsv`,
  `python3 toysumm.py toys.tsv` (`NOBROKEN=1` for `IB` runs, where NLL and bin-centre deviance differ by construction).
- Next if wanted: the same on the 2026 file and on all pi_tpc cells with EGE vs DSCB (cheap: about 1 minute for 6 pi cells x 7 shapes);
  a one-sided hybrid for pi_tpc; Xi toys with the config's 2 MeV limit.
