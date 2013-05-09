/** deque like structure with scalable allocator usage -*- C++ -*-
 * @file
 * @section License
 *
 * Galois, a framework to exploit amorphous data-parallelism in irregular
 * programs.
 *
 * Copyright (C) 2013, The University of Texas at Austin. All rights reserved.
 * UNIVERSITY EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES CONCERNING THIS
 * SOFTWARE AND DOCUMENTATION, INCLUDING ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR ANY PARTICULAR PURPOSE, NON-INFRINGEMENT AND WARRANTIES OF
 * PERFORMANCE, AND ANY WARRANTY THAT MIGHT OTHERWISE ARISE FROM COURSE OF
 * DEALING OR USAGE OF TRADE.  NO WARRANTY IS EITHER EXPRESS OR IMPLIED WITH
 * RESPECT TO THE USE OF THE SOFTWARE OR DOCUMENTATION. Under no circumstances
 * shall University be liable for incidental, special, indirect, direct or
 * consequential damages or loss of profits, interruption of business, or
 * related expenses which may arise from use of Software or Documentation,
 * including but not limited to those resulting from defects in Software and/or
 * Documentation, or loss or inaccuracy of data of any kind.
 *
 * @author Andrew Lenharth <andrew@lenharth.org>
 */

#ifndef GALOIS_GRAPH3_H
#define GALOIS_GRAPH3_H

#include <iterator>
#include <deque>

#include "Galois/Threads.h"
#include "Galois/Graphs/Bag.h"
#include "Galois/Runtime/DistSupport.h"
#include "Galois/Runtime/Context.h"
#include "Galois/Runtime/MethodFlags.h"
#include "Galois/Runtime/PerThreadStorage.h"
#include "Galois/Runtime/mm/Mem.h"

#include <boost/iterator/filter_iterator.hpp>

namespace Galois {
namespace Graph {

enum class EdgeDirection {Un, Out, InOut};

template<typename NodeTy, typename EdgeTy, EdgeDirection EDir>
class ThirdGraph;

template<typename NHTy>
class GraphNodeBase {
  NHTy nextNode;
  bool active;

protected:
  GraphNodeBase() :active(false) {}

  NHTy& getNextNode() { return nextNode; }

  void serialize(Galois::Runtime::SerializeBuffer& s) const {
    gSerialize(s, nextNode, active);
  }
  void deserialize(Galois::Runtime::DeSerializeBuffer& s) {
    gDeserialize(s,nextNode, active);
  }

  void dump(std::ostream& os) const {
    os << "next: ";
    nextNode.dump(os);
    os << " active: ";
    os << active;
  }

public:
  bool getActive() const {
    return active;
  }

  void setActive(bool b) {
    active = b;
  }
};


template<typename NodeDataTy>
class GraphNodeData {
  NodeDataTy data;
  
protected:

  void serialize(Galois::Runtime::SerializeBuffer& s) const {
    gSerialize(s,data);
  }
  void deserialize(Galois::Runtime::DeSerializeBuffer& s) {
    gDeserialize(s,data);
  }

  void dump(std::ostream& os) const {
    os << "data: " << data;
  }

public:
  template<typename... Args>
  GraphNodeData(Args&&... args) :data(std::forward<Args...>(args...)) {}
  GraphNodeData() :data() {}

  NodeDataTy& getData() {
    return data;
  }
};

template<>
class GraphNodeData<void> {};

template<typename NHTy, typename EdgeDataTy, EdgeDirection EDir>
class GraphNodeEdges;

template<typename NHTy, typename EdgeDataTy>
class Edge {
  NHTy dst;
  EdgeDataTy val;
public:
  template<typename... Args>
  Edge(const NHTy& d, Args&&... args) :dst(d), val(std::forward<Args...>(args...)) {}

  Edge() {}

  NHTy getDst() { return dst; }
  EdgeDataTy& getValue() { return val; }

  typedef int tt_has_serialize;
  void serialize(Galois::Runtime::SerializeBuffer& s) const {
    gSerialize(s, dst, val);
  }
  void deserialize(Galois::Runtime::DeSerializeBuffer& s) {
    gDeserialize(s,dst, val);
  }

  void dump(std::ostream& os) const {
    os << "<{Edge: dst: ";
    dst.dump(os);
    os << " dst active: ";
    os << dst->getActive();
    os << " val: ";
    os << val;
    os << "}>";
  }
};

template<typename NHTy>
class Edge<NHTy, void> {
  NHTy dst;
public:
  Edge(const NHTy& d) :dst(d) {}
  Edge() {}

  NHTy getDst() { return dst; }

  typedef int tt_has_serialize;
  void serialize(Galois::Runtime::SerializeBuffer& s) const {
    gSerialize(s,dst);
  }
  void deserialize(Galois::Runtime::DeSerializeBuffer& s) {
    gDeserialize(s,dst);
  }

  void dump(std::ostream& os) const {
    os << "<{Edge: dst: ";
    dst.dump(os);
    os << " dst active: ";
    os << dst->getActive();
    os << "}>";
  }
};

template<typename NHTy, typename EdgeDataTy>
class GraphNodeEdges<NHTy, EdgeDataTy, EdgeDirection::Out> {
  typedef Edge<NHTy, EdgeDataTy> EdgeTy;
  typedef gdeque<EdgeTy,4> EdgeListTy;

  EdgeListTy edges;

protected:
  void serialize(Galois::Runtime::SerializeBuffer& s) const {
    gSerialize(s,edges);
  }
  void deserialize(Galois::Runtime::DeSerializeBuffer& s) {
    gDeserialize(s,edges);
  }
  void dump(std::ostream& os) const {
    os << "numedges: " << edges.size();
    for (decltype(edges.size()) x = 0; x < edges.size(); ++x) {
      os << " ";
      edges[x].dump(os);
    }
  }
 public:
  typedef typename EdgeListTy::iterator iterator;

  template<typename... Args>
  void createEdge(const NHTy& src, const NHTy& dst, Args&&... args) {
    edges.emplace_back(dst, std::forward<Args...>(args...));
  }

  void createEdge(const NHTy& src, const NHTy& dst) {
    edges.emplace_back(dst);
  }

  void clearEdges() {
    edges.clear();
  }

  iterator begin() {
    return edges.begin();
  }

  iterator end() {
    return edges.end();
  }
};

template<typename NHTy, typename EdgeDataTy>
class GraphNodeEdges<NHTy, EdgeDataTy, EdgeDirection::InOut> {
  //FIXME
};

template<typename NHTy>
class GraphNodeEdges<NHTy, void, EdgeDirection::Un> {
  typedef Edge<NHTy, void> EdgeTy;
  typedef gdeque<EdgeTy,4> EdgeListTy;

  EdgeListTy edges;

protected:
  void serialize(Galois::Runtime::SerializeBuffer& s) const {
    gSerialize(s,edges);
  }
  void deserialize(Galois::Runtime::DeSerializeBuffer& s) {
    gDeserialize(s,edges);
  }
  void dump(std::ostream& os) const {
    os << "numedges: " << edges.size();
    for (auto ii = edges.begin(), ee = edges.end(); ii != ee; ++ii) {
      os << " ";
      ii->dump(os);
    }
  }
 public:
  typedef typename EdgeListTy::iterator iterator;

  void createEdge(NHTy& src, NHTy& dest) {
    //assert(*src == this);
    dest->edges.emplace_back(src);
    edges.emplace_back(dest);
  }

  void clearEdges() {
    edges.clear();
  }

  iterator begin() {
    return edges.begin();
  }

  iterator end() {
    return edges.end();
  }
};

template<typename NHTy, typename EdgeDataTy>
class GraphNodeEdges<NHTy, EdgeDataTy, EdgeDirection::Un> {
  //FIXME
};


#define SHORTHAND Galois::Runtime::gptr<GraphNode<NodeDataTy, EdgeDataTy, EDir> >

template<typename NodeDataTy, typename EdgeDataTy, EdgeDirection EDir>
class GraphNode
  : public Galois::Runtime::Lockable,
    public GraphNodeBase<SHORTHAND >,
    public GraphNodeData<NodeDataTy>,
    public GraphNodeEdges<SHORTHAND, EdgeDataTy, EDir>
{
  friend class ThirdGraph<NodeDataTy, EdgeDataTy, EDir>;

  using GraphNodeBase<SHORTHAND >::getNextNode;

public:
  typedef SHORTHAND Handle;
  typedef typename Galois::Graph::Edge<SHORTHAND,EdgeDataTy> EdgeType;
  typedef typename GraphNodeEdges<SHORTHAND,EdgeDataTy,EDir>::iterator edge_iterator;

  template<typename... Args>
  GraphNode(Args&&... args) :GraphNodeData<NodeDataTy>(std::forward<Args...>(args...)) {}

  GraphNode() {}

  //serialize
  typedef int tt_has_serialize;
  void serialize(Galois::Runtime::SerializeBuffer& s) const {
    GraphNodeBase<SHORTHAND >::serialize(s);
    GraphNodeData<NodeDataTy>::serialize(s);
    GraphNodeEdges<SHORTHAND, EdgeDataTy, EDir>::serialize(s);
  }
  void deserialize(Galois::Runtime::DeSerializeBuffer& s) {
    GraphNodeBase<SHORTHAND >::deserialize(s);
    GraphNodeData<NodeDataTy>::deserialize(s);
    GraphNodeEdges<SHORTHAND, EdgeDataTy, EDir>::deserialize(s);
  }
  void dump(std::ostream& os) const {
    os << this << " ";
    os << "<{GN: ";
    GraphNodeBase<SHORTHAND >::dump(os);
    os << " ";
    GraphNodeData<NodeDataTy>::dump(os);
    os << " ";
    GraphNodeEdges<SHORTHAND, EdgeDataTy, EDir>::dump(os);
    os << "}>";
  }
};

#undef SHORTHAND

/**
 * A Graph
 *
 * @param NodeTy type of node data (may be void)
 * @param EdgeTy type of edge data (may be void)
 * @param IsDir  bool indicated if graph is directed
 *
*/
template<typename NodeTy, typename EdgeTy, EdgeDirection EDir>
class ThirdGraph { //: public Galois::Runtime::DistBase<ThirdGraph> {
  typedef GraphNode<NodeTy, EdgeTy, EDir> gNode;

  typename Bag<gNode>::pointer localStateStore;
  typename Bag<Runtime::gptr<gNode>>::pointer localStatePtr;
  Runtime::PerThreadDist<ThirdGraph> basePtr;

  struct is_edge : public std::unary_function<typename gNode::EdgeType&, bool> {
    bool operator()(typename gNode::EdgeType& n) const { return n.getDst()->getActive(); }
  };
  struct is_node: public std::unary_function<Runtime::gptr<gNode>&, bool>{
    bool operator() (const Runtime::gptr<gNode>& g) const { return g->getActive(); }
  };
  struct makePtrLocal: public std::unary_function<gNode&, Runtime::gptr<gNode>> {
    Runtime::gptr<gNode> operator()(gNode& data) const { return Runtime::gptr<gNode>(&data); }
  };

public:
  typedef typename gNode::Handle NodeHandle;
  //! Edge iterator
  typedef typename boost::filter_iterator<is_edge,typename gNode::edge_iterator> edge_iterator;

  template<typename... Args>
  NodeHandle createNode(Args&&... args) {
    NodeHandle N(&*localStateStore->emplace(std::forward<Args...>(args...)));
    localStatePtr->push(N);
    return N;
  }

  NodeHandle createNode() {
    NodeHandle N(&*localStateStore->emplace());
    localStatePtr->push(N);
    return N;
  }
  
  void addNode(NodeHandle& N) {
    N->setActive(true);
  }
  
  void removeNode(NodeHandle& N) {
    if (N->getActive()) {
      N->setActive(false);
      // delete all the edges (in the deque)
      N->clearEdges();
    }
  }

  //! Node iterator
  typedef boost::filter_iterator<is_node, typename Bag<Runtime::gptr<gNode>>::local_iterator> local_iterator;

  local_iterator local_begin() {
    return boost::make_filter_iterator<is_node>(localStatePtr->local_begin(), localStatePtr->local_end());
  }

  local_iterator local_end() {
    assert(localStatePtr->local_end() == localStatePtr->local_end());
    return boost::make_filter_iterator<is_node>(localStatePtr->local_end(), localStatePtr->local_end());
  }

  typedef boost::filter_iterator<is_node, typename Bag<Runtime::gptr<gNode>>::iterator> iterator;
  
  iterator begin() {
    return boost::make_filter_iterator<is_node>(localStatePtr->begin(), localStatePtr->end());
  }

  iterator end() {
    return boost::make_filter_iterator<is_node>(localStatePtr->end(), localStatePtr->end());
  }


  //! Returns an iterator to the neighbors of a node 
  edge_iterator edge_begin(NodeHandle N) {
    assert(N);
    // prefetch all the nodes
    for (auto ii = N->begin(), ee = N->end(); ii != ee; ++ii) {
      ii->getDst().prefetch();
    }
    // lock all the nodes
    for (auto ii = N->begin(), ee = N->end(); ii != ee; ++ii) {
      // NOTE: Andrew thinks acquire may be needed for inactive nodes too
      //       not sure why though. he had to do this in the prev graph
      if (ii->getDst()->getActive()) {
        // modify the call when local nodes aren't looked up in directory
	//        ii->getDst().acquire();
      }
    }
    return boost::make_filter_iterator(is_edge(), N->begin(), N->end());
  }

  //! Returns the end of the neighbor iterator 
  edge_iterator edge_end(NodeHandle N) {
    assert(N);
    return boost::make_filter_iterator(is_edge(), N->end(), N->end());
  }

  void addEdge(NodeHandle src, NodeHandle dst) {
    assert(src);
    assert(dst);
    src->createEdge(src, dst);
  }

  NodeHandle getEdgeDst(edge_iterator ii) {
    assert(ii->getDst()->getActive());
    return ii->getDst();
  }

  NodeTy& getData(const NodeHandle& N) {
    assert(N);
    return N->getData();
  }

  bool containsNode(const NodeHandle& N) {
    assert(N);
    return N->getActive();
  }

  size_t size() const { return 0; }

  typedef Runtime::PerThreadDist<ThirdGraph> pointer;
  static pointer allocate() {
    return Runtime::PerThreadDist<ThirdGraph>::allocate();
  }
  static void deallocate(pointer ptr) {
    Runtime::PerThreadDist<ThirdGraph>::deallocate(ptr);
  }

  explicit ThirdGraph(pointer p) :basePtr(p) {
    localStateStore = Bag<gNode>::allocate();
    localStatePtr = Bag<Runtime::gptr<gNode>>::allocate();
  }

  ThirdGraph(pointer p, Runtime::DeSerializeBuffer& buf) :basePtr(p) {
    gDeserialize(buf, localStateStore, localStatePtr);
    assert(localStateStore);
    assert(localStatePtr);
  }

  ~ThirdGraph() {
    Bag<gNode>::deallocate(localStateStore);
    Bag<Runtime::gptr<gNode>>::deallocate(localStatePtr);
  }

  void getInitData(Runtime::SerializeBuffer& buf) {
    gSerialize(buf, localStateStore, localStatePtr);
  }
};

// used to find the size of the graph
struct R : public Galois::Runtime::Lockable {
  unsigned i;

  R(): i(0) {}

  void add(unsigned v) {
    i += v;
    return;
  }

  typedef int tt_dir_blocking;

  // serialization functions
  typedef int tt_has_serialize;
  void serialize(Galois::Runtime::SerializeBuffer& s) const {
    gSerialize(s,i);
  }
  void deserialize(Galois::Runtime::DeSerializeBuffer& s) {
    gDeserialize(s,i);
  }
};

template <typename GTy>
struct f {
  GTy graph;
  Runtime::gptr<R> r;

  f(const Runtime::gptr<R>& p, GTy g): graph(g), r(p) {}
  f() {}

  template<typename Context>
  void operator()(unsigned x, Context& cnx) const {
    unsigned size = std::distance(graph->local_begin(),graph->local_end());
    r->add(size);
  }

  // serialization functions
  typedef int tt_has_serialize;
  void serialize(Galois::Runtime::SerializeBuffer& s) const {
    gSerialize(s,r);
    gSerialize(s,graph);
  }
  void deserialize(Galois::Runtime::DeSerializeBuffer& s) {
    gDeserialize(s,r);
    gDeserialize(s,graph);
  }
};

template <typename GraphTy>
unsigned ThirdGraphSize(GraphTy g) {
  // should only be called from outside the for_each
  assert(!Galois::Runtime::inGaloisForEach);
  R tmp;
  Runtime::gptr<R> r(&tmp);
  Galois::on_each(f<GraphTy>(r,g));
  return r->i;
}

struct ThirdGraph_for_size {
  template<typename T>
  bool operator()(T& n) const { return true; }
};

template <typename GraphTy>
ptrdiff_t NThirdGraphSize(GraphTy g) {
  // should only be called from outside the for_each
  assert(!Galois::Runtime::inGaloisForEach);
  return Galois::ParallelSTL::count_if_local(g,ThirdGraph_for_size());
}

} //namespace Graph
} //namespace Galois

#endif
