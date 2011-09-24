/**
 * @file kernel.h
 *
 * Common statistical kernels.
 */

#ifndef MATH_KERNEL_H
#define MATH_KERNEL_H

#include "geometry.h"
#include "math_lib.h"

#include <math.h>

/* More to come soon - Gaussian, Epanechnakov, etc. */

/**
 * Standard multivariate Gaussian kernel.
 *
 */
class GaussianKernel {
 private:
  double neg_inv_bandwidth_2sq_;
  double bandwidth_sq_;

 public:
  static const bool HAS_CUTOFF = false;

 public:
  double bandwidth_sq() const {
    return bandwidth_sq_;
  }

  void Init(double bandwidth_in, size_t dims) {
    Init(bandwidth_in);
  }

  /**
   * Initializes to a specific bandwidth.
   *
   * @param bandwidth_in the standard deviation sigma
   */
  void Init(double bandwidth_in) {
    bandwidth_sq_ = bandwidth_in * bandwidth_in;
    neg_inv_bandwidth_2sq_ = -1.0 / (2.0 * bandwidth_sq_);
  }

  /**
   * Evaluates an unnormalized density, given the distance between
   * the kernel's mean and a query point.
   */
  double EvalUnnorm(double dist) const {
    return EvalUnnormOnSq(dist * dist);
  }

  /**
   * Evaluates an unnormalized density, given the square of the
   * distance.
   */
  double EvalUnnormOnSq(double sqdist) const {
    double d = exp(sqdist * neg_inv_bandwidth_2sq_);
    return d;
  }

  /** Unnormalized range on a range of squared distances. */
  DRange RangeUnnormOnSq(const DRange& range) const {
    return DRange(EvalUnnormOnSq(range.hi), EvalUnnormOnSq(range.lo));  //!! TODO explain
  }

  /**
   * Gets the maximum unnormalized value.
   */
  double MaxUnnormValue() {
    return 1;
  }

  /**
   * Divide by this constant when you're done.
   */
  double CalcNormConstant(size_t dims) const {
    // Changed because * faster than / and 2 * math::PI opt out.  RR
    //return pow((-math::PI/neg_inv_bandwidth_2sq_), dims/2.0);
    return pow(2 * math::PI * bandwidth_sq_, dims / 2.0);
  }
};

/**
 * Multivariate Epanechnikov kernel.
 *
 * To use, first get an unnormalized density, and divide by the
 * normalizeation factor.
 */
class EpanKernel {
 private:
  double inv_bandwidth_sq_;
  double bandwidth_sq_;

 public:
  static const bool HAS_CUTOFF = true;
  
 public:
  void Init(double bandwidth_in, size_t dims) {
    Init(bandwidth_in);
  }

  /**
   * Initializes to a specific bandwidth.
   */
  void Init(double bandwidth_in) {
    bandwidth_sq_ = (bandwidth_in * bandwidth_in);
    inv_bandwidth_sq_ = 1.0 / bandwidth_sq_;
  }
  
  /**
   * Evaluates an unnormalized density, given the distance between
   * the kernel's mean and a query point.
   */
  double EvalUnnorm(double dist) const {
    return EvalUnnormOnSq(dist * dist);
  }
  
  /**
   * Evaluates an unnormalized density, given the square of the
   * distance.
   */
  double EvalUnnormOnSq(double sqdist) const {
    // TODO: Try the fabs non-branching version.
    if (sqdist < bandwidth_sq_) {
      return 1 - sqdist * inv_bandwidth_sq_;
    } else {
      return 0;
    }
  }

  /** Unnormalized range on a range of squared distances. */
  DRange RangeUnnormOnSq(const DRange& range) const {
    return DRange(EvalUnnormOnSq(range.hi), EvalUnnormOnSq(range.lo));  //!! TODO explain
  }

  /**
   * Gets the maximum unnormalized value.
   */
  double MaxUnnormValue() {
    return 1.0;
  }
  
  /**
   * Divide by this constant when you're done.
   */
  double CalcNormConstant(size_t dims) const {
    return 2.0 * SphereVolume(sqrt(bandwidth_sq_), dims)
        / (dims + 2.0);
  }
  
  double SphereVolume(double r, int d) {
      int n = d / 2;
      double val;

      mlpack::IO::Assert(d >= 0);

      if (d % 2 == 0) {
        val = pow(r * sqrt(PI), d) / Factorial(n);
      }
      else {
        val = pow(2 * r, d) * pow(PI, n) * Factorial(n) / Factorial(d);
      }

      return val;
    }

  /**
   * Gets the squared bandwidth.
   */
  double bandwidth_sq() const {
    return bandwidth_sq_;
  }
  
  /**
  * Gets the reciproccal of the squared bandwidth.
   */
  double inv_bandwidth_sq() const {
    return inv_bandwidth_sq_;
  }
};


#endif
