#include <stdio.h>
#include "ValOrder.h"
#include "Instr.h"
#include "Edges.h"

// ===========
// Constructor
// ===========

ValOrder::ValOrder(Trace* t)
{
  trace = t;

  valOrders = new Graph* [trace->numAddrs];
  for (int a = 0; a < trace->numAddrs; a++)
    valOrders[a] = new Graph(trace->numData[a]);

  next = new Data** [trace->numAddrs];
  for (int a = 0; a < trace->numAddrs; a++) {
    next[a] = new Data* [trace->numData[a]];
    for (int d = 0; d < trace->numData[a]; d++)
      next[a][d] = new Data [trace->numThreads];
  }

  atomicRtoW = new Data* [trace->numAddrs];
  atomicWtoR = new Data* [trace->numAddrs];
  storers = new ThreadId* [trace->numAddrs];
  for (int a = 0; a < trace->numAddrs; a++) {
    atomicRtoW[a] = new Data [trace->numData[a]];
    atomicWtoR[a] = new Data [trace->numData[a]];
    storers[a] = new ThreadId [trace->numData[a]];
  }
  opOrder = new Graph(trace->numInstrs);
  localOpOrder = new Graph(trace->numInstrs);

  computeStorers();
  createSyncGraph();
}

// =============
// Deconstructor
// =============

ValOrder::~ValOrder()
{
  for (int a = 0; a < trace->numAddrs; a++) {
    delete valOrders[a];
    for (int d = 0; d < trace->numData[a]; d++)
      delete [] next[a][d];
    delete [] next[a];
    delete [] atomicRtoW[a];
    delete [] atomicWtoR[a];
    delete [] storers[a];
  }
  delete [] next;
  delete [] atomicRtoW;
  delete [] atomicWtoR;
  delete [] storers;
  delete [] syncId;
  delete syncGraph;
  delete opOrder;
  delete localOpOrder;
}

// ===============
// Compute storers
// ===============

// Determine the writer thread of each value to each address.

void ValOrder::computeStorers()
{
  for (int a = 0; a < trace->numAddrs; a++)
    storers[a][0] = 0;

  for (int i = 0; i < trace->numInstrs; i++) {
    Instr instr = trace->instrs[i];
    if (instr.op == ST || instr.op == RMW)
      storers[instr.addr][instr.writeVal] = instr.tid;
  }
}

// ===================
// Create 'sync' graph
// ===================

// Allocate space for a graph where nodes are sync instructions, and
// a mapping from instruction ids to node ids.

void ValOrder::createSyncGraph()
{
  syncId = new NodeId [trace->numInstrs];
  int numSyncs = 0;
  for (int i = 0; i < trace->numInstrs; i++) {
    Instr instr = trace->instrs[i];
    if (instr.op == SYNC) syncId[i] = numSyncs++;
    else syncId[i] = -1;
  }
  syncGraph = new Graph(trace->numSyncs);
}

// =========================================
// Propagate next values one level backwards
// =========================================

inline bool ValOrder::update(int* a, int b)
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

void ValOrder::propagateData(Addr a, Data from, Data to)
{
  ThreadId t = storers[a][from];
  update(&next[a][to][t], from);
}

bool ValOrder::propagateNext(Addr a, Data from, Data to)
{
  bool ch = false;
  if (back.stack.numElems == 0) {
    // If no checkpoint has been created, do fast update
    for (int t = 0; t < trace->numThreads; t++)
      ch = updateFast(&next[a][to][t], next[a][from][t]) || ch;
  }
  else {
    // Otherwise, do a backtrackable update
    for (int t = 0; t < trace->numThreads; t++)
      ch = update(&next[a][to][t], next[a][from][t]) || ch;
  }
  return ch;
}

// ============
// Compute next
// ============

// For each address, compute nearest value successor on each thread.

bool ValOrder::computeNext()
{
  bool ok;
  Seq<Data> nodes;
  Seq<Data> in(64);

  for (int a = 0; a < trace->numAddrs; a++) {
    ok = valOrders[a]->revTopSort(&nodes);
    if (!ok) return false;

    // Initialise
    for (int d = 0; d < trace->numData[a]; d++)
      for (int t = 0; t < trace->numThreads; t++)
        next[a][d][t] = trace->numData[a];

    // Backward propagation
    for (int i = 0; i < nodes.numElems; i++) {
      Data d = nodes.elems[i];
      valOrders[a]->incoming(d, &in);
      for (int j = 0; j < in.numElems; j++) {
        Data next = in.elems[j];
        propagateData(a, d, next);
        propagateNext(a, d, next);
      }
    }
  }

  return true;
}

bool ValOrder::existsPath(Addr a, Data src, Data dst)
{
  ThreadId t = storers[a][dst];
  return next[a][src][t] <= dst;
}

// ========
// Add edge
// ========

// Add an edge but don't update the nearest successors and don't
// worry about backtracking.

void ValOrder::addEdgeFast(Addr a, Data from, Data to)
{
  if (from == to) return;
  if (existsPath(a, from, to)) return;
  if (atomicRtoW[a][from] >= 0) from = atomicRtoW[a][from];
  if (atomicWtoR[a][to] >= 0) to = atomicWtoR[a][to];
  if (from == to) return;
  // If from is a 'final' value then add deliberate cycle
  if (trace->finalVals[a] == from) valOrders[a]->addEdge(from, from);
  valOrders[a]->addEdge(from, to);
}

// Add an edge, updating nearest successors, and allowing
// backtracking.

bool ValOrder::addEdge(Addr a, Data from, Data to)
{
  if (from == to) return true;
  if (existsPath(a, from, to)) return true;
  if (existsPath(a, to, from)) return false;
  if (atomicRtoW[a][from] >= 0) from = atomicRtoW[a][from];
  if (atomicWtoR[a][to] >= 0) to = atomicWtoR[a][to];
  if (from == to) return true;
  if (trace->finalVals[a] == from) return false;

  Seq<Data> stack(64);
  Seq<Data> in(64);

  back.addEdge(valOrders[a], edge(from, to));
  propagateData(a, to, from);
  propagateNext(a, to, from);
  stack.push(from);

  while (stack.numElems > 0) {
    Data node = stack.pop();
    if (node == to) return false; // Cycle
    valOrders[a]->incoming(node, &in);
    for (int i = 0; i < in.numElems; i++) {
      bool change = propagateNext(a, node, in.elems[i]);
      if (change) stack.push(in.elems[i]);
    }
  }

  return true;
}

// Add edges between values seen before/after given instructions.

void ValOrder::addEdgesFast(InstrId from, InstrId to)
{
  if (from < 0 || to < 0) return;

  int baseFrom = from * trace->numAddrs;
  int baseTo   = to * trace->numAddrs;

  for (int a = 0; a < trace->numAddrs; a++) {
    Data d0 = trace->prevSeen[baseFrom+a];
    Data d1 = trace->nextSeen[baseTo+a];
    if (d0 < 0 || d1 < 0) continue;
    addEdgeFast(a, d0, d1);
  }
}

bool ValOrder::addEdges(InstrId from, InstrId to)
{
  if (from < 0 || to < 0) return true;

  int baseFrom = from * trace->numAddrs;
  int baseTo   = to * trace->numAddrs;

  for (int a = 0; a < trace->numAddrs; a++) {
    Data d0 = trace->prevSeen[baseFrom+a];
    Data d1 = trace->nextSeen[baseTo+a];
    if (d0 < 0 || d1 < 0) continue;
    if (! addEdge(a, d0, d1)) return false;
  }

  return true;
}

bool ValOrder::edgesExist(InstrId from, InstrId to)
{
  if (from < 0 || to < 0) return true;

  int baseFrom = from * trace->numAddrs;
  int baseTo   = to * trace->numAddrs;

  for (int a = 0; a < trace->numAddrs; a++) {
    Data d0 = trace->prevSeen[baseFrom+a];
    Data d1 = trace->nextSeen[baseTo+a];
    if (d0 < 0 || d1 < 0) continue;
    if (d0 == d1) continue;
    if (!existsPath(a, d0, d1)) return false;
  }

  return true;
}

// ============
// Atomic edges
// ============

bool ValOrder::addAtomicEdges()
{
  Seq<InstrId> atomicInstrs;

  Data* prev = new Data [trace->numAddrs];
  for (int a = 0; a < trace->numAddrs; a++) {
    for (int i = 0; i < trace->numData[a]; i++) {
      atomicRtoW[a][i] = -1;
      atomicWtoR[a][i] = -1;
    }
  }

  // Add atomic edges
  for (int t = 0; t < trace->numThreads; t++) {
    Seq<InstrId>* thread = &trace->threads[t];
    for (int a = 0; a < trace->numAddrs; a++) prev[a] = 0;
    for (int i = 0; i < thread->numElems; i++) {
      Instr instr = trace->instrs[thread->elems[i]];
      if (instr.op == RMW) {
        atomicInstrs.append(instr.uid);
        if (atomicRtoW[instr.addr][instr.readVal] >= 0) {
          // Multiple atomic RMWs with same read value 
          delete [] prev;
          return false;
        }
        atomicRtoW[instr.addr][instr.readVal] = instr.writeVal;
        atomicWtoR[instr.addr][instr.writeVal] = instr.readVal;
        if (trace->finalVals[instr.addr] == instr.readVal) return false;
        if (prev[instr.addr] != instr.readVal)
          valOrders[instr.addr]->addEdge(prev[instr.addr], instr.readVal);
        valOrders[instr.addr]->addEdge(instr.readVal, instr.writeVal);
        prev[instr.addr] = instr.writeVal;
      }
    }
  }

  delete [] prev;

  // Fail if cycles present
  if (! computeNext()) return false;

  // Otherwise, compute closure of atomic edges
  Data** newRtoW = new Data* [trace->numAddrs];
  Data** newWtoR = new Data* [trace->numAddrs];
  for (int a = 0; a < trace->numAddrs; a++) {
    newRtoW[a] = new Data [trace->numData[a]];
    newWtoR[a] = new Data [trace->numData[a]];
    for (int i = 0; i < trace->numData[a]; i++) {
      newRtoW[a][i] = -1;
      newWtoR[a][i] = -1;
    }
  }

  Seq<InstrId> roots, rootsRev;
  for (int i = 0; i < atomicInstrs.numElems; i++) {
    InstrId id = atomicInstrs.elems[i];
    Instr instr = trace->instrs[id];
    Addr a = instr.addr;
    if (atomicRtoW[a][instr.writeVal] < 0)
      rootsRev.append(instr.uid);
    if (atomicWtoR[a][instr.readVal] < 0)
      roots.append(instr.uid);
  }

  for (int i = 0; i < rootsRev.numElems; i++) {
    Instr instr = trace->instrs[rootsRev.elems[i]];
    Addr a = instr.addr;
    Data base = instr.writeVal;
    Data w = base;
    for (;;) {
      Data r = atomicWtoR[a][w];
      if (r < 0) break;
      newRtoW[a][r] = base;
      w = r;
    }
  }

  for (int i = 0; i < roots.numElems; i++) {
    Instr instr = trace->instrs[rootsRev.elems[i]];
    Data a = instr.addr;
    Data base = instr.readVal;
    Data r = base;
    for (;;) {
      Data w = atomicRtoW[a][r];
      if (w < 0) break;
      newWtoR[a][w] = base;
      r = w;
    }
  }

  delete [] atomicRtoW;
  delete [] atomicWtoR;
  atomicRtoW = newRtoW;
  atomicWtoR = newWtoR;

  return true;
}

// ===========
// Local edges
// ===========

void ValOrder::addLocalEdges()
{
  Data* prev = new Data [trace->numAddrs];

  for (int t = 0; t < trace->numThreads; t++) {
    Seq<InstrId>* thread = &trace->threads[t];
    for (int a = 0; a < trace->numAddrs; a++) prev[a] = 0;
    for (int i = 0; i < thread->numElems; i++) {
      Instr instr = trace->instrs[thread->elems[i]];
      if (instr.op == LD || instr.op == RMW) {
        addEdgeFast(instr.addr, prev[instr.addr], instr.readVal);
        prev[instr.addr] = instr.readVal;
      }
      if (instr.op == ST || instr.op == RMW) {
        addEdgeFast(instr.addr, prev[instr.addr], instr.writeVal);
        prev[instr.addr] = instr.writeVal;
      }
    }
  }

  Seq<Edge> edges;
  localDepEdges(trace, &edges);
  localWMOEdges(trace, &edges);
  for (int i = 0; i < edges.numElems; i++) {
    Edge e = edges.elems[i];
    opOrder->addEdge(e.src, e.dst);
    localOpOrder->addEdge(e.src, e.dst);
  }
}

// ===============================
// Sync->Sync and Sync->Load edges
// ===============================

inline int max(int a, int b) { return a > b ? a : b; }

bool ValOrder::addSyncEdges()
{
  SmallSeq<InstrId> out;
  Seq<InstrId> nodes;
  if (! opOrder->topSort(&nodes)) return false;

  InstrId** prevSyncs = new InstrId* [trace->numInstrs];
  for (int i = 0; i < trace->numInstrs; i++) {
    prevSyncs[i] = new InstrId [trace->numThreads];
    for (int t = 0; t < trace->numThreads; t++)
      prevSyncs[i][t] = -1;
  }

  for (int i = 0; i < nodes.numElems; i++) {
    InstrId src = nodes.elems[i];
    Instr srcInstr = trace->instrs[src];
    opOrder->outgoing(src, &out);
    for (int j = 0; j < out.numElems; j++) {
      InstrId dst = out.elems[j];
      if (srcInstr.op == SYNC)
        prevSyncs[dst][srcInstr.tid] = src;
      for (int t = 0; t < trace->numThreads; t++)
        prevSyncs[dst][t] = max(prevSyncs[dst][t], prevSyncs[src][t]);
    }
  }

  for (int i = 0; i < trace->numInstrs; i++) {
    Instr instr = trace->instrs[i];
    if (instr.op == SYNC) {
      for (int t = 0; t < trace->numThreads; t++) {
        InstrId prev = prevSyncs[instr.uid][t];
        if (prev >= 0) addEdgesFast(prev, instr.uid);
      }
    }
    if (instr.op == LD || instr.op == RMW) {
      InstrId next = trace->beginAfter(instr.uid);
      if (next >= 0) {
        for (int t = 0; t < trace->numThreads; t++) {
          InstrId prev = prevSyncs[instr.uid][t];
          if (prev >= 0) addEdgesFast(prev, next);
        }
      }
    }
  }

  for (int i = 0; i < trace->numInstrs; i++)
    delete [] prevSyncs[i];
  delete [] prevSyncs;

  return true;
}

// ===================
// Communication edges
// ===================

bool ValOrder::addCommEdges()
{
  for (int i = 0; i < trace->numInstrs; i++) {
    Instr instr = trace->instrs[i];
    if (instr.op == ST || instr.op == RMW) {
      Seq<InstrId>* loads = &trace->readsFromInv[instr.uid];
      for (int j = 0; j < loads->numElems; j++) {
        Instr load = trace->instrs[loads->elems[j]];
        opOrder->addEdge(instr.uid, load.uid);
      }
    }
  }

  if (! addSyncEdges()) return false;
  return true;
}

// ==========================
// Enable use of 'sync' times
// ==========================

// Infer edges between any two sync instructions if, using
// timestamps, one of the syncs is known to finish before the other
// begins.  (This assumes that timestamps are global, i.e. timestamps
// of instructions on different threads are in the same clock domain.)

void ValOrder::useSyncTimes()
{
  int *dst = new int [trace->numThreads];

  for (int th = 0; th < trace->numThreads; th++) {
    for (int t = 0; t < trace->numThreads; t++)
      dst[t] = trace->firstSync[t];

    InstrId src = trace->firstSync[th];
    while (src >= 0) {
      Instr srcInstr = trace->instrs[src];
      if (srcInstr.endTime >= 0) {
        for (int t = 0; t < trace->numThreads; t++)
          if (t != th) {
            while (dst[t] >= 0) {
              Instr dstInstr = trace->instrs[dst[t]];
              if (dstInstr.beginTime > srcInstr.endTime) {
                opOrder->addEdge(src, dst[t]);
                break;
              }
              dst[t] = trace->nextSync[dst[t]];
            }
          }
      }
      src = trace->nextSync[src];
    }
  }

  delete [] dst;
}

// ==========
// Initialise
// ==========

bool ValOrder::initialise(bool globalClock)
{
  if (! addAtomicEdges()) return false;
  addLocalEdges();
  if (globalClock) useSyncTimes();
  if (! addCommEdges()) return false;
  return computeNext();
}

// ===========================
// Search for total sync order
// ===========================

// Delete a root.

void ValOrder::delRoot(
       InstrId root,
       Seq<InstrId>* roots,
       Seq<InstrId>* threadRoots)
{
  Instr instr = trace->instrs[root];
  SmallSeq<InstrId> in, out, localOut;

  opOrder->outgoing(root, &out);
  localOpOrder->outgoing(root, &localOut);
  back.delNode(opOrder, root);
  back.delNode(localOpOrder, root);
  back.delRoot(roots, root);
  back.delRoot(&threadRoots[instr.tid], root);

  // Update roots
  for (int i = 0; i < out.numElems; i++) {
    opOrder->incoming(out.elems[i], &in);
    if (in.numElems == 0) back.addRoot(roots, out.elems[i]);
  }
  for (int i = 0; i < localOut.numElems; i++) {
    localOpOrder->incoming(localOut.elems[i], &in);
    if (in.numElems == 0)
      back.addRoot(&threadRoots[instr.tid], localOut.elems[i]);
  }
}

// Consume nodes deterministically.

void ValOrder::consume(
       int* count,
       Seq<InstrId>* roots,
       Seq<InstrId>* threadRoots)
{
  bool change = true;
  while (change) {
    change = false;
    for (int i = 0; i < roots->numElems; i++) {
      Instr r = trace->instrs[roots->elems[i]];
      if (r.op == LD || r.op == RMW || r.op == ST) {
        delRoot(r.uid, roots, threadRoots);
        back.write(count, *count+1);
        change = true;
        break;
      }
    }
  }
}

void ValOrder::consumeSyncs(
       int* count,
       Seq<InstrId>* roots,
       Seq<InstrId>* threadRoots)
{
  bool change = true;
  while (change) {
    change = false;
    for (int i = 0; i < roots->numElems; i++) {
      Instr r = trace->instrs[roots->elems[i]];
      if (r.op == SYNC) {
        InstrId node = r.uid;
        Instr nodeInstr = r;
        bool fail = false;
        for (int t = 0; t < trace->numThreads; t++) {
          if (nodeInstr.tid != t) {
            for (int i = 0; i < threadRoots[t].numElems; i++) {
              InstrId dst = threadRoots[t].elems[i];
              Instr dstInstr = trace->instrs[dst];
              if (dstInstr.op == SYNC) {
                if (! edgesExist(node, dst)) { fail = true; break; }
              }
              else if (dstInstr.op == LD || dstInstr.op == RMW) {
                InstrId next = trace->beginAfter(dstInstr.uid);
                if (! edgesExist(node, next)) { fail = true; break; }
                next = trace->nextSync[dstInstr.uid];
                if (! edgesExist(node, next)) { fail = true; break; }
              }
              else {
                fprintf(stderr, "Internal error: thread roots\n");
                exit(EXIT_FAILURE);
              }
            }
          }
        }
        if (!fail) {
          delRoot(r.uid, roots, threadRoots);
          back.write(count, *count+1);
          consume(count, roots, threadRoots);
          change = true;
          break;
        }
      }
    }
  }
}

bool ValOrder::check()
{
  // Count of number of nodes removed.
  int count = 0;

  // Stack
  Seq<InstrId> stack;
  SmallSeq<InstrId> out;

  // Compute initial thread roots
  Seq<InstrId>* threadRoots = new SmallSeq<InstrId> [trace->numThreads];
  SmallSeq<InstrId> tmp;
  localOpOrder->roots(&tmp);
  for (int i = 0; i < tmp.numElems; i++) {
    InstrId id = tmp.elems[i];
    Instr instr = trace->instrs[id];
    threadRoots[instr.tid].append(id);
  }

  // Compute initial roots
  SmallSeq<InstrId> rs;
  opOrder->roots(&rs);
  consume(&count, &rs, threadRoots);
  consumeSyncs(&count, &rs, threadRoots);
  for (int i = 0; i < rs.numElems; i++)
    stack.push(rs.elems[i]);

  while (stack.numElems > 0 && count < trace->numInstrs) {
    InstrId node = stack.pop();
    if (node < 0) {
      back.backtrack();
    }
    else {
      back.checkpoint();

      // Order chosen sync with respect to thread roots
      Instr nodeInstr = trace->instrs[node];
      bool fail = false;
      for (int t = 0; t < trace->numThreads; t++) {
        if (nodeInstr.tid != t) {
          for (int i = 0; i < threadRoots[t].numElems; i++) {
            InstrId dst = threadRoots[t].elems[i];
            Instr dstInstr = trace->instrs[dst];
            if (dstInstr.op == SYNC) {
              if (! addEdges(node, dst)) { fail = true; break; }
            }
            else if (dstInstr.op == LD || dstInstr.op == RMW) {
              InstrId next = trace->beginAfter(dstInstr.uid);
              if (! addEdges(node, next)) { fail = true; break; }
              next = trace->nextSync[dstInstr.uid];
              if (! addEdges(node, next)) { fail = true; break; }
            }
            else {
              fprintf(stderr, "Internal error: thread roots\n");
              exit(EXIT_FAILURE);
            }
          }
        }
        if (fail) break;
      }
      if (fail) {
        back.backtrack();
        continue;
      }

      // Delete root
      delRoot(node, &rs, threadRoots);
      back.write(&count, count+1);
      consume(&count, &rs, threadRoots);
      consumeSyncs(&count, &rs, threadRoots);

      stack.push(-1);
      for (int i = 0; i < rs.numElems; i++)
        stack.push(rs.elems[i]);
    }
  }
  
  delete [] threadRoots;

  return count == trace->numInstrs;
}
