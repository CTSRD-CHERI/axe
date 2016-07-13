#!/bin/sh

DIRS="litmus random more-random"
MODELS="SC TSO PSO WMO POW"

if [ "$1" = "clean" ]; then
  echo "Cleaning... "
  for DIR in $DIRS; do
    echo $DIR
    rm -rf $DIR
  done
  echo "Done"
  exit 0
fi

for DIR in $DIRS; do
  if [ ! -d $DIR ]; then
    echo "Unpacking $DIR.tar.bz2"
    tar xjf $DIR.tar.bz2
  fi
done

for DIR in $DIRS; do
  for M in $MODELS; do
    echo Running $DIR tests against $M:
    ../src/axe test $M $DIR/tests.axe $DIR/$M.txt
    echo
  done
done

exit 0
