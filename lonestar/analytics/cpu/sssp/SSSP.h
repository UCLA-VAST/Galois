#ifndef SSSP_H
#define SSSP_H

#include <cstdint>
#include <iostream>

#include "galois/graphs/LC_CSR_Graph.h"

struct SSSP_Data {
  using dist_t = float;

  dist_t dist;
  galois::graphs::LC_CSR_Graph<void, void>::GraphNode parent;

  bool operator<(SSSP_Data other) const { return dist < other.dist; }
  bool operator>(SSSP_Data other) const { return dist > other.dist; }
  bool operator<=(SSSP_Data other) const { return dist <= other.dist; }
  bool operator>=(SSSP_Data other) const { return dist >= other.dist; }
  bool operator==(SSSP_Data other) const { return dist == other.dist; }
  bool operator!=(SSSP_Data other) const { return dist != other.dist; }

  float operator/(float div) const { return dist / div; }

  SSSP_Data operator+(const SSSP_Data& other) const {
    return {dist + other.dist, other.parent};
  }

  friend std::ostream& operator<<(std::ostream& os, SSSP_Data dist) {
    return os << "{distance=" << dist.dist << ",parent=" << dist.parent << "}";
  }
};

#endif // SSSP_H
