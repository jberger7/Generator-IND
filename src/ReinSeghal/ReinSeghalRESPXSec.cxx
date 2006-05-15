//____________________________________________________________________________
/*
 Copyright (c) 2003-2006, GENIE Neutrino MC Generator Collaboration
 All rights reserved.
 For the licensing terms see $GENIE/USER_LICENSE.

 Author: Costas Andreopoulos <C.V.Andreopoulos@rl.ac.uk>
         CCLRC, Rutherford Appleton Laboratory - May 05, 2004

 For the class documentation see the corresponding header file.

 Important revisions after version 2.0.0 :

*/
//____________________________________________________________________________

#include <TMath.h>

#include "Algorithm/AlgFactory.h"
#include "Algorithm/AlgConfigPool.h"
#include "BaryonResonance/BaryonResDataSetI.h"
#include "BaryonResonance/BaryonResUtils.h"
#include "Conventions/Constants.h"
#include "Conventions/RefFrame.h"
#include "Conventions/KineVar.h"
#include "Conventions/Units.h"
#include "Messenger/Messenger.h"
#include "PDG/PDGCodes.h"
#include "PDG/PDGUtils.h"
#include "ReinSeghal/ReinSeghalRESPXSec.h"
#include "ReinSeghal/RSHelicityAmplModelI.h"
#include "ReinSeghal/RSHelicityAmpl.h"
#include "Utils/KineUtils.h"
#include "Utils/MathUtils.h"
#include "Utils/Range1.h"

using namespace genie;
using namespace genie::constants;

//____________________________________________________________________________
ReinSeghalRESPXSec::ReinSeghalRESPXSec() :
XSecAlgorithmI("genie::ReinSeghalRESPXSec")
{

}
//____________________________________________________________________________
ReinSeghalRESPXSec::ReinSeghalRESPXSec(string config) :
XSecAlgorithmI("genie::ReinSeghalRESPXSec", config)
{

}
//____________________________________________________________________________
ReinSeghalRESPXSec::~ReinSeghalRESPXSec()
{

}
//____________________________________________________________________________
double ReinSeghalRESPXSec::XSec(
                 const Interaction * interaction, KinePhaseSpace_t kps) const
{
  if(! this -> ValidProcess    (interaction) ) return 0.;
  if(! this -> ValidKinematics (interaction) ) return 0.;

  //----- Get kinematical parameters
  const Kinematics & kinematics = interaction -> GetKinematics();
  double W  = kinematics.W();
  double q2 = kinematics.q2();

  //----- Under the DIS/RES joining scheme, xsec(RES)=0 for W>=Wcut
  if(fUsingDisResJoin) {
    if(W>=fWcut) {
       LOG("ReinSeghalRes", pDEBUG)
         << "RES/DIS Join Scheme: XSec[RES, W=" << W 
                                << " >= Wcut=" << fWcut << "] = 0";
       return 0;
    }
  }

  const InitialState & init_state = interaction -> GetInitialState();
  const ProcessInfo &  proc_info  = interaction -> GetProcessInfo();
  const Target & target = init_state.GetTarget();

  double E       = init_state.GetProbeE(kRfStruckNucAtRest);
  double Mnuc    = target.StruckNucleonMass(); 
  int    nucpdgc = target.StruckNucleonPDGCode();

  //-- Get the input baryon resonance
  Resonance_t resonance = interaction->GetExclusiveTag().Resonance();
  string      resname   = utils::res::AsString(resonance);

  //-- Compute Baryon Resonance params 
  fBRP.RetrieveData(resonance);  

  double Mres    = fBRP.Mass();
  int    nresidx = fBRP.ResonanceIndex();

  //-- Compute auxiliary & kinematical factors for the Rein-Seghal model
  double W2     = TMath::Power(W, 2);
  double Mnuc2  = TMath::Power(Mnuc, 2);
  double k      = 0.5 * (W2 - Mnuc2)/Mnuc;
  double v      = k - 0.5 * q2/Mnuc;
  double v2     = TMath::Power(v, 2);
  double Q2     = v2 - q2;
  double Q      = TMath::Sqrt(Q2);
  double Gf     = kGF2 / (4*kPi2);
  double Wf     = (-q2/Q2) * (W/Mnuc) * k;
  double Eprime = E - v;
  double U      = 0.5 * (E + Eprime + Q) / E;
  double V      = 0.5 * (E + Eprime - Q) / E;
  double U2     = TMath::Power(U, 2);
  double V2     = TMath::Power(V, 2);
  double UV     = U*V;

  //-- Calculate the Feynman-Kislinger-Ravndall parameters
  LOG("ReinSeghalRes", pDEBUG) << "Computing the FKR parameters";

  fFKR.Calculate(q2,W,Mnuc,nresidx);
  LOG("FKR", pDEBUG) 
           << "FKR params for RES=" << resname << " : " << fFKR;

  //-- Calculate the Rein-Seghal Helicity Amplitudes
  LOG("ReinSeghalRes", pDEBUG) << "Computing Helicity Amplitudes";

  const RSHelicityAmplModelI * hamplmod = 0;

  bool is_CC = proc_info.IsWeakCC();
  bool is_p  = pdg::IsProton(nucpdgc);

  if(is_CC)   { hamplmod = fHAmplModelCC; }
  else {
    if (is_p) { hamplmod = fHAmplModelNCp;}
    else      { hamplmod = fHAmplModelNCn;}
  }
  assert(hamplmod);
  
  RSHelicityAmpl * hampl = hamplmod->Compute(resonance, fFKR); 
  assert(hampl);

  LOG("RSHAmpl", pDEBUG)
     << "Helicity Ampl for RES=" << resname << " : " << *hampl;

  //-- Calculate Helicity Cross Sections
  double xsec_left   = TMath::Power( hampl->AmpPlus3(),  2. ) +
                       TMath::Power( hampl->AmpPlus1(),  2. );
  double xsec_right  = TMath::Power( hampl->AmpMinus3(), 2. ) +
                       TMath::Power( hampl->AmpMinus1(), 2. );
  double xsec_scalar = TMath::Power( hampl->Amp0Plus(),  2. ) +
                       TMath::Power( hampl->Amp0Minus(), 2. );
  delete hampl;

  double scale_lr = 0.5*(kPi/k)*(Mres/Mnuc);
  double scale_sc = 0.5*(kPi/k)*(Mnuc/Mres);

  xsec_left   *=  scale_lr;
  xsec_right  *=  scale_lr;
  xsec_scalar *= (scale_sc*(-Q2/q2));

  LOG("ReinSeghalRes", pDEBUG)
          << "SL = " << xsec_left << ", SR = " 
                             << xsec_right << " SSC = " << xsec_scalar;

  //-- Compute the cross section
  int  nupdgc  = init_state.GetProbePDGCode();
  bool is_nu   = pdg::IsNeutrino(nupdgc);

  double xsec = 0;
  if (is_nu) {
     xsec = Gf*Wf*(U2 * xsec_left + V2 * xsec_right + 2*UV*xsec_scalar);
  } else {
     xsec = Gf*Wf*(V2 * xsec_left + U2 * xsec_right + 2*UV*xsec_scalar);
  }

  //-- Check whether the cross section is to be weighted with a
  //   Breit-Wigner distribution (default: true)
  double bw = 1.0;
  if(fWghtBW) {
     bw = fBreitWigner->Eval(resonance, W);
     LOG("ReinSeghalRes", pDEBUG) 
       << "BreitWigner(RES=" << resname << ", W=" << W << ") = " << bw;
  } else {
      LOG("ReinSeghalRes", pDEBUG) << "Breit-Wigner wght is turned-off";
  }

  double wxsec = bw * xsec; // weighted-xsec

  LOG("ReinSeghalRes", pINFO) 
    << "\n d2xsec/dQ2dW"  << "[" << interaction->AsString()
          << "](W=" << W << ", q2=" << q2 << ", E=" << E << ") = " << wxsec;

  //-- The algorithm computes d^2xsec/dWdQ2
  //   Check whether variable tranformation is needed
  if(kps!=kPSWQ2fE) {
    double J = utils::kinematics::Jacobian(interaction,kPSWQ2fE,kps);
    xsec *= J;
  }

  //-- If requested return the free nucleon xsec even for input nuclear tgt
  if( interaction->TestBit(kIAssumeFreeNucleon) ) return wxsec;

  //-- number of scattering centers in the target
  int NNucl = (is_p) ? target.Z() : target.N();

  wxsec*=NNucl; // nuclear xsec (no nuclear suppression factor)

  return wxsec;
}
//____________________________________________________________________________
bool ReinSeghalRESPXSec::ValidProcess(const Interaction * interaction) const
{
  if(interaction->TestBit(kISkipProcessChk)) return true;

  const InitialState & init_state = interaction->GetInitialState();
  const ProcessInfo &  proc_info  = interaction->GetProcessInfo();
  const XclsTag &      xcls       = interaction->GetExclusiveTag();

  if(!proc_info.IsResonant()) return false;
  if(!proc_info.IsWeak())     return false;

  int  nuc = init_state.GetTarget().StruckNucleonPDGCode();
  int  nu  = init_state.GetProbePDGCode();

  if (!pdg::IsProton(nuc)  && !pdg::IsNeutron(nuc))     return false;
  if (!pdg::IsNeutrino(nu) && !pdg::IsAntiNeutrino(nu)) return false;

  if(!xcls.KnownResonance()) return false;

  return true;
}
//____________________________________________________________________________
bool ReinSeghalRESPXSec::ValidKinematics(const Interaction* interaction) const
{
  if(interaction->TestBit(kISkipKinematicChk)) return true;

  const Kinematics &   kinematics = interaction -> GetKinematics();
  const InitialState & init_state = interaction -> GetInitialState();

  double E    = init_state.GetProbeE(kRfStruckNucAtRest);
  double W    = kinematics.W();
  double q2   = kinematics.q2();

  //-- Check energy threshold & kinematical limits in q2, W
  double EvThr = utils::kinematics::EnergyThreshold(interaction);
  if(E <= EvThr) {
    LOG("ReinSeghalRes", pINFO) << "E  = " << E << " < Ethr = " << EvThr;
    return false;
  }

  //-- Check against physical range in W and Q2
  Range1D_t rW  = utils::kinematics::KineRange(interaction, kKVW);
  Range1D_t rQ2 = utils::kinematics::KineRange(interaction, kKVQ2);

  bool in_physical_range = utils::math::IsWithinLimits(W, rW) &&
                           utils::math::IsWithinLimits(-q2, rQ2);
  if(!in_physical_range) return false;

  return true;
}
//____________________________________________________________________________
void ReinSeghalRESPXSec::Configure(const Registry & config)
{
  Algorithm::Configure(config);
  this->LoadConfig();
}
//____________________________________________________________________________
void ReinSeghalRESPXSec::Configure(string config)
{
  Algorithm::Configure(config);
  this->LoadConfig();
}
//____________________________________________________________________________
void ReinSeghalRESPXSec::LoadConfig(void)
{
  AlgConfigPool * confp = AlgConfigPool::Instance();
  const Registry * gc = confp->GlobalParameterList();

  // Load all configuration data or set defaults

  fZeta  = fConfig->GetDoubleDef( "Zeta",  gc->GetDouble("RS-Zeta")  );
  fOmega = fConfig->GetDoubleDef( "Omega", gc->GetDouble("RS-Omega") );

  double ma  = fConfig->GetDoubleDef( "Ma", gc->GetDouble("RES-Ma") );
  double mv  = fConfig->GetDoubleDef( "Mv", gc->GetDouble("RES-Mv") );

  fMa2    = TMath::Power(ma,2);
  fMv2    = TMath::Power(mv,2);

  fWghtBW = fConfig->GetBoolDef("weight-with-breit-wigner", true);

  double thw = fConfig->GetDoubleDef(
                        "weinberg-angle", gc->GetDouble("WeinbergAngle"));

  fFKR.Configure(fZeta, fOmega, ma, mv, thw);

  // Load all the sub-algorithms needed

  fBaryonResDataSet = 0;
  fHAmplModelCC     = 0;
  fHAmplModelNCp    = 0;
  fHAmplModelNCn    = 0;
  fBreitWigner      = 0;

  //-- Access a "Baryon Resonance data-set" sub-algorithm
  fBaryonResDataSet = dynamic_cast<const BaryonResDataSetI *> (
    this->SubAlg("baryonres-dataset-alg-name", "baryonres-dataset-param-set"));
  assert(fBaryonResDataSet);

  fBRP.SetDataSet(fBaryonResDataSet); // <-- attach data set;

  if(fWghtBW) {
    //-- Access a "Breit-Wigner" sub-algorithm
    fBreitWigner = dynamic_cast<const BreitWignerI *> (
             this->SubAlg("breit-wigner-alg-name", "breit-wigner-param-set"));
    assert(fBreitWigner);
  }

  //-- Access a "Helicity Amplitudes model" sub-algorithms

  AlgFactory * algf = AlgFactory::Instance();

  fHAmplModelCC  = dynamic_cast<const RSHelicityAmplModelI *> (
	        algf->GetAlgorithm("genie::RSHelicityAmplModelCC","Default"));
  fHAmplModelNCp = dynamic_cast<const RSHelicityAmplModelI *> (
	       algf->GetAlgorithm("genie::RSHelicityAmplModelNCp","Default"));
  fHAmplModelNCn = dynamic_cast<const RSHelicityAmplModelI *> (
	       algf->GetAlgorithm("genie::RSHelicityAmplModelNCn","Default"));

  assert( fHAmplModelCC  );
  assert( fHAmplModelNCp );
  assert( fHAmplModelNCn );

  fUsingDisResJoin = fConfig->GetBoolDef("use-dis-res-joining-scheme", false);
  fWcut = 999999;
  if(fUsingDisResJoin) {
    fWcut = fConfig->GetDoubleDef("Wcut",gc->GetDouble("Wcut"));
  }
}
//____________________________________________________________________________

