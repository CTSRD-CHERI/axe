#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Models.h"
#include "Trace.h"
#include "Edges.h"
#include "Analysis.h"
#include "ValOrder.h"

// =======================
// Parse model from string
// =======================

void parseModel(char* str, Model* model)
{
  if (! strcasecmp(str, "SC"))
    model->tag = SC;
  else if (! strcasecmp(str, "TSO"))
    model->tag = TSO;
  else if (! strcasecmp(str, "PSO"))
    model->tag = PSO;
  else if (! strcasecmp(str, "WMO"))
    model->tag = WMO;
  else if (! strcasecmp(str, "POW"))
    model->tag = POW;
  else {
    fprintf(stderr, "Unsupported model '%s'\n", str);
    exit(EXIT_FAILURE);
  }
}

// =========================
// Check trace against model
// =========================

bool checkPOW(Seq<Instr>* instrs, bool globalClock)
{
  Trace trace(instrs);
  trace.computePrevSeen();
  trace.computeNextSeen();

  ValOrder valOrder(&trace);

  if (! valOrder.initialise(globalClock)) return false;

  return valOrder.check();
}

bool checkOther(Model* model, Seq<Instr>* instrs)
{
  Trace trace(instrs);

  Seq<Edge> edges(instrs->numElems);
  interEdges(&trace, &edges);
  initialValueEdges(&trace, &edges);
  finalValueEdges(&trace, &edges);

  switch (model->tag) {
    case SC:
      localSCEdges(&trace, &edges);
      break;
    case TSO:
      localTSOEdges(&trace, &edges);
      break;
    case PSO:
      localPSOEdges(&trace, &edges);
      break;
    case WMO:
      localWMOEdges(&trace, &edges);
      localDepEdges(&trace, &edges);
      break;
    default:
      fprintf(stderr, "Unknown model\n");
      exit(EXIT_FAILURE);
  }

  Analysis analysis(&trace, &edges);

  if (! analysis.computeNext()) return false;
  if (! analysis.inferEdges()) return false;

  return analysis.check();
}

bool check(Model* model, Seq<Instr>* instrs, bool globalClock)
{
  if (model->tag == POW)
    return checkPOW(instrs, globalClock);
  else
    return checkOther(model, instrs);
}
