/** Galois Network Layer -*- C++ -*-
 * @file
 * @section License
 *
 * Galois, a framework to exploit amorphous data-parallelism in irregular
 * programs.
 *
 * Copyright (C) 2012, The University of Texas at Austin. All rights reserved.
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
 * @author Manoj Dhanapal <madhanap@cs.utexas.edu>
 * @author Andrew Lenharth <andrewl@lenharth.org>
 */

#ifndef GALOIS_RUNTIME_NETWORK_H
#define GALOIS_RUNTIME_NETWORK_H

#include "Galois/Runtime/Serialize.h"
#include "Galois/Runtime/Context.h"

#include <cstdint>
#include <tuple>
#include <unordered_map>

namespace Galois {
namespace Runtime {
extern bool inDoAllDistributed;

extern uint32_t networkHostID;
extern uint32_t networkHostNum;

typedef SerializeBuffer SendBuffer;
typedef DeSerializeBuffer RecvBuffer;

typedef void (*recvFuncTy)(RecvBuffer&);

class NetworkInterface {
public:
  virtual ~NetworkInterface();

  //!send a message to a given (dest) host.  A message is simply a
  //!landing pad (recv) and some data (buf)
  //! buf is invalidated by this operation
  virtual void send(uint32_t dest, recvFuncTy recv, SendBuffer& buf) = 0;

  //!send a message to all hosts.  A message is simply a
  //!landing pad (recv) and some data (buf)
  //! buf is invalidated by this operation
  virtual void broadcast(recvFuncTy recv, SendBuffer& buf, bool self = false);

  //! send a message letting the network handle the serialization and deserialization
  //! slightly slower
  template<typename... Args>
  void sendAlt(uint32_t dest, void (*recv)(Args...), Args... param);

  //! broadcast a mssage allowing the network to handle serialization and deserialization
  template<typename... Args>
  void broadcastAlt(void (*recv)(Args...), Args... param);

  //!system barrier. all hosts should synchronize at this call
  virtual void systemBarrier() = 0;

  //!receive and dispatch messages
  //!returns true if at least one message was received
  //! if the network requires a dedicated thread, then 
  //!it is only valid for that thread to call this function
  virtual bool handleReceives() = 0;

  //! does this network layer need a dedicated thread?
  //! if false, then any thread can call send or receive and work will get done.
  //! if true, then only the master thread can process sends and receives
  //! if true, handleReceives will process pending sends
  virtual bool needsDedicatedThread() = 0;

};

NetworkInterface& getSystemNetworkInterface();

//!calls handleReceives on the worker threads
void networkStart();

//!terminate a distributed program
//! only the master host should call this
void networkTerminate();

//! Distributed barrier
//FIXME: See why this exists
void distWait();


////////////////////////////////////////////////////////////////////////////////
// objectRecord - used to store the requested remote objects
////////////////////////////////////////////////////////////////////////////////

// Not thread-safe make sure the caller takes care to lock before access to any
// method except empty or size
class ObjectRecord {
private:
  typedef std::multimap<Lockable*,std::function<void ()> > Map;

  Galois::Runtime::LL::SimpleLock<true> Lock;
  Map      objStore;
  unsigned num;

public:
  typedef std::function<void ()> FType;

  void insert(std::pair<Lockable*,FType>& val) {
    // multiple threads call insert so locking before use
    lock();
    objStore.insert(val);
    num = objStore.size();
    unlock();
  }

  void erase(Lockable* ptr) {
    // locking as it may be called along with insert
    lock();
    objStore.erase(ptr);
    num = objStore.size();
    unlock();
  }

  void clear() {
    objStore.clear();
    num = 0;
  }

  bool empty() {
    bool emp = (num == 0);
    return emp;
  }

  unsigned size() {
    return num;
  }

  bool find(Lockable* ptr) {
    typename Map::iterator got = objStore.find(ptr);
    bool emp = (got != objStore.end());
    return emp;
  }

  void lock() {
    Lock.lock();
  }

  void unlock() {
    Lock.unlock();
  }

  Map::iterator begin() {
    return objStore.begin();
  }

  Map::iterator end() {
    return objStore.end();
  }

  Map::iterator get_begin(Lockable* ptr) {
    return objStore.lower_bound(ptr);
  }

  Map::iterator get_end(Lockable* ptr) {
    return objStore.upper_bound(ptr);
  }

  unsigned get_count(Lockable* ptr) {
    unsigned got = objStore.count(ptr);
    return got;
  }
};

ObjectRecord& getSystemRemoteObjects();

////////////////////////////////////////////////////////////////////////////////
// Implementations
////////////////////////////////////////////////////////////////////////////////
template<typename... Args>
struct genericLandingPad {
  template<int ...> struct seq {};
  template<int N, int ...S> struct gens : gens<N-1, N-1, S...> {};
  template<int ...S> struct gens<0, S...>{ typedef seq<S...> type; };
  
  template<int ...S>
  static void callFunc(void (*fp)(Args...), std::tuple<Args...>& args, seq<S...>)
  {
    return fp(std::get<S>(args) ...);
  }

  //do this the new fancy way
  static void func(RecvBuffer& buf) {
    void (*fp)(Args...);
    std::tuple<Args...> args;
    gDeserialize(buf, fp, args);
    callFunc(fp, args, typename gens<sizeof...(Args)>::type());
  }
};

template<typename... Args>
void NetworkInterface::sendAlt(uint32_t dest, void (*recv)(Args...), Args... param) {
  SendBuffer buf;
  gSerialize(buf, recv, param...);
  send(dest, genericLandingPad<Args...>::func, buf);
}

template<typename... Args>
void NetworkInterface::broadcastAlt(void (*recv)(Args...), Args... param) {
  SendBuffer buf;
  gSerialize(buf, recv, param...);
  broadcast(genericLandingPad<Args...>::func, buf);
}

} //Runtime
} //Galois
#endif
