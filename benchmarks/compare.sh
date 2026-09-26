#!/bin/zsh
# The same measurements in every environment, the best of RUNS=3 runs:
# SGCL and classic C++ (this tree, optimized build), Go (benchmarks/go, at
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
VARIANTS=${VARIANTS:-sgcl unique shared std go java-zgc}
CASES=${CASES:-alloc copy weak stack queue cstack cmap umap set cow chan bqueue pqueue intern wmap spsc cache im bcast async io math net hash json xml time bt graph lt string}
want() { [[ " $VARIANTS " == *" $1 "* ]]; }
want_im() { case "$1" in sgcl|std) want "$1";; *) "$BIN/bench_immutable" vector "$1" 1 > /dev/null 2>&1;; esac; }   # an immer variant when the binary has it (-DSGCL_IMMER_INCLUDE)
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
    for v in sgcl unique shared; do want $v && { run "$BIN/bench_allocation" $v $t $s; echo "alloc|$s|$t|$v|$(field ns/alloc)|$(field cpu)"; }; done
    want go && { run "$T/allocation" $t $s; echo "alloc|$s|$t|go|$(field ns/alloc)|$(field cpu)"; }
    want java-zgc && { run "${JAVA[@]}" -Xmx256m Allocation $t $s; echo "alloc|$s|$t|java-zgc|$(field ns/alloc)|$(field cpu)"; }
done; done
fi

if case_ copy; then
KEY=ns/copy
echo "# pointer copy, 50 M per thread: copy|threads|mode|variant|ns per copy (mode stack: a local; heap: a field; 4 threads share the target)"
for t in 1 4; do for m in stack heap; do
    extra=""; [ $t != 1 ] && extra=shared
    for v in sgcl unique shared; do want $v && { run "$BIN/bench_write_barrier" $v $t $m 1 $extra; echo "copy|$t|$m|$v|$(field ns/copy)"; }; done
    want go && { run "$T/write_barrier" $t $m 1 $extra; echo "copy|$t|$m|go|$(field ns/copy)"; }
    want java-zgc && { run "${JAVA[@]}" -Xmx256m WriteBarrier $t $m 1 $extra; echo "copy|$t|$m|java-zgc|$(field ns/copy)"; }
done; done
fi

if case_ weak; then
KEY=ns/op
echo "# weak pointer, 20 M per thread: weak|op|threads|variant|ns per op"
for op in lock expired copy make; do for t in 1 4; do
    for v in sgcl shared; do want $v && { run "$BIN/bench_weak_ptr" $v $t $op; echo "weak|$op|$t|$v|$(field ns/op)"; }; done
    want go && { run "$T/weak_ptr" $t $op; echo "weak|$op|$t|go|$(field ns/op)"; }
    want java-zgc && { run "${JAVA[@]}" -Xmx256m WeakPtr $t $op; echo "weak|$op|$t|java-zgc|$(field ns/op)"; }
done; done
fi

if case_ string; then
KEY=ns/op
echo "# string, 2 M in nodes: string|op|len|variant|ns per op"
for op in make copy hash1 hashn; do for len in 10 100; do
    for v in sgcl std; do want $v && { run "$BIN/bench_string" $v $op $len; echo "string|$op|$len|$v|$(field ns/op)"; }; done
    want go && { run "$T/string" $op $len; echo "string|$op|$len|go|$(field ns/op)"; }
    want java-zgc && { run "${JAVA[@]}" -Xmx2g Strings $op $len; echo "string|$op|$len|java-zgc|$(field ns/op)"; }
done; done
fi

if case_ stack; then
KEY=ns/op
echo "# lock-free stack, mixed, 1 M per thread: stack|threads|variant|ns per op"
for t in 1 4 16 32 64; do
    for v in sgcl unique shared; do want $v && { run "$BIN/bench_lockfree_stack" $v $t mixed 1000000; echo "stack|$t|$v|$(field ns/op)"; }; done
    want go && { run "$T/lockfree_stack" $t mixed 1000000; echo "stack|$t|go|$(field ns/op)"; }
    want java-zgc && { run "${JAVA[@]}" -Xmx256m LockFreeStack $t mixed 1000000; echo "stack|$t|java-zgc|$(field ns/op)"; }
done
fi

if case_ queue || case_ cstack; then
KEY=ns/op
echo "# concurrent queue and stack, mixed, 200 k per thread: conc|queue or stack|threads|variant|ns per op (mutex: the std container of shared_ptr under a mutex; shared: the algorithm on atomic shared_ptr)"
for c in queue stack; do
    { [ $c = queue ] && case_ queue; } || { [ $c = stack ] && case_ cstack; } || continue
    for t in 1 4 16 32 64; do
        want sgcl && { v=sgcl; run "$BIN/bench_concurrent" $c $v $t mixed 200000; echo "conc|$c|$t|$v|$(field ns/op)"; }
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
    for t in 1 4 16 32 64; do
        want sgcl && { v=sgcl; run "$BIN/bench_concurrent" $c $v $t 200000 200000; mline $c $t $v; }
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
for t in 4 16 32 64; do
    want sgcl && { v=sgcl; run "$BIN/bench_concurrent" cow $v $t 2000000; cline $t $v; }
    want shared && { run "$BIN/bench_concurrent" cow shared $t 2000000; cline $t shared; run "$BIN/bench_concurrent" cow rwlock $t 2000000; cline $t rwlock; }
    want go && { run "$T/concurrent" cow $t 2000000; cline $t go; }
    want java-zgc && { run "${JAVA[@]}" -Xmx128m Concurrent cow $t 2000000; cline $t java-zgc; }
done
fi

if case_ chan; then
KEY=ns/op
echo "# channel, threads / 2 producers of 200 k items each, threads / 2 consumers: conc|chan|capacity|threads|variant|ns per item (sgcl: threads; task: sgcl::tasks on the scheduler; mutex: std::queue under a mutex with condition variables; Go: its channel, goroutines; Java: ArrayBlockingQueue, SynchronousQueue for 0)"
for cap in 0 64; do for t in 2 4 16 32 64; do
    want sgcl && { for v in sgcl task; do run "$BIN/bench_concurrent" chan $v $t $cap 200000; echo "conc|chan|$cap|$t|$v|$(field ns/op)"; done; }
    want shared && { run "$BIN/bench_concurrent" chan mutex $t $cap 200000; echo "conc|chan|$cap|$t|mutex|$(field ns/op)"; }
    want go && { run "$T/concurrent" chan $t $cap 200000; echo "conc|chan|$cap|$t|go|$(field ns/op)"; }
    want java-zgc && { run "${JAVA[@]}" -Xmx128m Concurrent chan $t $cap 200000; echo "conc|chan|$cap|$t|java-zgc|$(field ns/op)"; }
done; done
fi

if case_ bqueue; then
KEY=ns/op
echo "# bounded queue, threads / 2 producers of 200 k items each, threads / 2 consumers: conc|bqueue|capacity|threads|variant|ns per item (sgcl: concurrent::bounded_queue between threads; Go: its channel of the capacity, goroutines; Java: ArrayBlockingQueue)"
for cap in 64 1024; do for t in 2 4 16 32 64; do
    want sgcl && { run "$BIN/bench_concurrent" bqueue sgcl $t $cap 200000; echo "conc|bqueue|$cap|$t|sgcl|$(field ns/op)"; }
    want go && { run "$T/concurrent" chan $t $cap 200000; echo "conc|bqueue|$cap|$t|go|$(field ns/op)"; }
    want java-zgc && { run "${JAVA[@]}" -Xmx128m Concurrent chan $t $cap 200000; echo "conc|bqueue|$cap|$t|java-zgc|$(field ns/op)"; }
done; done
fi

if case_ pqueue; then
KEY=ns/op
echo "# priority queue, mixed, 200 k per thread, over an empty queue and over one holding 100 k: conc|pqueue|prefill|threads|variant|ns per op (sgcl: concurrent::priority_queue; mutex: std::priority_queue of shared_ptr under a std::mutex; Go: container/heap under a mutex; Java: PriorityBlockingQueue)"
for p in 0 100000; do for t in 1 4 16 32 64; do
    want sgcl && { run "$BIN/bench_concurrent" pqueue sgcl $t 200000 $p; echo "conc|pqueue|$p|$t|sgcl|$(field ns/op)"; }
    want shared && { run "$BIN/bench_concurrent" pqueue mutex $t 200000 $p; echo "conc|pqueue|$p|$t|mutex|$(field ns/op)"; }
    want go && { run "$T/concurrent" pqueue $t mixed 200000 $p; echo "conc|pqueue|$p|$t|go|$(field ns/op)"; }
    want java-zgc && { run "${JAVA[@]}" -Xmx256m Concurrent pqueue $t mixed 200000 $p; echo "conc|pqueue|$p|$t|java-zgc|$(field ns/op)"; }
done; done
fi

if case_ intern; then
KEY=ns/op
echo "# intern, 1000 distinct strings, 1 M per thread: conc|intern|threads|variant|ns per intern (sgcl: intern<string>::make; Go: unique.Make; Java: String.intern)"
for t in 1 4 16 32 64; do
    want sgcl && { run "$BIN/bench_concurrent" intern sgcl $t 1000 1000000; echo "conc|intern|$t|sgcl|$(field ns/op)"; }
    want go && { run "$T/concurrent" intern $t 1000 1000000; echo "conc|intern|$t|go|$(field ns/op)"; }
    want java-zgc && { run "${JAVA[@]}" -Xmx128m Concurrent intern $t 1000 1000000; echo "conc|intern|$t|java-zgc|$(field ns/op)"; }
done
fi

if case_ wmap; then
KEY=ns/op
echo "# concurrent weak map, 10 k objects, 1 M operations per thread, half insertions and half lookups: conc|wmap|threads|variant|ns per op (sgcl: concurrent::weak_map; Java: WeakHashMap under Collections.synchronizedMap; Go has none)"
for t in 1 4 16 32 64; do
    want sgcl && { run "$BIN/bench_concurrent" wmap sgcl $t 10000 1000000; echo "conc|wmap|$t|sgcl|$(field ns/op)"; }
    want java-zgc && { run "${JAVA[@]}" -Xmx128m Concurrent wmap $t 10000 1000000; echo "conc|wmap|$t|java-zgc|$(field ns/op)"; }
done
fi

if case_ spsc; then
KEY=ns/op
echo "# spsc queue, one producer of 200 k items, one consumer: conc|spsc|capacity|variant|ns per item (sgcl: spsc_queue; the bounded queue, Go and Java at two threads are in the bqueue table)"
for cap in 64 1024; do
    want sgcl && { run "$BIN/bench_concurrent" spsc sgcl 2 $cap 200000; echo "conc|spsc|$cap|sgcl|$(field ns/op)"; }
done
fi

if case_ cache; then
KEY=ns/op
echo "# LRU cache of 10 k entries over 20 k keys, 1 M operations per thread, 90% gets: conc|cache|threads|variant|ns per op|hit rate (sgcl: concurrent::cache; mutex: unordered_map and a list under a mutex, exact LRU; Go and Java have none in their standard libraries)"
for t in 1 4 16 32 64; do
    want sgcl && { run "$BIN/bench_concurrent" cache sgcl $t 10000 1000000; echo "conc|cache|$t|sgcl|$(field ns/op)|$(field hits)"; }
    want shared && { run "$BIN/bench_concurrent" cache mutex $t 10000 1000000; echo "conc|cache|$t|mutex|$(field ns/op)|$(field hits)"; }
done
fi

if case_ bcast; then
KEY=ns/op
echo "# broadcast, one sender of 1 M values, the subscribers receiving them all: conc|bcast|subscribers|variant|ns per value|received (sgcl: broadcast, the subscribers threads; task: the subscribers sgcl::tasks on the scheduler; Go: a channel per subscriber, a goroutine each, the sender sending every value to each, its library having no broadcast; Java has none either)"
for k in 1 4 16 64; do
    want sgcl && { for v in sgcl task; do run "$BIN/bench_concurrent" bcast $v $k 1024 1000000; echo "conc|bcast|$k|$v|$(field ns/op)|$(field received)"; done; }
    want go && { run "$T/concurrent" bcast $k 1024 1000000; echo "conc|bcast|$k|go|$(field ns/op)|$(field received)"; }
done
fi

if case_ im; then
echo "# immutable containers against immer, one thread (bench_immutable; the immer variants need -DSGCL_IMMER_INCLUDE): im|vector|variant|ns push_back (a version each)|ns get at random|ns set (a version each)|ns per element built at once"
for v in sgcl immer immer-unsafe; do want_im "$v" && { run "$BIN/bench_immutable" vector "$v" 1000000; echo "im|vector|$v|$(field push_back)|$(field get)|$(field set)|$(field build)"; }; done
echo "# im|list|variant|ns push_front (a version each, 1 M)|ns per element walked|ns pop_front (a version each)"
for v in sgcl std; do want "$v" && { run "$BIN/bench_immutable" list "$v" 1000000; echo "im|list|$v|$(field push_front)|$(field walk)|$(field pop_front)"; }; done
echo "# im|map|variant|ns insert (a version each, 200 k random long keys)|ns find|ns per element built at once (std: std::map in place)"
for v in sgcl immer immer-unsafe std; do want_im "$v" && { run "$BIN/bench_immutable" map "$v" 200000; echo "im|map|$v|$(field insert)|$(field find)|$(field build)"; }; done
echo "# im|map-builder|variant|ns per element built through the builder (200 k random long keys; immer: its transient)|ns find in the map built|ns per change of an edit through one builder (a tenth of the keys set, a tenth erased)|ns per change made a version each"
for v in builder immer-builder; do { [ $v = builder ] && want sgcl; } || { [ $v = immer-builder ] && want_im immer; } || continue; run "$BIN/bench_immutable" map "$v" 200000; echo "im|map-builder|$v|$(field build)|$(field find)|$(field edit)|$(field edit_each)"; done
fi


if case_ async; then
KEY=ns/op
echo "# the async module on the scheduler: async|case|variant|ns per operation (yield: a worker's yield; exyield: an executor's; strand: a round trip on_workers + on(strand); await: a task that returns at once, awaited; spawn: one spawned and awaited; whenall: when_all of two, per task; timeout: with_timeout(t, 1h) of a task that returns at once, per race; select: a channel case served at once beside a timeout case of an hour; cv: a turn handed between two tasks through a condition variable; pingpong: two tasks over two rendezvous channels, per hop; generator: an async::generator's value; mutex: an uncontended async lock; Go: goroutines, time.After, sync.Cond, a range-over-func iterator; Java: virtual threads, CompletableFuture.orTimeout, SynchronousQueue, Condition)"
for c in yield exyield strand await spawn whenall timeout select cv pingpong generator mutex; do
    want sgcl && { run "$BIN/bench_async" $c sgcl; echo "async|$c|sgcl|$(field ns/op)"; }
    case $c in exyield|strand) continue;; esac
    want go && { run "$T/async" $c; echo "async|$c|go|$(field ns/op)"; }
    case $c in select|generator) continue;; esac
    want java-zgc && { run "${JAVA[@]}" -Xmx256m Async $c; echo "async|$c|java-zgc|$(field ns/op)"; }
done
fi

if case_ io; then
KEY=ns/op
echo "# the io module, a child process: io|case|variant|ns per operation (run: command(\"true\").run() from a thread, posix_spawn and a wait; output: command(\"echo\", \"hello\").output(), a pipe and a copying task; asyncrun: async_run() from a task, the exit on the reactor; parallel: 32 async_run() of true in flight; Go: os/exec, Run, Output, 32 goroutines)"
for c in run output asyncrun parallel; do
    want sgcl && { run "$BIN/bench_io" $c sgcl; echo "io|$c|sgcl|$(field ns/op)"; }
    want go && { run "$T/exec" $c; echo "io|$c|go|$(field ns/op)"; }
done
fi

if case_ math; then
KEY=ns/op
echo "# the math module: math|op|n|variant|ns per operation (big_integer: add, mul, sqr, div of n limbs, tostr and parse of n decimal digits, sum += x of n limbs, fact of n, pow, modpow of n bits, gcd, modinv, sqrt, prime of n bits, pi, factorial, binomial, fib, harmonic; random: one draw, shuffle of a thousand; Go: math/big and math/rand/v2 ChaCha8)"
for c in "add 1" "add 100" "mul 10" "mul 100" "mul 1000" "mul 10000" "sqr 100" "sqr 3000" "div 100" "div 1000" "div 10000" "tostr 10000" "tostr 1000000" "parse 10000" "parse 1000000" "sum 100" "fact 1000" "pow 1000" "modpow 1024" "modpow 2048" "modpow 4096" "gcd 100" "gcd 1000" "modinv 100" "modinv 1000" "sqrt 1000" "sqrt 10000" "prime 1024" "pi 10000" "pi 100000" "factorial 100000" "binomial 10000" "fib 1000000" "harmonic 10000" "small 0" "uint64 0" "intn 0" "double 0" "normal 0" "shuffle 0"; do read -r op n <<< "$c"
    want sgcl && { run "$BIN/bench_math" sgcl $op $n; echo "math|$op|$n|sgcl|$(field ns/op)"; }
    want go && { run "$T/math" $op $n; echo "math|$op|$n|go|$(field ns/op)"; }
done
fi

if case_ net; then
KEY=ns/op
echo "# the net module: net|case|variant|ns per operation (pingpong: 64 B there and back over one TCP connection, per round trip; stream: 1 GB one way, 32 KB writes, per byte; connect: connect and accept on the loopback, per connection; parse, format: ip_address against netip.Addr, per address; url: net::url::parse against net/url; http_parse: a request head parsed; http_hello: a GET and its response over one kept connection)"
for c in pingpong stream connect parse format url http_parse http_hello; do
    want sgcl && { run "$BIN/bench_net" $c sgcl; echo "net|$c|sgcl|$(field ns/op)"; }
    want go && { run "$T/net" $c; echo "net|$c|go|$(field ns/op)"; }
done
fi

if case_ json; then
KEY=ns/op
echo "# the encoding module's JSON and CSV: json|op|corpus|variant|ns per operation|MB/s (Go: encoding/json/v2, jsontext, encoding/csv; the corpora of nativejson-benchmark in ~/Programming/oracles/nativejson)"
for c in "parse twitter" "parse citm_catalog" "parse canada" "write twitter" "pretty twitter" "tokens twitter" "skip twitter" "tokens strings" "typed twitter" "stringify twitter" "csv 10000" "csvtyped 10000" "csvwrite 10000"; do read -r op arg <<< "$c"
    want sgcl && { run "$BIN/bench_json" sgcl $op $arg; echo "json|$op|$arg|sgcl|$(field ns/op)|$(field MB/s)"; }
    want go && { run "$T/json" $op $arg; echo "json|$op|$arg|go|$(field ns/op)|$(field MB/s)"; }
done
fi

if case_ xml; then
KEY=ns/op
echo "# the encoding module's XML: xml|op|books|variant|ns per operation|MB/s (a catalog of 5000 books, 1.47 MB; Go: encoding/xml)"
for op in tokens stream tree write typed; do
    want sgcl && { run "$BIN/bench_xml" sgcl $op 5000; echo "xml|$op|5000|sgcl|$(field ns/op)|$(field MB/s)"; }
    want go && { run "$T/xml" $op 5000; echo "xml|$op|5000|go|$(field ns/op)|$(field MB/s)"; }
done
fi

if case_ time; then
KEY=ns/op
echo "# the time module, one call over 4096 instants: time|case|variant|ns per call (std: libc++ std::format, system_clock; Go: package time, http.ParseTime)"
for c in now fields_utc fields_local offset_now offset_random local_to_instant format_rfc3339 format_http format_pattern format_string parse_rfc3339 parse_http parse_pattern duration_string duration_parse load_cached; do
    want sgcl && { run "$BIN/bench_time" $c sgcl; echo "time|$c|sgcl|$(field ns/op)"; }
    case $c in now|fields_utc|format_rfc3339|format_http|format_pattern) want std && { run "$BIN/bench_time" $c std; echo "time|$c|std|$(field ns/op)"; };; esac
    want go && { run "$T/time" $c; echo "time|$c|go|$(field ns/op)"; }
done
want sgcl && { run1 "$BIN/bench_time" load_cold sgcl; echo "time|load_cold|sgcl|$(field ns/op)"; }
want go && { run1 "$T/time" load_cold; echo "time|load_cold|go|$(field ns/op)"; }
fi

if case_ hash; then
KEY=ns/op
echo "# the hash module, one call over random bytes: hash|case|length|variant|ns per call|GB/s (Go: hash/crc32, crc64, adler32, fnv, maphash; github.com/zeebo/xxh3, github.com/dchest/siphash; an FNV Reset, Write and Sum)"
for c in crc32 crc32c crc64 crc64_iso adler32 fnv32a fnv64a fnv128a xxh3_64 xxh3_128 xxh3_64-seeded maphash siphash; do for len in 16 64 1024 65536 1048576; do
    want sgcl && { run "$BIN/bench_hash" $c sgcl $len; echo "hash|$c|$len|sgcl|$(field ns/op)|$(field GB/s)"; }
    want go && { run "$T/hash" $c $len; echo "hash|$c|$len|go|$(field ns/op)|$(field GB/s)"; }
done; done
for len in 16 64 1024 65536; do want sgcl && { run "$BIN/bench_hash" string-hash sgcl $len; echo "hash|string-hash|$len|sgcl|$(field ns/op)|$(field GB/s)"; }; done
fi

if case_ bt; then
KEY=wall
echo "# binary-trees: bt|depth|threads|variant|wall s|cpu s|rss MB"
for d in 16 18 21; do for t in 1 4; do
    for v in sgcl unique shared; do want $v && { run "$BIN/bench_binary_trees" $v $d $t; echo "bt|$d|$t|$v|$(field wall)|$(field cpu)|$RSS"; }; done
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
    for v in sgcl shared; do want $v && { run "$BIN/bench_graph_latency" $v 16 3 $r; echo "graph|$r|$v|$(gfields)"; }; done
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
    want sgcl && { v=sgcl; run "$BIN/bench_large_tree" $v $big $SMALL $ITER $t; ltline $big $t $v full; }
    for v in unique shared; do want $v && { run "$BIN/bench_large_tree" $v $big $SMALL $ITER $t; ltline $big $t $v; }; done
    want go && { run "$T/large_tree" $big $SMALL $ITER $t; ltline $big $t go; }
    want java-zgc && { run "${JAVA[@]}" -Xmx$XMX LargeTree $big $SMALL $ITER $t; ltline $big $t java-zgc; }
done; done
fi
rm -rf "$T"
