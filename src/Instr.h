#ifndef _INSTR_H_
#define _INSTR_H_

#define MAX_ADDRS   256
#define MAX_THREADS 1024
#define MAX_DATA    8388608

typedef int InstrId;
typedef int ThreadId;
typedef int Data;
typedef int Addr;
typedef int Time;

enum Op {
    LD     // Load
  , ST     // Store
  , RMW    // Read-modify-write
  , SYNC   // Barrier
  , NOP    // No-op
  , END    // Can mark end of an instruction sequence
  , FINAL  // Constraint on final value observable at some address
};

struct Instr {
  // Unique instruction identifier
  // (Can be compared to other instructions on the same thread 
  // to deduce program-order relationship)
  InstrId uid;

  // Hardware thread id
  ThreadId tid;

  // Operation
  Op op;

  // Address
  Addr addr;

  // Value read
  Data readVal;

  // Value written
  Data writeVal;

  // Optional timestamps
  Time beginTime, endTime;
};

void printInstr(Instr i);

inline bool hasAddr(Instr i)
  { return i.op == LD || i.op == ST || i.op == RMW || i.op == FINAL; }

#endif
