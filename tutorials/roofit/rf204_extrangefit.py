## \file
## \ingroup tutorial_roofit
## \notebook -nodraw
## 'ADDITION AND CONVOLUTION' RooFit tutorial macro #204
## Extended maximum likelihood fit with alternate range definition
## for observed number of events.
##
## \macro_code
##
## \date February 2018
## \author Clemens Lange
## \author Wouter Verkerke (C version)

import ROOT

# Set up component pdfs
# ---------------------------------------

# Declare observable x
x = ROOT.RooRealVar("x", "x", 0, 10)

# Create two Gaussian PDFs g1(x,mean1,sigma) anf g2(x,mean2,sigma) and
# their parameters
mean = ROOT.RooRealVar("mean", "mean of gaussians", 5)
sigma1 = ROOT.RooRealVar("sigma1", "width of gaussians", 0.5)
sigma2 = ROOT.RooRealVar("sigma2", "width of gaussians", 1)

sig1 = ROOT.RooGaussian("sig1", "Signal component 1", x, mean, sigma1)
sig2 = ROOT.RooGaussian("sig2", "Signal component 2", x, mean, sigma2)

# Build Chebychev polynomial p.d.f.
a0 = ROOT.RooRealVar("a0", "a0", 0.5, 0., 1.)
a1 = ROOT.RooRealVar("a1", "a1", -0.2, 0., 1.)
bkg = ROOT.RooChebychev("bkg", "Background", x, ROOT.RooArgList(a0, a1))

# Sum the signal components into a composite signal p.d.f.
sig1frac = ROOT.RooRealVar(
    "sig1frac", "fraction of component 1 in signal", 0.8, 0., 1.)
sig = ROOT.RooAddPdf(
    "sig", "Signal", ROOT.RooArgList(sig1, sig2), ROOT.RooArgList(sig1frac))

# Construct extended comps with range spec
# ------------------------------------------------------------------------------

# Define signal range in which events counts are to be defined
x.setRange("signalRange", 4, 6)

# Associated nsig/nbkg as expected number of events with sig/bkg
# _in_the_range_ "signalRange"
nsig = ROOT.RooRealVar(
    "nsig", "number of signal events in signalRange", 500, 0., 10000)
nbkg = ROOT.RooRealVar(
    "nbkg", "number of background events in signalRange", 500, 0, 10000)
esig = ROOT.RooExtendPdf(
    "esig", "extended signal p.d.f", sig, nsig, "signalRange")
ebkg = ROOT.RooExtendPdf(
    "ebkg", "extended background p.d.f", bkg, nbkg, "signalRange")

# Sum extended components
# ---------------------------------------------

# Construct sum of two extended p.d.f. (no coefficients required)
model = ROOT.RooAddPdf("model", "(g1+g2)+a", ROOT.RooArgList(ebkg, esig))

# Sample data, fit model
# -------------------------------------------

# Generate 1000 events from model so that nsig, come out to numbers <<500
# in fit
data = model.generate(ROOT.RooArgSet(x), 1000)

# Perform unbinned extended ML fit to data
r = model.fitTo(data, ROOT.RooFit.Extended(ROOT.kTRUE), ROOT.RooFit.Save())
r.Print()
