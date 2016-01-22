#ifndef _ANALYSIS_H_
#define _ANALYSIS_H_

#include "Instr.h"
#include "Graph.h"
#include "Trace.h"
#include "Edges.h"
#include "Backtrack.h"

class Analysis
{
 private:
   // Internal analysis routines
   inline bool update(int* a, int b);
   void propagateInstr(InstrId from, InstrId to);
   bool propagateNext(InstrId from, InstrId to);
   bool addEdgeHelper(Edge e, Seq<Edge>* inferred);
   bool existsPath(InstrId src, InstrId dstStore);
   void inferFrom(InstrId src, Seq<Edge>* inferred);

   // Internal checker routines
   void delRoot(InstrId root, Seq<InstrId>* roots, InstrId* lastStore);
   bool performStore(Instr instr, Seq<InstrId>* roots, InstrId* lastStore);
   void consume(int* count, Seq<InstrId>* roots, InstrId* lastStore);

 public:
   Trace* trace;
   Graph* graph;
   InstrId** nextLoad;
   InstrId** nextStore;
   Backtrack back;

   Analysis(Trace* trace, Seq<Edge>* edges);
   ~Analysis();

   // Analysis routines
   bool computeNext();
   bool inferEdges();
   bool addEdge(Edge e);

   // Checker
   bool check();
};

#endif
