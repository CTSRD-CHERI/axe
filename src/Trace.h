#ifndef _TRACE_H_
#define _TRACE_H_

#include "Seq.h"
#include "Instr.h"

class Trace {
 private:
   void computeInstrMap(Seq<Instr>*);  // Pass 1
   void compactThreadAndAddrRanges();  // Pass 2
   void compactDataRanges();           // Pass 3
   void computeReadsFrom();            // Pass 4
   void splitThreads();                // Pass 5
   void sanityCheck();                 // Pass 6

   void computeFinalVals();
   void computePrevLocalStore();
   void computeNextLocalStore();
   void computeNextLocalLoad();
   void computeFirstStore();
   void computeFinalStore();
   void computeReadsFromInv();
   void computeNextBegin();
   void computeFirstSync();
   void computePrevSync();
   void computeNextSync();

   void traceError(Instr instr, const char* msg);

 public:
   int numInstrs;
   int numThreads;
   int numAddrs;
   int* numData;
   int numSyncs;
   int numRMWs;
   Instr* instrs;
   SmallSeq<Instr> finals;
   Data* finalVals;
   InstrId* readsFrom;
   Seq<InstrId>* threads;
   InstrId* prevLocalStore;
   InstrId* nextLocalStore;
   InstrId* nextLocalLoad;
   InstrId** firstStore;
   InstrId** finalStore;
   Seq<InstrId>* readsFromInv;
   InstrId* nextBegin;
   InstrId* firstSync;
   InstrId* prevSync;
   InstrId* nextSync;
   Data* prevSeen;
   Data* nextSeen;

   Trace(Seq<Instr>* instrs);
   ~Trace();

   void display();
   void computePrevSeen();
   void computeNextSeen();
   InstrId beginAfter(InstrId load);
};

#endif
