#ifndef _PARSER_H_
#define _PARSER_H_

#include "Instr.h"
#include "Seq.h"

class Parser {
  private:
    FILE* fp;
    int lineNumber;
    InstrId nextId;
    bool interactive;
    bool done;

    void parseError(const char* msg);
    inline char getChar();
    inline void ungetChar(char c);
    void demand(char x);
    void demandString(const char* s);
    bool eat(char x);
    int  parseNat();
    void spaces();
    Addr parseAddr();
    void parseTimestamp(Time* begin, Time* end);
    Instr parseInstr();
  public:
    Parser(const char* filename);
    ~Parser();
    bool parseTrace(Seq<Instr>* instrs);
};

#endif
