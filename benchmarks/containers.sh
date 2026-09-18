#!/bin/sh
# The container matrix: sgcl against std, ns per operation, the footprint
# of the built container (see containers/containers.cpp) and the peak RSS of the run.
#   benchmarks/containers.sh [build-dir=build-release] [n=1000000]
BIN=${1:-build-release}/benchmarks/bench_containers
N=${2:-1000000}
echo "| case | sgcl ns/op | std ns/op | sgcl footprint | std footprint | sgcl peak RSS | std peak RSS |"
echo "|---|---|---|---|---|---|---|"
for c in vector_push vector_iterate vector_ptr deque_push deque_iterate list_push list_iterate list_erase forward_list_push map_insert map_find map_iterate set_insert unordered_insert unordered_find unordered_erase unordered_ptr; do
    a=$("$BIN" sgcl $c $N); b=$("$BIN" std $c $N)
    f() { echo "$1" | tr ' ' '\n' | grep "^$2=" | cut -d= -f2 | tr -d 'MB'; }
    echo "| $c | $(f "$a" ns/op) | $(f "$b" ns/op) | $(f "$a" footprint) | $(f "$b" footprint) | $(f "$a" rss) | $(f "$b" rss) |"
done
