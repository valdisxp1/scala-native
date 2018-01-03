#!/usr/bin/env bash

mkdir -p valdis_data
i=5
sbt "run --threads 1 --iterations 1 --format csv" | tee valdis_data/jvm_build.log
for t in {1..40}
do
  echo "running $i iterations on $t threads"
  sbt "run --threads $t --iterations $i --format csv" > valdis_data/jvm_${i}_$t.log
  cat valdis_data/jvm_${i}_$t.log | grep montecarlo | cut -d ' ' -f 2 | sed -r "s/\x1B\[([0-9]{1,2}(;[0-9]{1,2})?)?[mGK]//g" | tee valdis_data/jvm_${i}_$t.csv
done
