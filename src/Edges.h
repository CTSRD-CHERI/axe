#ifndef _EDGES_H_
#define _EDGES_H_

#include "Trace.h"
#include "Instr.h"
#include "Seq.h"

struct Edge {
  int src;
  int dst;
};

Edge edge(InstrId src, InstrId dst);
void localSCEdges(Trace* trace, Seq<Edge>* result);
void localTSOEdges(Trace* trace, Seq<Edge>* result);
void localPSOEdges(Trace* trace, Seq<Edge>* result);
void localWMOEdges(Trace* trace, Seq<Edge>* result);
void localDepEdges(Trace* trace, Seq<Edge>* result);
void interEdges(Trace* trace, Seq<Edge>* result);
void initialValueEdges(Trace* trace, Seq<Edge>* result);
void finalValueEdges(Trace* trace, Seq<Edge>* result);

#endif
