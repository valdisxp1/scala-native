#!/usr/bin/env bash

mkdir -p valdis_data
i=5
for t in {1..40}
do
  echo "running $i iterations on $t threads"
  sbt "run --threads $t --iterations $i --format csv" | tee valdis_data/none_gc_${i}_$t.csv
done
