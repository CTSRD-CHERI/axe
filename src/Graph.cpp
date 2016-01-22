#include <assert.h>
#include <stdio.h>
#include "Graph.h"

// ===========
// Constructor
// ===========

Graph::Graph(int n) {
   numNodes = n;
   inEdges  = new TinySeq<NodeId> [n];
   outEdges = new TinySeq<NodeId> [n];
   present  = new bool [n];
   for (int i = 0; i < n; i++)
     present[i] = true;
}

// =============
// Deconstructor
// =============

Graph::~Graph() {
  delete [] inEdges;
  delete [] outEdges;
  delete [] present;
}

// =======
// Methods
// =======

// Invert the direction of all edges.

void Graph::invert()
{
  Seq<NodeId>* tmp = inEdges;
  inEdges = outEdges;
  outEdges = tmp;
}

// Add an edge.

void Graph::addEdge(NodeId src, NodeId dst)
{
  inEdges[dst].insert(src);
  outEdges[src].insert(dst);
}

// Delete an edge.

void Graph::delEdge(NodeId src, NodeId dst)
{
  inEdges[dst].remove(src);
  outEdges[src].remove(dst);
}

// Delete a node.

void Graph::delNode(NodeId node)
{
  present[node] = false;
}

// Un-delete a node.

void Graph::undelNode(NodeId node)
{
  present[node] = true;
}

// Find incoming edges.

void Graph::incoming(NodeId node, Seq<NodeId>* result)
{
  result->clear();
  for (int i = 0; i < inEdges[node].numElems; i++) {
    NodeId inc = inEdges[node].elems[i];
    if (present[inc]) result->append(inc);
  }
}

// Find outgoing edges.

void Graph::outgoing(NodeId node, Seq<NodeId>* result)
{
  result->clear();
  for (int i = 0; i < outEdges[node].numElems; i++) {
    NodeId out = outEdges[node].elems[i];
    if (present[out]) result->append(out);
  }
}

// Find the roots of the graph.

void Graph::roots(Seq<NodeId>* result)
{
  result->clear();
  SmallSeq<NodeId> inc;
  for (int i = 0; i < numNodes; i++) {
    incoming(i, &inc);
    if (inc.numElems == 0) result->append(i);
  }
}

// Topological sort.  Returns false if a cycle is detected.

bool Graph::topSort(Seq<NodeId>* result)
{
  result->clear();

  // Save 'present' array.
  bool* presentOld = new bool [numNodes];
  for (int i = 0; i < numNodes; i++)
    presentOld[i] = present[i];

  // Compute initial roots
  Seq<NodeId> rs;
  roots(&rs);

  // Temporary variables
  SmallSeq<NodeId> in;
  SmallSeq<NodeId> out;

  // Remove a root on each iteration
  while (rs.numElems > 0) {
    NodeId root = rs.pop();
    result->append(root);
    outgoing(root, &out);
    delNode(root);
    for (int i = 0; i < out.numElems; i++) {
      NodeId o = out.elems[i];
      incoming(o, &in);
      if (in.numElems == 0) {
        rs.append(o);
      }
    }
  }

  // Cycle found?
  bool cyclic = result->numElems < numNodes;

  // Restore 'present' array.
  for (int i = 0; i < numNodes; i++)
    present[i] = presentOld[i];
  delete [] presentOld;

  return !cyclic;
}

bool Graph::revTopSort(Seq<NodeId>* result)
{
  invert();
  bool ok = topSort(result);
  invert();
  return ok;
}

// ===========
// Count edges
// ===========

int Graph::countEdges()
{
  int count = 0;
  for (int i = 0; i < numNodes; i++)
    count += inEdges[i].numElems;
  return count;
}
