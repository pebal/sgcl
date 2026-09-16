#!/bin/zsh
# The same measurements in every environment, the best of RUNS=3 runs:
# SGCL (sgcl:: and, as gc, gc::) and classic C++ (this tree, optimized build), Go (benchmarks/go, at
# its default GOGC=100) and Java with ZGC (benchmarks/java). Every run is one process
# under /usr/bin/time for the peak resident size. Prints one line per run:
# fields separated by `|`, the first the case, the last the numbers of
# that case. Java runs under a heap ceiling (-Xmx) of about twice SGCL's
# peak memory in the same case: with the default ceiling (a quarter of
# the machine) ZGC hardly collects in runs this short. VARIANTS="java-zgc"
# (or any list of variants) reruns those only; CASES="alloc copy" (or any
# list of cases) runs those only.
#   benchmarks/compare.sh [build-dir=build-release] [java=/opt/homebrew/opt/openjdk/bin]
set -e
BIN=${1:-build-release}/benchmarks
JBIN=${2:-/opt/homebrew/opt/openjdk/bin}
T=$(mktemp -d)
(cd benchmarks/go && go build -o "$T/" ./...)
"$JBIN/javac" -d "$T/jout" benchmarks/java/*.java
# OnSpinWaitInst=isb: Thread.onSpinWait as the pause instruction (HotSpot's
# default on arm64 is yield, a no-op on Apple silicon), for the backoff of
# the lock-free stack; harmless where nothing spins
JAVA=("$JBIN/java" -XX:+UseZGC -XX:+UnlockDiagnosticVMOptions -XX:OnSpinWaitInst=isb -Duser.language=en -Duser.country=US -cp "$T/jout")
CORES=$(getconf _NPROCESSORS_ONLN)
VARIANTS=${VARIANTS:-sgcl gc unique shared std go java-zgc}
CASES=${CASES:-alloc copy weak stack queue cstack cmap umap set cow chan bt graph lt string}
want() { [[ " $VARIANTS " == *" $1 "* ]]; }
case_() { [[ " $CASES " == *" $1 "* ]]; }

RUNS=${RUNS:-3}
field() { echo "$OUT" | tr ' ' '\n' | grep "^$1=" | tail -1 | cut -d= -f2 | tr -d 's'; }
run1() {   # run1 cmd... -> sets OUT (stdout) and RSS (MB)
    /usr/bin/time -l "$@" >"$T/out" 2>"$T/time" || true
    OUT=$(cat "$T/out"); RSS=$(grep 'maximum resident' "$T/time" | awk '{printf "%.0f", $1/1048576}')
}
KEY=wall   # the field the best of RUNS is chosen by (the run's other numbers come with it)
run() {    # run cmd... -> OUT and RSS of the best of RUNS runs: the lowest KEY, or the highest when KEY is ops/s
    local best="" bout brss
    for i in $(seq 1 $RUNS); do
        run1 "$@"
        local v=$(field $KEY)
        if [ -z "$best" ] || { [ "$KEY" = ops/s ] && [ "$(echo "$v > $best" | bc)" = 1 ]; } || { [ "$KEY" != ops/s ] && [ "$(echo "$v < $best" | bc)" = 1 ]; }; then
            best=$v; bout=$OUT; brss=$RSS
        fi
    done
    OUT=$bout; RSS=$brss
}

if case_ alloc; then
KEY=ns/alloc
echo "# allocation: alloc|size|threads|variant|ns per alloc|cpu s"
for s in 32 256; do for t in 1 4 $CORES; do
    for v in sgcl gc unique shared; do want $v && { run "$BIN/bench_allocation" $v $t $s; echo "alloc|$s|$t|$v|$(field ns/alloc)|$(field cpu)"; }; done
    want go && { run "$T/allocation" $t $s; echo "alloc|$s|$t|go|$(field ns/alloc)|$(field cpu)"; }
    want java-zgc && { run "${JAVA[@]}" -Xmx256m Allocation $t $s; echo "alloc|$s|$t|java-zgc|$(field ns/alloc)|$(field cpu)"; }
done; done
fi

if case_ copy; then
KEY=ns/copy
echo "# pointer copy, 50 M per thread: copy|threads|mode|variant|ns per copy (mode stack: a local; heap: a field; 4 threads share the target)"
for t in 1 4; do for m in stack heap; do
    extra=""; [ $t != 1 ] && extra=shared
    for v in sgcl gc unique shared; do want $v && { run "$BIN/bench_write_barrier" $v $t $m 1 $extra; echo "copy|$t|$m|$v|$(field ns/copy)"; }; done
    want go && { run "$T/write_barrier" $t $m 1 $extra; echo "copy|$t|$m|go|$(field ns/copy)"; }
    want java-zgc && { run "${JAVA[@]}" -Xmx256m WriteBarrier $t $m 1 $extra; echo "copy|$t|$m|java-zgc|$(field ns/copy)"; }
done; done
fi

if case_ weak; then
KEY=ns/op
echo "# weak pointer, 20 M per thread: weak|op|threads|variant|ns per op"
for op in lock expired copy make; do for t in 1 4; do
    for v in sgcl gc shared; do want $v && { run "$BIN/bench_weak_ptr" $v $t $op; echo "weak|$op|$t|$v|$(field ns/op)"; }; done
    want go && { run "$T/weak_ptr" $t $op; echo "weak|$op|$t|go|$(field ns/op)"; }
    want java-zgc && { run "${JAVA[@]}" -Xmx256m WeakPtr $t $op; echo "weak|$op|$t|java-zgc|$(field ns/op)"; }
done; done
fi

if case_ string; then
KEY=ns/op
echo "# string, 2 M in nodes: string|op|len|variant|ns per op"
for op in make copy hash1 hashn; do for len in 10 100; do
    for v in sgcl gc std; do want $v && { run "$BIN/bench_string" $v $op $len; echo "string|$op|$len|$v|$(field ns/op)"; }; done
    want go && { run "$T/string" $op $len; echo "string|$op|$len|go|$(field ns/op)"; }
    want java-zgc && { run "${JAVA[@]}" -Xmx2g Strings $op $len; echo "string|$op|$len|java-zgc|$(field ns/op)"; }
done; done
fi

if case_ stack; then
KEY=ns/op
echo "# lock-free stack, mixed, 1 M per thread: stack|threads|variant|ns per op"
for t in 1 4 16; do
    for v in sgcl gc unique shared; do want $v && { run "$BIN/bench_lockfree_stack" $v $t mixed 1000000; echo "stack|$t|$v|$(field ns/op)"; }; done
    want go && { run "$T/lockfree_stack" $t mixed 1000000; echo "stack|$t|go|$(field ns/op)"; }
    want java-zgc && { run "${JAVA[@]}" -Xmx256m LockFreeStack $t mixed 1000000; echo "stack|$t|java-zgc|$(field ns/op)"; }
done
fi

if case_ queue || case_ cstack; then
KEY=ns/op
echo "# concurrent queue and stack, mixed, 200 k per thread: conc|queue or stack|threads|variant|ns per op (mutex: the std container of shared_ptr under a mutex; shared: the algorithm on atomic shared_ptr)"
for c in queue stack; do
    { [ $c = queue ] && case_ queue; } || { [ $c = stack ] && case_ cstack; } || continue
    for t in 1 4 16; do
        for v in sgcl gc; do want $v && { run "$BIN/bench_concurrent" $c $v $t mixed 200000; echo "conc|$c|$t|$v|$(field ns/op)"; }; done
        want shared && { for v in mutex shared; do run "$BIN/bench_concurrent" $c $v $t mixed 200000; echo "conc|$c|$t|$v|$(field ns/op)"; done; }
        want go && { run "$T/concurrent" $c $t mixed 200000; echo "conc|$c|$t|go|$(field ns/op)"; }
        want java-zgc && { run "${JAVA[@]}" -Xmx256m Concurrent $c $t mixed 200000; echo "conc|$c|$t|java-zgc|$(field ns/op)"; }
    done
done
fi

if case_ cmap || case_ umap || case_ set; then
KEY=wall
echo "# concurrent map (a skip list), umap (a hash map) and set (a skip list), 200 k keys, 200 k operations per thread: conc|map or umap or set|threads|variant|ns per insert|ns per find|ns per mixed op|cpu s|rss MB (mutex: the std container under a mutex, rwlock: under a shared_mutex; umap in Go: sync.Map)"
mline() { echo "conc|$1|$2|$3|$(field insert)|$(field find)|$(field mixed)|$(field cpu)|$RSS"; }
for c in map umap set; do
    { [ $c = map ] && case_ cmap; } || { [ $c = umap ] && case_ umap; } || { [ $c = set ] && case_ set; } || continue
    for t in 1 4 16; do
        for v in sgcl gc; do want $v && { run "$BIN/bench_concurrent" $c $v $t 200000 200000; mline $c $t $v; }; done
        want shared && { run "$BIN/bench_concurrent" $c mutex $t 200000 200000; mline $c $t mutex; run "$BIN/bench_concurrent" $c rwlock $t 200000 200000; mline $c $t rwlock; }
        want go && { run "$T/concurrent" $c $t 200000 200000; mline $c $t go; }
        want java-zgc && { run "${JAVA[@]}" -Xmx128m Concurrent $c $t 200000 200000; mline $c $t java-zgc; }
    done
done
fi

if case_ cow; then
KEY=ns/read
echo "# copy_on_write over 64 longs, threads - 1 readers of 2 M snapshots each, one writer: conc|cow|threads|variant|ns per read|ns per write|writes (shared: shared_ptr with the atomic operations of <memory>; rwlock: the array under a shared_mutex, changed in place)"
cline() { echo "conc|cow|$1|$2|$(field ns/read)|$(field ns/write)|$(field writes)"; }
for t in 4 16; do
    for v in sgcl gc; do want $v && { run "$BIN/bench_concurrent" cow $v $t 2000000; cline $t $v; }; done
    want shared && { run "$BIN/bench_concurrent" cow shared $t 2000000; cline $t shared; run "$BIN/bench_concurrent" cow rwlock $t 2000000; cline $t rwlock; }
    want go && { run "$T/concurrent" cow $t 2000000; cline $t go; }
    want java-zgc && { run "${JAVA[@]}" -Xmx128m Concurrent cow $t 2000000; cline $t java-zgc; }
done
fi

if case_ chan; then
KEY=ns/op
echo "# channel, threads / 2 producers of 200 k items each, threads / 2 consumers: conc|chan|capacity|threads|variant|ns per item (mutex: std::queue under a mutex with condition variables; Go: its channel; Java: ArrayBlockingQueue, SynchronousQueue for 0)"
for cap in 0 64; do for t in 2 4 16; do
    for v in sgcl gc; do want $v && { run "$BIN/bench_concurrent" chan $v $t $cap 200000; echo "conc|chan|$cap|$t|$v|$(field ns/op)"; }; done
    want shared && { run "$BIN/bench_concurrent" chan mutex $t $cap 200000; echo "conc|chan|$cap|$t|mutex|$(field ns/op)"; }
    want go && { run "$T/concurrent" chan $t $cap 200000; echo "conc|chan|$cap|$t|go|$(field ns/op)"; }
    want java-zgc && { run "${JAVA[@]}" -Xmx128m Concurrent chan $t $cap 200000; echo "conc|chan|$cap|$t|java-zgc|$(field ns/op)"; }
done; done
fi

if case_ bt; then
KEY=wall
echo "# binary-trees: bt|depth|threads|variant|wall s|cpu s|rss MB"
for d in 16 18 21; do for t in 1 4; do
    for v in sgcl gc unique shared; do want $v && { run "$BIN/bench_binary_trees" $v $d $t; echo "bt|$d|$t|$v|$(field wall)|$(field cpu)|$RSS"; }; done
    want go && { run "$T/binary_trees" $d $t; echo "bt|$d|$t|go|$(field wall)|$(field cpu)|$RSS"; }
    XMX=256m; [ $d -ge 18 ] && XMX=512m; [ $d -ge 21 ] && XMX=1g
    want java-zgc && { run "${JAVA[@]}" -Xmx$XMX BinaryTrees $d $t; echo "bt|$d|$t|java-zgc|$(field wall)|$(field cpu)|$RSS"; }
done; done
fi

if case_ graph; then
KEY=ops/s
echo "# graph, 16 threads, 3 s: graph|roots|variant|insert p50|p99|p99.9|walk p50|p99|p99.9|drop-all ms|ops/s|cpu s|rss MB"
gfields() { local ins=$(echo "$OUT" | grep '^insert'); local wlk=$(echo "$OUT" | grep '^walk'); local p() { echo "$1" | tr ' ' '\n' | grep "^$2=" | cut -d= -f2; }; echo "$(p "$ins" p50)|$(p "$ins" p99)|$(p "$ins" p99.9)|$(p "$wlk" p50)|$(p "$wlk" p99)|$(p "$wlk" p99.9)|$(field max)|$(field ops/s)|$(field cpu)|$RSS"; }
for r in 4096 65536; do
    for v in sgcl gc shared; do want $v && { run "$BIN/bench_graph_latency" $v 16 3 $r; echo "graph|$r|$v|$(gfields)"; }; done
    want go && { run "$T/graph_latency" 16 3 $r; echo "graph|$r|go|$(gfields)"; }
    want java-zgc && { run "${JAVA[@]}" -Xmx4g GraphLatency 16 3 $r; echo "graph|$r|java-zgc|$(gfields)"; }
done
fi

if case_ lt; then
KEY=wall
echo "# large tree, 500 k small trees of depth 8 per thread: lt|big depth|threads|variant|wall s|cpu s|trees per s|rss MB|cycles (full) for SGCL"
SMALL=8; ITER=500000
ltline() { echo "lt|$1|$2|$3|$(field wall)|$(field cpu)|$(field trees/s)|$RSS|$(field cycles)${4:+ ($(field full))}"; }
for big in 22 24; do for t in 1 4; do
    XMX=1g; [ $big -ge 24 ] && XMX=4g
    for v in sgcl gc; do want $v && { run "$BIN/bench_large_tree" $v $big $SMALL $ITER $t; ltline $big $t $v full; }; done
    for v in unique shared; do want $v && { run "$BIN/bench_large_tree" $v $big $SMALL $ITER $t; ltline $big $t $v; }; done
    want go && { run "$T/large_tree" $big $SMALL $ITER $t; ltline $big $t go; }
    want java-zgc && { run "${JAVA[@]}" -Xmx$XMX LargeTree $big $SMALL $ITER $t; ltline $big $t java-zgc; }
done; done
fi
rm -rf "$T"
