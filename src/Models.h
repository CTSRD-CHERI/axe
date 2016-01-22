#ifndef _MODELS_H_
#define _MODELS_H_

#include "Seq.h"
#include "Instr.h"

enum ModelTag { SC, TSO, PSO, WMO, POW };

struct Model {
  ModelTag tag;
};

void parseModel(char* str, Model* model);
bool check(Model* model, Seq<Instr>* instrs, bool globalClock = false);

#endif
