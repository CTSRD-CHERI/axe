#ifndef _VALORDER_H_
#define _VALORDER_H_

#include "Instr.h"
#include "Edges.h"
#include "Graph.h"
#include "Backtrack.h"

class ValOrder {
  private:
    Data*** next;
    Data** atomicRtoW;
    Data** atomicWtoR;
    ThreadId** storers;
    Graph* syncGraph;
    NodeId* syncId;
    Graph** valOrders;
    Graph* opOrder;
    Graph* localOpOrder;
    Seq<InstrId>* fromSync;
    Backtrack back;

    void computeStorers();
    void createSyncGraph();
    inline bool update(int* a, int b);
    void propagateData(Addr a, Data from, Data to);
    bool propagateNext(Addr a, Data from, Data to);
    bool computeNext();
    bool existsPath(Addr a, Data src, Data dstStore);
    void addEdgeFast(Addr a, Data from, Data to);
    bool addEdge(Addr a, Data from, Data to);
    void addEdgesFast(InstrId from, InstrId to);
    bool addEdges(InstrId from, InstrId to);
    bool edgesExist(InstrId from, InstrId to);
    bool addAtomicEdges();
    void addLocalEdges();
    bool addSyncEdges();
    bool addCommEdges();
    void useSyncTimes();
    void delRoot(InstrId root, Seq<InstrId>* roots, Seq<InstrId>* threadRoots);
    void consume(int* count, Seq<InstrId>* roots, Seq<InstrId>* threadRoots);
    void consumeSyncs(int* count, Seq<InstrId>* roots,
                      Seq<InstrId>* threadRoots);
    
  public:
    Trace* trace;

    ValOrder(Trace* trace);
    ~ValOrder();
    bool initialise(bool globalClock);
    bool check();
};

#endif
