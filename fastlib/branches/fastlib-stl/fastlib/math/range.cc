/**
 * @file range.cc
 *
 * Implementation of the Range class.
 */
#include "range.h"
#include <float.h>

/**
 * Initialize the range to 0.
 */
Range::Range() :
    lo(0), hi(0) { /* nothing else to do */ }

/**
 * Initialize a range to enclose only the given point.
 */
Range::Range(double point) :
    lo(point), hi(point) { /* nothing else to do */ }

/**
 * Initializes the range to the specified values.
 */
Range::Range(double lo_in, double hi_in) :
    lo(lo_in), hi(hi_in) { /* nothing else to do */ }

/** Initialize to an empty set, where lo > hi. */
void Range::InitEmptySet() {
  lo = DBL_MAX;
  hi = -DBL_MAX;
}

/** Initializes to -infinity to infinity. */
void Range::InitUniversalSet() {
  lo = -DBL_MAX;
  hi = DBL_MAX;
}

/**
 * Resets to a range of values.
 *
 * Since there is no dynamic memory this is the same as Init, but calling
 * Reset instead of Init probably looks more similar to surrounding code.
 */
void Range::Reset(double lo_in, double hi_in) {
  lo = lo_in;
  hi = hi_in;
}

/**
 * Gets the span of the range, hi - lo.
 */
double Range::width() const {
  return hi - lo;
}

/**
 * Gets the midpoint of this range.
 */
double Range::mid() const {
  return (hi + lo) / 2;
}

/**
 * Interpolates (factor) * hi + (1 - factor) * lo.
 */
double Range::interpolate(double factor) const {
  return factor * width() + lo;
}

/**
 * Takes the maximum of upper and lower bounds independently.
 */
void Range::MaxWith(const Range& range) {
  if (range.lo > lo)
    lo = range.lo;
  if (range.hi > hi)
    hi = range.hi;
}

/**
 * Takes the minimum of upper and lower bounds independently.
 */
void Range::MinWith(const Range& range) {
  if (range.lo < lo)
    lo = range.lo;
  if (range.hi < hi)
    hi = range.hi;
}

/**
 * Takes the maximum of upper and lower bounds independently.
 */
void Range::MaxWith(double v) {
  if (v > lo) {
    lo = v;
    if (v > hi)
      hi = v;
  }
}

/**
 * Takes the minimum of upper and lower bounds independently.
 */
void Range::MinWith(double v) {
  if (v < hi) {
    hi = v;
    if (v < lo)
      lo = v;
  }
}

/**
 * Expands range to include the other range.
 */
Range& Range::operator|=(const Range& rhs) {
  if (rhs.lo < lo)
    lo = rhs.lo;
  if (rhs.hi > hi)
    hi = rhs.hi;

  return *this;
}

Range Range::operator|(const Range& rhs) const {
  return Range((rhs.lo < lo) ? rhs.lo : lo,
               (rhs.hi > hi) ? rhs.hi : hi);
}

/**
 * Shrinks range to be the overlap with another range, becoming an empty
 * set if there is no overlap.
 */
Range& Range::operator&=(const Range& rhs) {
  if (rhs.lo > lo)
    lo = rhs.lo;
  if (rhs.hi < hi)
    hi = rhs.hi;

  return *this;
}

Range Range::operator&(const Range& rhs) const {
  return Range((rhs.lo > lo) ? rhs.lo : lo,
               (rhs.hi < hi) ? rhs.hi : hi);
}

/**
 * Scale the bounds by the given double.
 */
Range& Range::operator*=(const double d) {
  lo *= d;
  hi *= d;

  // Now if we've negated, we need to flip things around so the bound is valid.
  if (lo > hi) {
    double tmp = hi;
    hi = lo;
    lo = tmp;
  }

  return *this;
}

Range Range::operator*(const double d) const {
  double nlo = lo * d;
  double nhi = hi * d;

  if (nlo <= nhi)
    return Range(nlo, nhi);
  else
    return Range(nhi, nlo);
}

// Symmetric case.
Range operator*(const double d, const Range& r) {
  double nlo = r.lo * d;
  double nhi = r.hi * d;

  if (nlo <= nhi)
    return Range(nlo, nhi);
  else
    return Range(nhi, nlo);
}

/**
 * Compare with another range for strict equality.
 */
bool Range::operator==(const Range& rhs) const {
  return (lo == rhs.lo) && (hi == rhs.hi);
}

bool Range::operator!=(const Range& rhs) const {
  return (lo != rhs.lo) || (hi != rhs.hi);
}

/**
 * Compare with another range.  For Range objects x and y, x < y means that x is
 * strictly less than y and does not overlap at all.
 */
bool Range::operator<(const Range& rhs) const {
  return hi < rhs.lo;
}

bool Range::operator>(const Range& rhs) const {
  return lo > rhs.hi;
}

/**
 * Determines if a point is contained within the range.
 */
bool Range::Contains(double d) const {
  return d >= lo && d <= hi;
}
