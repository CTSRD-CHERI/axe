#include <stdio.h>
#include "Trace.h"
#include "Hash.h"

// =====================
// Raise error and abort
// =====================

void Trace::traceError(Instr instr, const char* msg)
{
  fprintf(stderr, "Input trace error");
  if (instr.lineNumber >= 0) fprintf(stderr, " on line %i", instr.lineNumber);
  fprintf(stderr, ":\n%s\n", msg);
  exit(EXIT_FAILURE);
}

// ===================================
// Pass 1: compute instruction mapping
// ===================================

// Create mapping from each instruction id to that instruction.  Set
// numInstrs to the number of instructions.  Raise error unless each
// instruction id lies between 0 and numInstrs-1.  This pass also
// separates final constraints from the memory operations.

void Trace::computeInstrMap(Seq<Instr>* instrSeq)
{
  numInstrs = instrSeq->numElems;
  numSyncs = numRMWs = 0;
  instrs = new Instr [numInstrs];
  for (int i = 0; i < instrSeq->numElems; i++) {
    Instr instr = instrSeq->elems[i];
    if (instr.op == FINAL) {
      numInstrs--;
      finals.append(instr);
      continue;
    }
    if (instr.uid < 0 || instr.uid >= numInstrs)
      traceError(instr, "Instruction id out of range");
    if (instr.op == SYNC) numSyncs++;
    if (instr.op == RMW) numRMWs++;
    instrs[instr.uid] = instr;
  }
}

// ============================================
// Pass 2: compact thread id and address ranges
// ============================================

// Set numThreads and numAddrs to the number of threads and addresses
// respectively.  After this pass, every thread id lies between 0 and
// numThreads-1, and every address between 0 and numAddrs-1.

void Trace::compactThreadAndAddrRanges()
{
  // Thread and address mappings
  ThreadId tidMap[MAX_THREADS];
  Addr addrMap[MAX_ADDRS];

  // Initialise address and thread mappings
  for (int i = 0; i < MAX_THREADS; i++) tidMap[i] = -1;
  for (int i = 0; i < MAX_ADDRS; i++) addrMap[i] = -1;
  numThreads = numAddrs = 0;

  // Compute address and thread mappings
  for (int i = 0; i < numInstrs; i++) {
    Instr instr = instrs[i];
    if (hasAddr(instr) && instr.addr >= MAX_ADDRS)
      traceError(instr, "Address out of range");
    if (instr.tid >= MAX_THREADS)
      traceError(instr, "Thread id out of range");
    if (hasAddr(instr) && addrMap[instr.addr] < 0)
      addrMap[instr.addr] = numAddrs++;
    if (tidMap[instr.tid] < 0)
      tidMap[instr.tid] = numThreads++;
  }

  for (int i = 0; i < finals.numElems; i++) {
    Instr instr = finals.elems[i];
    if (instr.addr >= MAX_ADDRS)
      traceError(instr, "Address out of range");
    if (addrMap[instr.addr] < 0)
      addrMap[instr.addr] = numAddrs++;
  }

  // Apply address and thread mappings
  for (int i = 0; i < numInstrs; i++) {
    Instr instr = instrs[i];
    if (hasAddr(instr)) instr.addr = addrMap[instr.addr];
    instr.tid = tidMap[instr.tid];
    instrs[i] = instr;
  }

  for (int i = 0; i < finals.numElems; i++) {
    Instr instr = finals.elems[i];
    instr.addr = addrMap[instr.addr];
    finals.elems[i] = instr;
  }
}

// ===========================
// Pass 3: compact data ranges
// ===========================

// Set numData[a] to the number of distinct data values written to
// address 'a' respectively.  After this pass, every data value
// lies between 0 and numData[a]-1 (inclusive).

void Trace::compactDataRanges()
{
  int logBuckets = intLog2(numInstrs >> 4);
  Hash<Data> dataMap(logBuckets+1);
  numData = new Data [numAddrs];

  // Initialise data mapping
  for (int a = 0; a < numAddrs; a++) {
    dataMap.insert(a, 0);
    numData[a] = 1;
  }

  // Compute data mapping
  for (int i = 0; i < numInstrs; i++) {
    Instr instr = instrs[i];
    if (instr.op == ST || instr.op == RMW) {
      if (instr.writeVal >= MAX_DATA)
        traceError(instr, "Data value out of range");
      int idx = instr.writeVal*numAddrs + instr.addr;
      if (! dataMap.member(idx))
        dataMap.insert(idx, numData[instr.addr]++);
    }
  }

  for (int i = 0; i < numInstrs; i++) {
    Instr instr = instrs[i];
    if (instr.op == LD || instr.op == RMW) {
      if (instr.readVal >= MAX_DATA)
        traceError(instr, "Data value out of range");
      int idx = instr.readVal*numAddrs + instr.addr;
      if (! dataMap.member(idx))
        dataMap.insert(idx, numData[instr.addr]++);
    }
  }

  for (int i = 0; i < finals.numElems; i++) {
    Instr instr = finals.elems[i];
    if (instr.readVal >= MAX_DATA)
      traceError(instr, "Data value out of range");
    int idx = instr.readVal*numAddrs + instr.addr;
    if (! dataMap.member(idx))
      dataMap.insert(idx, numData[instr.addr]++);
  }

  // Apply data mapping
  for (int i = 0; i < numInstrs; i++) {
    Instr instr = instrs[i];
    if (instr.op == LD || instr.op == RMW) {
      int idx = instr.readVal*numAddrs + instr.addr;
      dataMap.lookup(idx, &instr.readVal);
    }
    if (instr.op == ST || instr.op == RMW) {
      int idx = instr.writeVal*numAddrs + instr.addr;
      dataMap.lookup(idx, &instr.writeVal);
    }
    instrs[i] = instr;
  }

  for (int i = 0; i < finals.numElems; i++) {
    Instr instr = finals.elems[i];
    int idx = instr.readVal*numAddrs + instr.addr;
    dataMap.lookup(idx, &instr.readVal);
    finals.elems[i] = instr;
  }
}

// ===================================
// Pass 4: compute reads-from function
// ===================================

// For each load, find the store that it reads from.  Raise an
// error message if this relationship is ambiguous.  Raise an error if
// there exists a load with no corresponding write.

void Trace::computeReadsFrom()
{
  int logBuckets = intLog2(numInstrs>>4);
  Hash<InstrId> hash(logBuckets+1);

  readsFrom = new InstrId [numInstrs];

  for (int i = 0; i < numInstrs; i++) {
    Instr instr = instrs[i];
    readsFrom[i] = -1;
    if (instr.op == ST || instr.op == RMW) {
      int key = (instr.writeVal * numAddrs) + instr.addr;
      InstrId result;
      bool found = hash.lookup(key, &result);
      if (found)
        traceError(instr, "Reads-from function is ambiguous");
      else
        hash.insert(key, instr.uid);
    }
  }

  for (int i = 0; i < numInstrs; i++) {
    Instr instr = instrs[i];
    if (instr.op == LD || instr.op == RMW) {
      int key = (instr.readVal * numAddrs) + instr.addr;
      InstrId result;
      bool found = hash.lookup(key, &result);
      if (found)
        readsFrom[instr.uid] = result;
      else if (instr.readVal != 0)
        traceError(instr, "Found load with no corresponding store");
    }
  }

  // Set uid of any final constraints to uid of store from which it reads.
  for (int i = 0; i < finals.numElems; i++) {
    Instr instr = finals.elems[i];
    int key = (instr.readVal * numAddrs) + instr.addr;
    InstrId result;
    bool found = hash.lookup(key, &result);
    if (found)
      finals.elems[i].uid = result;
    else if (instr.readVal != 0)
      traceError(instr, "Found load with no corresponding store");
  }
}

// ==========================
// Pass 5: split into threads
// ==========================

void Trace::splitThreads()
{
  threads = new Seq<InstrId> [numThreads];

  for (int i = 0; i < numInstrs; i++) {
    Instr instr = instrs[i];
    threads[instr.tid].append(instr.uid);
  }
}

// ====================
// Pass 6: sanity check
// ====================

// Perform the following checks on the input trace, and raise an error
// if any are not satisified.
//
//  * value 0 (initial value) can only be read, not written
//  * begin times are strictly increasing per thread
//  * end times are stictly larger than begin times
//  * store operations don't contain end times
//
// (Prohibiting end-times on stores is done to give the
// property that PSO is always a subset of WMO.)

void Trace::sanityCheck()
{
  for (int t = 0; t < numThreads; t++) {
    Time prev = -1;
    for (int i = 0; i < threads[t].numElems; i++) {
      Instr instr = instrs[threads[t].elems[i]];
      if (instr.op == ST && instr.endTime >= 0)
        traceError(instr, "End-times for stores are currently disallowed");
      if (instr.op == ST || instr.op == RMW)
        if (instr.writeVal == 0)
          traceError(instr, "Initial value '0' can not be written");
      if (instr.beginTime >= 0 && instr.endTime >= 0 &&
            instr.endTime <= instr.beginTime)
        traceError(instr, "End times must be greater than start times");
      if (prev >= 0 && instr.beginTime >= 0 && prev >= instr.beginTime)
        traceError(instr, "Start times must be strictly increasing per thread");
      if (instr.beginTime >= 0) prev = instr.beginTime;
    }
  }
}

// ========================
// Final values per address
// ========================

// Compute mapping from address to final value that must be seen at
// that address.

void Trace::computeFinalVals()
{
  finalVals = new Data [numAddrs];
  for (int a = 0; a < numAddrs; a++) finalVals[a] = -1;
  for (int i = 0; i < finals.numElems; i++) {
    Instr fin = finals.elems[i];
    if (finalVals[fin.addr] >= 0)
      traceError(fin, "'final' constraints are contradictory");
    finalVals[fin.addr] = fin.readVal;
  }
}

// =================
// Next local stores
// =================

// For each instruction, compute next local store to same address in
// program-order.

void Trace::computeNextLocalStore()
{
  InstrId* prev = new InstrId [numAddrs];

  nextLocalStore = new InstrId [numInstrs];
  for (int i = 0; i < numInstrs; i++)
    nextLocalStore[i] = -1;

  for (int t = 0; t < numThreads; t++) {
    for (int i = 0; i < numAddrs; i++)
      prev[i] = -1;
    for (int i = threads[t].numElems-1; i >= 0; i--) {
      Instr instr = instrs[threads[t].elems[i]];
      if (hasAddr(instr))
        nextLocalStore[instr.uid] = prev[instr.addr];
      if (instr.op == ST || instr.op == RMW)
        prev[instr.addr] = instr.uid;
    }
  }

  delete [] prev;
}

// =====================
// Previous local stores
// =====================

// For each instruction, compute previous local store to same address
// in program-order.

void Trace::computePrevLocalStore()
{
  InstrId* prev = new InstrId [numAddrs];

  prevLocalStore = new InstrId [numInstrs];
  for (int i = 0; i < numInstrs; i++)
    prevLocalStore[i] = -1;

  for (int t = 0; t < numThreads; t++) {
    for (int i = 0; i < numAddrs; i++)
      prev[i] = -1;
    for (int i = 0; i < threads[t].numElems; i++) {
      Instr instr = instrs[threads[t].elems[i]];
      if (hasAddr(instr))
        prevLocalStore[instr.uid] = prev[instr.addr];
      if (instr.op == ST || instr.op == RMW)
        prev[instr.addr] = instr.uid;
    }
  }

  delete [] prev;
}

// ================
// Next local loads
// ================

// For each instruction, compute next local load to same address in
// program-order.

void Trace::computeNextLocalLoad()
{
  InstrId* prev = new InstrId [numAddrs];

  nextLocalLoad = new InstrId [numInstrs];
  for (int i = 0; i < numInstrs; i++)
    nextLocalLoad[i] = -1;

  for (int t = 0; t < numThreads; t++) {
    for (int i = 0; i < numAddrs; i++)
      prev[i] = -1;
    for (int i = threads[t].numElems-1; i >= 0; i--) {
      Instr instr = instrs[threads[t].elems[i]];
      if (hasAddr(instr))
        nextLocalLoad[instr.uid] = prev[instr.addr];
      if (instr.op == LD || instr.op == RMW)
        prev[instr.addr] = instr.uid;
    }
  }

  delete [] prev;
}

// ============
// First stores
// ============

// For each instruction, compute the first store to each address on
// each thread.

void Trace::computeFirstStore()
{
  firstStore = new InstrId* [numAddrs];
  for (int i = 0; i < numAddrs; i++) {
    firstStore[i] = new InstrId [numThreads];
    for (int t = 0; t < numThreads; t++)
      firstStore[i][t] = -1;
  }

  for (int t = 0; t < numThreads; t++) {
    for (int i = 0; i < threads[t].numElems; i++) {
      Instr instr = instrs[threads[t].elems[i]];
      if (instr.op == ST || instr.op == RMW)
        if (firstStore[instr.addr][t] < 0)
          firstStore[instr.addr][t] = instr.uid;
    }
  }
}

// ============
// Final stores
// ============

// For each instruction, compute the final store to each address on
// each thread.

void Trace::computeFinalStore()
{
  finalStore = new InstrId* [numAddrs];
  for (int i = 0; i < numAddrs; i++) {
    finalStore[i] = new InstrId [numThreads];
    for (int t = 0; t < numThreads; t++)
      finalStore[i][t] = -1;
  }

  for (int t = 0; t < numThreads; t++) {
    for (int i = threads[t].numElems-1; i >= 0; i--) {
      Instr instr = instrs[threads[t].elems[i]];
      if (instr.op == ST || instr.op == RMW)
        if (finalStore[instr.addr][t] < 0)
          finalStore[instr.addr][t] = instr.uid;
    }
  }
}

// ===========================
// Inverse reads-from relation
// ===========================

void Trace::computeReadsFromInv()
{
  readsFromInv = new TinySeq<InstrId> [numInstrs];
  for (int i = 0; i < numInstrs; i++) {
    if (readsFrom[i] >= 0)
      readsFromInv[readsFrom[i]].append(i);
  }
}

// ========================================
// Next instruction containing a begin time
// ========================================

// For each instruction, compute the next instruction in program-order
// that contains a begin time.

void Trace::computeNextBegin()
{
  nextBegin = new InstrId [numInstrs];

  for (int t = 0; t < numThreads; t++) {
    InstrId next = -1;
    for (int i = threads[t].numElems-1; i >= 0; i--) {
      Instr instr = instrs[threads[t].elems[i]];
      nextBegin[instr.uid] = next;
      if (instr.beginTime >= 0) next = instr.uid;
    }
  }
}

// ===========
// First syncs
// ===========

// Compute first sync on each thread.

void Trace::computeFirstSync()
{
  firstSync = new InstrId [numThreads];
  for (int t = 0; t < numThreads; t++)
    firstSync[t] = -1;

  for (int t = 0; t < numThreads; t++) {
    for (int i = 0; i < threads[t].numElems; i++) {
      Instr instr = instrs[threads[t].elems[i]];
      if (instr.op == SYNC) {
        firstSync[t] = instr.uid;
        break;
      }
    }
  }
}


// ============================
// Compute previous/next 'sync'
// ============================

// For each instruction, compute previous program-order sync.

void Trace::computePrevSync()
{
  prevSync = new InstrId [numInstrs];
  for (int t = 0; t < numThreads; t++) {
    InstrId sync = -1;
    for (int i = 0; i < threads[t].numElems; i++) {
      Instr instr = instrs[threads[t].elems[i]];
      prevSync[instr.uid] = sync;
      if (instr.op == SYNC) sync = instr.uid;
    }
  }
}


// For each instruction, compute next program-order sync.

void Trace::computeNextSync()
{
  nextSync = new InstrId [numInstrs];
  for (int t = 0; t < numThreads; t++) {
    InstrId sync = -1;
    for (int i = threads[t].numElems-1; i >= 0; i--) {
      Instr instr = instrs[threads[t].elems[i]];
      nextSync[instr.uid] = sync;
      if (instr.op == SYNC) sync = instr.uid;
    }
  }
}

// =====================
// Compute 'seen' values
// =====================

// For each address, compute latest value seen at or before each
// instruction.

void Trace::computePrevSeen()
{
  if (prevSeen != NULL) delete [] prevSeen;
  prevSeen = new Data [numInstrs*numAddrs];

  Data* initial  = new Data [numAddrs];
  for (int a = 0; a < numAddrs; a++)
    initial[a] = -1;
  
  for (int t = 0; t < numThreads; t++) {
    int* prev = initial;
    for (int i = 0; i < threads[t].numElems; i++) {
      Instr instr = instrs[threads[t].elems[i]];
      int base = instr.uid*numAddrs;
      for (int a = 0; a < numAddrs; a++)
        prevSeen[base+a] = prev[a];
      if (instr.op == LD)
        prevSeen[base+instr.addr] = instr.readVal;
      else if (instr.op == ST || instr.op == RMW)
        prevSeen[base+instr.addr] = instr.writeVal;
      prev = &prevSeen[base];
    }
  }

  delete [] initial;
}

// For each address, compute latest value seen at or after each
// instruction.

void Trace::computeNextSeen()
{
  if (nextSeen != NULL) delete [] nextSeen;
  nextSeen = new Data [numInstrs*numAddrs];

  Data* initial  = new Data [numAddrs];
  for (int a = 0; a < numAddrs; a++)
    initial[a] = -1;
  
  for (int t = 0; t < numThreads; t++) {
    int* prev = initial;
    for (int i = threads[t].numElems-1; i >= 0; i--) {
      Instr instr = instrs[threads[t].elems[i]];
      int base = instr.uid*numAddrs;
      for (int a = 0; a < numAddrs; a++)
        nextSeen[base+a] = prev[a];
      if (instr.op == LD || instr.op == RMW)
        nextSeen[base+instr.addr] = instr.readVal;
      else if (instr.op == ST)
        nextSeen[base+instr.addr] = instr.writeVal;
      prev = &nextSeen[base];
    }
  }

  delete [] initial;
}

// ===============
// Begin-after-end
// ===============

// Find first instruction that begins after given load finishes.

InstrId Trace::beginAfter(InstrId load)
{
  Instr instr = instrs[load];
  if (instr.endTime >= 0) {
    InstrId next = nextBegin[instr.uid];
    while (next >= 0) {
      Instr nextInstr = instrs[next];
      if (nextInstr.beginTime > instr.endTime) return next;
      next = nextBegin[next];
    }
  }
  return -1;
}

// ===========
// Constructor
// ===========

Trace::Trace(Seq<Instr>* instrs)
{
  computeInstrMap(instrs);
  compactThreadAndAddrRanges();
  compactDataRanges();
  computeReadsFrom();
  splitThreads();
  sanityCheck();
  computeFinalVals();
  computePrevLocalStore();
  computeNextLocalStore();
  computeNextLocalLoad();
  computeFirstStore();
  computeFinalStore();
  computeReadsFromInv();
  computeNextBegin();
  computeFirstSync();
  computePrevSync();
  computeNextSync();
  prevSeen = nextSeen = NULL;
}

// ==========
// Destructor
// ==========

Trace::~Trace()
{
  delete [] instrs;
  delete [] threads;
  delete [] numData;
  delete [] readsFrom;
  delete [] finalVals;
  delete [] prevLocalStore;
  delete [] nextLocalStore;
  delete [] nextLocalLoad;
  for (int i = 0; i < numAddrs; i++)
    delete [] firstStore[i];
  delete [] firstStore;
  delete [] finalStore;
  delete [] readsFromInv;
  delete [] nextBegin;
  if (prevSeen != NULL) delete [] prevSeen;
  if (nextSeen != NULL) delete [] nextSeen;
  delete [] firstSync;
  delete [] prevSync;
  delete [] nextSync;
}

// =============
// Display trace
// =============

void Trace::display()
{
  for (int i = 0; i < numInstrs; i++)
    printInstr(instrs[i]);
}
