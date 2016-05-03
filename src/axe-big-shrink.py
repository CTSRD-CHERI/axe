#!/usr/bin/env python

# Given an axe trace that fails a given consistency model, try to find
# the smallest subset of the trace that also fails the model.  This
# makes it easier to determine *why* a trace does not satisfy a model.

# This shrinker aims to deal well with large traces.

import subprocess
import sys
import re
import random
import sets

# =============================================================================
# Misc
# =============================================================================

def abort(msg):
  print "ERROR:", msg
  sys.exit(-1)

# =============================================================================
# Trace parser
# =============================================================================

# Op represents a single operation in a trace

class Op:
  # Concrete syntax of lines of an axe trace
  var   = "(M\[\d*\]|v\d*)"
  load  = re.compile("(\d*):\s*"+var+"\s*==\s*(\d*)\s*(@.*)?")
  store = re.compile("(\d*):\s*"+var+"\s*:=\s*(\d*)\s*(@.*)?")
  rmw   = re.compile("(\d*):\s*{\s*"+var+"\s*==\s*(\d*)\s*;\s*"
                                  +var+"\s*:=\s*(\d*)\s*}\s*(@.*)?")
  sync  = re.compile("(\d*):\s*sync\s*(@.*)?")

  def __init__(self, line):
    self.op = None
    self.addr = None
    self.readVal = None
    self.writeVal = None
    self.neverRead = False

    # Parse a load operation
    match = re.search(self.load, line)
    if match is not None:
      self.op = "LD"
      self.tid, self.addr, self.readVal, self.times = match.groups()
      return

    # Parse a store operation
    match = re.search(self.store, line)
    if match is not None:
      self.op = "ST"
      self.tid, self.addr, self.writeVal, self.times = match.groups()
      return

    # Parse a read-modify-write operation
    match = re.search(self.rmw, line)
    if match is not None:
      self.op = "RMW"
      self.tid, addr1, self.readVal, addr2, self.writeVal, self.times = \
        match.groups()
      if addr1 != addr2: abort("RMW addresses differ")
      self.addr = addr1
      return

    # Parse a sync operation
    match = re.search(self.sync, line)
    if match is not None:
      self.op = "SYNC"
      self.tid,self.times = match.groups()
      return

    abort("PARSE ERROR")

  def show(self):
    if self.op == "LD":
      result = self.tid + ": " + self.addr + " == " + self.readVal
    if self.op == "ST":
      result = self.tid + ": " + self.addr + " := " + self.writeVal
    if self.op == "RMW":
      result = (self.tid + ": {" + self.addr + " == " + self.readVal +
                  "; " + self.addr + " := " + self.writeVal + "}")
    if self.op == "SYNC":
      result = self.tid + ": sync"
    if self.times is not None:
      result = result + " " + self.times
    return result

  def isLoad(self):
    return self.op == "LD"

  def isUnreadStore(self):
    return self.op == "ST" and self.neverRead

  def isUnreadRMW(self):
    return self.op == "RMW" and self.neverRead

# =============================================================================
# Shrinker paramters
# =============================================================================

# Probability of dropping an operation on each iteration
dropRate = 0.05

# Number of retries during subsampling
maxRetries = 5

# =============================================================================
# Main
# =============================================================================

# Check args
if len(sys.argv) != 3:
  print "Usage: axe-big-shrink.py [MODEL] [FILE]"
  sys.exit()

# Open trace
if sys.argv[2] == "-":
  f = sys.stdin
else:
  f = open(sys.argv[2], 'r')
  if f == None:
    print "File not found: ", sys.argv[2]
    sys.exit()

# Globals
trace = []
omit = []
omitted = 0

# Parse a trace
for line in f:
  i = line.find("#")
  if i >= 0: line = line[0:i]
  if not line.strip(): continue
  trace.append(Op(line))
  omit.append(False)

# Play a trace to axe: return true if axe responds 'NO';
# otherwise return false.
def play():
  try:
    p = subprocess.Popen(['axe', 'check', sys.argv[1], '-'],
          stdin=subprocess.PIPE, stdout=subprocess.PIPE,
          stderr=subprocess.PIPE)
    for i in range(0, len(trace)):
      if not omit[i]: p.stdin.write(trace[i].show() + '\n')
    p.stdin.close()
    out = p.stdout.read()
    if out == "NO\n": return True
    else: return False
  except subprocess.CalledProcessError:
    return False

# Subsample trace using predicate
def subsample(pred):
  global omit, omitted
  undo = []
  left = len(trace) - omitted
  for i in range(0, len(trace)):
    if not omit[i] and pred(trace[i]):
      if random.uniform(0, left) < dropRate*left: 
        omit[i] = True
        undo.append(i)
  if not play():
    for i in undo:
      omit[i] = False
  else:
    omitted = omitted + len(undo)

# Subsample trace iteratively
def subsampleIter(pred):
  retries = 0
  total = len(trace)
  while retries < maxRetries:
    before = omitted
    subsample(pred)
    if before == omitted:
      retries = retries+1
    else:
      retries = 0
    print >> sys.stderr, "Omitted", omitted, "of", total, "        \r",
  print >> sys.stderr

# Drop all accesses to given address
def dropAddr(addr):
  global omit, omitted
  undo = []
  for i in range(0, len(trace)):
    if trace[i].addr is not None:
      if trace[i].addr == addr:
        omit[i] = True
        undo.append(i)
  if not play():
    for i in undo:
      omit[i] = False
  else:
    omitted = omitted + len(undo)

# Try dropping addresses, one at a time
def shrinkByAddr():
  total = len(trace)
  addrs = set()
  for op in trace:
    if op.addr is not None:
      addrs.add(op.addr);
  for addr in addrs:
    dropAddr(addr)
    print >> sys.stderr, "Omitted", omitted, "of", total, "        \r",
  print >> sys.stderr

# Find the writes that are never read
def computeUnread():
  reads = {}
  neverRead = []
  for i in range(0, len(trace)):
    if not omit[i]:
      op = trace[i]
      if op.readVal is not None:
        if op.addr not in reads: reads[op.addr] = set()
        reads[op.addr].add(op.readVal)
  for i in range(0, len(trace)):
    if not omit[i]:
      op = trace[i]
      if op.writeVal is not None:
        if op.writeVal not in reads.get(op.addr, set()):
          trace[i].neverRead = True

# Strategies for shrinking
def shrinkStrategies():
  print >> sys.stderr, "Shrinking by address"
  shrinkByAddr()
  print >> sys.stderr, "Subsampling loads"
  subsampleIter(Op.isLoad)
  computeUnread()
  print >> sys.stderr, "Subsampling unread stores"
  subsampleIter(Op.isUnreadStore)
  print >> sys.stderr, "Subsampling unread read-modify-writes"
  subsampleIter(Op.isUnreadRMW)

# Exhaustive shrinking procedure
def shrinkExhaustive():
  global omit, omitted
  total = len(trace)
  for i in reversed(range(0, len(trace))):
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
    shrinkExhaustive()
    print >> sys.stderr
    after = omitted
    count = count+1
    if before == after: break
  
# Shrink and print trace
random.seed()
shrinkStrategies()
shrinkFixedPoint()
sys.stderr.flush()
for i in range(0, len(trace)):
  if not omit[i]: print trace[i].show()
