/*  _______________________________________________________________________

    DAKOTA: Design Analysis Kit for Optimization and Terascale Applications
    Copyright 2014-2022
    National Technology & Engineering Solutions of Sandia, LLC (NTESS).
    This software is distributed under the GNU Lesser General Public License.
    For more information, see the README file in the top Dakota directory.
    _______________________________________________________________________ */

//- Class:	 NonDEnsembleSampling
//- Description: Implementation code for NonDEnsembleSampling class
//- Owner:       Mike Eldred
//- Checked by:
//- Version:

#include "dakota_system_defs.hpp"
#include "dakota_data_io.hpp"
#include "dakota_tabular_io.hpp"
#include "DakotaModel.hpp"
#include "DakotaResponse.hpp"
#include "NonDEnsembleSampling.hpp"
#include "ProblemDescDB.hpp"
#include "ActiveKey.hpp"
#include "DakotaIterator.hpp"

static const char rcsId[]="@(#) $Id: NonDEnsembleSampling.cpp 7035 2010-10-22 21:45:39Z mseldre $";


namespace Dakota {

/** This constructor is called for a standard letter-envelope iterator 
    instantiation.  In this case, set_db_list_nodes has been called and 
    probDescDB can be queried for settings from the method specification. */
NonDEnsembleSampling::
NonDEnsembleSampling(ProblemDescDB& problem_db, Model& model):
  NonDSampling(problem_db, model),
  //pilotSamples(problem_db.get_sza("method.nond.pilot_samples")),
  pilotMgmtMode(
    problem_db.get_short("method.nond.ensemble_sampling_solution_mode")),
  randomSeedSeqSpec(problem_db.get_sza("method.random_seed_sequence")),
  mlmfIter(0), equivHFEvals(0.), // also reset in pre_run()
  //allocationTarget(problem_db.get_short("method.nond.allocation_target")),
  //qoiAggregation(problem_db.get_short("method.nond.qoi_aggregation")),
  finalStatsType(problem_db.get_short("method.nond.final_statistics")),
  exportSampleSets(problem_db.get_bool("method.nond.export_sample_sequence")),
  exportSamplesFormat(
    problem_db.get_ushort("method.nond.export_samples_format")),
  seedIndex(SZ_MAX)
{
  // Support multilevel LHS as a specification override.  The estimator variance
  // is known/correct for MC and an assumption/approximation for LHS.  To get an
  // accurate LHS estimator variance, one would need:
  // (a) assumptions about separability -> analytic variance reduction by a
  //     constant factor
  // (b) similarly, assumptions about the form relative to MC (e.g., a constant
  //     factor largely cancels out within the relative sample allocation.)
  // (c) numerically-generated estimator variance (from, e.g., replicated LHS)
  if (!sampleType) // SUBMETHOD_DEFAULT
    sampleType = SUBMETHOD_RANDOM;

  switch (pilotMgmtMode) {
  case PILOT_PROJECTION: // no iteration
    maxIterations = 0; //finalCVRefinement = false;
    break;
  case OFFLINE_PILOT:
    maxIterations = 1; //finalCVRefinement = true;
    // convergenceTol option is problematic since the reference EstVar
    // comes from offline eval with Oracle/overkill N
    if (maxFunctionEvals == SZ_MAX) {
      Cerr << "Error: evaluation budget required for offline pilot mode."
	   << std::endl;
      abort_handler(METHOD_ERROR);
    }
    break;
  default: // ONLINE_PILOT
    // MLMF-specific default: don't let allocator get stuck in fine-tuning
    if (maxIterations    == SZ_MAX) maxIterations    = 25;
  //if (maxFunctionEvals == SZ_MAX) maxFunctionEvals = ; // allow inf budget
    break;
  }

  initialize_final_statistics();
}


NonDEnsembleSampling::~NonDEnsembleSampling()
{ }


bool NonDEnsembleSampling::resize()
{
  bool parent_reinit_comms = NonDSampling::resize();

  Cerr << "\nError: Resizing is not yet supported in method "
       << method_enum_to_string(methodName) << "." << std::endl;
  abort_handler(METHOD_ERROR);

  return parent_reinit_comms;
}


void NonDEnsembleSampling::assign_specification_sequence(size_t index)
{
  // Note: seedSpec/randomSeed initialized from randomSeedSeqSpec in ctor

  // Advance any sequence specifications, as admissible.  Refer to
  // NonDSampling::initialize_sample_driver() for logic.
  // Note: no colloc pts sequence as load_pilot_sample() handles this separately
  int seed_i = seed_sequence(index);
  if (seed_i) randomSeed = seed_i;
  // else previous value will allow existing RNG to continue for varyPattern
}


void NonDEnsembleSampling::pre_run()
{
  NonDSampling::pre_run();

  // remove default key (empty activeKey) since this interferes with approx
  // combination in MF surrogates.  Also useful for ML/MF re-entrancy.
  iteratedModel.clear_model_keys();

  // reset shared accumulators
  // Note: numLHSRuns is interpreted differently here (accumulation of LHS runs
  //       for each execution of ensemble sampler) than for base NonDSampling
  //       (total accumulation of LHS runs)
  mlmfIter = numLHSRuns = 0;  equivHFEvals = 0.;
  seedSpec = randomSeed = seed_sequence(0); // (re)set seeds to sequence
}


void NonDEnsembleSampling::post_run(std::ostream& s)
{
  // Final moments are generated within core_run() by convert_moments().
  // No addtional stats are currently supported.
  //if (statsFlag) // calculate statistics on allResponses
  //  compute_statistics(allSamples, allResponses);

  // NonD::update_aleatory_final_statistics() pushes momentStats into
  // finalStatistics
  update_final_statistics();

  Analyzer::post_run(s);
}


void NonDEnsembleSampling::initialize_final_statistics()
{
  switch (finalStatsType) {
  case ESTIMATOR_PERFORMANCE: { // MSE in stat goal(s) used for method selection
    size_t num_final = 2;
    ActiveSet set(num_final);//, num_active_vars); // default RV = 1
    set.derivative_vector(iteratedModel.inactive_continuous_variable_ids());
    finalStatistics = Response(SIMULATION_RESPONSE, set);

    StringArray stats_labels(num_final);
    stats_labels[0] = "avg_est_var";  stats_labels[1] = "equiv_HF_cost";
    finalStatistics.function_labels(stats_labels);
    break;
  }
  case QOI_STATISTICS: // final stats: moments + level mappings
    NonD::initialize_final_statistics();  break;
  }
}


void NonDEnsembleSampling::update_final_statistics()
{
  if (finalStatistics.is_null()) // some ctor chains do not track final stats
    return;

  /*
  if (epistemicStats) {
    size_t i, cntr = 0;
    for (i=0; i<numFunctions; ++i) {
      finalStatistics.function_value(extremeValues[i].first,  cntr++);
      finalStatistics.function_value(extremeValues[i].second, cntr++);
    }
  }
  */
  switch (finalStatsType) {
  case ESTIMATOR_PERFORMANCE:
    finalStatistics.function_value(avgEstVar,    0);
    finalStatistics.function_value(equivHFEvals, 1);
    break;
  case QOI_STATISTICS: // final stats: moments + level mappings
    NonD::update_final_statistics(); break;
  }
}


void NonDEnsembleSampling::active_set_mapping()
{
  switch (finalStatsType) {
  case ESTIMATOR_PERFORMANCE:
    // transfer ASV requests from finalStatistics to activeSet; we won't
    // support derivatives of estimator performance metrics (see also
    // initialize_final_statistics() above)
    activeSet.request_values(1);        break;
  case QOI_STATISTICS: // final stats: moments + level mappings
    NonDSampling::active_set_mapping(); break;
  }
}


void NonDEnsembleSampling::print_results(std::ostream& s, short results_state)
{
  if (statsFlag)
    switch (pilotMgmtMode) {
    case PILOT_PROJECTION:
      print_multilevel_evaluation_summary(s, NLev, "Projected"); // *** FOR CONSISTENCY WITH equivHFEvals, SHOULD BE ALLOCATED RATHER THAN RECOVERED ***
      s << "<<<<< Projected number of equivalent high fidelity evaluations: "
	<< std::scientific  << std::setprecision(write_precision)
	<< equivHFEvals << '\n';
      print_variance_reduction(s);

      //s << "\nStatistics based on multilevel sample set:\n";
      //print_moments(s, "response function",
      //	      iteratedModel.truth_model().response_labels());
      //archive_moments();
      //archive_equiv_hf_evals(equivHFEvals);
      break;
    default:
      // Any offline pilot samples are excluded
      print_multilevel_evaluation_summary(s, NLev, "Online"); // *** FOR CONSISTENCY WITH equivHFEvals, SHOULD BE ALLOCATED RATHER THAN RECOVERED ***
      s << "<<<<< Equivalent number of high fidelity evaluations: "
	<< std::scientific  << std::setprecision(write_precision)
	<< equivHFEvals << '\n';
      print_variance_reduction(s);

      s << "\nStatistics based on multilevel sample set:\n";
      //print_statistics(s);
      print_moments(s, "response function",
		    iteratedModel.truth_model().response_labels());
      archive_moments();
      archive_equiv_hf_evals(equivHFEvals);
      break;
    }
}


void NonDEnsembleSampling::
export_all_samples(String root_prepend, const Model& model, size_t iter,
		   size_t step)
{
  String tabular_filename(root_prepend);
  const String& iface_id = model.interface_id();
  size_t i, num_samp = allSamples.numCols();
  if (iface_id.empty()) tabular_filename += "NO_ID_i";
  else                  tabular_filename += iface_id + "_i";
  tabular_filename += std::to_string(iter)     +  "_l"
                   +  std::to_string(step)     +  '_'
                   +  std::to_string(num_samp) + ".dat";
  Variables vars(model.current_variables().copy());

  String context_message("NonDEnsembleSampling::export_all_samples");
  StringArray no_resp_labels;
  String cntr_label("sample_id"), interf_label("interface");

  // Rather than hard override, rely on output_precision user spec
  //int save_wp = write_precision;
  //write_precision = 16; // override
  std::ofstream tabular_stream;
  TabularIO::open_file(tabular_stream, tabular_filename, context_message);
  TabularIO::write_header_tabular(tabular_stream, vars, no_resp_labels,
				  cntr_label, interf_label,exportSamplesFormat);
  for (i=0; i<num_samp; ++i) {
    sample_to_variables(allSamples[i], vars); // NonDSampling version
    TabularIO::write_data_tabular(tabular_stream, vars, iface_id, i+1,
				  exportSamplesFormat);
  }

  TabularIO::close_file(tabular_stream, tabular_filename, context_message);
  //write_precision = save_wp; // restore
}


void NonDEnsembleSampling::
convert_moments(const RealMatrix& raw_mom, RealMatrix& final_mom)
{
  // Note: raw_mom is numFunctions x 4 and final_mom is the transpose
  if (final_mom.empty())
    final_mom.shapeUninitialized(4, numFunctions);

  // Convert uncentered raw moment estimates to central moments
  if (finalMomentsType == Pecos::CENTRAL_MOMENTS) {
    for (size_t qoi=0; qoi<numFunctions; ++qoi)
      uncentered_to_centered(raw_mom(qoi,0), raw_mom(qoi,1), raw_mom(qoi,2),
			     raw_mom(qoi,3), final_mom(0,qoi), final_mom(1,qoi),
			     final_mom(2,qoi), final_mom(3,qoi));
  }
  // Convert uncentered raw moment estimates to standardized moments
  else { //if (finalMomentsType == Pecos::STANDARD_MOMENTS) {
    Real cm1, cm2, cm3, cm4;
    for (size_t qoi=0; qoi<numFunctions; ++qoi) {
      uncentered_to_centered(raw_mom(qoi,0), raw_mom(qoi,1), raw_mom(qoi,2),
			     raw_mom(qoi,3), cm1, cm2, cm3, cm4);
      centered_to_standard(cm1, cm2, cm3, cm4, final_mom(0,qoi),
			   final_mom(1,qoi), final_mom(2,qoi),
			   final_mom(3,qoi));
    }
  }

  if (outputLevel >= DEBUG_OUTPUT)
    for (size_t qoi=0; qoi<numFunctions; ++qoi)
      Cout <<  "raw mom 1 = "   << raw_mom(qoi,0)
	   << " final mom 1 = " << final_mom(0,qoi) << '\n'
	   <<  "raw mom 2 = "   << raw_mom(qoi,1)
	   << " final mom 2 = " << final_mom(1,qoi) << '\n'
	   <<  "raw mom 3 = "   << raw_mom(qoi,2)
	   << " final mom 3 = " << final_mom(2,qoi) << '\n'
	   <<  "raw mom 4 = "   << raw_mom(qoi,3)
	   << " final mom 4 = " << final_mom(3,qoi) << "\n\n";
}

} // namespace Dakota
