#ifndef _GRAPH_H_
#define _GRAPH_H_

#include <stdlib.h>
#include <assert.h>
#include "Seq.h"

typedef int NodeId;

class Graph
{
 public:
   int numNodes;
   Seq<NodeId>* inEdges;
   Seq<NodeId>* outEdges;
   bool* present;
   
   Graph(int numNodes);
   ~Graph();

   void invert();
   void addEdge(NodeId src, NodeId dst);
   void delEdge(NodeId src, NodeId dst);
   void delNode(NodeId node);
   void undelNode(NodeId node);
   void incoming(NodeId node, Seq<NodeId>* result);
   void outgoing(NodeId node, Seq<NodeId>* result);
   void roots(Seq<NodeId>* result);
   bool topSort(Seq<NodeId>* result);
   bool revTopSort(Seq<NodeId>* result);
   int countEdges();
};

#endif
