#!/bin/zsh
# The HTTP load test: hello, world served by the module's server
# (server.cpp, bench_http_hello with SGCL_WORKERS=4 and 24 in its
# environment) and by Go's net/http (gosrv, GOMAXPROCS=4 and 24), loaded by
# load/main.go: C goroutines, one kept connection each, one request in
# flight per connection, D seconds. Both sides on this machine, the
# loopback between them. For each server, each count of workers and each
# C one line:
#
#   http_load|server|workers|c|req/s|p50|p99|rss MB|cpu us/req
#
# rss is the server's peak resident size, cpu its user and system time
# over the requests served (the load generator's own time not counted).
#
#   benchmarks/net/http_load/run.sh [build-dir=build-release]
#   SERVERS="sgcl go" WORKERS="4 24" CONNS="16 64 256" DURATION=10s
#   SCHEME=https: both servers over TLS 1.3 with the test certificate of the tree
#   (tests/net/tls_testdata, ECDSA P-256), the generator's connections TLS ones
#   made before the clock, the handshake outside the measurement (crypto/tls;
#   ALPN http/1.1; plain http dials inside the measured time); the servers are named
#   sgcl-https and go-https
#   REQUEST="-method POST -path /echo -body 1000" shapes the request, "-proto h2"
#   makes it HTTP/2 (h2c on http, h2 by ALPN on https); TAG=name is added to the
#   servers' names (sgcl-h2-post1k)
set -e
HERE=${0:A:h}
BIN=${1:-build-release}/benchmarks
T=$(mktemp -d)
# GO_BIN=dir: load and gosrv built there beforehand (nothing built while measuring)
if [ -n "$GO_BIN" ]; then
    cp "$GO_BIN/load" "$GO_BIN/gosrv" "$T/"
else
    (cd "$HERE/load" && go build -o "$T/load" main.go)
    (cd "$HERE/gosrv" && go build -o "$T/gosrv" main.go)
fi
SERVERS=${SERVERS:-sgcl go}
WORKERS=${WORKERS:-4 24}
CONNS=${CONNS:-16 64 256}
DURATION=${DURATION:-10s}
PORT=${PORT:-18090}
SCHEME=${SCHEME:-http}
CERTS="$HERE/../../../tests/net/tls_testdata"
TLS_ARGS=()
LOAD_ARGS=()
SUFFIX=""
REQUEST=${REQUEST:-}
TAG=${TAG:-}
if [ "$SCHEME" = https ]; then
    TLS_ARGS=("$CERTS/ecdsa.pem" "$CERTS/ecdsa.key")
    LOAD_ARGS=(-ca "$CERTS/ca.pem")
    SUFFIX=-https
fi

# one server under /usr/bin/time, loaded once, then stopped: prints the line
one() {   # one name workers c cmd...
    local name=$1 w=$2 c=$3
    shift 3
    /usr/bin/time -l "$@" >"$T/srv.out" 2>"$T/time" &
    local tp=$!
    local tries=0
    until nc -z 127.0.0.1 $PORT 2>/dev/null; do
        sleep 0.1
        tries=$((tries + 1))
        if [ $tries -gt 100 ]; then echo "http_load|$name|$w|$c|no server"; kill $tp 2>/dev/null; return; fi
    done
    local out=$("$T/load" -addr 127.0.0.1:$PORT -c $c -d $DURATION "${LOAD_ARGS[@]}" ${=REQUEST})
    pkill -TERM -P $tp 2>/dev/null || true
    wait $tp 2>/dev/null || true
    local rps=$(echo "$out" | sed -E 's/.*: ([0-9]+) req\/s.*/\1/')
    local p50=$(echo "$out" | sed -E 's/.*p50 ([^,]+),.*/\1/')
    local p99=$(echo "$out" | sed -E 's/.*p99 (.*)$/\1/')
    local user=$(grep -E '^ *[0-9.]+ real' "$T/time" | awk '{print $3}')
    local sys=$(grep -E '^ *[0-9.]+ real' "$T/time" | awk '{print $5}')
    local rss=$(grep 'maximum resident' "$T/time" | awk '{printf "%.0f", $1/1048576}')
    local secs=${DURATION%s}
    local cpu=$(awk -v u=$user -v s=$sys -v r=$rps -v d=$secs 'BEGIN { if (r > 0) printf "%.1f", (u + s) * 1e6 / (r * d); else print "-" }')
    echo "http_load|$name|$w|$c|$rps|$p50|$p99|$rss|$cpu"
    sleep 1   # the port's connections in TIME_WAIT settle before the next server
}

echo "# http_load|server|workers|c|req/s|p50|p99|rss MB|cpu us/req (server's user+sys over the requests)"
if [ "$SCHEME" = https ]; then
    echo "# https: TLS 1.3, the connections and their handshakes made before the clock (the handshake outside the measurement)"
fi
if [ -n "$TAG" ]; then SUFFIX="$SUFFIX-$TAG"; fi
for w in ${=WORKERS}; do
    for c in ${=CONNS}; do
        if [[ " $SERVERS " == *" sgcl "* ]]; then one sgcl$SUFFIX $w $c env SGCL_WORKERS=$w "$BIN/bench_http_hello" ":$PORT" "${TLS_ARGS[@]}"; fi
        if [[ " $SERVERS " == *" go "* ]]; then one go$SUFFIX $w $c env GOMAXPROCS=$w "$T/gosrv" ":$PORT" "${TLS_ARGS[@]}"; fi
    done
done
rm -rf "$T"
