#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Options.h"

// ===========
// Constructor
// ===========

Options::Options()
{
  globalClock      = false;
  ignoreTimestamps = false;
}

// =============
// Set an option
// =============

void Options::set(char* flag)
{
  if (!strcmp(flag, "-g"))
    globalClock = true;
  else if (!strcmp(flag, "-i"))
    ignoreTimestamps = true;
  else {
    fprintf(stderr, "Unknown option: '%s'\n", flag);
    exit(EXIT_FAILURE);
  }
}

// ==================
// Display usage info
// ==================

void usage()
{
  printf("Usage:\n");
  printf("  axe check <MODEL> <FILE> [-g] [-i]\n");
  printf("  axe test  <MODEL> <FILE> <FILE> [-g] [-i]\n");
  printf("Where:\n");
  printf("  <MODEL> ::= SC|TSO|PSO|WMO|POW\n");
  printf("  -g          assume global clock domain\n");
  printf("  -i          ignore timestamps\n");
}
