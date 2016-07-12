#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "Parser.h"
#include "Instr.h"
#include "Seq.h"

// ===========
// Constructor
// ===========

Parser::Parser(const char* filename)
{
  if (filename[0] == '-')
    fp = stdin;
  else {
    fp = fopen(filename, "rt");
    if (fp == NULL) {
      fprintf(stderr, "Can't open trace file '%s'.\n", filename);
      exit(EXIT_FAILURE);
    }
  }
  nextId = 0;
  lineNumber = 1;
  done = interactive = false;
}

// =============
// Deconstructor
// =============

Parser::~Parser()
{
  if (fp != NULL) fclose(fp);
}

// ========================
// General parsing routines
// ========================

// Exit with an error message and a line number.

void Parser::parseError(const char* msg)
{
  fprintf(stderr, "Parse error on line %i:\n%s\n", lineNumber, msg);
  exit(EXIT_FAILURE);
}

// Get next character.

inline char Parser::getChar()
{
  int c = fgetc(fp);
  if (c == EOF) parseError("Unexpected EOF");
  if (c == '\n') lineNumber++;
  return (char) c;
}

// Un-get next character.

inline void Parser::ungetChar(char c)
{
  if (c == '\n') lineNumber--;
  ungetc((int) c, fp);
}

// Parse given character or fail hard.

void Parser::demand(char x)
{
  char c = getChar();
  if (c != x) parseError("Unexpected character");
}

// Parse given string or fail hard.

void Parser::demandString(const char* s)
{
  while (*s != '\0') {
    demand(*s);
    s++;
  }
}

// Parse given character or fail soft.

bool Parser::eat(char x)
{
  char c = getChar();
  if (c != x) {
    ungetChar(c);
    return false;
  }
  return true;
}

// Parse natural number or fail.

int Parser::parseNat()
{
  char nat[10];
  int i;
  for (i = 0; i < 10; i++) {
    char c = getChar();
    if (isdigit(c))
      nat[i] = c;
    else {
      nat[i] = '\0';
      ungetChar(c);
      break;
    }
  }
  if (i == 0) parseError("Expected number");
  if (i == 10) parseError("Number too long");
  return atoi(nat);
}

// Consume as many spaces as possible.

void Parser::spaces()
{
  bool comment = false;
  for (;;) {
    int c = fgetc(fp);
    if (c == EOF) return;
    if (c == '\n') { lineNumber++; comment = false; }
    if (c == '#') comment = true;
    if (!comment && !isspace(c)) {
      ungetChar((char) c);
      return;
    }
  }
}

// ==================
// Instruction parser
// ==================

Addr Parser::parseAddr()
{
  Addr addr;
  char c = getChar();
  if (c == 'M') {
    demand('[');
    spaces();
    addr = parseNat();
    spaces();
    demand(']');
  }
  else if (c == 'v') {
    addr = parseNat();
  }
  else
    parseError("Variable expected");
  return addr;
}

void Parser::parseTimestamp(Time* begin, Time* end)
{
  spaces();
  if (eat('@')) {
    spaces();
    if (! eat(':')) {
      *begin = parseNat();
      demand(':');
    }
    char c = getChar();
    ungetChar(c);
    if (isdigit(c))
      *end = parseNat();
  }
  spaces();
}

Instr Parser::parseInstr()
{
  Instr i;
  i.lineNumber = lineNumber;
  if (eat('c')) {
    demandString("heck");
    i.uid = -1;
    i.op = END;
    return i;
  }
  if (eat('f')) {
    demandString("inal");
    i.uid = -1;
    i.op = FINAL;
    spaces();
    i.addr = parseAddr();   spaces();
    demandString("==");     spaces();
    i.readVal = parseNat(); spaces();
    return i;
  }
  i.uid = nextId++;
  i.tid = parseNat();
  i.beginTime = i.endTime = -1;
  spaces();
  demand(':');
  spaces();
  if (eat('{')) {
    // Read-modify-write
    i.op = RMW;
    spaces();
    i.addr = parseAddr();
    spaces();
    demandString("==");
    spaces();
    i.readVal = parseNat();
    spaces();
    demand(';');
    spaces();
    if (parseAddr() != i.addr)
      parseError("Addresses in read-modify-write must be the same");
    spaces();
    demandString(":=");
    spaces();
    i.writeVal = parseNat();
    spaces();
    demand('}');
  }
  else {
    if (eat('s')) {
      // Barrier
      demandString("ync");
      i.op = SYNC;
    }
    else {
      i.addr = parseAddr();
      spaces();
      char c = getChar();
      if (c == '=' || c == ':') {
        // Load or store
        i.op = c == '=' ? LD : ST;
        demand('=');
        spaces();
        Data d = parseNat();
        if (i.op == LD) i.readVal = d; else i.writeVal = d;
      }
      else {
        parseError("Unexpected character");
      }
    }
  }
  spaces();
  if (!feof(fp)) parseTimestamp(&i.beginTime, &i.endTime);
  spaces();
  return i;
}

// ======================
// Top-level trace parser
// ======================

bool Parser::parseTrace(Seq<Instr>* instrs)
{
  if (done) return false;
  instrs->clear();
  nextId = 0;
  while (! feof(fp)) {
    spaces();
    Instr i = parseInstr();
    if (i.op == END) {
      interactive = true;
      return true;
    }
    else
      instrs->append(i);
  }
  if (interactive)
    return false;
  else {
    done = true;
    return true;
  }
}
