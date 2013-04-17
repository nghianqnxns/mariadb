#!/bin/bash

set -e

test $# -ge 4

bin=$1; shift
size=$1; shift
runs=$1; shift
abortcode=$1; shift

$bin -C -n $size
$bin -C -i 0 -n $size
for (( i = 1; i < $runs; i++ ))
do
    echo -n "$i: " && date
    set +e
    $bin -c -i $i -n $size -X novalgrind 2>dir.checkpoint_stress.tdb/error.$i
    test $? -eq $abortcode || exit 1
    set -e
    grep -q 'HAPPY CRASH' dir.checkpoint_stress.tdb/error.$i
done
