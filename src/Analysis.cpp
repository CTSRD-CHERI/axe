#include <stdlib.h>
#include <stdio.h>
#include "Analysis.h"

// ===========
// Constructor
// ===========

Analysis::Analysis(Trace* t, Seq<Edge>* es)
{
  trace = t;
  graph = new Graph(trace->numInstrs);
  for (int i = 0; i < es->numElems; i++) {
    Edge e = es->elems[i];
    graph->addEdge(e.src, e.dst);
  }
  nextLoad = new InstrId* [trace->numInstrs];
  for (int i = 0; i < trace->numInstrs; i++)
    nextLoad[i] = new InstrId [trace->numThreads*trace->numAddrs];
  nextStore = new InstrId* [trace->numInstrs];
  for (int i = 0; i < trace->numInstrs; i++)
    nextStore[i] = new InstrId [trace->numThreads*trace->numAddrs];
}

// ==========
// Destructor
// ==========

Analysis::~Analysis()
{
  delete graph;
  for (int i = 0; i < trace->numInstrs; i++)
    delete nextLoad[i];
  delete [] nextLoad;
  for (int i = 0; i < trace->numInstrs; i++)
    delete nextStore[i];
  delete [] nextStore;
}

// =====================================
// Propagate next loads/stores one level
// =====================================

inline bool Analysis::update(int* a, int b)
{
  // This update can be undone as it is
  // performed via the backtracking class
  if (*a <= b) return false;
  back.write(a, b);
  return true;
}

inline bool updateFast(int* a, int b)
{
  // This update is performed directly
  // and is not undone on backtracking
  if (*a <= b) return false;
  *a = b;
  return true;
}

void Analysis::propagateInstr(InstrId from, InstrId to)
{
  Instr instr = trace->instrs[from];
  int idx = instr.tid*trace->numAddrs+instr.addr;
  if (instr.op == LD || instr.op == RMW)
    update(&nextLoad[to][idx], from);
  if (instr.op == ST || instr.op == RMW)
    update(&nextStore[to][idx], from);
}

bool Analysis::propagateNext(InstrId from, InstrId to)
{
  bool ch = false;
  int idxTop = trace->numThreads*trace->numAddrs;
  if (back.stack.numElems == 0) {
    // If no checkpoint has been created, do fast update
    for (int idx = 0; idx < idxTop; idx++) {
      ch = updateFast(&nextLoad[to][idx], nextLoad[from][idx]) || ch;
      ch = updateFast(&nextStore[to][idx], nextStore[from][idx]) || ch;
    }
  }
  else {
    // Otherwise, do a backtrackable update
    for (int idx = 0; idx < idxTop; idx++) {
      ch = update(&nextLoad[to][idx], nextLoad[from][idx]) || ch;
      ch = update(&nextStore[to][idx], nextStore[from][idx]) || ch;
    }
  }
  return ch;
}

// =========================
// Compute next stores/loads
// =========================

bool Analysis::computeNext()
{
  bool ok;
  Seq<InstrId> nodes;
  Seq<InstrId> in(64);
  ok = graph->revTopSort(&nodes);
  if (!ok) return false;

  // Initialise
  for (int i = 0; i < trace->numInstrs; i++)
    for (int j = 0; j < trace->numAddrs * trace->numThreads; j++) {
      nextLoad[i][j]  = trace->numInstrs;
      nextStore[i][j] = trace->numInstrs;
    }

  // Backward propagation
  for (int i = 0; i < nodes.numElems; i++) {
    InstrId n = nodes.elems[i];
    graph->incoming(n, &in);
    for (int j = 0; j < in.numElems; j++) {
      InstrId next = in.elems[j];
      propagateInstr(n, next);
      propagateNext(n, next);
    }
  }

  return true;
}

bool Analysis::existsPath(InstrId src, InstrId dstStore)
{
  Instr dstInstr = trace->instrs[dstStore];
  int idx = dstInstr.tid * trace->numAddrs + dstInstr.addr;
  return nextStore[src][idx] <= dstStore;
}

void Analysis::inferFrom(InstrId src, Seq<Edge>* inferred)
{
  Instr instr = trace->instrs[src];
  if (instr.op == ST || instr.op == RMW) {
    for (int t = 0; t < trace->numThreads; t++) {
      int idx = t*trace->numAddrs+instr.addr;
      InstrId store = nextStore[src][idx];
      if (store < trace->numInstrs) {
        Seq<InstrId>* loads = &trace->readsFromInv[src];
        for (int i = 0; i < loads->numElems; i++) {
          InstrId load = loads->elems[i];
          if (load != store)
            if (! existsPath(load, store))
              inferred->append(edge(load, store));
        }
      }

      InstrId load = nextLoad[src][idx];
      if (load < trace->numInstrs) {
        InstrId s = trace->readsFrom[load];
        while (s >= 0 && s == src) {
          load = trace->nextLocalLoad[load];
          if (load < 0) break;
          s = trace->readsFrom[load];
        }
        if (s >= 0 && src != s)
          if (! existsPath(src, s))
            inferred->append(edge(src, s));
      }
    }
  }
}

// ========
// Add edge
// ========

bool Analysis::addEdgeHelper(Edge e, Seq<Edge>* inferred)
{
  Seq<InstrId> stack(64);
  Seq<InstrId> in(64);

  if (graph->outEdges[e.src].member(e.dst)) return true;
  back.addEdge(graph, e);
  propagateInstr(e.dst, e.src);
  propagateNext(e.dst, e.src);
  stack.push(e.src);

  while (stack.numElems > 0) {
    InstrId node = stack.pop();
    inferFrom(node, inferred);
    if (node == e.dst) return false; // Cycle
    graph->incoming(node, &in);
    for (int i = 0; i < in.numElems; i++) {
      bool change = propagateNext(node, in.elems[i]);
      if (change) stack.push(in.elems[i]);
    }
  }

  return true;
}

bool Analysis::addEdge(Edge e)
{
  Seq<Edge> inferred;

  if (! addEdgeHelper(e, &inferred)) return false;
  while (inferred.numElems > 0) {
    Edge e = inferred.pop();
    if (! addEdgeHelper(e, &inferred)) return false;
  }

  return true;
}

// ===============
// Infer new edges
// ===============

bool Analysis::inferEdges()
{
  Seq<Edge> inferred;
  for (int i = 0; i < trace->numInstrs; i++) {
    Instr instr = trace->instrs[i];
    if (instr.op == ST || instr.op == RMW)
      inferFrom(instr.uid, &inferred);
  }

  for (int i = 0; i < inferred.numElems; i++)
    if (! addEdge(inferred.elems[i]))
      return false;

  return true;
}

// =======
// Checker
// =======

// Remove a root and update the list of roots.

void Analysis::delRoot(InstrId root, Seq<InstrId>* roots, InstrId* lastStore)
{
  SmallSeq<InstrId> in, out;

  graph->outgoing(root, &out);
  back.delNode(graph, root);
  back.delRoot(roots, root);

  // Update roots
  for (int i = 0; i < out.numElems; i++) {
    graph->incoming(out.elems[i], &in);
    if (in.numElems == 0) back.addRoot(roots, out.elems[i]);
  }

  // Update most recent store
  Instr instr = trace->instrs[root];
  if (instr.op == ST || instr.op == RMW)
    back.write(&lastStore[instr.tid*trace->numAddrs + instr.addr], root);
}

// Introduce new edges when a store is performed.

bool Analysis::performStore(
       Instr instr,
       Seq<InstrId>* roots,
       InstrId* lastStore
     )
{
  SmallSeq<InstrId> in, drop;
  Seq<InstrId>* loads = &trace->readsFromInv[instr.uid];

  for (int t = 0; t < trace->numThreads; t++) {
    InstrId last = lastStore[t*trace->numAddrs + instr.addr];
    InstrId store;
    if (last < 0)
      store = trace->firstStore[instr.addr][t];
    else
      store = trace->nextLocalStore[last];
    if (store >= 0) {
      bool added = false;
      for (int i = 0; i < loads->numElems; i++) {
        InstrId load = loads->elems[i];
        if (graph->present[load] && load != store) {
          Edge e;
          e.src = load;
          e.dst = store;
          if (! addEdge(e)) return false;
          added = true;
        }
      }
      if (added) {
        drop.clear();
        for (int i = 0; i < roots->numElems; i++) {
          InstrId r = roots->elems[i];
          graph->incoming(r, &in);
          if (in.numElems > 0) drop.append(r);
        }
        for (int i = 0; i < drop.numElems; i++)
          back.delRoot(roots, drop.elems[i]);
      }
    }
  }
  return true;
}

// Consume nodes deterministically.

void Analysis::consume(
       int* count
     , Seq<InstrId>* roots
     , InstrId* lastStore
     )
{
  SmallSeq<InstrId> in;
  bool change = true;
  while (change) {
    change = false;
    for (int i = 0; i < roots->numElems; i++) {
      Instr r = trace->instrs[roots->elems[i]];
      if (r.op == LD || r.op == SYNC) {
        delRoot(r.uid, roots, lastStore);
        back.write(count, *count+1);
        change = true;
        break;
      }
      else if (r.op == ST || r.op == RMW) {
        Seq<InstrId>* loads = &trace->readsFromInv[r.uid];
        if (loads->numElems == 0) {
          delRoot(r.uid, roots, lastStore);
          back.write(count, *count+1);
          change = true;
          break;
        }
      }
    }
  }
}

bool Analysis::check()
{
  // Most-recently-performed store
  InstrId* lastStore = new InstrId [trace->numThreads*trace->numAddrs];
  for (int i = 0; i < trace->numThreads * trace->numAddrs; i++)
    lastStore[i] = -1;

  // Count of number of nodes removed.
  int count = 0;

  // Stack
  Seq<InstrId> stack;

  // Compute initial roots
  SmallSeq<NodeId> rs;
  graph->roots(&rs);
  consume(&count, &rs, lastStore);
  for (int i = 0; i < rs.numElems; i++)
    stack.push(rs.elems[i]);

  while (stack.numElems > 0 && count < trace->numInstrs) {
    InstrId node = stack.pop();
    if (node < 0) {
      back.backtrack();
    }
    else {
      back.checkpoint();
      delRoot(node, &rs, lastStore);
      back.write(&count, count+1);
      if (! performStore(trace->instrs[node], &rs, lastStore)) {
        back.backtrack();
        continue;
      }
      consume(&count, &rs, lastStore);
      stack.push(-1);
      for (int i = 0; i < rs.numElems; i++)
        stack.push(rs.elems[i]);
    }
  }
  
  delete [] lastStore;
  return count == trace->numInstrs;
}
