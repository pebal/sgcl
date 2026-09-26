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
        # a program that is not asked this question answers n/a, so that a
        # matrix still has a cell for it: it is a dash here and never
        # reaches bc, which would stop the script under set -e
        case $v in ''|n/a) continue;; esac
        if [ -z "$min" ] || [ "$(echo "$v < $min" | bc)" = 1 ]; then min=$v; fi
    done
    echo "${min:-—}"
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

echo
echo "## txt::format, ns per call (best of $RUNS)"
echo "| what | sgcl | std::format |"
echo "|---|---|---|"
# The ten both can be asked; std goes through format_to_n, the same
# contract as ours — which costs it, and the literal row is where that
# shows most
for op in simple padded centred mixed literal whole text five alloc runtime; do
    echo "| $op | $(best ns/op "$BIN/bench_format" sgcl $op) | $(best ns/op "$BIN/bench_format" std $op) |"
done
# And the six the standard has no answer for in C++20
for op in list list100 listwidth elements words pair; do
    echo "| $op | $(best ns/op "$BIN/bench_format" sgcl $op) | — |"
done

echo
echo "## txt search, microseconds for one pass over the text (best of $RUNS)"
echo "| text | question | a call at a time | a range | the mapping alone |"
echo "|---|---|---|---|---|"
# 256 KB is left to a hand run: a call at a time is quadratic and one
# pass over it takes the better part of ten seconds
for kb in 16 64; do
    for op in fold normalized; do
        echo "| $kb KB | $op | $(best us/op "$BIN/bench_search" oneshot $op $kb) | $(best us/op "$BIN/bench_search" prepared $op $kb) | $(best us/op "$BIN/bench_search" build $op $kb) |"
    done
    echo "| $kb KB | bytes | $(best us/op "$BIN/bench_search" oneshot bytes $kb) | $(best us/op "$BIN/bench_search" prepared bytes $kb) | — |"
done

echo
echo "## txt collate, ns per call (best of $RUNS)"
echo "| what | compare | sort key |"
echo "|---|---|---|"
for op in root polish numeric shifted case backwards; do
    echo "| $op | $(best ns/op "$BIN/bench_collate" compare $op) | $(best ns/op "$BIN/bench_collate" key $op) |"
done

echo
echo "## txt collate search, microseconds for one pass over the text (best of $RUNS)"
echo "| text | a call at a time | a range | the weighing alone |"
echo "|---|---|---|---|"
# 64 KB a call at a time is a second and a quarter a pass, so it is run
# once rather than best-of; 256 KB is left to a hand run altogether
for kb in 8 64; do
    echo "| $kb KB | $("$BIN/bench_collate" search oneshot $kb | sed 's/.*us\/op=\([0-9.]*\).*/\1/') | $(best us/op "$BIN/bench_collate" search prepared $kb) | $(best us/op "$BIN/bench_collate" search build $kb) |"
done

echo
echo "## txt bidi, nanoseconds for one pass over the text (best of $RUNS)"
echo "| text | levels | runs | visual order | mirroring | mirroring, levels given |"
echo "|---|---|---|---|---|---|"
for t in mixed plain arabic; do
    line="| $t"
    for v in levels runs order mirror mirror-levels; do
        line="$line | $(best ns/op "$BIN/bench_bidi" $v $t 1)"
    done
    echo "$line |"
done
echo "| mixed, 40 lines | $(best ns/op "$BIN/bench_bidi" levels mixed 40) | $(best ns/op "$BIN/bench_bidi" runs mixed 40) | $(best ns/op "$BIN/bench_bidi" order mixed 40) | $(best ns/op "$BIN/bench_bidi" mirror mixed 40) | $(best ns/op "$BIN/bench_bidi" mirror-levels mixed 40) |"

echo
echo "## txt identifiers, nanoseconds per name (best of $RUNS)"
echo "| what | ascii | latin | upper | compat | folded |"
echo "|---|---|---|---|---|---|"
# The five the capitals change nothing for: the quick check and the
# walks do the same work whether the letters are big or small
for op in identifier profile casefolded allowed level script; do
    echo "| $op | $(best ns/op "$BIN/bench_identifier" $op ascii) | $(best ns/op "$BIN/bench_identifier" $op latin) | — | $(best ns/op "$BIN/bench_identifier" $op compat) | $(best ns/op "$BIN/bench_identifier" $op folded) |"
done
# And the five that fold or decompose, where `upper` is the road with
# no quick check to save it — fold_case is there as the scale to read
# nfkc_casefold against
for op in casefold fold skeleton confusable marks; do
    line="| $op"
    for t in ascii latin upper compat folded; do
        line="$line | $(best ns/op "$BIN/bench_identifier" $op $t)"
    done
    echo "$line |"
done
echo
echo "## regex, ns per call, 4000 bytes of prose unless said otherwise (best of $RUNS)"
echo "| op | sgcl | std::regex |"
echo "|---|---|---|"
for op in literal boundary class alt date address dotstar upper hit line find all replace split unicode build blowup; do
    line="| $op"
    for v in sgcl std; do
        line="$line | $(best ns/op "$BIN/bench_regex" $v $op)"
    done
    echo "$line |"
done
echo
echo "## txt template, ns per render unless said otherwise (best of $RUNS)"
echo "| what | render | parse |"
echo "|---|---|---|"
for op in empty simple five spec branch rows rows100 pipe deep miss to; do
    echo "| $op | $(best ns/op "$BIN/bench_stencil" sgcl $op) | $(best ns/op "$BIN/bench_stencil" parse $op) |"
done
