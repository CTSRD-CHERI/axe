#!/usr/bin/env python

# Given an axe trace that fails a given consistency model, try to find
# the smallest subset of the trace that also fails the model.  This
# makes it easier to determine *why* a trace does not satisfy a model.

# This simple shrinker is often effective but also rather slow for
# large traces.

import subprocess
import sys

# Check args
if len(sys.argv) != 3:
  print "Usage: axe-shrink.py [MODEL] [FILE]"
  sys.exit()

# Open trace
if sys.argv[2] == "-":
  f = sys.stdin
else:
  f = open(sys.argv[2], 'r')
  if f == None:
    print "File not found: ", sys.argv[2]
    sys.exit()

# Read trace
lines = []
omit = []
omitted = 0
for line in f:
  lines.append(line)
  omit.append(False)

# Play a trace to axe: return true if axe responds 'NO';
# otherwise return false.
def play():
  try:
    p = subprocess.Popen(['axe', 'check', sys.argv[1], '-'],
          stdin=subprocess.PIPE, stdout=subprocess.PIPE,
          stderr=subprocess.PIPE)
    for i in range(0, len(lines)):
      if not omit[i]: p.stdin.write(lines[i] + '\n')
    p.stdin.close()
    out = p.stdout.read()
    if out == "NO\n": return True
    else: return False
  except subprocess.CalledProcessError:
    return False

# Simple shrinking procedure
def shrink():
  global omit, omitted
  total = len(lines)
  for i in reversed(range(0, len(lines))):
    print >> sys.stderr, "Omitted", omitted, "of", total, "        \r",
    if not omit[i]: 
      omit[i] = True
      if not play(): omit[i] = False
      else: omitted = omitted + 1

# Shrink until fixed-point
def shrinkFixedPoint():
  count = 0
  while True:
    before = omitted
    print >> sys.stderr, "Pass", count
    shrink()
    print >> sys.stderr
    after = omitted
    count = count+1
    if before == after: break
  
# Shrink and print trace
shrinkFixedPoint()
sys.stderr.flush()
for i in range(0, len(lines)):
  if not omit[i]: print lines[i],
