#!/usr/bin/env python

import matplotlib.pyplot as plt
import numpy as np
import sys

if len(sys.argv) < 3:
  print "usage: plot.py [output.png] [input1.txt] [input2.txt] .. [inputN.txt]"
  sys.exit()

Xs=[[] for arg in sys.argv[2:]]
Ys=[[] for arg in sys.argv[2:]]
count = 0
for inputFile in sys.argv[2:]:
  with open(inputFile) as f:
    for line in f:
      fields = line.split()
      Xs[count].append(float(fields[0]))
      Ys[count].append(float(fields[1]))
  count = count+1

fig = plt.figure(figsize=(4,3))
ax = fig.add_subplot(111)
ax.spines['top'].set_color('none')
ax.spines['right'].set_color('none')
ax.yaxis.set_ticks_position('left')
ax.xaxis.set_ticks_position('bottom')

plt.xticks([8192,16384,24576,32768], fontsize=8)
ax.set_xticklabels(["8K", "16K", "24K", "32K"])
plt.yticks([0,1,2,3,4,5], fontsize=8)
plt.gcf().subplots_adjust(bottom=0.15)

plt.xlim([8192,33000])
plt.ylim([0,5])

plt.ylabel("Time (s)", fontsize=9)
plt.xlabel("Memory operations", fontsize=9)

t32, = plt.plot(Xs[2][0:], Ys[2][0:], 'r.-', label="t=32")
t16, = plt.plot(Xs[1][0:], Ys[1][0:], 'k.-', label="t=16")
t4,  = plt.plot(Xs[0][0:], Ys[0][0:], 'b.-', label="t=4")

leg = ax.legend(loc=2, frameon=False, labelspacing=0, numpoints=1,
                handletextpad=0.2, handlelength=0.7)
for label in leg.get_texts():
  label.set_fontsize(8)

#plt.show()
plt.savefig(sys.argv[1])
