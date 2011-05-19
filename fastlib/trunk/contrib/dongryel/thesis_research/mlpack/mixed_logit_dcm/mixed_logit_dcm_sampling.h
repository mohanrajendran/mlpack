/** @file mixed_logit_dcm_sampling.h
 *
 *  The Monte Carlo samples generated for a given fixed parameter
 *  $\theta$ for mixed logit discrete choice model.
 *
 *  @author Dongryeol Lee (dongryel@cc.gatech.edu)
 */

#ifndef MLPACK_MIXED_LOGIT_DCM_MIXED_LOGIT_DCM_SAMPLING_H
#define MLPACK_MIXED_LOGIT_DCM_MIXED_LOGIT_DCM_SAMPLING_H

#include <armadillo>
#include <boost/scoped_array.hpp>
#include <vector>
#include "core/monte_carlo/mean_variance_pair_matrix.h"

namespace mlpack {
namespace mixed_logit_dcm {

/** @brief The sampling class for mixed logit discrete choice model.
 */
template<typename DCMTableType>
class MixedLogitDCMSampling {
  public:

    /** @brief The sampling type.
     */
    typedef MixedLogitDCMSampling<DCMTableType> SamplingType;

  private:

    /** @brief The simulated choice probabilities (sample mean
     *         and sample variance information).
     */
    boost::scoped_array< core::monte_carlo::MeanVariancePair >
    simulated_choice_probabilities_;

    /** @brief The quantities necessary for computing the gradient
     *         approximation error.
     */
    boost::scoped_array< core::monte_carlo::MeanVariancePairMatrix >
    gradient_error_quantities_;

    /** @brief The gradient of the simulated choice probability per
     *         person.
     */
    boost::scoped_array <
    core::monte_carlo::MeanVariancePairVector >
    simulated_choice_probability_gradients_;

    /** @brief The Hessian of the simulated log likelihood per
     *         person. Each is composed of a pair, the first of which
     *         is, $\frac{\partial}{\partial \theta}
     *         \beta^{\nu}(\theta) \frac{\partial^2}{\partial \beta^2}
     *         P_{i,j_i^*}(\beta^{nu}(\theta))
     *         (\frac{\partial}{\partial \theta}
     *         \beta^{\nu}(\theta))^T$. The second is $\frac{\partial}
     *         {\partial \theta} \beta^{\nu}(\theta) \frac{\partial}
     *         {\beta} {P_{i,j_i^*} (\beta^{\nu}(\theta))$. This vector
     *         keeps track of Equation 8.14.
     */
    boost::scoped_array <
    std::pair < core::monte_carlo::MeanVariancePairMatrix,
        core::monte_carlo::MeanVariancePairVector > >
        simulated_loglikelihood_hessians_;

    /** @brief The pointer to the discrete choice model table from
     *         which we access the attribute information of different
     *         discrete choices.
     */
    const DCMTableType *dcm_table_;

    /** @brief The number of active outer-terms in the simulated
     *         log-likelihood score.
     */
    int num_active_people_;

    /** @brief The number of integration samples for each person.
     */
    arma::ivec num_integration_samples_;

    /** @brief The parameters that parametrize the distribution from
     *         which each $\beta$ vector is drawn from. This is
     *         $\theta$.
     */
    arma::vec parameters_;

  private:

    /** @brief The common initialization routine that is called from
     *         the Init functions.
     */
    void InitCommon_() {

      // This vector maintains the running simulated choice
      // probabilities per person.
      boost::scoped_array <
      core::monte_carlo::MeanVariancePair >
      tmp_simulated_choice_probabilities(
        new core::monte_carlo::MeanVariancePair[dcm_table_->num_people()]);
      simulated_choice_probabilities_.swap(tmp_simulated_choice_probabilities);

      // This vector maintains the running statistics on the gradient
      // error quantities per person.
      boost::scoped_array < core::monte_carlo::MeanVariancePairMatrix >
      tmp_gradient_error_quantities(
        new core::monte_carlo::MeanVariancePairMatrix[
          dcm_table_->num_people()]);
      gradient_error_quantities_.swap(tmp_gradient_error_quantities);
      for(int i = 0; i < dcm_table_->num_people(); i++) {
        gradient_error_quantities_[i].Init(
          dcm_table_->num_parameters() + 1, dcm_table_->num_parameters() + 1);
      }

      // This vector maintains the gradients of the simulated choice
      // probability per person.
      boost::scoped_array <
      core::monte_carlo::MeanVariancePairVector >
      tmp_simulated_choice_probability_gradients(
        new core::monte_carlo::MeanVariancePairVector[
          dcm_table_->num_people()]);
      simulated_choice_probability_gradients_.swap(
        tmp_simulated_choice_probability_gradients);
      for(int i = 0; i < dcm_table_->num_people(); i++) {
        simulated_choice_probability_gradients_[i].Init(
          dcm_table_->num_parameters());
      }

      // This vector maintains the components necessary to regenerate
      // the Hessians of the simulated loglikelihood per person.
      boost::scoped_array <
      std::pair < core::monte_carlo::MeanVariancePairMatrix,
          core::monte_carlo::MeanVariancePairVector > >
          tmp_simulated_loglikelihood_hessians(
            new std::pair < core::monte_carlo::MeanVariancePairMatrix,
            core::monte_carlo::MeanVariancePairVector > [
              dcm_table_->num_people()]);
      simulated_loglikelihood_hessians_.swap(
        tmp_simulated_loglikelihood_hessians);
      for(int i = 0; i < dcm_table_->num_people(); i++) {
        simulated_loglikelihood_hessians_[i].first.Init(
          dcm_table_->num_parameters(), dcm_table_->num_parameters());
        simulated_loglikelihood_hessians_[i].second.Init(
          dcm_table_->num_parameters());
      }

      // Build up the samples so that it matches the initial number of
      // integration samples.
      BuildSamples_();
    }

    /** @brief Adds an integration sample to the person_index-th
     *         person so that the person's running simulated choice
     *         probabilities can be updated.
     */
    void AddIntegrationSample_(
      int person_index, const arma::vec &beta_vector) {

      // Given the beta vector, compute the choice probabilities.
      arma::vec choice_probabilities;
      dcm_table_->choice_probabilities(
        person_index, beta_vector, &choice_probabilities);

      // Given the choice probabilities, compute the choice
      // probability weighted attribute vector.
      arma::vec choice_prob_weighted_attribute_vector;
      dcm_table_->distribution().ChoiceProbabilityWeightedAttributeVector(
        *dcm_table_, person_index, choice_probabilities,
        &choice_prob_weighted_attribute_vector);

      // This is the gradient of the choice probability for a fixed
      // realization of $\beta$.
      arma::vec choice_probability_gradient_wrt_parameter;

      // j_i^* index, the person's discrete choice index.
      int discrete_choice_index =
        dcm_table_->get_discrete_choice_index(person_index);

      // The distribution knows how to compute the choice probability
      // gradient with respect to parameter
      dcm_table_->distribution().
      ChoiceProbabilityGradientWithRespectToParameter(
        parameters_, *dcm_table_, person_index,
        beta_vector, choice_probabilities,
        choice_prob_weighted_attribute_vector,
        &choice_probability_gradient_wrt_parameter);

      // Update the simulated choice probabilities.
      simulated_choice_probabilities_[person_index].push_back(
        choice_probabilities[discrete_choice_index]);

      // Update the gradient error quantities.
      gradient_error_quantities_[person_index].get(0, 0).push_back(
        core::math::Sqr(choice_probabilities[discrete_choice_index]));
      for(int i = 0; i < dcm_table_->num_parameters(); i++) {
        gradient_error_quantities_[person_index].get(0, i + 1).push_back(
          choice_probabilities[discrete_choice_index] *
          choice_probability_gradient_wrt_parameter[i]);
        gradient_error_quantities_[person_index].get(i + 1, 0).push_back(
          choice_probabilities[discrete_choice_index] *
          choice_probability_gradient_wrt_parameter[i]);
        for(int j = 0; j < dcm_table_->num_parameters(); j++) {
          gradient_error_quantities_[person_index].get(i + 1, j + 1).push_back(
            choice_probability_gradient_wrt_parameter[i] *
            choice_probability_gradient_wrt_parameter[j]);
        }
      }

      // Simulated log-likelihood gradient update.
      simulated_choice_probability_gradients_[person_index].push_back(
        choice_probability_gradient_wrt_parameter);

      // Update the Hessian of the simulated loglikelihood for the
      // given person.
      arma::mat hessian_first_part;
      arma::vec hessian_second_part;
      dcm_table_->distribution().HessianProducts(
        parameters_, *dcm_table_, person_index, beta_vector,
        choice_probabilities, choice_prob_weighted_attribute_vector,
        &hessian_first_part, &hessian_second_part);
      simulated_loglikelihood_hessians_[person_index].first.push_back(
        hessian_first_part);
      simulated_loglikelihood_hessians_[person_index].second.push_back(
        hessian_second_part);
    }

    /** @brief Draw an additional number of necessary samples so that
     *         each person has samples up to its quota.
     */
    void BuildSamples_() {

      // Set up the distribution so that the samples can be drawn.
      dcm_table_->distribution().SetupDistribution(parameters_);

      for(int i = 0; i < num_active_people_; i++) {

        // Get the index of the active person.
        int person_index = dcm_table_->shuffled_indices_for_person(i);
        this->BuildSamples_(person_index);

      } // end of looping over each active person.
    }

    /** @brief Draw an additional number of necessary samples for a
     *         fixed person.
     */
    void BuildSamples_(int person_index) {

      // The drawn beta for building the samples.
      arma::vec random_beta;

      for(int j = simulated_choice_probabilities_[person_index].num_samples();
          j < num_integration_samples_[person_index]; j++) {

        // Draw a beta from the parameter theta and add it to the
        // sample pool.
        dcm_table_->distribution().DrawBeta(parameters_, &random_beta);
        dcm_table_->distribution().SamplingAccumulatePrecompute(
          parameters_, random_beta);
        this->AddIntegrationSample_(person_index, random_beta);

      } // end of looping each new beta sample.
    }

  public:

    /** @brief Returns the number of integral samples collected for a
     *         given person.
     */
    int num_integration_samples(int person_index) const {
      return num_integration_samples_[person_index];
    }

    /** @brief Returns the minimum and the maximum number of samples
     *         collected.
     */
    void num_integration_samples_stat(
      int *min_num_samples, int *max_num_samples,
      double *avg_num_samples, double *variance) const {

      core::monte_carlo::MeanVariancePair accum;
      *min_num_samples = std::numeric_limits<int>::max();
      *max_num_samples = 0;
      for(int i = 0; i < this->num_active_people(); i++) {
        int person_index = dcm_table_->shuffled_indices_for_person(i);
        *min_num_samples = std::min(
                             *min_num_samples,
                             this->num_integration_samples(person_index));
        *max_num_samples = std::max(
                             *max_num_samples,
                             this->num_integration_samples(person_index));
        accum.push_back(this->num_integration_samples(person_index));
      }
      *avg_num_samples = accum.sample_mean();
      *variance = accum.sample_variance();
    }

    /** @brief Returns the associated discrete choice model table.
     */
    const DCMTableType *dcm_table() const {
      return dcm_table_;
    }

    /** @brief Returns the negative simulated loglikelihood.
     */
    double NegativeSimulatedLogLikelihood() const {
      return - this->SimulatedLogLikelihood();
    }

    /** @brief Returns the gradient of the negative simulated
     *         log-likelihood objective.
     */
    void NegativeSimulatedLogLikelihoodGradient(
      arma::vec *negative_likelihood_gradient) const {
      this->SimulatedLogLikelihoodGradient(negative_likelihood_gradient);
      (*negative_likelihood_gradient) = - (*negative_likelihood_gradient);
    }

    /** @brief Returns the hessian of the negative simulated
     *         log-likelihood objective.
     */
    void NegativeSimulatedLogLikelihoodHessian(
      arma::mat *negative_likelihood_hessian) const {
      this->SimulatedLogLikelihoodHessian(negative_likelihood_hessian);
      (*negative_likelihood_hessian) = - (*negative_likelihood_hessian);
    }

    /** @brief Returns the parameters associated with the sampling.
     */
    const arma::vec &parameters() const {
      return parameters_;
    }

    /** @brief Returns the parameters associated with the sampling.
     */
    arma::vec &parameters() {
      return parameters_;
    }

    /** @brief Returns the number of active people in the sampling.
     */
    int num_active_people() const {
      return num_active_people_;
    }

    /** @brief Returns the simulated choice probability for the given
     *         person.
     */
    double simulated_choice_probability(int person_index) const {
      return simulated_choice_probabilities_[person_index].sample_mean();
    }

    /** @brief Returns the simulated choice probability statistics for
     *         the given person.
     */
    const core::monte_carlo::MeanVariancePair &
    simulated_choice_probability_stat(int person_index) const {
      return simulated_choice_probabilities_[person_index];
    }

    /** @brief Returns the gradient of the simulated choice
     *         probability for the given person.
     */
    void simulated_choice_probability_gradient(
      int person_index, arma::vec *gradient_out) const {
      simulated_choice_probability_gradients_[
        person_index].sample_means(gradient_out);
    }

    /** @brief Returns the sample mean of the gradient error component
     *         for the given person.
     */
    void gradient_error_quantities(
      int person_index, arma::mat *quantities_out) const {

      gradient_error_quantities_[
        person_index].sample_mean_variances(quantities_out);
    }

    /** @brief Returns the Hessian of the current simulated log
     *         likelihood score objective. This completes the
     *         computation of Equation 8.14 in the paper.
     */
    void SimulatedLogLikelihoodHessian(
      arma::mat *likelihood_hessian) const {

      likelihood_hessian->set_size(
        dcm_table_->num_parameters(), dcm_table_->num_parameters());
      likelihood_hessian->zeros();

      // For each active person,
      for(int i = 0; i < num_active_people_; i++) {

        // Get the index in the shuffled indices to find out the ID of
        // the person in the sample pool.
        int person_index = dcm_table_->shuffled_indices_for_person(i);

        // Get the simulated choice probability for the given person.
        double simulated_choice_probability =
          this->simulated_choice_probability(person_index);

        // Get the components for the Hessian for the person.
        const core::monte_carlo::MeanVariancePairMatrix &hessian_first_part =
          simulated_loglikelihood_hessians_[person_index].first;
        const core::monte_carlo::MeanVariancePairVector &hessian_second_part =
          simulated_loglikelihood_hessians_[person_index].second;
        arma::mat hessian_first_scaled;
        arma::vec hessian_second_scaled;

        // Compute the first part of the Hessian and scale it.
        hessian_first_part.sample_means(&hessian_first_scaled);
        hessian_first_scaled /= simulated_choice_probability;

        // Compute the second part of the Hessian and scale it.
        hessian_second_part.sample_means(&hessian_second_scaled);
        hessian_second_scaled /= simulated_choice_probability;

        // Construct the contribution on the fly.
        (*likelihood_hessian) += hessian_first_scaled;
        (*likelihood_hessian) -=
          hessian_second_scaled * arma::trans(hessian_second_scaled);
      }

      // Divide by the number of people.
      (*likelihood_hessian) =
        (1.0 / static_cast<double>(num_active_people_)) *
        (*likelihood_hessian);
    }

    /** @brief Return the gradient of the current simulated log
     *         likelihood score objective. This computes Equation 8.7
     *         in the paper.
     */
    void SimulatedLogLikelihoodGradient(
      arma::vec *likelihood_gradient) const {

      likelihood_gradient->set_size(dcm_table_->num_parameters());
      likelihood_gradient->zeros();

      // For each active person,
      for(int i = 0; i < num_active_people_; i++) {

        // Get the index in the shuffled indices to find out the ID of
        // the person in the sample pool.
        int person_index = dcm_table_->shuffled_indices_for_person(i);

        // Get the simulated choice probability for the given person.
        double simulated_choice_probability =
          this->simulated_choice_probability(person_index);

        // Get the gradient for the person.
        arma::vec gradient_divided_by_simulated_choice_probability;
        this->simulated_choice_probability_gradient(
          person_index, &gradient_divided_by_simulated_choice_probability);

        // Scale by the simulated choice probability.
        gradient_divided_by_simulated_choice_probability /=
          simulated_choice_probability;

        // Add the inverse probability weighted gradient vector for
        // the current person to the total tally.
        (*likelihood_gradient) =
          (*likelihood_gradient) +
          gradient_divided_by_simulated_choice_probability;
      }

      // Divide by the number of people.
      (*likelihood_gradient) =
        (1.0 / static_cast<double>(num_active_people_)) *
        (*likelihood_gradient);
    }

    /** @brief Return the current simulated log likelihood score.
     */
    double SimulatedLogLikelihood() const {
      double current_simulated_log_likelihood = 0;
      for(int i = 0; i < num_active_people_; i++) {

        // Get the index in the shuffled indices to find out the ID of
        // the person in the sample pool.
        int person_index = dcm_table_->shuffled_indices_for_person(i);

        // Get the simulated choice probability for the
        // given person corresponding to its discrete choice.
        double simulated_choice_probability =
          this->simulated_choice_probability(person_index);
        current_simulated_log_likelihood +=
          log(simulated_choice_probability);
      }
      current_simulated_log_likelihood /=
        static_cast<double>(num_active_people_);
      return current_simulated_log_likelihood;
    }

    /** @brief Initializes a sampling object with another sampling
     *         object. This does not copy exactly, but makes sure that
     *         the new sample gets the right number of people, each
     *         with the same number of integration samples.  The
     *         iterate from the previous sample information is stepped
     *         by an appropriate amount before the sampling commences.
     */
    void Init(
      const SamplingType &sampling_in, const arma::vec &step) {

      dcm_table_ = sampling_in.dcm_table();
      num_active_people_ = sampling_in.num_active_people();

      // Step the parameter from the previous sample.
      parameters_ = sampling_in.parameters() + step;

      // This maintains the number of integration samples collected
      // for each person.
      num_integration_samples_.zeros(dcm_table_->num_people());
      for(int i = 0; i < num_active_people_; i++) {
        int person_index = dcm_table_->shuffled_indices_for_person(i);
        num_integration_samples_[person_index] =
          sampling_in.num_integration_samples(person_index);
      }

      // The common initialization part.
      this->InitCommon_();
    }

    /** @brief Initializes a sampling object with an initial number of
     *         people, each with a pre-specified number of initial
     *         samples.
     */
    void Init(
      DCMTableType *dcm_table_in,
      const arma::vec &initial_parameters_in,
      int num_active_people_in,
      int initial_num_integration_samples_in) {

      dcm_table_ = dcm_table_in;
      num_active_people_ = num_active_people_in;

      // Initialize the associated parameter.
      parameters_ = initial_parameters_in;

      // This maintains the number of integration samples collected
      // for each person.
      num_integration_samples_.zeros(dcm_table_->num_people());
      for(int i = 0; i < num_active_people_; i++) {
        int person_index = dcm_table_->shuffled_indices_for_person(i);
        num_integration_samples_[person_index] =
          initial_num_integration_samples_in;
      }

      // The common initialization part.
      this->InitCommon_();
    }

    /** @brief Add samples to a given person.
     */
    void AddSamples(int person_index, int num_additional_samples) {
      num_integration_samples_[person_index] += num_additional_samples;

      // Build up additional samples for the new people.
      BuildSamples_(person_index);
    }

    /** @brief Add an additional number of people to the outer term,
     *         each starting with an initial number of integral
     *         samples.
     */
    void AddActivePeople(
      int num_additional_people, int initial_num_integration_samples_in) {

      for(int i = 0; i < num_additional_people; i++) {
        int person_index =
          dcm_table_->shuffled_indices_for_person(i + num_active_people_);
        num_integration_samples_[person_index] =
          initial_num_integration_samples_in;
      }
      num_active_people_ += num_additional_people;

      // Build up additional samples for the new people.
      BuildSamples_();
    }
};
}
}

#endif
