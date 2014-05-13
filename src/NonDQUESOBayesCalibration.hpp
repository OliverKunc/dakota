/*  _______________________________________________________________________

    DAKOTA: Design Analysis Kit for Optimization and Terascale Applications
    Copyright 2014 Sandia Corporation.
    This software is distributed under the GNU Lesser General Public License.
    For more information, see the README file in the top Dakota directory.
    _______________________________________________________________________ */

//- Class:	 NonDBayesCalibration
//- Description: Base class for generic Bayesian inference
//- Owner:       Laura Swiler
//- Checked by:
//- Version:

#ifndef NOND_QUESO_BAYES_CALIBRATION_H
#define NOND_QUESO_BAYES_CALIBRATION_H

#include "NonDBayesCalibration.hpp"
#include "ExperimentData.hpp"

// forward declare isolate QUESO includes to Dakota .cpp files
namespace QUESO {
  class GslVector;
  class GslMatrix;
}

namespace Dakota {


/// Bayesian inference using the QUESO library from UT Austin

/** This class provides a wrapper to the QUESO library developed 
 * as part of the Predictive Science Academic Alliance Program (PSAAP), 
 * specifically the PECOS (Predictive Engineering and 
 * Computational Sciences) Center at UT Austin. The name QUESO stands for 
 * Quantification of Uncertainty for Estimation, Simulation, and 
 * Optimization.    */

class NonDQUESOBayesCalibration: public NonDBayesCalibration
{
public:

  //
  //- Heading: Constructors and destructor
  //

  /// standard constructor
  NonDQUESOBayesCalibration(ProblemDescDB& problem_db, Model& model);
  /// destructor
  ~NonDQUESOBayesCalibration();

  /// MCMC type (DRAM or Multilevel, both within QUESO) 
  String mcmcType;
  /// Rejection type (standard or delayed, in the DRAM framework) 
  String rejectionType;
  /// Metropolis type (hastings or adaptive, in the DRAM framework) 
  String metropolisType;
  /// number of samples in the chain (e.g. number of MCMC samples)
  int numSamples;
  /// scale factor for proposal covariance
  RealVector proposalCovScale;
  /// scale factor for likelihood
  Real likelihoodScale;
  /// flag to indicated if the sigma terms should be calibrated (default true)
  bool calibrateSigmaFlag;
         
protected:

  //
  //- Heading: Virtual function redefinitions
  //

  /// redefined from DakotaNonD
  void quantify_uncertainty();
  // redefined from DakotaNonD
  //void print_results(std::ostream& s);
  
  //The likelihood routine is in the format that QUESO requires, 
  //with a particular argument list that QUESO expects. 
  //We are not using all of these arguments but may in the future.
  /// Likelihood function for call-back from QUESO to DAKOTA for evaluation
  static double dakotaLikelihoodRoutine(
  const QUESO::GslVector& paramValues,
  const QUESO::GslVector* paramDirection,
  const void*  functionDataPtr,
  QUESO::GslVector*       gradVector,
  QUESO::GslMatrix*       hessianMatrix,
  QUESO::GslVector*       hessianEffect);

  //
  //- Heading: Data
  //

  /// random seed to pass to QUESO
  int randomSeed;

private:
  //
  // - Heading: Data
  // 
  //     
  /// the emulator type: NO_EMULATOR, GP_EMULATOR, PCE_EMULATOR, or SC_EMULATOR
  short emulatorType;
  
  /// Pointer to current class instance for use in static callback functions
  static NonDQUESOBayesCalibration* NonDQUESOInstance;
  
};

} // namespace Dakota

#endif
