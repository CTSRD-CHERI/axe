#ifndef _OPTIONS_H_
#define _OPTIONS_H_

// Display usage info
void usage();

// Command-line options
struct Options {
  bool globalClock;
  bool ignoreTimestamps;

  // Constructor
  Options();

  // Set an option
  void set(char* flag);
};

#endif
