#!/bin/sh
# Runs the benchmark matrix against the binaries in a build directory and
# prints the best of N runs for each cell. Optimized builds only.
#   benchmarks/run.sh [build-dir=build-release] [runs=3]
set -e
BIN=${1:-build-release}/benchmarks
RUNS=${2:-3}
CORES=$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc)

best() {   # best (minimum) of the numeric field named $1 over RUNS runs of the command
    field=$1; shift
    min=""
    for i in $(seq 1 "$RUNS"); do
        v=$("$@" | tr ' ' '\n' | grep "^$field=" | head -1 | cut -d= -f2 | tr -d 's')
        if [ -z "$min" ] || [ "$(echo "$v < $min" | bc)" = 1 ]; then min=$v; fi
    done
    echo "$min"
}

echo "## allocation, ns per allocation (best of $RUNS)"
echo "| threads | size | sgcl | shared_ptr | unique_ptr |"
echo "|---|---|---|---|---|---|"
for t in 1 4 "$CORES"; do
    for s in 8 32 256; do
        line="| $t | $s B"
        for v in sgcl shared unique; do
            line="$line | $(best ns/alloc "$BIN/bench_allocation" $v "$t" $s)"
        done
        echo "$line |"
    done
done

echo
echo "## pointer copy, ns per copy (best of $RUNS)"
echo "| threads | mode | targets | sgcl | shared_ptr |"
echo "|---|---|---|---|---|---|"
for t in 1 4; do
    for m in stack heap; do
        for tg in 1 4096; do
            extra=""; [ "$t" != 1 ] && extra=shared
            echo "| $t | $m | $tg${extra:+ $extra} | $(best ns/copy "$BIN/bench_write_barrier" sgcl "$t" $m $tg $extra) | $(best ns/copy "$BIN/bench_write_barrier" shared "$t" $m $tg $extra) |"
        done
    done
done

echo
echo "## stack root, ns per construction (best of $RUNS)"
echo "| mode | 1 thread | 4 threads |"
echo "|---|---|---|"
for m in null copy; do
    echo "| $m | $(best ns/construction "$BIN/bench_stack_root" 1 $m) | $(best ns/construction "$BIN/bench_stack_root" 4 $m) |"
done

echo
echo "## weak_ptr, ns per operation (best of $RUNS)"
echo "| op, threads | sgcl | std::weak_ptr |"
echo "|---|---|---|---|"
for op in lock copy make; do
    for t in 1 4; do
        echo "| $op, $t | $(best ns/op "$BIN/bench_weak_ptr" sgcl "$t" $op) | $(best ns/op "$BIN/bench_weak_ptr" shared "$t" $op) |"
    done
done
echo
echo "## lock-free stack, mixed, ns per push or pop, 1 M per thread (best of $RUNS)"
echo "| threads | sgcl | shared_ptr |"
echo "|---|---|---|---|"
for t in 1 4 16; do
    line="| $t"
    for v in sgcl shared; do
        line="$line | $(best ns/op "$BIN/bench_lockfree_stack" $v "$t" mixed)"
    done
    echo "$line |"
done

echo
echo "## concurrent queue and stack, mixed, ns per push or pop, 200 k per thread (best of $RUNS)"
echo "| container, threads | sgcl | std::shared_ptr | atomic shared_ptr |"
echo "|---|---|---|---|---|"
for c in queue stack; do
    for t in 1 4 16; do
        line="| $c, $t"
        for v in sgcl mutex shared; do
            line="$line | $(best ns/op "$BIN/bench_concurrent" $c $v "$t" mixed)"
        done
        echo "$line |"
    done
done

echo
echo "## concurrent map, umap and set, 200 k keys, ns per insert / find / mixed op, 200 k ops per thread (best of $RUNS)"
echo "| container, threads | sgcl | std, std::mutex | std, std::shared_mutex |"
echo "|---|---|---|---|---|"
for c in map umap set; do
    for t in 1 4 16; do
        line="| $c, $t"
        for v in sgcl mutex rwlock; do
            line="$line | $(best insert "$BIN/bench_concurrent" $c $v "$t") / $(best find "$BIN/bench_concurrent" $c $v "$t") / $(best mixed "$BIN/bench_concurrent" $c $v "$t")"
        done
        echo "$line |"
    done
done

echo
echo "## copy_on_write over 64 longs, ns per read / per write, threads - 1 readers of 2 M snapshots each, one writer (best of $RUNS)"
echo "| threads | sgcl | shared_ptr, atomic | shared_mutex |"
echo "|---|---|---|---|---|"
for t in 4 16; do
    line="| $t"
    for v in sgcl shared rwlock; do
        line="$line | $(best ns/read "$BIN/bench_concurrent" cow $v "$t") / $(best ns/write "$BIN/bench_concurrent" cow $v "$t")"
    done
    echo "$line |"
done

echo
echo "## channel, ns per item, threads / 2 producers of 200 k items each, threads / 2 consumers (best of $RUNS)"
echo "| capacity, threads | sgcl | std::queue + mutex + condition variables |"
echo "|---|---|---|---|"
for cap in 0 64; do
    for t in 2 4 16; do
        line="| $cap, $t"
        for v in sgcl mutex; do
            line="$line | $(best ns/op "$BIN/bench_concurrent" chan $v "$t" $cap)"
        done
        echo "$line |"
    done
done

echo
echo "## binary-trees, depth 18, wall / cpu seconds (best wall of $RUNS)"
echo "| threads | sgcl | shared_ptr | unique_ptr | raw |"
echo "|---|---|---|---|---|---|"
for t in 1 4; do
    line="| $t"
    for v in sgcl shared unique raw; do
        w=$(best wall "$BIN/bench_binary_trees" $v 18 "$t")
        c=$("$BIN/bench_binary_trees" $v 18 "$t" | tr ' ' '\n' | grep "^cpu=" | cut -d= -f2)
        line="$line | $w / $c"
    done
    echo "$line |"
done

echo
echo "## graph latency, $CORES threads, 3 s (single run)"
for v in sgcl shared; do
    echo "### $v"
    "$BIN/bench_graph_latency" $v "$CORES" 3
done
