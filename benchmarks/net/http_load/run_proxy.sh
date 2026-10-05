#!/bin/zsh
# The reverse proxy load test: the module's http::reverse_proxy
# (proxy.cpp, bench_http_proxy) in front of the module's server
# (bench_http_hello), against Go's httputil.ReverseProxy (goproxy) in front
# of Go's net/http (gosrv); each proxy and its backend with W workers
# (SGCL_WORKERS, GOMAXPROCS), loaded by load/main.go: C connections to the
# proxy, one request in flight each, D seconds. The sides alternate, PAIRS
# times over (the machine may be shared: compare a pair, not two runs far
# apart). For each run one line:
#
#   http_proxy|side|workers|c|req/s|p50|p99|proxy rss MB|proxy cpu us/req
#
# rss and cpu are the proxy's alone (its backend and the load generator
# not counted).
#
#   benchmarks/net/http_load/run_proxy.sh [build-dir=build-release]
#   SIDES="sgcl go" WORKERS=4 CONNS="1 64" DURATION=5s PAIRS=3
#   REQUEST="-path /stream?size=1048576&parts=16": streaming through the
#   proxy (the case's name gets TAG=stream); default GET / (hello, world)
set -e
HERE=${0:A:h}
BIN=${1:-build-release}/benchmarks
T=$(mktemp -d)
if [ -n "$GO_BIN" ]; then
    cp "$GO_BIN/load" "$GO_BIN/gosrv" "$GO_BIN/goproxy" "$T/"
else
    (cd "$HERE/load" && go build -o "$T/load" main.go)
    (cd "$HERE/gosrv" && go build -o "$T/gosrv" main.go)
    (cd "$HERE/goproxy" && go build -o "$T/goproxy" main.go)
fi
SIDES=${SIDES:-sgcl go}
WORKERS=${WORKERS:-4}
CONNS=${CONNS:-1 64}
DURATION=${DURATION:-5s}
PAIRS=${PAIRS:-3}
PORT=${PORT:-18090}
BACK=$((PORT + 1))
REQUEST=${REQUEST:-}
TAG=${TAG:-}

wait_port() {
    local tries=0
    until nc -z 127.0.0.1 $1 2>/dev/null; do
        sleep 0.1
        tries=$((tries + 1))
        if [ $tries -gt 100 ]; then return 1; fi
    done
}

# one side: its backend, its proxy under /usr/bin/time, the load, both stopped
one() {   # one name workers c backend-cmd -- proxy-cmd...
    local name=$1 w=$2 c=$3 backend=$4
    shift 4
    env SGCL_WORKERS=$w GOMAXPROCS=$w ${=backend} ":$BACK" >/dev/null 2>&1 &
    local bp=$!
    wait_port $BACK || { echo "http_proxy|$name|$w|$c|no backend"; kill $bp; return; }
    /usr/bin/time -l "$@" >"$T/srv.out" 2>"$T/time" &
    local tp=$!
    wait_port $PORT || { echo "http_proxy|$name|$w|$c|no proxy"; kill $bp; kill $tp; return; }
    local out=$("$T/load" -addr 127.0.0.1:$PORT -c $c -d $DURATION ${=REQUEST})
    pkill -TERM -P $tp 2>/dev/null || true
    wait $tp 2>/dev/null || true
    kill $bp 2>/dev/null || true
    wait $bp 2>/dev/null || true
    local rps=$(echo "$out" | sed -E 's/.*: ([0-9]+) req\/s.*/\1/')
    local errs=$(echo "$out" | sed -E 's/.*errors ([0-9]+),.*/\1/')
    local p50=$(echo "$out" | sed -E 's/.*p50 ([^,]+),.*/\1/')
    local p99=$(echo "$out" | sed -E 's/.*p99 (.*)$/\1/')
    local user=$(grep -E '^ *[0-9.]+ real' "$T/time" | awk '{print $3}')
    local sys=$(grep -E '^ *[0-9.]+ real' "$T/time" | awk '{print $5}')
    local rss=$(grep 'maximum resident' "$T/time" | awk '{printf "%.0f", $1/1048576}')
    local secs=${DURATION%s}
    local cpu=$(awk -v u=$user -v s=$sys -v r=$rps -v d=$secs 'BEGIN { if (r > 0) printf "%.1f", (u + s) * 1e6 / (r * d); else print "-" }')
    echo "http_proxy|$name|$w|$c|$rps|$p50|$p99|$rss|$cpu|errors $errs"
    sleep 1
}

SUFFIX=""
if [ -n "$TAG" ]; then SUFFIX="-$TAG"; fi
echo "# http_proxy|side|workers|c|req/s|p50|p99|proxy rss MB|proxy cpu us/req"
for w in ${=WORKERS}; do
    for c in ${=CONNS}; do
        for pair in $(seq $PAIRS); do
            for side in ${=SIDES}; do
                if [ "$side" = sgcl ]; then one sgcl$SUFFIX $w $c "$BIN/bench_http_hello" env SGCL_WORKERS=$w "$BIN/bench_http_proxy" ":$PORT" "http://127.0.0.1:$BACK"; fi
                if [ "$side" = go ]; then one go$SUFFIX $w $c "$T/gosrv" env GOMAXPROCS=$w "$T/goproxy" ":$PORT" "http://127.0.0.1:$BACK"; fi
            done
        done
    done
done
rm -rf "$T"
