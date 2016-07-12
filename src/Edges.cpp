#include "Edges.h"
#include <assert.h>

// ================
// Edge constructor
// ================

Edge edge(InstrId src, InstrId dst)
{
  Edge e;
  e.src = src;
  e.dst = dst;
  return e;
}

// ==============
// Local SC edges
// ==============

void localSCEdges(Trace* trace, Seq<Edge>* result)
{
  for (int t = 0; t < trace->numThreads; t++) {
    InstrId prev = -1;

    for (int i = 0; i < trace->threads[t].numElems; i++) {
      InstrId me = trace->threads[t].elems[i];

      if (prev >= 0)
        result->append(edge(prev, me));

      prev = me;
    }
  }
}

// ===============
// Local TSO edges
// ===============

void localTSOEdges(Trace* trace, Seq<Edge>* result)
{
  for (int t = 0; t < trace->numThreads; t++) {
    InstrId prevLoad  = -1;
    InstrId prevStore = -1;
    InstrId prevSync  = -1;

    for (int i = 0; i < trace->threads[t].numElems; i++) {
      InstrId me = trace->threads[t].elems[i];
      Op op = trace->instrs[me].op;

      if (op == LD || op == RMW) {
        if (prevLoad >= 0)
          result->append(edge(prevLoad, me));
        if (prevSync >= 0 && prevLoad < 0)
          result->append(edge(prevSync, me));
      }

      if (op == ST || op == RMW) {
        if (prevStore >= 0)
          result->append(edge(prevStore, me));
      }

      if (op == ST) {
        if (prevLoad >= 0 && prevLoad != prevStore)
          result->append(edge(prevLoad, me));
        if (prevSync >= 0 && prevLoad < 0 && prevStore < 0)
          result->append(edge(prevSync, me));
      }
  
      if (op == SYNC) {
        if (prevLoad >= 0)
          result->append(edge(prevLoad, me));
        if (prevStore >= 0 && prevLoad != prevStore)
          result->append(edge(prevStore, me));
        if (prevSync >= 0 && prevLoad < 0 && prevStore < 0)
          result->append(edge(prevSync, me));
      }
  
      if (op == LD || op == RMW)
        prevLoad = me;
  
      if (op == ST || op == RMW)
        prevStore = me;
  
      if (op == SYNC) {
        prevLoad = prevStore = -1;
        prevSync = me;
      }
    }
  }
}

// ===============
// Local PSO edges
// ===============

void localPSOEdges(Trace* trace, Seq<Edge>* result)
{
  InstrId* prevStore = new InstrId [trace->numAddrs];

  for (int t = 0; t < trace->numThreads; t++) {
    InstrId prevLoad  = -1;
    InstrId prevSync  = -1;
    for (int a = 0; a < trace->numAddrs; a++)
      prevStore[a] = -1;

    for (int i = 0; i < trace->threads[t].numElems; i++) {
      InstrId me = trace->threads[t].elems[i];
      Op op = trace->instrs[me].op;
      Addr addr = trace->instrs[me].addr;

      if (op == LD || op == RMW) {
        if (prevLoad >= 0)
          result->append(edge(prevLoad, me));
        if (prevSync >= 0 && prevLoad < 0)
          result->append(edge(prevSync, me));
      }

      if (op == ST || op == RMW) {
        if (prevStore[addr] >= 0)
          result->append(edge(prevStore[addr], me));
      }

      if (op == ST) {
        if (prevLoad >= 0 && prevLoad != prevStore[addr])
          result->append(edge(prevLoad, me));
        if (prevSync >= 0 && prevLoad < 0 && prevStore[addr] < 0)
          result->append(edge(prevSync, me));
      }
  
      if (op == SYNC) {
        if (prevLoad >= 0)
          result->append(edge(prevLoad, me));
        for (int a = 0; a < trace->numAddrs; a++)
          if (prevStore[a] >= 0 && prevLoad != prevStore[a])
            result->append(edge(prevStore[a], me));
        if (prevSync >= 0 && prevLoad < 0)
          result->append(edge(prevSync, me));
      }
  
      if (op == LD || op == RMW)
        prevLoad = me;
  
      if (op == ST || op == RMW)
        prevStore[addr] = me;
  
      if (op == SYNC) {
        prevLoad = -1;
        for (int a = 0; a < trace->numAddrs; a++)
          prevStore[a] = -1;
        prevSync = me;
      }
    }
  }

  delete [] prevStore;
}

// ===============
// Local WMO edges
// ===============

void localWMOEdges(Trace* trace, Seq<Edge>* result)
{
  InstrId* prevStore = new InstrId [trace->numAddrs];
  InstrId* prevLoad  = new InstrId [trace->numAddrs];

  for (int t = 0; t < trace->numThreads; t++) {
    InstrId prevSync  = -1;
    for (int a = 0; a < trace->numAddrs; a++)
      prevStore[a] = prevLoad[a] = -1;

    for (int i = 0; i < trace->threads[t].numElems; i++) {
      InstrId me = trace->threads[t].elems[i];
      Op op = trace->instrs[me].op;
      Addr addr = trace->instrs[me].addr;

      if (op == LD || op == RMW) {
        if (prevLoad[addr] >= 0)
          result->append(edge(prevLoad[addr], me));
        if (prevSync >= 0 && prevLoad[addr] < 0)
          result->append(edge(prevSync, me));
      }

      if (op == ST || op == RMW) {
        if (prevStore[addr] >= 0)
          result->append(edge(prevStore[addr], me));
      }

      if (op == ST) {
        if (prevLoad[addr] >= 0 && prevLoad[addr] != prevStore[addr])
          result->append(edge(prevLoad[addr], me));
        if (prevSync >= 0 && prevStore[addr] < 0)
          result->append(edge(prevSync, me));
      }
  
      if (op == SYNC) {
        for (int a = 0; a < trace->numAddrs; a++) {
          if (prevLoad[a] >= 0)
            result->append(edge(prevLoad[a], me));
          if (prevStore[a] >= 0 && prevLoad[a] != prevStore[a])
            result->append(edge(prevStore[a], me));
        }
        if (prevSync >= 0)
          result->append(edge(prevSync, me));
      }
  
      if (op == LD || op == RMW)
        prevLoad[addr] = me;
  
      if (op == ST || op == RMW)
        prevStore[addr] = me;
  
      if (op == SYNC) {
        for (int a = 0; a < trace->numAddrs; a++)
          prevLoad[a] = prevStore[a] = -1;
        prevSync = me;
      }
    }
  }

  delete [] prevStore;
  delete [] prevLoad;
}

// ==================
// Local dependencies
// ==================

// Compare timestamps.  True if t0 is known to come after t1.

static inline bool after(Time t0, Time t1)
{
  if (t0 >= 0 && t1 >= 0)
    return (t0 > t1);
  else
    return false;
}

// Keep track of most recent operations known to have finished.

static void addFinished(
         Instr newInstr,
         Seq<Instr>* finished,
         Seq<Edge>* edges)
{
  Seq<Instr> fin(finished->numElems+1);

  for (int i = 0; i < finished->numElems; i++) {
    Instr instr = finished->elems[i];
    if (after(newInstr.beginTime, instr.endTime))
      edges->append(edge(instr.uid, newInstr.uid));
    else
      fin.append(instr);
  }

  finished->clear();
  finished->append(newInstr);
  for (int i = 0; i < fin.numElems; i++)
    finished->append(fin.elems[i]);
}

// Keep track of in-flight operations, i.e. operations that are not
// yet known to have finished.

class InFlight {
  private:
    Time mostRecentStartTime;
    SmallSeq<Instr> finished;
    SmallSeq<Instr> inFlight;
    
  public:
    InFlight();
    void insert(Instr instr, Seq<Edge>* edges);
};

InFlight::InFlight() { mostRecentStartTime = -1; }

void InFlight::insert(Instr newInstr, Seq<Edge>* edges)
{
  Seq<Instr> in(inFlight.numElems+1);

  mostRecentStartTime =
      newInstr.beginTime < 0
    ? mostRecentStartTime : newInstr.beginTime;

  for (int i = 0; i < inFlight.numElems; i++) {
    Instr instr = inFlight.elems[i];
    if (after(mostRecentStartTime, instr.endTime))
      addFinished(instr, &finished, edges);
    else
      in.append(instr);
  }

  for (int i = 0; i < finished.numElems; i++)
    edges->append(edge(finished.elems[i].uid, newInstr.uid));

  inFlight.clear();
  if (newInstr.endTime >= 0)
    inFlight.append(newInstr);
  for (int i = 0; i < in.numElems; i++)
    inFlight.append(in.elems[i]);
}

// Now compute local dependencies using timestamps (if available).

void localDepEdges(Trace* trace, Seq<Edge>* result)
{
  for (int t = 0; t < trace->numThreads; t++) {
    InFlight inFlight;
    for (int i = 0; i < trace->threads[t].numElems; i++) {
      Instr instr = trace->instrs[trace->threads[t].elems[i]];
      if (instr.op != SYNC)
        inFlight.insert(instr, result);
    }
  }
}

// ==================
// Inter-thread edges
// ==================

void interEdges(Trace* trace, Seq<Edge>* result)
{
  for (int i = 0; i < trace->numInstrs; i++) {
    Instr instr = trace->instrs[i];
    if (instr.op == LD || instr.op == RMW) {
      InstrId store = trace->readsFrom[instr.uid];
      if (store >= 0 && trace->instrs[store].tid != instr.tid) {
        result->append(edge(store, instr.uid));
        //InstrId next = trace->nextLocalStore[store];
        //if (next >= 0) result->append(edge(instr.uid, next));
        InstrId prev = trace->prevLocalStore[instr.uid];
        if (prev >= 0) result->append(edge(prev, store));
      }
    }
  }
}

// ===================
// Initial-value edges
// ===================

// For each load of value '0', add edge to first store to the load's
// address on each thread.

void initialValueEdges(Trace* trace, Seq<Edge>* result)
{
  for (int i = 0; i < trace->numInstrs; i++) {
    Instr instr = trace->instrs[i];
    if (instr.op == LD || instr.op == RMW)
      if (instr.readVal == 0)
        for (int t = 0; t < trace->numThreads; t++) {
          InstrId store = trace->firstStore[instr.addr][t];
          if (store >= 0 && store != instr.uid)
            result->append(edge(instr.uid, store));
        }
  }
}

// ========================
// Locally-consistent edges
// ========================

// For each load of a value V from address A, add an edge to this load
// from the previous local store to A if that store wrote a value
// different than V.

void locallyConsistentEdges(Trace* trace, Seq<Edge>* result)
{
  for (int i = 0; i < trace->numInstrs; i++) {
    Instr instr = trace->instrs[i];
    if (instr.op == LD || instr.op == RMW) {
      InstrId prev = trace->prevLocalStore[instr.uid];
      if (prev >= 0) {
        Instr prevInstr = trace->instrs[prev];
        if (prevInstr.writeVal != instr.readVal)
          result->append(edge(prev, instr.uid));
      }
    }
  }
}

// =================
// Final-value edges
// =================

// For emulating litmus tests, traces can contain constraints on the
// final values of certain variables in global memory.

void finalValueEdges(Trace* trace, Seq<Edge>* result)
{
  for (int i = 0; i < trace->finals.numElems; i++) {
    Instr finalInstr = trace->finals.elems[i];
    for (int t = 0; t < trace->numThreads; t++) {
      InstrId store = trace->finalStore[finalInstr.addr][t];
      if (store >= 0) {
        if (finalInstr.readVal == 0) {
          // Final value cannot be 0 if there exists a store:
          // fail by adding deliberate cycle.
          result->append(edge(store, store));
          return;
        }
        if (store != finalInstr.uid)
          result->append(edge(store, finalInstr.uid));
      }
    }
  }
}
