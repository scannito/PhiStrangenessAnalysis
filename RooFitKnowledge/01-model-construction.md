# 01 - Model construction and the PDF catalog (COMPLETED 14 October 2026, second pass)

Status: the first pass (paused) wrote the head-to-head experiment, the catalog and the grammar from source. A third pass (15 October 2026, section "Third pass") explained `$REMOVE()`, tested RooParametricStepFunction, RooPoisson and RooNDKeysPdf through the factory, compared the plots of the factory-built model with the project's `DrawFit`, and wrote and read back a workspace with a custom compiled class. The second pass completed the grammar tests, the error behaviour, ranges / Extended / SumCoefRange, RooHistPdf / RooKeysPdf, a custom class, the equivalence over 1440 real cells, the Cling behaviour and a robust recipe (section "Second pass" below). The final pass (15 October 2026) found why the restricted `$REMOVE(prodname)` fails, and closed the note: the final NOT DONE heading lists only items that are impossible here. The decision document that uses this note is `10-model-parsing-recommendation.md`. Second-pass scratch directory: `SC2 = /Users/scannito/alice/Analysis/RooFitKnowledge/scratch/study0102` (`gram/`, `misc/`, `proto/`).

Provenance tags: **[doc]** Doxygen comment or tutorial, **[source]** code read, **[verified]** something that was run here, **[inferred]** conclusion not tested.
Source tree used: `~/alice/sw/SOURCES/ROOT/v6-36-10-alice2/v6-36-10-alice2/` (called `SRC` below). The installed ROOT is `v6-36-10-alice4-local1`; I did not diff the two trees, so a source claim is "as read in alice2". Everything marked [verified] ran against the installed alice4 build.

## Summary

- **[verified] `RooWorkspace::factory` can replace `ParseModelString`/`CreatePdfComponent`.** First pass: DSCB+Chebychev1 (cells mult0_pt2, mult5_pt10), DSCB+Chebychev2 (mult0_pt4, mult3_pt4), Voigtian+Chebychev2, Gaussian+Gaussian sum, Chebychev1*Exponential product, and on a synthetic phi histogram Voigtian+BkgSourav2 / BkgMattia (as `EXPR::`): minNll, status, covQual, edm, every value and error bit-identical (difference exactly 0.0) between `DynamicRooFitter` and the factory-built model. Second pass: the same on **all 1440 real cells** (k0s 340, xi 260, pi_tpc 360, pi_tof 340, phi offline 70, phi online 70) with the project's own `ExtractYieldsAndPurity` running on the factory-built model: 1440 of 1440 identical in every field.
- **[verified] Parameter naming is fully under your control.** The factory only creates what you name; with names `mean_sig`, `c1_bkg`, `nsig` ... the project's `FindVar` lookups work unchanged, and so do the pdf names (`DSCB_sig`, `Chebychev1_bkg`, `model`).
- **Caveats of the factory route** (all [verified] unless noted): it needs Cling at run time (`createArg` calls `gROOT->ProcessLineFast`, [source]) and each class it constructs costs ~1.7 ms (cacheable, see "Construction cost"); `SUM::` without coefficients throws `std::invalid_argument` out of `factory()`; a failed `factory()` returns nullptr except when the name to be created already exists; a formula that does not compile in `EXPR::` terminates the process; `$Typedef` (used in a tutorial) does not exist, only `$Alias`; `$Match` does not exist at all.
- **[verified] The "p.d.f value is less than zero" messages are real negative Chebychev2 values**, reachable inside the JSON limits c1,c2 in [-1,1]: about 29 % of the (c1,c2) square gives a negative minimum on [-1,1], about 63 % of the (c1,c2,c3) cube (grid computation, python). Chebychev1 with |c1| <= 1 is safe (minimum 1-|c1| >= 0). In the cpu backend a negative value is packed into a NaN so Minuit backs out ("Returning maximum FCN so far ... to force MIGRAD to back out"), seen in a re-run of cell mult3_pt4.
- **[verified] Several K0S fits end with Minuit status 3 and large EDM even in the project's own path** (mult0_pt2: status 3, edm 0.84; mult0_pt4: status 3, edm 0.42; mult3_pt4: status 3, edm 2.6; mult5_pt10: status 0, edm 0.17). The factory path reproduces them exactly, so this is the fit, not the construction route. (Note 03 establishes that status 3 means MIGRAD ended with EDM above 0.01.)
- **Environment hazard [verified]:** `alienv printenv` fails when several processes run it at once, leaving `root`/`root-config` unfound. Use the file `../rooenv.sh` (captured once). For PyROOT also `export DYLD_LIBRARY_PATH=$ROOTSYS/lib` (the alienv environment leaves it empty).

## API reference

### Factory grammar (RooWorkspace::factory -> RooFactoryWSTool::process)

Source: `SRC/roofit/roofitcore/src/RooFactoryWSTool.cxx`, the Doxygen comment of `RooFactoryWSTool::process`, plus the code of `processSingleExpression`, `createArg`, `SpecialsIFace::create` [doc/source]. Class page: https://root.cern/doc/master/classRooFactoryWSTool.html (fetch only showed the member list; the grammar tables are in the .cxx comment). Tutorials: `SRC/tutorials/roofit/roofit/rf511_wsfactory_basic.C`, `rf512_wsfactory_oper.C`, `rf513_wsfactory_tools.C` [doc].

Variables [doc, process() comment; behaviour [verified]]:

| Expression | Meaning |
|---|---|
| `x[-10,10]` | RooRealVar with range; the RooRealVar(name,title,min,max) constructor sets the value to the midpoint (verified: value 0 for [-10,10]) |
| `x[3,-10,10]` | initial value 3, range [-10,10] |
| `x[3]` | constant 3 (verified: `isConstant()==1`, range [-inf,inf]) |
| `3.5`, `-2` | numeric literal -> `RooConst` wherever a RooAbsReal/RooAbsArg argument is expected |
| `c[A,B,C]`, `tag[B0=1,B0bar=-1]` | RooCategory (first token non-numeric) |

Objects [doc]:
- `Class::name(a,b,c)` builds `RooClass(name,name,a,b,c)`; the class name may omit the `Roo` prefix (`resolveClassName` tries `Roo`+name) [source, verified by use of `Gaussian::`, `CrystalBall::`].
- `Class(a,b,c)` auto-names the object (verified: `gobj0`).
- Arguments may be created inline and nested: `Gaussian::g(x[-10,10],m[0],3)`. `{a,b,c}` becomes a RooArgSet or RooArgList depending on the constructor argument.
- Constructor choice (`ctorArgs`): the first public non-default constructor whose argument count can accept the number of tokens given; numbers are converted to double/int/bool, enum labels (e.g. `NoMirror` for `KeysPdf`) are resolved, dataset names are looked up in the workspace [source, rf511 tutorial]. Consequence [inferred from source]: overloads with the same argument count are ambiguous; `CrystalBall::` with 7 arguments (x,x0,sigma,aL,nL,aR,nR) picks the symmetric-core double-sided constructor because the other overloads take 8 or 5-6 arguments. [verified: that exact call produced the same fit as `RooCrystalBall` built by hand].

Operators:

| Expression | Effect |
|---|---|
| `SUM::m(f1*A,f2*B,C)` | RooAddPdf, `f1*A+f2*B+(1-f1-f2)*C` (fractions) [doc] |
| `SUM::m(n1*A,n2*B)` | N coefficients for N pdfs -> yields, extended RooAddPdf. [verified] `SUM::sume(ns[100,0,1000]*g1,nb[50,0,1000]*ch)`: `canBeExtended=1`, `expectedEvents=150`; same with fractions: `canBeExtended=0` |
| `RSUM::m(f1*A,f2*B,C)` | recursive fractions `f1*A+(1-f1)(f2*B+(1-f2)C)` [verified builds] |
| `ASUM::m(f1*a1,...)` | RooRealSumPdf of amplitudes (functions, may be negative, sum may not) [doc] |
| `PROD::m(A,B)` | RooProdPdf; `PROD::m(A\|x,B)` conditional [doc]. [verified, second pass] `PROD::pp(g1,ch)` integrates to 1.000000000000 and equals the numerically normalised product g1*ch/Int(g1*ch) to 9e-12; `PROD::cond(gy\|x,g1)` integrates to 1.0000000000 over (x,y) (`SC2/misc/out-grammar2.txt`) |
| `SIMUL::m(cat,a=A,b=B)` | RooSimultaneous [doc] |
| `EXPR::m('expr',x,p1,...)` / `expr::f(...)` | RooGenericPdf / RooFormulaVar; `@0,@1..` refer to the listed arguments in order. [verified] commas, parentheses and `*` inside the single-quoted string are protected (see the `BkgMattia` run, which has `pow(@0 - thr, @1)`) |
| `sum::f(a,b)`, `prod::f(a,b)` | RooAddition / RooProduct; `sum::f(a*b,c*d)` gives a weighted sum |
| `FCONV::m(x,A,B)`, `NCONV::m(x,A,B)`, `nconv::` | RooFFTConvPdf, RooNumConvPdf, RooNumConvolution. There is no `CONV::` keyword [source; verified: `CONV::` gives `class CONV not found in factory alias table` and returns nullptr]. [verified, second pass] Gaussian(0.6) x Gaussian(0.8) on [-10,10] against the analytic Gaussian(sigma=1): NCONV max relative deviation 2.4e-13 at 4 points; FCONV (default 930 cache bins) -5e-4 at x=0, -1.7e-2 at 0.5, -3.2e-2 at 1, -6.3e-2 at 2, -1.5e-1 at 5, i.e. the FFT is only an approximation (`SC2/misc/g3.cpp`) |
| `EDIT::m(orig, node=subst, ...)` | RooCustomizer clone with replacements; `node=$REMOVE()` removes a term from PROD [doc/source]. [verified, second pass] replacing works: `EDIT::g1b(g1,s=s2[0.9,0.1,3])` gives variables `m s2 x`, `EDIT::sumE(sumf,g1=g1b)` has components `g1b ch`, unreplaced leaves (`f`) are shared. **`$REMOVE()` as documented does nothing; `$REMOVE` without parentheses works** [verified, third pass, `SC2/misc2/remove.cpp`, `out-remove.txt`]: `EDIT::ppR(pp,ch=$REMOVE())` and `EDIT::ppR2(pp,ch=$REMOVE(pp))` leave `pdfList = { g1 ch }`, `EDIT::ppR3(pp,ch=$REMOVE)` gives `{ g1 }` and evaluates to g1 exactly (0.2994549313 both). Cause [source `RooCustomizer.cxx:691-711`]: when the text after `$REMOVE` contains a `(`, the removal dummy gets attributes `REMOVE_FROM_<name>` for the names inside, and only otherwise `REMOVE_ALL`; with empty parentheses no attribute at all is set, and `RooAbsArg::redirectServers` (`RooAbsArg.cxx:1044-1056`) drops a dummy that has neither `REMOVE_ALL` nor `REMOVE_FROM_<ORIGNAME of the product>`, so nothing is removed and no message is printed. The restricted form `$REMOVE(pp)` also does nothing, for a different reason [source + verified, final pass, `scratch/study-final-010708/remove/remove2.cpp`, `out-remove2.txt`]: the check at `RooAbsArg.cxx:1052` reads the *string* attribute `ORIGNAME` of the product clone, but nothing in RooFit sets a string attribute of that name: `RooCustomizer::build` sets the *boolean* attribute `ORIGNAME:<name>` (`RooCustomizer.cxx:446,474,505,562`) and the string attribute `origName` in lower case (`RooCustomizer.cxx:451,479,567`). So `getStringAttribute("ORIGNAME")` is null, the test becomes `REMOVE_FROM_(null)` (what `Form` prints for a null `%s` here, checked) and never matches. Setting the string attribute by hand, `w.pdf("pp")->setStringAttribute("ORIGNAME","pp")` before the `EDIT`, makes `$REMOVE(pp)` work (`{ g1 }`) and `$REMOVE(other)` correctly leave the product alone (`{ g1 ch }`), which confirms the cause. Each `$REMOVE` creates a new dummy (`RooRealConstant.cxx:71-80`), so a `REMOVE_ALL` does not leak into a later `$REMOVE()` (checked). The same removal done in C++ (`RooCustomizer::replaceArg(ch, RooRealConstant::removalDummy())` with `REMOVE_ALL`, `build()`, even imported into the workspace like the factory does) works |
| `SIMCLONE::`, `MSIMCLONE::` | RooSimWSTool, with `$SplitParam`, `$SplitParamConstrained`, `$Restrict`, `$AddPdf` [doc/source] |
| `CEXPR::m('expr',{x,a[1]})`, `cexpr::` | compiled RooAbsPdf/RooAbsReal through RooClassFactory (ACLiC) [doc]. [verified, second pass] works in a compiled program that has Cling: `CEXPR::ce('exp(-0.5*x*x/(s*s))',{x,s})` gave class `RooCFAuto000Pdf`, value 0.3753110989 = expected, normalised value equals the Gaussian; ACLiC wrote `RooCFAuto000Pdf_cxx.so` into the current directory, and the workspace then logged `ERROR ... cannot retrieve code file names for class RooCFAuto000Pdf ... unable to import code` (the class cannot be persisted with the workspace) |
| `int::name(func,obs)`, `int::n(func,obs\|range)`, `int::n(func,obs,normobs)` | integral object [source]; [verified] `int::ig(g1,x)`, `int::ig2(g1,x\|sub)` build (RooRealIntegral) |
| `deriv::n(func,obs[,order])`, `cdf::n(pdf,obs[,extranorm])`, `PROJ::n(pdf,obs)`, `profile::`, `taylorexpand::`, `lagrangianmorph::`, `set::name(a,b)`, `dataobs::` | [source, `SpecialsIFace::create`]; [verified] `deriv::dg(g1,x,1)` (RooDerivative) and `cdf::cg(g1,x)` build; `set::obs(x)` returned nullptr but `w.set("obs")` is non-null afterwards (a set is not an arg) |
| `nll::`, `chi2::` | compiled only with `ROOFIT_LEGACY_EVAL_BACKEND` [source: `#ifdef`]. [verified] `nll::nl(g1,data)` fails here with `RooAbsData named data not found` (the data set did not exist, so this does not settle whether the keyword is compiled in) |
| `$Alias(Type,Alias)` | type alias, the only global meta argument [doc: process() comment; source `processAliasExpression`]. [verified, second pass] `$Alias(Gaussian,Gaus)` then `Gaus::galias(x,0,1)` builds a RooGaussian. The tutorial rf512 uses `$Typedef(Gaussian,Gaus)`; in alice2 `processExpression` only recognizes `$Alias(` [source]: [verified] after `$Typedef(Chebychev,Cheby)` the line `Cheby::cb(x,{0.1})` fails with `class Cheby not defined in ROOT class table` and returns nullptr (`SC2/gram/out-grammar.txt`) |
| `$Match` | no occurrence in `SRC/roofit/roofitcore` (grep over `src` and `inc`) [verified by grep]; `$Match(cat,foo)` returns nullptr. Only `$SplitParam`, `$SplitParamConstrained`, `$Restrict`, `$AddPdf`, `$REMOVE`, `$Alias` exist |

Whitespace is stripped from the whole expression before parsing, including inside quoted formulas [source: `process()` copies without `isspace`].

Error behaviour [verified; the first-pass text here was partly wrong and is corrected in the second pass, see "Factory errors in full" below]:
- Syntax and workspace errors are printed as `ERROR:ObjectHandling` and the workspace transaction is rolled back. `factory()` returns `ws().arg(out)`. A non-null return does not prove success **only when the name that was to be created already exists** (for `x[-5,5]` with `x` existing it returned the old `x` after the rollback message); for every other failed statement tested it returned nullptr. Pdf objects with an existing name are silently reused with no message (`Gaussian::ok(x,m,s)` run twice). [verified, `SC2/proto/out-fac-errors.txt`]
- Variables cannot be redeclared (`another instance with same name already in the workspace`); refer to an existing variable by its bare name.
- `SUM::sumnoc(g1,ch)` (N pdfs, no coefficients) and `SUM::sumext(e1,e2)`: `RooAddPdf::RooAddPdf(...) number of pdfs and coefficients inconsistent` is thrown as `std::invalid_argument`, which `process()` does not catch (it catches only `std::string`). In compiled code the exception propagates out of `factory()` and can be caught by the caller (the first pass saw an abort only because its call was not wrapped in try/catch); the workspace stays usable afterwards (a later `factory()` call worked). Under Cling the catch does NOT work, see below. The constructor `RooAddPdf(name,title,RooArgList(pdfs))` for extended components exists but the factory cannot reach it [source: `RooFactoryWSTool::add` always calls the 5-argument constructor]; direct construction gave extendMode 2 and expectedEvents 200 for two ExtendPdf components [verified].
- Fractions summing to more than 1 (`0.8*A,0.8*B,C`) build without any message (no check at construction). [verified]

### Programmatic construction (what the project does)

- `RooRealVar(name,title,val)` is constant; `RooRealVar(name,title,val,min,max)` floats; `(name,title,min,max)` starts at the midpoint [source: project comment in `DynamicRooFitter::CreateVar` plus verified midpoint].
- `RooAddPdf(name,title,RooArgList pdfs,RooArgList coefs,bool recursive=false)` [source, `RooAddPdf.cxx` constructor doc]:
  - Ncoef = Npdf-1: fractions, last coefficient `1-sum`; not extended.
  - Ncoef = Npdf: coefficients are yields, extended; the pdf is "must be extended".
  - `recursive=true` requires Ncoef = Npdf-1.
  - `RooAddPdf(name,title,RooArgList extendedPdfs)`: all components must be extendable.
  - Components must be normalized pdfs, independent of the coefficients.
- `RooProdPdf(name,title,RooArgList)`: pdfs that share the observable form an irreducible group that is normalized by explicit numeric integration [doc, `RooProdPdf.cxx`]. The project's `*` grammar (e.g. `Chebychev1*Exponential`) therefore uses numeric normalization. [verified] it fits (cell mult0_pt2, same result both routes) and was not slower in a single timing (36.8 ms vs 22.3 ms, not a benchmark).
- `RooExtendPdf(name,title,pdf,norm[,rangeName])` adds an extended term; with `rangeName`, `norm` counts events in that range only [doc].
- `RooWorkspace::import`, `RooWorkspace::pdf/var/set`, `writeToFile`: the persistence round trip was run by note 07 (cell (3,10): 18 KB, reload 0.27 s); `ws.pdf/var` and `import(RooDataHist)` are used throughout the second-pass programs. [verified]

## Catalog

All rows from reading `SRC/roofit/roofit/src/*.cxx`, `SRC/roofit/roofitcore/src/*.cxx`, `SRC/roofit/roofitcore/inc/RooFit/Detail/MathFuncs.h` [source], plus a stress run [verified] (`model-construction/catalog.cpp`, output `out-catalog.txt`: each pdf built through the factory, scanned at 2001 points on its domain). "norm" means what RooFit divides by; "analytic" is whether `createIntegral(x)` has no numeric part [verified]. Nothing here is extended by itself ([verified] `extendable=no` for every class listed). All raw values below are evaluate() values; the cpu backend normalizes them with RooNormalizedPdf [source].

Safety summary [verified]: in the cpu backend a negative or NaN raw value becomes a packed NaN (`RooAbsPdf::normalizeWithNaNPacking`, `RooAbsPdf.cxx` around line 291) and a normalization integral <= 0 does the same. Unnormalized `getVal()` (no normalization set) replaces NaN by 0 (`RooAbsPdf::getValV`), so the numeric integrator can hide a NaN region: for `EXPR::P('sqrt(@0-0.2)',x,p1)` on [0,1] the numeric integral came out finite (0.477) while the normalized value was NaN at 400 of 2001 points.

| Class (factory name) | Formula / parameters | Analytic integral | Quirks and numerical safety |
|---|---|---|---|
| RooGaussian `Gaussian::g(x,mean,sigma)` | exp(-(x-mean)^2/(2 sigma^2)) | yes (over x and over mean) | sigma=0: raw 0, normalized NaN; sigma<0: raw fine but analytic integral negative -> invalid. ctor checks the range of sigma [source] |
| RooBreitWigner `BreitWigner::b(x,mean,width)` | 1/((x-mean)^2+0.25 width^2); width is the FWHM Gamma | yes (atan) | width=0 gives inf at x=mean; negative width accepted (squared) |
| RooVoigtian `Voigtian::v(x,mean,width,sigma[,doFast])` | BW (FWHM `width`) convolved with Gaussian `sigma`, via the Faddeeva function | NO: numeric (verified; no `getAnalyticalIntegral` in source) | `abs()` taken of width and sigma; sigma=0 -> pure BW, width=0 -> pure Gaussian, both 0 -> constant 1; sigma=1e-9 still finite. Project `width_sig` is the FWHM. |
| RooCBShape `CBShape::c(m,m0,sigma,alpha,n)` | one-sided Crystal Ball, alpha<0 flips the tail to the right | yes | alpha=0 and n=0: integral NaN; n=-1: raw negative; n near 1 uses a log branch |
| RooCrystalBall `CrystalBall::c(...)` | see the DSCB note below | yes | alpha and sigma enter as `abs()`; n is NOT abs'd: n<=0 gives NaN (n=0, alpha=0) or negative raw (n=-1) |
| RooChebychev `Chebychev::c(x,{c1,...,ck})` | 1 + sum_i c_i T_i(x'), x' in [-1,1] over the variable's range; the coefficient of T0 is fixed to 1 | yes | CAN BE NEGATIVE, see below. Uses `_refRangeName` (set by `selectNormalizationRange`) as the range that maps to [-1,1] [source] |
| RooBernstein `Bernstein::b(x,{c0..cn})` | sum c_i B_{i,n}(x') on [xmin,xmax]; n+1 coefficients | yes | non-negative if every c_i >= 0 [doc]; verified (1,-1,1) was still >=0 (not a general statement), (0,0,0) is identically 0 (norm 0) |
| RooPolynomial `Polynomial::p(x,{a..},lowestOrder)` | default lowestOrder=1: 1 + a0 x + a1 x^2 + ...; lowestOrder=0: sum a_i x^i (no implicit 1) | yes | uses raw x, not rescaled: ill-conditioned for x ~ 0.5; negative raw if coefficients allow [verified] |
| RooExponential `Exponential::e(x,c[,negate])` | exp(c x) | yes | c=+1000 overflowed to inf over 581 points [verified]; project's `slope` is c, not -c |
| RooArgusBG `ArgusBG::a(m,m0,c,p)` | m (1-(m/m0)^2)^p exp(c(1-(m/m0)^2)), 0 above m0 | only if `p` is a constant equal to 0.5 | m0<=0 -> NaN integral; p<0 gives huge values near m0 |
| RooLandau `Landau::l(x,mean,sigma)` | Landau pdf of (x-mean)/sigma | yes (CDF) | sigma<=0 -> raw 0 (analytic integral NaN for 0) |
| RooBifurGauss `BifurGauss::b(x,mean,sigmaL,sigmaR)` | Gaussian with different sigma left and right | yes | sigmaR=0: single NaN at x=mean after normalization |
| RooGenericPdf `EXPR::g('formula',x,p..)` | TFormula string | NO (numeric) | evaluates event by event through `TFormula::EvalPar` (`RooFormula::doEval`); NaN where the formula is undefined, e.g. sqrt below a threshold; negative where the formula allows |
| RooFormulaVar `expr::f(...)` | TFormula string | n/a | a function, not a pdf; if given a pdf it uses its raw value [doc] |
| RooHistPdf, RooKeysPdf, RooNDKeysPdf | template shapes | see source | `RooHistPdf(name,title,vars,dhist,intOrder)`: the RooDataHist is neither owned nor cloned [doc]. [verified, second pass] `HistPdf::sigH({mass},tplHist,2)` (RooDataHist imported into the workspace first, interpolation order 2) builds in 11 ms and fits: with a 200-bin template (signal shape sampled from the fit of cell (5,10)) as the signal and `Chebychev1` as background, cell (0,2) fitted to status 0, minNll -121036153.26 (the two-Gaussian model of the same cell gave -121388047.98, so the template shape is worse, as expected for another cell's shape). `KeysPdf::sigK(mass,ds,NoMirror,1.5)` built from a weighted RooDataSet in 2.8 ms (`SC2/misc/out-ranges.txt`). Neither is a candidate for this project (no MC template is used); the point is that the factory reaches them by dataset name. RooNDKeysPdf [verified, third pass]: `NDKeysPdf::k({x,y},d)` with a 500-event 2D RooDataSet in the workspace builds (normalised integral 1.0000, value 0.176 at the origin against 0.159 of the generating Gaussian, the usual kernel smoothing); the options string cannot be passed: `'am'`, `"am"` and `am` are each looked up as an object name (`Object named 'am' not found`), so only the defaults are reachable from the factory |
| RooGamma `Gamma::g(x,gamma,beta,mu)` | (x-mu)^(gamma-1) exp(-(x-mu)/beta)/(Gamma(gamma) beta^gamma) | yes | for x<mu, `TMath::GammaDist` prints an "illegal parameter values" ROOT Error for every point (noisy) and returns 0 [verified]; gamma<1 diverges at mu |
| RooLognormal `Lognormal::l(x,m0,k[,useStd])` | median m0, k=exp(sigma) | yes | k=1 gives ln k = 0 -> normalized NaN; k<1 treated as 1/k; ctor warns when k's range includes 1; x or m0 <= 0 -> NaN |
| RooPowerSum `PowerSum::p(x,{a_i},{b_i})` | sum a_i x^b_i | yes | negative raw if a_i negative [verified] |
| RooNovosibirsk `Novosibirsk::n(x,peak,width,tail)` | Belle Novosibirsk | yes | zero beyond the cut (arg < 1e-7); tail<1e-7 -> Gaussian |
| RooBukinPdf `BukinPdf::b(x,Xp,sigp,xi,rho1,rho2)` | Bukin asymmetric peak | NO (numeric, verified) | ctor warns about parameter ranges (sigp>0, rho1 in [-1,0], rho2 in [0,1], xi in [-1,1]) |
| RooJohnson `Johnson::j(mass,mu,lambda,gamma,delta[,threshold])` | Johnson S_U | yes | lambda or delta negative -> every raw value negative [verified] |
| RooLegacyExpPoly `LegacyExpPoly::e(x,{a..},lowestOrder)` | exp(sum a_i x^i) | NO (numeric) | overflow gives inf (coefficient 1000) [verified] |
| RooUniform `Uniform::u({x})` | 1 | yes | safe |
| RooChiSquarePdf `ChiSquarePdf::c(x,ndof)` | chi2 pdf | analytic only without a range name [source] | |
| RooDstD0BG `DstD0BG::d(dm,dm0,C,A,B)` | (1-exp(-(dm-dm0)/C))(dm/dm0)^A + B(dm/dm0-1), clipped at 0 | NO (numeric, verified) | clipped, never negative |
| RooPoisson `Poisson::p(n,mean[,noRounding])` | Poisson in a continuous `n` (Gamma-function interpolation unless `noRounding`) | yes | [verified, third pass, `SC2/misc2/others.cpp`, `out-others.txt`] `Poisson::po(n[0,0,30],mu[4.2,0,20])` builds; raw value equals `TMath::Poisson(k, 4.2)` at n = 0..12 (max difference 0); normalised integral over [0,30] 1.000000; `Poisson::po2(n,mu,1)` (noRounding) builds |
| RooParametricStepFunction (pdf, Nbins-1 coefficients, sum <= 1), RooStepFunction (function) | piecewise constant | yes | [verified, third pass] **cannot be built by the factory**: the constructor needs a `TArrayD` of bin limits and the factory looks the argument up as a workspace object (`Object named limits not found`); built in C++ and `ws.import`ed it works (3 bins on [0,3], c1 = c2 = 0.3: value 0.400000 in the last bin, integral 1.000000) |

DSCB mapping, the project's `RooCrystalBall(name,title,x,m,s,a1,n1,a2,n2)`:
- It is the second constructor in `RooCrystalBall.cxx`: `(x, x0, sigmaLR, alphaL, nL, alphaR, nR)` [source]. So `mean`=x0, `sigma`=sigmaL=sigmaR, `alpha1`/`n1` = left tail, `alpha2`/`n2` = right tail. For t=(x-x0)/sigma: tail left for t < -alpha1 with A_L(B_L - t)^(-n1), Gaussian core in between, tail right for t > alpha2.
- Other variants [source]: 8 arguments with separate sigmaL, sigmaR (asymmetric core); 5 arguments + `doubleSided=false` (CBShape-like, negative alpha flips the tail to the right) or `true` (same alpha and n on both sides).
- [verified] factory string `CrystalBall::DSCB_sig(mass,mean_sig[..],sigma_sig[..],alpha1_sig[..],n1_sig[..],alpha2_sig[..],n2_sig[..])` gives the identical fit.
- Quirk [source]: only alpha and sigma are passed through `abs()`; n is not. The ctor calls `RooHelpers::checkRangeOfParameters` for sigma, alpha, n >= 0. The project's limits (alpha in [0.5,5], n in [1,150]) are safe. n close to 1 switches to a log expression for the integral (threshold 1e-5).

Chebychev negativity [verified, python grid and RooFit scan]:
- Chebychev1: min = 1-|c1| over [-1,1], so c1 in [-1,1] is safe; c1=1.2 gave negative values at 167 of 2001 points.
- Chebychev2 within the JSON limits [-1,1]^2: 28.8 % of a 41x41 grid has a negative minimum. Examples from the RooFit scan: (c1,c2)=(-0.5,0.998) is negative at 242 of 2001 points (the starting trajectory seen in the mult3_pt4 fit log: c2 jumps from 0.05 to 0.998 in the first MIGRAD steps); (-1,-1), (1,1) negative.
- Chebychev3 within [-1,1]^3: 63 % of an 11^3 grid negative.
- The project's start value (-0.5,0.05) has minimum 0.55. [verified]
- In the project log `Logs/log_output_globalConfigMCClosurepp.log` the message shows `numerator=Chebychev2_bkg=0.0586179` which is positive: that number is the value at the point where the message was logged, not necessarily the negative one [verified for the second, inferred for the first].

## Custom pdfs

- RooGenericPdf: parses the string with TFormula; the cpu evaluation path calls `RooFormula::doEval`, which loops over the entries calling `TFormula::EvalPar` (`SRC/roofit/roofitcore/src/RooFormula.cxx` line ~406) [source]. Normalization is fully numeric [doc: `RooGenericPdf.cxx` class comment].
- A custom compiled class only needs `evaluate()`: the default `RooAbsReal::doEval` (`RooAbsReal.cxx` line ~4303) falls back to calling `evaluate()` per entry [source]. Generated skeletons from `RooClassFactory::makePdf(name,"x,A,B",...)` also implement `doEval` and a `codegenImpl` hook [source]; analytic integrals are added with the string form `"x:expr;..."` [doc]. Tutorial: `SRC/tutorials/roofit/roofit/rf104_classfactory.C`.
- `RooClassFactory::makePdfInstance(name, "expr", RooArgSet(x,alpha))` is the compiled replacement of RooGenericPdf [doc].
- [verified, second pass] An `evaluate()`-only class (`MyGauss`, 13 lines in the header, `SC2/misc/custom/`) compiled with ACLiC (`.L MyGauss.cxx+`, 5.0 s once) can be created by the factory by its class name, `MyGauss::sig(mass,m[...],s[...])`, and fitted: same minNll to the last digit as the same shape written as `EXPR::sig('exp(-0.5*pow((@0-@1)/@2,2))',...)` (-121341994.269163 both, same parameters); steady-state fit 1.0 ms vs 1.2 ms for RooGenericPdf, so RooGenericPdf is not slower in practice on this 200-bin problem. RooGaussian (analytic integral) gives -121342006.918184: the 12.65 difference on 1.2e8 is the numerical normalisation of the two custom shapes, not a model difference. A standalone compiled program can only create such a class by name if it has a ROOT dictionary (`ClassDef`+`ClassImp`, rootcling); under `root` the ACLiC dictionary is enough. Not tested: building the class into the project's CMake.

## Head-to-head experiment (first pass)

Programs and outputs are in `/Users/scannito/alice/Analysis/RooFitKnowledge/scratch/model-construction/`.

Method [verified]: `h2h.cpp` includes the project's `DynamicRooFitter.h` (with `#define private public` around that include only, to read `fitResult`), loads the real `FitConfig` through `FitConfigManager` from `JSONConfigs/fitConfig.json` (read only), projects the cell from `h3K0sCandidatesMass` (read only) with `ProjectionZ(name, mult+1,mult+1, pt+1,pt+1)`, then builds the same model with factory strings produced from the same `MathParam` values (`name[val,min,max]`, `name[val]` for constants, values printed with `%.17g`), and fits both with `Optimize(1), Extended(1), Save(1), PrintLevel(-1)`. Nothing under the project tree was written. (Note: `h2h.cpp` formats with `std::format("{:.17g}")`, which does not compile under Cling; the second-pass generator uses `snprintf`.)

Factory strings generated for the DSCB+Chebychev1 case:
```
mass[0.45,0.55]
CrystalBall::DSCB_sig(mass,mean_sig[0.497,0.49,0.505],sigma_sig[0.004,0.002,0.008],alpha1_sig[1.2,0.5,5],n1_sig[5,1,150],alpha2_sig[1.2,0.5,5],n2_sig[5,1,150])
Chebychev::Chebychev1_bkg(mass,{c1_bkg[-0.8,-1,1]})
SUM::model(nsig[100000,0,100000000]*DSCB_sig,nbkg[50000,0,25000000]*Chebychev1_bkg)
```

Results (A = `DynamicRooFitter`, B = factory; time columns are single runs, not benchmarks):

| Case | minNll (A = B) | status | covQual | edm | parameter differences | time A / B (ms) |
|---|---|---|---|---|---|---|
| K0S mult0_pt2, DSCB+Chebychev1 | -121421924.3445870131 | 3 | 3 | 0.844 | all exactly 0 (9 parameters, values and errors) | 25.0 / 18.3 |
| K0S mult0_pt4, DSCB+Chebychev2 (config override) | -175202865.0068521500 | 3 | 3 | 0.423 | all 0 | 22.3 / 19.4 |
| K0S mult3_pt4, DSCB+Chebychev2 | -617912966.2198921442 | 3 | 3 | 2.62 | all 0; RooFit printed 362 "less than zero" lines in both routes | 26.1 / 20.1 |
| K0S mult5_pt10, DSCB+Chebychev1 | -717490197.7629370689 | 0 | 3 | 0.169 | all 0 | 22.9 / 15.7 |
| K0S mult0_pt2, Voigtian (width_sig fixed 0.0005, made up) + Chebychev2 | -119936135.2309771478 | 3 | 2 | 3.9e12 | all 0 | 29.7 / 25.2 |
| K0S mult0_pt2, Gaussian+Gaussian (sum, `frac_sig_0`) + Chebychev1 | -121342006.9183348268 | 1 | 3 | 3.9e-4 | all 0 | 19.9 / 17.2 |
| K0S mult0_pt2, DSCB + (Chebychev1*Exponential) (product) | -121421938.3085040152 | 3 | 3 | 0.394 | all 0 | 36.8 / 22.3 |
| synthetic phi histogram, Voigtian + BkgSourav2 (`EXPR::`) | -71806725.7527715266 | 3 | 2 | 0.155 | all 0 | 41.6 / 31.1 |
| synthetic phi histogram, Voigtian + BkgMattia | -71417367.0912874192 | 3 | 2 | 2.9e3 | all 0 | 44.1 / 36.2 |

Notes [verified]: the synthetic phi histogram was the first researcher's own (superseded by the real phi cells of the second pass); the K0S Voigtian case adds `width_sig` as a constant that is not in `fitConfig.json`; parameter order in `floatParsFinal` was the same in both routes.

What is equivalent: model value, parameter values, errors, status, covariance quality, NLL to the last digit, object and parameter names (if you choose them), messages emitted by RooFit. What is not equivalent / extra work: (1) Cling is needed at run time; (2) numbers must be printed with 17 significant digits; (3) error handling (see "Factory errors in full"); (4) classes are looked up by name, a typo is a runtime error; (5) the compile-time `RequireThresholdBelowRange` check must be kept outside the strings; (6) the workspace owns the objects, so `DynamicRooFitter`'s `ownedObjects` teardown order is no longer needed [inferred; the prototype has no such code and ran 1440 cells without a crash].

## Relevance to this project

- [verified] Replacing `CreatePdfComponent`/`ParseModelString` by strings is feasible with no change of fit results (1440 real cells, second pass).
- [verified] `Voigtian` width is the FWHM, matching `width_sig: 0.00426` for the phi (comment in `RooVoigtian.cxx`: "FULL width at half maximum"). It has no analytic integral, so the Voigtian fit pays for numeric integration (inferred cost; the phi fits take 9-13 ms against 2-7 ms for K0S/Xi in the cmp runs, which have other differences too).
- [verified] The "p.d.f value is less than zero" messages for Chebychev2 backgrounds are explained by the parameter limits: the allowed box [-1,1]^2 includes ~29 % of shapes with negative parts. Tightening the limits (keep 1+c1 x'+c2(2x'^2-1) >= 0, a region inside the box) or a pdf positive by construction (`Bernstein` with coefficients >= 0, or `Exponential`) would remove the messages. Not tested in a fit.
- [verified] The project passes `Extended(1)` explicitly. In this version the default is automatic (`extendedFitDefault=2` in `FitHelpers.cxx`: extended when the pdf can be extended; the second pass confirmed identical results for `Extended(true)` and no argument), and `Extended(false)` on a "must be extended" pdf logs an error. The Doxygen line "Extended: off by default" (`RooAbsPdf.cxx` ~827) disagrees with the code [source].
- For generic backgrounds (`BkgSourav*`, `BkgMattia`) the factory `EXPR::` is equivalent to `RooGenericPdf` ([verified] identical fit, now on the real phi cells).

## Pitfalls

1. [verified] `SUM::` with fractions: N-1 fractions for N pdfs; the last coefficient is implicit. N coefficients means yields (extended). Mixing them up changes the model silently; fractions summing to more than 1 build without a message.
2. [verified, second pass] A sub-range fit changes the meaning of fractions and Chebychev coefficients and was unstable for a two-Gaussian model (see "Ranges, Extended, SumCoefRange"); the project fits the full range.
3. [verified] A pdf evaluating to NaN or negative at some points: normalized value becomes a packed NaN, Minuit backs out; unnormalized getVal masks NaN as 0, so numeric integrals can look finite.
4. [verified] `factory()` returns nullptr on most failures but not when the output name already exists, silently accepts values outside their range (clipped) and duplicate pdf names; `SUM::` without coefficients throws; a bad `EXPR::` formula terminates the process.
5. [verified] `x[-10,10]` starts at the midpoint, `x[3]` is a constant with an infinite range.
6. [source] `RooPolynomial` default `lowestOrder=1` means an implicit constant term 1.
7. [verified] `RooCrystalBall`: n is not `abs`-ed; `alpha`, `sigma` are.
8. [verified] RooGamma prints ROOT errors for each point with x<mu.
9. [source] `RooProdPdf` of two pdfs of the same observable is normalized numerically.
10. [verified] `getVal(RooArgSet(*x))` with an rvalue set throws in this version.
11. [verified] Environment: parallel `alienv printenv` calls collide; use `rooenv.sh`.
12. [verified, third pass + final pass] `EDIT::...(orig, node=$REMOVE())` with empty parentheses (the documented form) silently removes nothing, and the restricted `$REMOVE(prodname)` silently removes nothing either (case mismatch `ORIGNAME` / `origName` in the source); the working form is `$REMOVE` without parentheses. To restrict the removal to one product, set the string attribute `ORIGNAME` on that product first (see the `EDIT` row).
13. [verified, third pass] A workspace that contains an object of a custom class cannot be opened without that class's dictionary: the read crashes the process (segmentation violation) instead of failing; `importClassCode` does not help in 6.36 (section "Third pass").

## Second pass (completed 14 October 2026)

All programs and outputs under `SC2` (`/Users/scannito/alice/Analysis/RooFitKnowledge/scratch/study0102`). Compile line used everywhere: `source ../rooenv.sh; clang++ -std=c++20 -O2 $(root-config --cflags | sed 's/-std=[^ ]*//') -I/Users/scannito/alice/Analysis/Codes [-I proto] x.cpp -o x $(root-config --libs) -lRooFit -lRooFitCore -Wl,-rpath,$ROOTSYS/lib`.

### Equivalence over all real cells (route B yields included)

[verified] `proto/DynamicRooFitterB.h` is a copy of `DynamicRooFitter.h` in which only the model construction was replaced (constructor, `ownedObjects`, destructor, `CreateVar`, `CreatePdfComponent`, `ParseModelString`, `RequireThresholdBelowRange`, `TwoKaonThreshold` removed; `FactoryModel.h` added). Everything else, in particular `DoFit`, `ExtractYieldsAndPurity` (window, snap-to-bin, purity, sideband) and `CalculateIntegrationLimits`, is the project's code, byte for byte. `proto/cmp.cpp` runs both classes on every real cell of the file `AnalysisResults_136_2026_PtMod_23luglio.root` (cells with Integral >= 1000) and compares with `==` (not a tolerance) the status, covQual, edm, minNll, every floatParsFinal value and error, S, its error, B, its error, purity, sideband and ratio:

| section (histogram) | cells | identical in every field | different |
|---|---|---|---|
| k0s (`h3K0sCandidatesMass`, DSCB+Chebychev1, 20 cells with Chebychev2 override) | 340 | 340 | 0 |
| xi (`h3XiCandidatesMass`) | 260 | 260 | 0 |
| pi_tpc (`h3PionTPCnSigma`, DSCB+Gaussian) | 360 | 360 | 0 |
| pi_tof (`h3PionTOFnSigma`, Gaussian+Exponential) | 340 | 340 | 0 |
| phi, offline (`phi/h3PhiData`, Voigtian+BkgSourav2, fixed-range window + histogram sideband) | 70 | 70 | 0 |
| phi, online (`..._OnlineEfficiency/.../h3PhiData`, non-integer weights) | 70 | 70 | 0 |
| k0s with composite models (extra parameters made up in the config through the `EXTRA` env of `cmp`): signal `Gaussian+Gaussian`; background `Chebychev1*Exponential`; background `Chebychev1+Exponential` (`proto/out-composites.txt`) | 340 each | all | 0 |
| k0s, pi_tpc, phi again with `n_sigma_integration = 3` and `snap_to_bin = true` forced in the config (these two features are not used by the current `fitConfig.json`, which uses `integration_range` everywhere) | 340, 360, 70 | all | 0 |

Outputs: `proto/out-cmp-<section>.txt`, `proto/out-cmp-pi_tof.txt`, `proto/out-nsigma-snap.txt`. So the answer to "compute the window yields through the factory-built model" is: the unchanged `ExtractYieldsAndPurity` runs on the factory-built model and gives the same numbers to the last bit, because `getVariables()`, `createIntegral(..., Range("signalRegion"))`, `RooProduct` and `getPropagatedError` only see the RooFit objects, whose names (`nsig`, `nbkg`, `mean_sig`, `sigma_sig`, `model`, `<Type>_sig`, `sig`/`bkg` for composites) the generator chose to match. The first-pass experiment used a hard-coded observable name `mass`; the prototype uses `cfg.obs.name` (the pion sections use `nsigma`) and sets the title (`cfg.obs.title`) afterwards, because the factory makes title = name and the plot axis label comes from the title [source: not verified in a drawn plot].

### The prototype generator (what to copy), `proto/FactoryModel.h`

The model library is one line per model; `$(x)` is the observable, `$(name)` the object name, `$(thr)` the K+K- threshold, any other `$(key)` is the config parameter `key + suffix`. Key lines (full file quoted in note 10):
```
{"DSCB",       {"CrystalBall::$(name)($(x),$(mean),$(sigma),$(alpha1),$(n1),$(alpha2),$(n2))", false}},
{"Chebychev2", {"Chebychev::$(name)($(x),{$(c1),$(c2)})", false}},
{"BkgSourav2", {"EXPR::$(name)('1.0 + @1*@0 + @2*@0*@0 + @3*sqrt(@0 - $(thr))',$(x),$(c1),$(c2),$(c3))", true}},
```
The `+` / `*` grammar of the model strings (sum with `frac_<role>_<i>`, product) is kept, producing `SUM::sig(frac_sig_0[...]*A,B)` or `PROD::bkg(A,B)`; the top is `SUM::model(nsig[..]*sig,nbkg[..]*bkg)`.

### Factory errors in full, and a validation recipe

[verified] `proto/fac-errors.cpp` (output `proto/out-fac-errors.txt`) runs 15 faulty statements in a workspace twice: raw `factory()`, and through the checked `Exec` (statement run under `RooHelpers::HijackMessageStream(RooFit::ERROR, all topics)`, reject on nullptr, on an escaped exception, or on any captured ERROR text).

| statement | raw `factory()` | checked |
|---|---|---|
| `x[-5,5]` (x exists) | non-null (old x) + ERROR text | rejected |
| unknown argument name / too few arguments / unknown class / unbalanced `(` / non-numeric value `mq[abc,0,1]` | nullptr + ERROR text | rejected |
| `m[0,1,0]` (min > max) | nullptr + ERROR text (`RooUniformBinning::setRange: low bound > high bound`) and two WARNINGs | rejected |
| `mm[5,0,1]` (value outside range) | **non-null, no text** (value is clipped) | **accepted** (needs a separate check) |
| `Chebychev::cb(x,{c1[0.1,-1,1],c2[2,-1,1]})` (start value outside range, inside a list) | **non-null, no text** | **accepted** |
| duplicate pdf name | **non-null, no text** (existing object returned) | **accepted** |
| `SUM::nocoef(g1,ch)` | C++ `std::invalid_argument` escapes | rejected (caught) |
| `EXPR::p('1+foo(@0)',x)` (formula does not compile) | **process terminates** (rc 134, `terminating due to uncaught exception of type std::runtime_error`) | not catchable |
| `EXPR::p('1+@3*@0',x,m)` (index out of range) | **process terminates** (`std::invalid_argument`) | not catchable |

The last two are the constructors of objects created by the factory through Cling (`createArg` builds `new RooGenericPdf(...)` with `ProcessLineFast`): an exception thrown there cannot be caught by the caller (`proto/expr-abort.cpp`: the same RooGenericPdf constructor called directly from C++ throws a catchable `std::exception`, through the factory the process aborts). So bad formulas must be caught before the factory call. In the project the formulas are fixed library text, so this is a developer-time failure, found by running each model once; a self-test that builds every library entry on startup would be the guard.

Recipe (what the prototype does, ~60 lines):
1. **Check the input before building anything**: every parameter name is an identifier (a name with a comma, space or bracket would inject syntax into the string), observable name likewise, `min < max` for free parameters, start value inside `[min,max]`, every `$(key)` resolves to a config entry (message `Missing parameter in JSON: <name>`, same text as `CreateVar`), model type is in the library (`Unknown model type: <t>`), the threshold check for the phase-space backgrounds (`RequireThresholdBelowRange`, kept).
2. **Run each statement under a message hijack and reject** on nullptr, captured ERROR text, or exception. Do not rely on the pointer alone.
3. **Check the output**: `model` exists and `canBeExtended()`; every declared parameter has in the workspace exactly the value, limits and constness of the config (this catches the silent clipping above and any number the factory parsed differently; numbers are written with `%.17g`).
4. **Check the model at the start point before fitting**: the normalised pdf is finite and non-negative at every bin centre (200 evaluations, run once per distinct configuration). This is generic: it also catches a Chebychev2 whose start values make it negative, or a fraction sum above 1 (`proto/defects.cpp`, cases 9 and 16; `proto/out-defects.txt`).
5. Under Cling do not rely on `catch`: see next section.

Defect tests, `proto/defects.cpp` (A = hand-written, B = factory prototype), real K0S cell (0,2) and phi cell (0,2):

| defect | A (project today) | B (prototype) |
|---|---|---|
| missing parameter, missing `nsig`, unknown model, trailing `+`, sum without `frac_sig_0` | exception, clear message | the same exception text |
| phi observable starting below 2 m_K | exception (`RequireThresholdBelowRange`, long message) | exception (same check, shorter text) |
| `c1_bkg` with min > max | **runs** (2 ERROR lines in the log), status 3, S = 6.289e6 against 6.137e6 of the control | rejected before building |
| start value outside limits | **runs**, status 3 | rejected before building |
| Chebychev2 start (c1,c2) = (-0.5, 0.998) (negative over part of the range) | **runs**, status 3, S = 5.314e6 | rejected: `model is nan at mass=0.50025 with the start parameters` |
| fraction 1.5 in a sum of two Gaussians | **runs**, status 3 | rejected (same NaN check) |
| observable name `m inv`; parameter name `c1,bkg` | runs | rejected (name check) |
| extra unused parameter (typo `c1_bk`) | silently ignored | silently ignored (an unused-key check could be added to either) |

Only the first two rows are properties of the hand-written code; rows 3-7 are checks that were added in B and **could equally be added to A** (they act on the config or on the built model, not on the construction route). What the factory route adds on top is the error capture of step 2 and the name check.

### Cling and the factory

[verified] The prototype header and `DynamicRooFitterB` load and run in `root -l -b -q` (macro `proto/cling-b.C`, output `proto/out-cling-b.txt`): K0S cell (0,2), phi cell (0,2) and pi_tpc cell (3,10) give S, its error and the purity equal to 17 digits between `DynamicRooFitter` and `DynamicRooFitterB`, both interpreted (K0S S = 6136909.0814745603 +- 2567.2972733023666, the same value as the compiled run). The first construction costs 5-10 ms.
Two constraints from the project's own notes bit and were handled: (1) `std::format("{:.17g}", x)` does not compile in this Cling [verified: `consteval ... __builtin_clzg`; this is DESIGN_NOTES section 11], so the generator formats with `snprintf("%.17g")`, which the first-pass `h2h.cpp` strings did not do; (2) a `try`/`catch` in interpreted code around `factory()` does not work: [verified] `proto/cling-exc.C` mode 1 terminated with `uncaught exception of type std::invalid_argument` although a `catch (const std::exception&)` was present, the behaviour described in `root-issue-cling-eh/issue.md`. `HijackMessageStream` works in Cling (mode 0: result nullptr, 274 characters captured). So the validation must not depend on exceptions from RooFit.

### Ranges, Extended, SumCoefRange (K0S cell (0,2), 8.99e6 entries, `misc/ranges.cpp`, `misc/out-ranges.txt`)

[verified] Model: signal = two Gaussians with a fraction, background Chebychev1, extended sum.
- `Extended(true)` and no `Extended` argument gave identical results (status 0, minNll -121388047.981558, nsig 6097633.5): the default is automatic, as the source says (`FitHelpers.cxx`). `Extended(false)` logged `You used the Extended(false) option on a pdf where the fit MUST be extended! ...nonsensical results` and returned minNll -24684554.3 with nsig 300648 and fraction 0.2347: a silent change of the meaning of the parameters. The project passes `Extended(1)`, which is right.
- Fit in a sub-range `Range("sub")` = [0.46,0.535] of [0.45,0.55]: minNll -120073581.17, nsig 6066651 (-0.5 %), `c1_bkg` runs to its limit -1.0, the two Gaussians swap roles (fraction 0.2714 against 0.7655); window yield 6063436 against 6092101 (-0.47 %). `SumCoefRange("full")` changed nothing here (the -120073581.170690 vs ...170369 difference is minimiser noise). On a toy of two Gaussians (truth fr 0.6, widths 0.003/0.008, 3e6 events) the full-range fit returns fr 0.5985, s2 0.00800; fitting [0.485,0.515] returns fr 0.5333 and s2 at its limit 0.03, with or without SumCoefRange. So a sub-range fit of a model with a free second width is not stable; [inferred] the meaning of fractions and of Chebychev coefficients (mapped to [-1,1] over the fit range, `refRangeName`) depends on the range used, and I did not isolate which part of the changes is the range semantics and which is the loss of constraint. The project fits the full observable range and uses ranges only for the integration windows, so this does not affect it today.
- `getVal(RooArgSet(*x))` with an rvalue set throws `calling RooAbsReal::getVal() with r-value references to the normalization set is not allowed` in this version [verified, first attempt of `grammar2.cpp`]; the project does not call it that way (it uses `getPropagatedError(..., RooArgSet(*obs))`, which worked in all 1440 cells).

### Construction cost (the fitter is built ~1000 times)

[verified] `proto/bench-build.cpp` (5 repeats of 60-300 builds, median CPU time, machine load 2-4 from other jobs; wall = CPU within 1 %): hand-written 0.005-0.006 ms (phi 0.075 ms), factory without cache 3.4-3.7 ms for K0S and 4.4-5.4 ms for pi_tpc / phi (`out-bench-build.txt`). `proto/prof-build.cpp`: each statement that constructs a class costs 1.6-1.8 ms (`createArg` compiles a constructor call in Cling), variable declarations and `SUM::` cost microseconds, `HijackMessageStream` 0.01 ms. A `RooWorkspace` copy costs 0.012 ms (`prof-clone.cpp`), and the copy is independent. The prototype therefore builds the validated workspace once per distinct list of statements (a static map keyed by the statement text) and gives each fitter a copy: 0.017-0.024 ms per fitter for K0S and pi_tpc, 0.11 ms for phi, against 0.005 and 0.075 ms hand-written. Results with the cache are bit-identical (table above, the cache was on). Without the cache the 960 K0S/Xi/pi_tpc cells would spend about 4 ms each on construction, about the same as one fit.

## Third pass (15 October 2026)

Scratch: `SC2/misc2/` (`remove.cpp`, `others.cpp`, `customdict/`, `aclic/`, `readws.C`, outputs `out-*.txt`) and `SC2/prov/` (`draw.cpp` with `LABELS=1`, `out-draw-labels.txt`). Build: `SC2/build.sh <file.cpp>`; run after `source SC2/../rooenv.sh`.

### `$REMOVE()`
See the `EDIT` row of the grammar table: the documented `$REMOVE()` is a no-op because of the parsing in `RooCustomizer::CustIFace::create`; `$REMOVE` without parentheses removes the factor [verified + source]. The restricted `$REMOVE(prodname)` is broken in 6.36.10 by a name mismatch: `RooAbsArg.cxx:1052` looks for the string attribute `ORIGNAME`, `RooCustomizer` sets `origName` (`RooCustomizer.cxx:567` for the cloned product) [source], and setting `ORIGNAME` by hand makes it work [verified, final pass, `scratch/study-final-010708/remove/out-remove2.txt`]. A bug report upstream would be a one-line fix (read `origName`). Not relevant to the project's models (no product edited), relevant to anyone following rf512.

### RooParametricStepFunction, RooPoisson, RooNDKeysPdf
See the catalog rows. Summary [verified]: RooPoisson works through the factory and matches `TMath::Poisson`; RooParametricStepFunction cannot be built by the factory (it needs a `TArrayD`), only in C++ and then imported; RooNDKeysPdf builds with its defaults, its options string cannot be passed.

### The plot of the factory-built model
[verified, `SC2/prov/draw.cpp` rebuilt in the new scratch, run with `LABELS=1`, output `SC2/prov/out-draw-labels.txt`] The project's `DrawFit` (single pad as in `PurityTask.h`, and the ratio canvas with pulls) on the hand-written model (A), the factory model (B) and the HS3 model (C), five real cells (k0s (0,2), (3,4), phi (0,2), pi_tpc (3,10), pi_tof (2,8)): x-axis titles identical (e.g. `M(#pi^{+}#pi^{-}) (GeV/c^{2})`, `M(K^{+}K^{-}) (GeV/c^{2})`, `n#sigma_{TPC} (#pi)`), y-axis titles identical (`Events / ( 0.0005 )` ... and `Pull (#sigma)` on the pull pad), legend entries identical in text and target (`Data -> Data_Plot`, `Total Fit -> Model_Plot`, `Signal -> Sig_Plot`, `Background -> Bkg_Plot`), every curve point identical, and the PNG files byte-identical (same md5) in all 10 canvases. The factory prototype sets the observable title from the config after building (`FactoryModel.h`, `Realize`); without that line the axis would show the name `mass` (seen for the HS3 route in note 02, 10.3).

### Writing and reading back a workspace whose model uses a custom compiled class
[verified, `SC2/misc2/customdict/` and `SC2/misc2/aclic/`; reader outputs in `SC2/misc2/out-customdict.txt`] Class `MyGauss` (an `evaluate()`-only Gaussian, `ClassDefOverride`), dictionary made with `rootcling -f MyGaussDict.cxx -rmf libMyGauss.rootmap -rml libMyGauss.so MyGauss.h LinkDef.h` and compiled into `libMyGauss.so`. Writer (`writews.cpp`, linked to the library): factory `MyGauss::sig(mass,...)` + Chebychev + `SUM::model`, fitted on the real K0S cell (0,2) (status 0, minNll -121341994.2691631615), data, `postfit` snapshot and `RooFitResult` imported, `writeToFile` (16 KB). Readers, each in a fresh process:

| reader | result |
|---|---|
| compiled, not linked to the library, nothing loaded | `Warning in <TClass::Init>: no dictionary for class MyGauss is available`, `Error in <TBufferFile::ReadObject>: trying to read an emulated class (MyGauss) to store in a compiled pointer (RooAbsArg)`, then **segmentation violation** (exit 139) inside `RooWorkspace::Streamer`; the workspace cannot even be listed |
| compiled, `gSystem->Load("libMyGauss.so")` before opening | works: `sig` is a `MyGauss`, NLL at the `postfit` snapshot -121341994.2691631615, equal to the writer's |
| compiled, nothing loaded but `ROOT_DYN_PATH` extended with the directory holding `libMyGauss.rootmap` | works (the rootmap makes ROOT autoload the library); same NLL |
| Cling (`root -l -b -q readws.C`), nothing loaded | the same crash |
| Cling, `gSystem->Load` first | works, same NLL |
| Cling, `.L MyGauss.cxx+` (ACLiC) of the source first | works, same NLL |

Embedding the source in the workspace does not work here: `RooWorkspace::importClassCode(MyGauss::Class())` returned true but stored nothing when the rootmap was visible, because `CodeRepo::autoImportClass` treats any class listed in a rootmap as "in ROOT distribution, skipping" [source `RooWorkspace.cxx:1525-1529`]; with the rootmap hidden it failed with `cannot retrieve code file names for class MyGauss through ROOT TClass interface` (the class has no implementation-file name in its dictionary; the same happened for a class compiled with ACLiC in Cling and for `CEXPR::`, and the reader then crashed in the same way). So in 6.36.10 **a workspace with a custom class is not self-contained**: whoever reads it needs the library (load it, or put its rootmap on `ROOT_DYN_PATH`) or the source to compile with ACLiC before `TFile::Get`, and a missing dictionary is a crash, not an error. For the project this is one more reason not to introduce custom pdf classes unless they are built into the project's library with a dictionary and that library is always loaded before any workspace file is opened. [inferred]

## NOT DONE

FINAL (final pass, 15 October 2026). Nothing is left NOT DONE. The open item of the third pass is closed, and the two that remain cannot be done here:
- Done (final pass): the restricted `$REMOVE(prodname)` form. It is broken in 6.36.10 by a case mismatch: `RooAbsArg.cxx:1052` reads the string attribute `ORIGNAME`, while `RooCustomizer` sets `origName` (`RooCustomizer.cxx:451,479,567`). The cause was confirmed by setting `ORIGNAME` by hand. Working forms: `$REMOVE` without parentheses, or `ORIGNAME` set on the target product before the `EDIT` (see the `EDIT` row, item 12 and "Third pass").
- NOT POSSIBLE here: the behaviour of the factory in ROOT 6.38/6.40. Neither version is installed. The only other ROOT, 6.36.04, does not start on this SDK (modulemap error, note 07 (f)), and `alienv` is off limits. What the primary 6.38 notes imply is in note 08: `RooRealVar::setVal` throws outside the range. Inside the factory that exception would be thrown in Cling-built code and terminate the process; the prototype's start-value check prevents that case.
- NOT POSSIBLE for a researcher: building a custom class into the project's own CMake library. It needs changes to `Codes/` and the build, which the briefs forbid, and it is the owner's decision. The dictionary recipe in "Third pass" is what it would need.

## Resume here

Final. Everything in the briefs for note 01 was run; see `NOT DONE` for the two items that are impossible here. Final pass: `scratch/study-final-010708/remove/remove2.cpp` (output `out-remove2.txt`; rebuild with `scratch/study-final-010708/build.sh <file.cpp>`). Scratch: `SC2 = /Users/scannito/alice/Analysis/RooFitKnowledge/scratch/study0102` (executables are not kept: rebuild with `SC2/build.sh <file.cpp>`; the `customdict/` library with the `rootcling` and `clang++ -shared` lines quoted above). Main evidence: `cd SC2/proto; source ../../rooenv.sh; ./cmp k0s k0s-reduced-cand-producer/k0sReducedCandidates/h3K0sCandidatesMass` (other sections: `xi xi-reduced-cand-producer/xiReducedCandidates/h3XiCandidatesMass`, `pi_tpc pion-track-producer/pionTracks/h3PionTPCnSigma`, `pi_tof pion-track-producer/pionTracks/h3PionTOFnSigma`, `phi phi-strange-correlation/phiStrangenessCorrelation/phi/h3PhiData`, `phi_online phi-strange-correlation_OnlineEfficiency/phiStrangenessCorrelation/phi/h3PhiData`; env `NSIGMA=3`, `SNAP=1`, `NOCACHE=1`, `SIG`, `BKG`, `EXTRA`, `VERBOSE=1`). Third pass: `SC2/misc2/remove`, `others`, `customdict/writews [code]`, `customdict/readws <file> [load]`, `root -l -b -q 'readws.C("<file>",mode)'`; `SC2/prov/draw` (run inside `draw-out/`, `LABELS=1`). For PyROOT also `export DYLD_LIBRARY_PATH=$ROOTSYS/lib`.
