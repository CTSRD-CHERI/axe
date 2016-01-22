#include <stdio.h>
#include "Instr.h"

// ===============================
// Pretty printer for instructions
// ===============================

static void printTimestamp(Instr i)
{
  if (i.beginTime < 0 && i.endTime < 0) {
    printf("\n");
    return;
  }
  printf(" @ ");
  if (i.beginTime >= 0) printf("%i", i.beginTime);
  printf(":");
  if (i.endTime >= 0) printf("%i", i.endTime);
  printf("\n");
}

void printInstr(Instr i)
{
  if (i.op == LD) {
    printf("%i: M[%i] == %i", i.tid, i.addr, i.readVal);
  }
  else if (i.op == ST) {
    printf("%i: M[%i] := %i", i.tid, i.addr, i.writeVal);
  }
  else if (i.op == SYNC) {
    printf("%i: sync", i.tid);
  }
  else if (i.op == RMW) {
    printf("%i: { M[%i] == %i; M[%i] := %i }",
      i.tid, i.addr, i.readVal, i.addr, i.writeVal);
  }
  else if (i.op == NOP) {
    printf("nop");
    return;
  }
  else if (i.op == FINAL) {
    printf("final M[%i] == %i", i.addr, i.readVal);
    return;
  }

  printTimestamp(i);
}
