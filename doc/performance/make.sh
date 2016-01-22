#!/bin/sh

./plot.py tso.pdf results/tso/4t.txt results/tso/16t.txt results/tso/32t.txt
./plot.py wmo.pdf results/wmo/4t.txt results/wmo/16t.txt results/wmo/32t.txt
./plot.py pow.pdf results/pow/4t.txt results/pow/16t.txt results/pow/32t.txt
