#ifndef _BACKTRACK_H_
#define _BACKTRACK_H_

#include "Graph.h"
#include "Edges.h"

enum BacktrackItemTag {
    CHECKPOINT
  , WRITE_INT
  , ADD_EDGE
  , DEL_NODE
  , ADD_ROOT
  , DEL_ROOT
};

struct BacktrackItem {
  BacktrackItemTag tag;
  union {
    struct { int* addr; int data; } writeInt;
    struct { Graph* graph; Edge edge;} addEdge;
    struct { Graph* graph; InstrId id; } delNode;
    struct { Seq<InstrId>* roots; InstrId id; } addDelRoot;
  };
};

class Backtrack {
  public:
    Seq<BacktrackItem> stack;

    inline void write(int* addr, int data) {
      if (stack.numElems > 0) {
        BacktrackItem item;
        item.tag           = WRITE_INT;
        item.writeInt.addr = addr;
        item.writeInt.data = *addr;
        stack.push(item);
      }
      *addr = data;
    }

    inline void addEdge(Graph* g, Edge e) {
      if (stack.numElems > 0) {
        BacktrackItem item;
        item.tag           = ADD_EDGE;
        item.addEdge.graph = g;
        item.addEdge.edge  = e;
        stack.push(item);
      }
      g->addEdge(e.src, e.dst);
    }

    inline void delNode(Graph* g, InstrId id) {
      if (stack.numElems > 0) {
        BacktrackItem item;
        item.tag           = DEL_NODE;
        item.delNode.graph = g;
        item.delNode.id    = id;
        stack.push(item);
      }
      g->delNode(id);
    }

    inline void addRoot(Seq<InstrId>* roots, InstrId id) {
      if (stack.numElems > 0) {
        BacktrackItem item;
        item.tag              = ADD_ROOT;
        item.addDelRoot.roots = roots;
        item.addDelRoot.id    = id;
        stack.push(item);
      }
      roots->append(id);
    }

    inline void delRoot(Seq<InstrId>* roots, InstrId id) {
      if (stack.numElems > 0) {
        BacktrackItem item;
        item.tag              = DEL_ROOT;
        item.addDelRoot.roots = roots;
        item.addDelRoot.id    = id;
        stack.push(item);
      }
      roots->remove(id);
    }

    void checkpoint() {
      BacktrackItem item;
      item.tag = CHECKPOINT;
      stack.push(item);
    }

    void backtrack() {
      while (stack.numElems > 0) {
        BacktrackItem item = stack.pop();
        switch (item.tag) {
          case CHECKPOINT: return;
          case WRITE_INT: {
            *item.writeInt.addr = item.writeInt.data;
            break;
          }
          case ADD_EDGE: {
            Edge e = item.addEdge.edge;
            item.addEdge.graph->delEdge(e.src, e.dst);
            break;
          }
          case DEL_NODE: {
            InstrId id = item.delNode.id;
            item.delNode.graph->undelNode(id);
            break;
          }
          case ADD_ROOT: {
            item.addDelRoot.roots->remove(item.addDelRoot.id);
            break;
          }
          case DEL_ROOT: {
            item.addDelRoot.roots->append(item.addDelRoot.id);
            break;
          }
        }
      }
    }
};

#endif
