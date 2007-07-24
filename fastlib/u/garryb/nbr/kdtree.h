// Copyright 2007 Georgia Institute of Technology. All rights reserved.
// ABSOLUTELY NOT FOR DISTRIBUTION
/**
 * @file kdtree.h
 *
 * Tools for kd-trees.
 *
 * @experimental
 */

#ifndef TREE_SPKDTREE_H
#define TREE_SPKDTREE_H

#include "spnode.h"
#include "spbounds.h"
#include "cachearray.h"

#include "base/common.h"
#include "col/arraylist.h"
#include "file/serialize.h"
#include "fx/fx.h"

/* Implementation */

/**
 * Single-threaded kd-tree builder.
 *
 * Rearranges points in place and attempts to take advantage of the block
 * structure.
 *
 * The algorithm uses a combination of midpoint and median splits.
 * At the higher levels of the tree, a median-like split is done such that
 * the split falls on the block boundary (or otherwise specified chunk_size)
 * that is closest to the middle index.  Once the number of points
 * considered is smaller than the chunk size, midpoint splits are done.
 * The median splits simplify load balancing and allow more efficient
 * storage of data, and actually help the dual-tree algorithm in the
 * initial few layers -- however, the midpoint splits help to separate
 * outliers from the rest of the data.  Leaves are created once the number
 * of points is at most leaf_size.
 */
template<typename TPoint, typename TNode, typename TParam>
class KdTreeHybridBuilder {
 public:
  typedef TNode Node;
  typedef TPoint Point;
  typedef typename TNode::Bound Bound;
  typedef TParam Param;

 public:
  /**
   * Builds a kd-tree.
   *
   * See class comments.
   *
   * @param module module for tuning parameters: leaf_size (maximum
   *        number of points per leaf), and chunk_size (rounding granularity
   *        for median splits)
   * @param param parameters needed by the bound or other structures
   * @param begin_index the first index that I'm building
   * @param end_index one beyond the last index
   * @param points_inout the points, to be reordered
   * @param nodes_create the nodes, which will be allocated one by one
   */
  static void Build(struct datanode *module, const Param &param,
      index_t begin_index, index_t end_index,
      CacheArray<Point> *points_inout, CacheArray<Node> *nodes_create) {
    KdTreeHybridBuilder builder;
    builder.Doit(module, &param, begin_index, end_index,
        points_inout, nodes_create);
  }

 private:
  const Param* param_;
  CacheArray<Point> points_;
  CacheArray<Node>* nodes_;
  index_t leaf_size_;
  index_t chunk_size_;
  index_t dim_;
  index_t begin_index_;
  index_t end_index_;

 public:
  void Doit(
      struct datanode *module,
      const Param* param_in_,
      index_t begin_index,
      index_t end_index,
      CacheArray<Point> *points_inout,
      CacheArray<Node> *nodes_create) {
    param_ = param_in_;
    begin_index_ = begin_index;
    end_index_ = end_index;

    points_.Init(points_inout, BlockDevice::M_MODIFY);

    nodes_ = nodes_create;

    {
      CacheRead<Point> first_point(&points_, points_.begin_index());
      dim_ = first_point->vec().length();
    }

    leaf_size_ = fx_param_int(module, "leaf_size", 32);
    chunk_size_ = fx_param_int(module, "chunk_size",
        points_inout->n_block_elems());
    DEBUG_ASSERT(points_inout->n_block_elems() % chunk_size_ == 0);
    if (!math::IsPowerTwo(chunk_size_)) {
      NONFATAL("With NBR, it's best to have chunk_size be a power of 2.");
    }

    fx_timer_start(module, "tree_build");
    Build_();
    fx_timer_stop(module, "tree_build");

    points_.Flush(false);
    nodes_->Flush(false);
  }
  index_t Partition_(
      index_t split_dim, double splitvalue,
      index_t begin, index_t count,
      Bound* left_bound, Bound* right_bound);
  void FindBoundingBox_(index_t begin, index_t count, Bound *bound);
  void Build_(index_t node_i);
  void Build_();
};

template<typename TPoint, typename TNode, typename TParam>
void KdTreeHybridBuilder<TPoint, TNode, TParam>::FindBoundingBox_(
    index_t begin, index_t count, Bound *bound) {
  CacheReadIter<Point> point(&points_, begin);
  for (index_t i = count; i--; point.Next()) {
    *bound |= point->vec();
  }
}

template<typename TPoint, typename TNode, typename TParam>
index_t KdTreeHybridBuilder<TPoint, TNode, TParam>::Partition_(
    index_t split_dim, double splitvalue,
    index_t begin, index_t count,
    Bound* left_bound, Bound* right_bound) {
  index_t left_i = begin;
  index_t right_i = begin + count - 1;

  /* At any point:
   *
   *   everything < left_i is correct
   *   everything > right_i is correct
   */
  for (;;) {
    for (;;) {
      if (unlikely(left_i > right_i)) return left_i;
      CacheRead<Point> left_v(&points_, left_i);
      if (left_v->vec().get(split_dim) >= splitvalue) {
        *right_bound |= left_v->vec();
        break;
      }
      *left_bound |= left_v->vec();
      left_i++;
    }

    for (;;) {
      if (unlikely(left_i > right_i)) return left_i;
      CacheRead<Point> right_v(&points_, right_i);
      if (right_v->vec().get(split_dim) < splitvalue) {
        *left_bound |= right_v->vec();
        break;
      }
      *right_bound |= right_v->vec();
      right_i--;
    }

    points_.Swap(left_i, right_i);

    DEBUG_ASSERT(left_i <= right_i);
    right_i--;
  }

  abort();
}

template<typename TPoint, typename TNode, typename TParam>
void KdTreeHybridBuilder<TPoint, TNode, TParam>::Build_(
    index_t node_i) {
  Node *node = nodes_->StartWrite(node_i);
  bool leaf = true;

  if (node->count() > leaf_size_) {
    index_t split_dim = BIG_BAD_NUMBER;
    double max_width = -1;

    // Short loop to find widest dimension
    for (index_t d = 0; d < dim_; d++) {
      double w = node->bound().get(d).width();

      if (unlikely(w > max_width)) {
        max_width = w;
        split_dim = d;
      }
    }

    if (max_width != 0) {
      index_t left_i = nodes_->Alloc();
      index_t right_i = nodes_->Alloc();
      Node *left = nodes_->StartWrite(left_i);
      Node *right = nodes_->StartWrite(right_i);

      left->bound().Reset();
      right->bound().Reset();

      index_t split_col;
      index_t begin_col = node->begin();
      index_t end_col = node->end();
      // attempt to make all leaves of identical size
      double split_val;
      SpRange current_range = node->bound().get(split_dim);

      if (node->count() <= chunk_size_) {
        // perform a midpoint split
        split_val = current_range.mid();
        split_col = Partition_(split_dim, split_val,
              begin_col, end_col - begin_col,
              &left->bound(), &right->bound());
      } else {
        // perform a block-rounded median split.  goal_col is the
        // block-rounded index that we want to serve as the boundary.
        index_t goal_col = (begin_col + end_col + chunk_size_)
            / chunk_size_ / 2 * chunk_size_;
        typename Node::Bound left_bound;
        typename Node::Bound right_bound;
        left_bound.Init(dim_);
        right_bound.Init(dim_);

        for (;;) {
          // use linear interpolation to guess the value to split on.
          // this to lead to convergence rather quickly.
          split_val = current_range.interpolate(
              (goal_col - begin_col) / double(end_col - begin_col));

          left_bound.Reset();
          right_bound.Reset();
          split_col = Partition_(split_dim, split_val,
                begin_col, end_col - begin_col,
                &left_bound, &right_bound);

          if (split_col == goal_col) {
            left->bound() |= left_bound;
            right->bound() |= right_bound;
            break;
          } else if (split_col < goal_col) {
            left->bound() |= left_bound;
            current_range = right_bound.get(split_dim);
            if (current_range.width() == 0) {
              right->bound() |= right_bound;
              break;
            }
            begin_col = split_col;
          } else if (split_col > goal_col) {
            right->bound() |= right_bound;
            current_range = left_bound.get(split_dim);
            if (current_range.width() == 0) {
              left->bound() |= left_bound;
              break;
            }
            end_col = split_col;
          }
        }

        // Don't accept no for an answer.  Block bounadaries are very
        // important, so if we straddle a boundary because there are
        // duplicates, just have the duplicate on both sides.
        split_col = goal_col;
        DEBUG_ASSERT(split_col % points_.n_block_elems() == 0);
      }

      DEBUG_MSG(3.0,"split (%d,[%d],%d) split_dim %d on %f (between %f, %f)",
          node->begin(), split_col,
          node->begin() + node->count(), split_dim, split_val,
          node->bound().get(split_dim).lo,
          node->bound().get(split_dim).hi);
      // This should never happen if max_width > 0
      DEBUG_ASSERT(left->count() != 0 && right->count() != 0);

      left->set_range(node->begin(), split_col - node->begin());
      right->set_range(split_col, node->end() - split_col);

      Build_(left_i);
      Build_(right_i);

      node->set_child(0, left_i);
      node->set_child(1, right_i);

      node->stat().Accumulate(*param_, left->stat(),
          left->bound(), left->count());
      left->stat().Postprocess(*param_, node->bound(),
          node->count());
      node->stat().Accumulate(*param_, right->stat(),
          right->bound(), right->count());
      right->stat().Postprocess(*param_, node->bound(),
          node->count());

      leaf = false;
      nodes_->StopWrite(left_i);
      nodes_->StopWrite(right_i);
    } else {
      NONFATAL("There is probably a bug somewhere else - "
          "%"LI"d points are all identical.",
          node->count());
    }
  }

  if (leaf) {
    node->set_leaf();
    // ensure leaves don't straddle block boundaries
    DEBUG_SAME_INT(node->begin() / points_.n_block_elems(),
        (node->end() - 1) / points_.n_block_elems());
    for (index_t i = node->begin(); i < node->end(); i++) {
      CacheRead<Point> point(&points_, i);
      node->stat().Accumulate(*param_, *point);
    }
  }

  nodes_->StopWrite(node_i);
}

template<typename TPoint, typename TNode, typename TParam>
void KdTreeHybridBuilder<TPoint, TNode, TParam>::Build_() {
  index_t node_i = nodes_->Alloc();
  Node *node = nodes_->StartWrite(node_i);

  DEBUG_SAME_INT(node_i, 0);

  node->set_range(begin_index_, end_index_);

  FindBoundingBox_(node->begin(), node->end(), &node->bound());

  Build_(node_i);
  node->stat().Postprocess(*param_, node->bound(), node->count());

  nodes_->StopWrite(node_i);
}

#endif
