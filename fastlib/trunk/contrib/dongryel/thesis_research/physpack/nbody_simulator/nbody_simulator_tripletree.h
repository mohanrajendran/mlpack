/** @file nbody_tripletree.h
 *
 *  @author Dongryeol Lee (dongryel@cc.gatech.edu)
 */

#ifndef PHYSPACK_NBODY_SIMULATOR_NBODY_SIMULATOR_TRIPLETREE_H
#define PHYSPACK_NBODY_SIMULATOR_NBODY_SIMULATOR_TRIPLETREE_H

#include <deque>
#include <vector>
#include <armadillo>

#include "core/gnp/triple_range_distance_sq.h"
#include "axilrod_teller.h"
#include <boost/math/distributions/normal.hpp>
#include <boost/utility.hpp>
#include "core/monte_carlo/mean_variance_pair.h"

namespace physpack {
namespace nbody_simulator {
class NbodySimulatorPostponed {

  public:

    core::math::Range negative_potential_;

    core::math::Range positive_potential_;

    double pruned_;

    double used_error_;

  public:
    NbodySimulatorPostponed() {
      Init();
    }

    NbodySimulatorPostponed(double num_tuples) {
      negative_potential_.Init(0, 0);
      positive_potential_.Init(0, 0);
      pruned_ = num_tuples;
      used_error_ = 0;
    }

    void Init() {
      SetZero();
    }

    template<typename NbodyDelta, typename ResultType>
    void ApplyDelta(
      const NbodyDelta &delta_in, int node_index, ResultType *query_results) {
      negative_potential_ = negative_potential_ +
                            delta_in.negative_potential_[node_index];
      positive_potential_ = positive_potential_ +
                            delta_in.positive_potential_[node_index];
      pruned_ = pruned_ + delta_in.pruned_[node_index];
      used_error_ = used_error_ + delta_in.used_error_[node_index];
    }

    void ApplyPostponed(const NbodySimulatorPostponed &other_postponed) {
      negative_potential_ =
        negative_potential_ + other_postponed.negative_potential_;
      positive_potential_ =
        positive_potential_ + other_postponed.positive_potential_;
      pruned_ = pruned_ + other_postponed.pruned_;
      used_error_ = used_error_ + other_postponed.used_error_;
    }

    void SetZero() {
      negative_potential_.Init(0, 0);
      positive_potential_.Init(0, 0);
      pruned_ = 0;
      used_error_ = 0;
    }
};

class NbodySimulatorDelta {
  public:

    std::vector< core::math::Range > negative_potential_;

    std::vector< core::math::Range > positive_potential_;

    std::vector<double> pruned_;

    std::vector<double> used_error_;

    std::vector< std::pair< core::monte_carlo::MeanVariancePair,  core::monte_carlo::MeanVariancePair> > *mean_variance_pair_;

    std::vector< std::pair< core::monte_carlo::MeanVariancePair, core::monte_carlo::MeanVariancePair > > *mean_variance_pair() {
      return mean_variance_pair_;
    }

    template<typename GlobalType, typename TreeType>
    void ResetMeanVariancePairs(
      GlobalType &global,
      const std::vector<TreeType *> &nodes,
      int node_start_index) {

      for(int i = node_start_index; i < 3; i++) {
        TreeType *node = nodes[i];
        if(i == 0 || node != nodes[i - 1]) {

          // Get the iterator for the node.
          typename GlobalType::TableType::TreeIterator node_it =
            global.table()->get_node_iterator(node);
          int qpoint_index;
          for(int j = 0; j < node_it.count(); j++) {
            node_it.get_id(j, &qpoint_index);
            (*mean_variance_pair_)[qpoint_index].first.SetZero();
            (*mean_variance_pair_)[qpoint_index].second.SetZero();
          }
        }
      }
    }

    NbodySimulatorDelta() {
      negative_potential_.resize(3);
      positive_potential_.resize(3);
      pruned_.resize(3);
      used_error_.resize(3);
      SetZero();
    }

    void SetZero() {
      for(unsigned int i = 0; i < negative_potential_.size(); i++) {
        negative_potential_[i].Init(0, 0);
        positive_potential_[i].Init(0, 0);
        pruned_[i] = 0;
        used_error_[i] = 0;
      }
      mean_variance_pair_ = NULL;
    }

    template<typename MetricType, typename GlobalType>
    void DeterministicCompute(
      const MetricType &metric,
      const GlobalType &global,
      const core::gnp::TripleRangeDistanceSq <
      typename GlobalType::TableType > &triple_range_distance_sq) {

      // Set the mean variance pair pointer.
      mean_variance_pair_ =
        const_cast< GlobalType & >(global).mean_variance_pair();

      // Compute the potential range.
      core::math::Range negative_potential_range;
      core::math::Range positive_potential_range;
      global.potential().RangeUnnormOnSq(
        triple_range_distance_sq, &negative_potential_range,
        &positive_potential_range);
      core::math::Range range_sum = negative_potential_range +
                                    positive_potential_range;

      for(
        unsigned int i = 0; i < pruned_.size(); i++) {
        pruned_[i] = triple_range_distance_sq.num_tuples(i);
        used_error_[i] = pruned_[i] * 0.5 * range_sum.width();
        negative_potential_[i] = pruned_[i] * negative_potential_range;
        positive_potential_[i] = pruned_[i] * positive_potential_range;
      }
    }
};

class NbodySimulatorResult {
  public:
    std::vector< core::math::Range > negative_potential_;
    std::vector< core::math::Range > positive_potential_;
    std::vector<double> potential_e_;
    std::vector<double> pruned_;
    std::vector<double> used_error_;
    int num_deterministic_prunes_;
    int num_monte_carlo_prunes_;

    template<typename MetricType, typename GlobalType>
    void PostProcess(
      const MetricType &metric,
      int q_index, const GlobalType &global) {
      potential_e_[q_index] = (
                                negative_potential_[q_index].mid() +
                                positive_potential_[q_index].mid());
    }

    void PrintDebug(const std::string &file_name) {
      FILE *file_output = fopen(file_name.c_str(), "w+");
      for(unsigned int i = 0; i < potential_e_.size(); i++) {
        fprintf(file_output, "%g %g\n", potential_e_[i], pruned_[i]);
      }
      fclose(file_output);
    }

    void Init(int num_points) {
      negative_potential_.resize(num_points);
      positive_potential_.resize(num_points);
      potential_e_.resize(num_points);
      pruned_.resize(num_points);
      used_error_.resize(num_points);

      SetZero();
    }

    void SetZero() {
      for(int i = 0; i < static_cast<int>(negative_potential_.size()); i++) {
        negative_potential_[i].Init(0.0, 0.0);
        positive_potential_[i].Init(0.0, 0.0);
        potential_e_[i] = 0.0;
        pruned_[i] = 0.0;
        used_error_[i] = 0.0;
      }
    }

    template<typename GlobalType>
    void ApplyProbabilisticDelta(
      GlobalType &global,
      const core::gnp::TripleRangeDistanceSq <
      typename GlobalType::TableType > &triple_range_distance_sq_in,
      const std::vector<double> &failure_probabilities,
      int probabilistic_node_start_index,
      const NbodySimulatorDelta &delta_in) {

      for(int node_index = probabilistic_node_start_index;
          node_index < 3; node_index++) {

        typename GlobalType::TableType::TreeType *node =
          triple_range_distance_sq_in.node(node_index);
        if(node_index == 0 || node !=
            triple_range_distance_sq_in.node(node_index - 1)) {

          // Get the iterator for the node.
          typename GlobalType::TableType::TreeIterator node_it =
            global.table()->get_node_iterator(node);
          arma::vec qpoint;
          int qpoint_index;

          // Look up the number of standard deviations.
          double num_standard_deviations =
            global.compute_quantile(failure_probabilities[node_index]);

          do {
            // Get each point and apply contribution.
            node_it.Next(&qpoint, &qpoint_index);
            core::math::Range negative_contribution;
            core::math::Range positive_contribution;
            (*delta_in.mean_variance_pair_)[
              qpoint_index].first.scaled_interval(
                delta_in.pruned_[node_index], num_standard_deviations,
                &negative_contribution);
            (*delta_in.mean_variance_pair_)[
              qpoint_index].second.scaled_interval(
                delta_in.pruned_[node_index], num_standard_deviations,
                &positive_contribution);

            negative_potential_[qpoint_index] += negative_contribution;
            positive_potential_[qpoint_index] += positive_contribution;
            pruned_[qpoint_index] += delta_in.pruned_[node_index];
            used_error_[qpoint_index] +=
              0.5 * std::max(
                negative_contribution.width(),
                positive_contribution.width());
          }
          while(node_it.HasNext());
        }
      } // end of looping over each node.
    }

    void ApplyPostponed(
      int q_index,
      const NbodySimulatorPostponed &postponed_in) {
      negative_potential_[q_index] += postponed_in.negative_potential_;
      positive_potential_[q_index] += postponed_in.positive_potential_;
      pruned_[q_index] = pruned_[q_index] + postponed_in.pruned_;
      used_error_[q_index] = used_error_[q_index] + postponed_in.used_error_;
    }
};

template<typename IncomingTableType>
class NbodySimulatorGlobal {

  public:
    typedef IncomingTableType TableType;

  private:

    double relative_error_;

    double probability_;

    TableType *table_;

    physpack::nbody_simulator::AxilrodTeller potential_;

    double total_num_tuples_;

    boost::math::normal normal_dist_;

    std::vector < std::pair < core::monte_carlo::MeanVariancePair,
        core::monte_carlo::MeanVariancePair > > mean_variance_pair_;

    double summary_compute_quantile_;

  public:

    std::vector< double > sort_negative_potential_hi_;

    std::vector< double > sort_positive_potential_lo_;

    std::vector< double > sort_used_error_;

    std::vector< double > sort_pruned_;

    double summary_compute_quantile() const {
      return summary_compute_quantile_;
    }

    std::vector < std::pair < core::monte_carlo::MeanVariancePair,
    core::monte_carlo::MeanVariancePair > > *mean_variance_pair() {
      return &mean_variance_pair_;
    }

    double compute_quantile(double tail_mass) const {
      double mass = 1 - 0.5 * tail_mass;
      if(mass > 0.999) {
        return 3;
      }
      else {
        return boost::math::quantile(normal_dist_, mass);
      }
    }

    const physpack::nbody_simulator::AxilrodTeller &potential() const {
      return potential_;
    }

    void ApplyContribution(
      const core::gnp::TripleDistanceSq &range_in,
    std::vector< NbodySimulatorPostponed > *postponeds) const {

      double potential_value = potential_.EvalUnnormOnSq(range_in);

      for(unsigned int i = 0; i < postponeds->size(); i++) {
        if(potential_value < 0) {
          (*postponeds)[i].negative_potential_.Init(
            potential_value, potential_value);
          (*postponeds)[i].positive_potential_.Init(0, 0);
        }
        else {
          (*postponeds)[i].negative_potential_.Init(0, 0);
          (*postponeds)[i].positive_potential_.Init(
            potential_value, potential_value);
        }
        (*postponeds)[i].pruned_ = (*postponeds)[i].used_error_ = 0.0;
      }
    }

    TableType *table() {
      return table_;
    }

    const TableType *table() const {
      return table_;
    }

    double relative_error() const {
      return relative_error_;
    }

    double probability() const {
      return probability_;
    }

    double total_num_tuples() const {
      return total_num_tuples_;
    }

    void Init(
      TableType *table_in,
      double relative_error_in,
      double probability_in,
    double summary_compute_quantile_in) {

      relative_error_ = relative_error_in;
      probability_ = probability_in;
      table_ = table_in;
      total_num_tuples_ = core::math::BinomialCoefficient<double>(
                            table_in->n_entries() - 1, 2);

      // Initialize the temporary vector for storing the Monte Carlo
      // results.
      mean_variance_pair_.resize(table_->n_entries());

      // Initialize the potential.
      potential_.Init(total_num_tuples_);

      summary_compute_quantile_ = summary_compute_quantile_in;
    }
};

class NbodySimulatorSummary {

  private:

    void StartReaccumulateCommon_() {
      negative_potential_.Init(
        std::numeric_limits<double>::max(),
        - std::numeric_limits<double>::max());
      positive_potential_.Init(
        std::numeric_limits<double>::max(),
        - std::numeric_limits<double>::max());
      pruned_ = std::numeric_limits<double>::max();
      used_error_ = 0;
    }

    template<typename TableType, typename MetricType>
    void ReplacePoints_(
      const TableType &table,
      const MetricType &metric_in,
      const std::vector<int> &random_combination,
      int node_index_fix,
      core::gnp::TripleDistanceSq *distance_sq_out) const {

      arma::vec point;
      for(int i = 1; i < 3; i++) {
        int index = (node_index_fix + i) % 3;
        table.get(random_combination[index], &point);
        distance_sq_out->ReplaceOnePoint(
          metric_in, point, random_combination[index], i);
      }
    }

    template<typename TableType>
    void TranslateCombination_(
      TableType &table,
      const core::gnp::TripleRangeDistanceSq<TableType> &range_sq_in,
      std::vector<int> *random_combination_out) const {

      for(int node_index = 0; node_index < 3; node_index++) {
        int real_point_id;
        typename TableType::TreeIterator node_it =
          table.get_node_iterator(range_sq_in.node(node_index));
        node_it.get_id(
          (*random_combination_out)[node_index] - node_it.begin(),
          &real_point_id);
        (*random_combination_out)[node_index] = real_point_id;
      }
    }

    template<typename TableType>
    void RandomCombination_(
      const core::gnp::TripleRangeDistanceSq<TableType> &range_sq_in,
      int node_index_fix,
      std::vector<int> *random_combination_out) const {

      if(range_sq_in.node(0) == range_sq_in.node(1)) {

        // All three nodes are equal.
        if(range_sq_in.node(1) == range_sq_in.node(2)) {
          core::math::RandomCombination(
            range_sq_in.node(0)->begin(),
            range_sq_in.node(0)->end(), 2,
            random_combination_out, false);
        }

        // node 0 equals node 1, node 1 does not equal node 2.
        else {
          if(node_index_fix <= 1) {
            core::math::RandomCombination(
              range_sq_in.node(0)->begin(),
              range_sq_in.node(0)->end(), 1,
              random_combination_out, false);
            core::math::RandomCombination(
              range_sq_in.node(2)->begin(),
              range_sq_in.node(2)->end(), 1,
              random_combination_out, false);
          }
          else {
            core::math::RandomCombination(
              range_sq_in.node(0)->begin(),
              range_sq_in.node(0)->end(), 2,
              random_combination_out, false);
          }
        }
      }
      else {

        // node 0 does not equal node 1, node 1 equals node 2.
        if(range_sq_in.node(1) == range_sq_in.node(2)) {

          if(node_index_fix == 0) {
            core::math::RandomCombination(
              range_sq_in.node(1)->begin(),
              range_sq_in.node(1)->end(), 2,
              random_combination_out, false);
          }
          else {
            core::math::RandomCombination(
              range_sq_in.node(0)->begin(),
              range_sq_in.node(0)->end(), 1,
              random_combination_out, false);
            core::math::RandomCombination(
              range_sq_in.node(2)->begin(),
              range_sq_in.node(2)->end(), 1,
              random_combination_out, false);
          }
        }

        // All three nodes are different.
        else {
          for(int i = 0; i < 3; i++) {
            if(i != node_index_fix) {
              core::math::RandomCombination(
                range_sq_in.node(i)->begin(),
                range_sq_in.node(i)->end(), 1,
                random_combination_out, false);
            }
          }
        }
      }

      // Put the random combination in the right order.
      int fixed_element = (*random_combination_out)[0];
      random_combination_out->erase(random_combination_out->begin());
      random_combination_out->insert(
        random_combination_out->begin() + node_index_fix, fixed_element);
    }

  public:

    core::math::Range negative_potential_;

    core::math::Range positive_potential_;

    double pruned_;

    double used_error_;

    NbodySimulatorSummary() {
      SetZero();
    }

    NbodySimulatorSummary(const NbodySimulatorSummary &summary_in) {
      negative_potential_ = summary_in.negative_potential_;
      positive_potential_ = summary_in.positive_potential_;
      pruned_ = summary_in.pruned_;
      used_error_ = summary_in.used_error_;
    }

    template < typename MetricType, typename GlobalType,
             typename DeltaType, typename ResultType >
    bool CanProbabilisticSummarize(
      const MetricType &metric,
      GlobalType &global, DeltaType &delta,
      const core::gnp::TripleRangeDistanceSq <
      typename GlobalType::TableType > &range_sq_in,
      const std::vector<double> &failure_probabilities,
      int node_index,
      ResultType *query_results,
      const arma::vec &query_point,
      int qpoint_dfs_index,
      int query_point_index,
      const arma::vec *previous_query_point,
      int *previous_query_point_index) {

      // Sampled result for the current query point.
      std::pair < core::monte_carlo::MeanVariancePair,
          core::monte_carlo::MeanVariancePair > &mean_variance_pair =
            (* delta.mean_variance_pair())[query_point_index];

      // Decide whether to incorporate the previous query point's
      // result. Currently, this is disabled.
      if(false && previous_query_point_index != NULL) {

        // Distance between the current and the previous queries.
        double squared_distance = metric.DistanceSq(
                                    query_point, *previous_query_point);

        std::pair < core::monte_carlo::MeanVariancePair,
            core::monte_carlo::MeanVariancePair > &previous_mean_variance_pair =
              (* delta.mean_variance_pair())[*previous_query_point_index];

        // If close, then steal.
        if(squared_distance <= std::numeric_limits<double>::epsilon()) {
          mean_variance_pair.first.CopyValues(
            previous_mean_variance_pair.first);
          mean_variance_pair.second.CopyValues(
            previous_mean_variance_pair.second);
        }
      }

      const int num_samples = 30;

      // Look up the number of standard deviations.
      double num_standard_deviations =
        global.compute_quantile(failure_probabilities[node_index]);

      // The random combination to be used.
      std::vector<int> random_combination(1, 0);

      // Triple range distance square object to keep track.
      core::gnp::TripleDistanceSq triple_distance_sq;
      triple_distance_sq.ReplaceOnePoint(
        metric, query_point, query_point_index, 0);



      // The comparison for pruning.
      double left_hand_side = 0;
      double right_hand_side = 0;
      int num_new_samples = 0;
      do {

        // Increment the number of new samples.
        num_new_samples++;

        // The first in the list is the query point DFS index.
        random_combination[0] = qpoint_dfs_index;
        random_combination.resize(1);

        // Generate the random combination.
        RandomCombination_(
          range_sq_in, node_index, &random_combination);

        // Translate the DFS indices to the real point indices.
        TranslateCombination_(
          *(global.table()), range_sq_in, &random_combination);
        ReplacePoints_(
          *(global.table()), metric, random_combination,
          node_index, &triple_distance_sq);

        // Evaluate the potential and add it to the result of each
        // point involved.
        double potential = global.potential().EvalUnnormOnSq(
                             triple_distance_sq);

        if(potential < 0) {
          mean_variance_pair.first.push_back(potential);
          (* delta.mean_variance_pair())[
            random_combination[(node_index + 1) % 3]].first.push_back(
              potential);
          (* delta.mean_variance_pair())[
            random_combination[(node_index + 2) % 3]].first.push_back(
              potential);
        }
        else if(potential > 0) {
          mean_variance_pair.second.push_back(potential);
          (* delta.mean_variance_pair())[
            random_combination[(node_index + 1) % 3]].second.push_back(
              potential);
          (* delta.mean_variance_pair())[
            random_combination[(node_index + 2) % 3]].second.push_back(
              potential);
        }

        // Check whether the current query point can be pruned.
        core::math::Range negative_delta_contribution;
        core::math::Range positive_delta_contribution;
        mean_variance_pair.first.scaled_interval(
          range_sq_in.num_tuples(node_index), num_standard_deviations,
          &negative_delta_contribution);
        mean_variance_pair.second.scaled_interval(
          range_sq_in.num_tuples(node_index), num_standard_deviations,
          &positive_delta_contribution);
        negative_delta_contribution.hi =
          std::min(
            negative_delta_contribution.hi, 0.0);
        positive_delta_contribution.lo =
          std::max(
            positive_delta_contribution.lo, 0.0);

        left_hand_side =
          0.5 * std::max(
            negative_delta_contribution.width(),
            positive_delta_contribution.width());
        right_hand_side =
          (global.relative_error() * (
             - negative_potential_.hi - negative_delta_contribution.hi +
             positive_potential_.lo + positive_delta_contribution.lo) -
           used_error_) * (
            delta.pruned_[node_index] /
            static_cast<double>(global.total_num_tuples() - pruned_));
      }
      while(left_hand_side > right_hand_side &&
            num_new_samples < num_samples);

      return (left_hand_side <= right_hand_side);
    }

    template < typename GlobalType, typename DeltaType, typename ResultType >
    bool CanSummarize(
      const GlobalType &global, const DeltaType &delta,
      const core::gnp::TripleRangeDistanceSq <
      typename GlobalType::TableType > &triple_range_distance_sq_in,
      int node_index, ResultType *query_results) const {

      double left_hand_side = delta.used_error_[node_index];

      if(
        left_hand_side < 0 ||
        isinf(left_hand_side) ||
        isnan(left_hand_side) ||
        isinf(used_error_) ||
        isinf(delta.negative_potential_[node_index].lo) ||
        isinf(delta.negative_potential_[node_index].hi) ||
        isinf(delta.positive_potential_[node_index].lo) ||
        isinf(delta.positive_potential_[node_index].hi) ||
        isnan(delta.negative_potential_[node_index].lo) ||
        isnan(delta.negative_potential_[node_index].hi) ||
        isnan(delta.positive_potential_[node_index].lo) ||
        isnan(delta.positive_potential_[node_index].hi) ||
        delta.negative_potential_[node_index].lo > 0 ||
        delta.negative_potential_[node_index].hi > 0 ||
        delta.positive_potential_[node_index].lo < 0 ||
        delta.positive_potential_[node_index].hi < 0) {
        return false;
      }

      double right_hand_side =
        (global.relative_error() * (
           - negative_potential_.hi + positive_potential_.lo) - used_error_) *
        (delta.pruned_[node_index] /
         static_cast<double>(global.total_num_tuples() - pruned_));

      return left_hand_side <= right_hand_side;
    }

    void SetZero() {
      negative_potential_.Init(0.0, 0.0);
      positive_potential_.Init(0.0, 0.0);
      pruned_ = 0.0;
      used_error_ = 0.0;
    }

    void Init() {
      SetZero();
    }

    template<typename GlobalType>
    void StartReaccumulate(GlobalType &global) {
      StartReaccumulateCommon_();
      global.sort_negative_potential_hi_.resize(0);
      global.sort_positive_potential_lo_.resize(0);
      global.sort_used_error_.resize(0);
      global.sort_pruned_.resize(0);
    }

    void StartReaccumulate() {
      StartReaccumulateCommon_();
    }

    template<typename GlobalType>
    void PostAccumulate(GlobalType &global) {

      if(global.sort_pruned_.size() > 0) {

        // Sort the lists.
        std::sort(
          global.sort_negative_potential_hi_.begin(),
          global.sort_negative_potential_hi_.end(),
          std::greater<double>());
        std::sort(
          global.sort_positive_potential_lo_.begin(),
          global.sort_positive_potential_lo_.end());
        std::sort(
          global.sort_used_error_.begin(),
          global.sort_used_error_.end(),
          std::greater<double>());
        std::sort(
          global.sort_pruned_.begin(),
          global.sort_pruned_.end());

        // Take the appropriate quantile.
        int index =
          (int) floor(
            global.sort_negative_potential_hi_.size() *
            global.summary_compute_quantile());
        negative_potential_.hi = global.sort_negative_potential_hi_[index];
        positive_potential_.lo = global.sort_positive_potential_lo_[index];
        used_error_ = global.sort_used_error_[index];
        pruned_ = global.sort_pruned_[index];
      }
    }

    template<typename GlobalType, typename ResultType>
    void Accumulate(
      GlobalType &global, const ResultType &results, int q_index) {

      // Push the results into the temporary vectors so that they can
      // be sorted in the PostAccumulate function.
      if(results.pruned_[q_index] < global.total_num_tuples()) {
        negative_potential_.lo = std::min(
                                   negative_potential_.lo,
                                   results.negative_potential_[q_index].lo);
        global.sort_negative_potential_hi_.push_back(
          results.negative_potential_[q_index].hi);
        global.sort_positive_potential_lo_.push_back(
          results.positive_potential_[q_index].lo);
        positive_potential_.hi = std::max(
                                   positive_potential_.hi,
                                   results.positive_potential_[q_index].hi);
        global.sort_pruned_.push_back(results.pruned_[q_index]);
        global.sort_used_error_.push_back(results.used_error_[q_index]);
      }
    }

    template<typename GlobalType>
    void Accumulate(
      GlobalType &global_in,
      const NbodySimulatorSummary &summary_in,
      const NbodySimulatorPostponed &postponed_in) {

      if(summary_in.pruned_ + postponed_in.pruned_ <
          global_in.total_num_tuples()) {
        negative_potential_ |=
          (summary_in.negative_potential_ + postponed_in.negative_potential_);
        positive_potential_ =
          (summary_in.positive_potential_ + postponed_in.positive_potential_);
        pruned_ = std::min(
                    pruned_, summary_in.pruned_ + postponed_in.pruned_);
        used_error_ = std::max(
                        used_error_,
                        summary_in.used_error_ + postponed_in.used_error_);
      }
    }

    void ApplyDelta(const NbodySimulatorDelta &delta_in, int node_index) {
      negative_potential_ += delta_in.negative_potential_[node_index];
      positive_potential_ += delta_in.positive_potential_[node_index];
    }

    void ApplyPostponed(const NbodySimulatorPostponed &postponed_in) {
      negative_potential_ += postponed_in.negative_potential_;
      positive_potential_ += postponed_in.positive_potential_;
      pruned_ = pruned_ + postponed_in.pruned_;
      used_error_ = used_error_ + postponed_in.used_error_;
    }
};

class NbodySimulatorStatistic: public boost::noncopyable {

  public:

    physpack::nbody_simulator::NbodySimulatorPostponed postponed_;

    physpack::nbody_simulator::NbodySimulatorSummary summary_;

    double self_num_tuples_;

    void SetZero() {
      postponed_.SetZero();
      summary_.SetZero();
      self_num_tuples_ = 0;
    }

    /** @brief Initializes by taking statistics on raw data.
     */
    template<typename GlobalType, typename TreeType>
    void Init(const GlobalType &global, TreeType *node) {
      SetZero();
    }

    /** @brief Initializes by combining statistics of two partitions.
     *
     * This lets you build fast bottom-up statistics when building trees.
     */
    template<typename GlobalType, typename TreeType>
    void Init(
      const GlobalType &global,
      TreeType *node,
      const NbodySimulatorStatistic &left_stat,
      const NbodySimulatorStatistic &right_stat) {
      SetZero();
    }
};
}
}

#endif
