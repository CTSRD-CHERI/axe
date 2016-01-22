#!/bin/bash

g++ -O2 -DEMULATION_MODE -Wconversion -std=c++0x -I . -o axe \
  Main.cpp       \
  Instr.cpp      \
  Parser.cpp     \
  Graph.cpp      \
  Edges.cpp      \
  Trace.cpp      \
  Analysis.cpp   \
  Models.cpp     \
  ValOrder.cpp
