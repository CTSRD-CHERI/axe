#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Seq.h"
#include "Parser.h"
#include "Instr.h"
#include "Models.h"
#include "Options.h"

// =================
// Top-level checker
// =================

void axeCheck(char* modelName, char* fileName, Options opts)
{
  // Parse model name and trace file
  Model model;
  parseModel(modelName, &model);
  Parser parser(fileName);

  // Check trace(s)
  Seq<Instr> instrs;
  while (parser.parseTrace(&instrs)) {
    bool ok = check(&model, &instrs, opts);
    if (ok)
      printf("OK\n");
    else
      printf("NO\n");
    fflush(stdout);
  }
}

// ======================
// Top-level test routine
// ======================

void testError(const char* msg)
{
  fprintf(stderr, "Test error:\n  %s\n", msg);
  exit(EXIT_FAILURE);
}

int axeTest(char* modelName,
            char* traceFileName,
            char* answerFileName,
            Options opts)
{
  // Open answer file
  FILE* fp = fopen(answerFileName, "rt");
  if (fp == NULL) testError("Can't open answer file");
 
  // Parse model name and trace file
  Model model;
  parseModel(modelName, &model);
  Parser parser(traceFileName);

  // Read answers and check
  Seq<Instr> instrs;
  char line[1024];
  int testNum = 0;
  while (fgets(line, sizeof(line), fp) != NULL) {
    printf("%i\r", testNum);

    bool ans;
    if (line[0] == 'O') ans = true;
    else if (line[0] == 'N') ans = false;
    else testError("Answer file has invalid format");

    bool got = parser.parseTrace(&instrs);
    if (! got) testError("Answer file longer than trace file");
    bool ok = check(&model, &instrs, opts);
    if (ok != ans) {
      printf("Test %i failed\n", testNum);
      if (strlen(line) > 3)
        printf("Test name: %s", &line[3]);
      return -1;
    }

    testNum++;
  }

  printf("Ok, passed %i tests.\n", testNum);
  
  // Close answer file
  fclose(fp);

  return 0;
}

// ====
// Main
// ====

int main(int argc, char* argv[])
{
  Options opts;
  if (argc >= 4 && strcmp(argv[1], "check") == 0) {
    for (int i = 4; i < argc; i++) opts.set(argv[i]);
    axeCheck(argv[2], argv[3], opts);
  }
  else if (argc >= 5 && strcmp(argv[1], "test") == 0) {
    for (int i = 5; i < argc; i++) opts.set(argv[i]);
    return axeTest(argv[2], argv[3], argv[4], opts);
  }
  else {
    usage();
    return -1;
  }

  return 0;
}
