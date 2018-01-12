#!/usr/bin/env bash

mkdir -p valdis_data
export SCALANATIVE_GC=none
sbt rebuild "benchmarks/run --threads 1 --iterations 1 --format csv" | tee valdis_data/build.log
i=5
for t in {1..16}
do
  echo "running $i iterations on $t threads"
  benchmarks/target/scala-2.11/benchmarks-out --threads $t --iterations $i --format csv | tee valdis_data/none_gc_${i}_$t.csv
done
