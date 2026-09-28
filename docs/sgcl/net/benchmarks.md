# Benchmarks: the net module

The setup, the machine and how the timers are read are described with [the benchmarks of the engine](../../garbage_collector/benchmarks.md). The single-operation cases of the module (a ping-pong over TCP, a stream, a connect, an HTTP request after another) are in `benchmarks/net/net.cpp` and `benchmarks/go/net`, run by `benchmarks/compare.sh` (`CASES=net`). This page is the server under load.

## HTTP under load

`benchmarks/net/http_load/run.sh [build-dir]`: hello, world on `GET /`, served by the module's server (`server.cpp`, built as `bench_http_hello`, run with `SGCL_WORKERS=4` and `24` in its environment, as Go's server with `GOMAXPROCS`) and by Go's `net/http` (`gosrv`, `GOMAXPROCS=4` and `24`), loaded by `load/main.go`: C goroutines, one kept connection each, one request in flight per connection, 10 seconds, the loopback between them, both on the same machine (24 cores, Apple silicon; the desktop's own load of about three cores beside it, and the load generator itself takes five to six). Requests per second; the server's peak resident size; its user and system time over the requests served.

### The reactor per descriptor (2026-09-26)

Before: every wait of a socket made a one-shot registration with the kernel (`kevent` with `EV_ADD | EV_ONESHOT`) and an entry in the reactor's table, under the one mutex the reactor's thread took after every `kevent` of its own to find the waits of the events: at 4 workers a third of the workers' time went to that mutex and that system call, and the ceiling, 95 000 requests a second, did not move with the workers. After: a descriptor is registered once, edge-triggered, at its first wait in a direction, and a wait is a compare-exchange on the descriptor's slot in the reactor, the reactor's thread another on the event (the [reactor](../async/reactor.md)). Before: two runs, the median; after: the final state, one run (two runs of the change before its last step, the readiness dropped before a try, gave the same within 2 per cent). The machine under its desktop load, no other build or test running:

| server | workers | c | req/s before | req/s after | RSS MB before → after | CPU µs/request before → after |
|---|---|---|---|---|---|---|
| SGCL | 4 | 16 | 95 100 | 95 600 | 31 → 17 | 44.0 → 36.6 |
| SGCL | 4 | 64 | 93 700 | 106 200 | 33 → 18 | 44.8 → 33.0 |
| SGCL | 4 | 256 | 93 200 | 111 300 | 43 → 20 | 45.9 → 32.5 |
| SGCL | 24 | 16 | 83 200 | 96 700 | 81 → 33 | 73.9 → 46.0 |
| SGCL | 24 | 64 | 84 000 | 104 000 | 118 → 42 | 110.2 → 52.9 |
| SGCL | 24 | 256 | 90 800 | 108 500 | 131 → 48 | 111.8 → 52.3 |
| Go | 4 | 16 / 64 / 256 | 94 100 / 106 000 / 111 100 | 96 600 / 105 300 / 113 000 | 18 / 19 / 23 → 18 / 19 / 23 | 33.7 / 32.3 / 31.5 → 33.0 / 32.6 / 31.5 |
| Go | 24 | 16 / 64 / 256 | 59 200 / 66 600 / 93 100 | 58 400 / 68 500 / 97 600 | 26 / 28 / 35 → 27 / 28 / 35 | 122.8 / 137.0 / 105.3 → 124.1 / 126.4 / 91.9 |

"Before" is the server before the reactor per descriptor (2026-09-26). "After" is the state of 2026-09-27 (commit e269695: the reactor, and every step after it listed below, the io objects as handles included), one run of `run.sh` with Go in the same run.

What the numbers say. At four workers the server climbs with the connections, as Go's does, level with Go at every count of connections (within 2 per cent), and costs 17 to 29 per cent less CPU per request than before: 33 µs at 64 connections against Go's 32.6, its resident size at or below Go's. At 24 workers it gained 16 to 24 per cent of throughput and lost 38 to 53 per cent of its CPU per request, and it is far ahead of Go at the same count (Go's scheduler at 24 threads loses a third of its throughput on this machine), but still above itself at four in CPU: on one machine the load generator's six cores and the desktop's three take what 24 workers would need, and the workers that find no work spin before they sleep (`SGCL_WORKER_SPIN_US`). What is left per request is in the scheduler rather than the reactor: every wake of a task still allocates a node of the global queue from the reactor's thread (`Waiter::wake`), to be measured on its own.

The module's single-operation cases, the same binaries, five processes each alternated, the median (ns per operation; the stream in GB/s):

| case | before | after |
|---|---|---|
| async ping-pong (two tasks, rendezvous channels, per hop; no descriptor on the way) | 175.7 | 178.3 |
| net ping-pong (64 B there and back over TCP) | 20 187 | 19 731 |
| net connect (connect, accept, both closed) | 53 733 | 52 605 |
| net http_hello (a GET after another over one kept connection) | 38 033 | 37 353 |
| net stream (8 GB one way, 32 KB writes; eight pairs) | 4.10 GB/s, 6.5 s CPU | 4.02 GB/s, 7.4 s CPU |

The stream is the one case the change costs something in: a descriptor registered for good gets an event from the kernel for every segment that arrives or every acknowledgement that frees room in the send buffer, whether a task waits or not, where a one-shot registration made only by a wait got one event per wait. Each event is a compare-exchange on the reactor's thread, but also a wake of that thread in the kernel: the process takes about 14 per cent more CPU for the same bytes, and the throughput moved by about 2 per cent, near the run-to-run spread. A readiness kept from before a try is dropped before the next try (Go's `pollReset`); without that the stream was 4 per cent slower, since a wait took the stale readiness and made one more system call for nothing.

After the reactor (2026-09-27). The steps that followed it, each measured the same way on the loopback, took the 24-worker server's CPU per request at 64 connections from 58.7 to about 53 µs and its peak resident size from about 110 to about 42 MB, with the requests per second unchanged (the table's "after"): one timer per deadline on a descriptor instead of one per wait (one managed object per request fewer), the `Date:` line under a sequence lock instead of a mutex (−5 % CPU at 24 workers), a request's head read and its answer written without a coroutine frame per layer (21.7 → 10.1 managed objects per request, collector cycles −54 %), and the slots of the scheduler's rings cleared once taken (a finished frame no longer holds a request or a connection alive: resident size 110 → 42 MB at 24 workers). What remains between four workers (33 µs) and 24 (about 53) is the cost of 24 workers on a load that four serve: wakes 2.7 times as frequent, work stolen across 24 rings, a worker woken with nothing left to take. Every policy that woke fewer workers was measured against it and paid in latency or in liveness instead (the notes in DESIGN.md list them); the one change kept is a hand-over to the running worker that wakes nobody, since nobody else could take it.

### https (2026-09-27)

The same load over TLS 1.3 (`SCHEME=https run.sh`): the module's server behind `net::tls::listen` (the [tls](tls.md) listener, ALPN `http/1.1`), Go's behind `http.ListenAndServeTLS`, each with the ECDSA P-256 certificate of `tests/net/tls_testdata`; the load generator's connections are `crypto/tls` ones, their handshakes made before the clock, so the handshake is outside the measurement and what is measured is the record layer of both sides (AES-128-GCM, the group X25519MLKEM768, both servers' choice). Two rounds, each server alternated with Go in the same run; the two rounds within 4 per cent of each other in every cell. Requests per second (round 1 / round 2), the ratio of the means, the server's peak resident size and its CPU per request (round 1):

| workers | c | SGCL req/s | Go req/s | SGCL / Go | RSS MB SGCL / Go | CPU µs/request SGCL / Go |
|---|---|---|---|---|---|---|
| 4 | 16 | 98 600 / 99 000 | 95 900 / 95 200 | 1.03 | 35 / 20 | 40.2 / 33.7 |
| 4 | 64 | 107 500 / 105 800 | 107 000 / 106 800 | 1.00 | 49 / 20 | 37.9 / 32.7 |
| 4 | 256 | 111 400 / 109 400 | 113 400 / 112 400 | 0.98 | 92 / 31 | 37.7 / 31.7 |
| 24 | 16 | 92 500 / 92 200 | 58 600 / 57 900 | 1.59 | 58 / 27 | 54.3 / 129.4 |
| 24 | 64 | 102 400 / 103 200 | 70 200 / 72 800 | 1.44 | 94 / 29 | 60.4 / 138.2 |
| 24 | 256 | 105 900 / 107 100 | 97 200 / 97 500 | 1.09 | 143 / 40 | 62.3 / 105.7 |

What the numbers say. TLS barely moves either server at this size: a hello-world response is one record each way, and both servers stay within a few per cent of their plain-HTTP throughput above. At four workers the module's server is level with Go (98 to 103 per cent) at 16 to 20 per cent more CPU per request (38 to 40 µs against 32 to 34; over plain HTTP the gap in the table above is 1 to 11 per cent, so most of the difference is in the TLS layer, not yet profiled). At 24 workers it is 9 to 59 per cent ahead, as over plain HTTP. Its resident size is 1.7 to 3.5 times Go's, growing with the connections: each connection keeps about 100 KB of record buffers (the framer's two records, two for sealing, one of plaintext, the assembler's) in an unmanaged block of its own for as long as it lives, where Go's `crypto/tls` sizes its buffers per record.

### HTTP/2 (2026-09-28)

The same machine and load generator, the requests HTTP/2 (`REQUEST="-proto h2 ..." run.sh`): `load` drives Go's `http.Transport` with its HTTP/2 (C goroutines, one request in flight each, as streams of the Transport's connections: one connection up to the server's MAX_CONCURRENT_STREAMS of 250, two at c = 256), its first request made before the clock. h2 over TLS 1.3 by ALPN (the module's `server::serve_tls`, Go's `ListenAndServeTLS`, the certificate of `tests/net/tls_testdata`) and h2c by prior knowledge on the plain port (`server::h2c`, Go's `http.Protocols` with unencrypted HTTP/2); `GET /` (hello, world) and `POST /echo` with 1000 bytes echoed. Each cell one run of 2 seconds, the module's server and Go's in turn; requests per second, their ratio, the server's CPU (user and system) per request and its peak resident size:

| case | workers | c | req/s SGCL | req/s Go | SGCL / Go | CPU µs/req SGCL | CPU µs/req Go | RSS MB SGCL | RSS MB Go |
|---|---|---|---|---|---|---|---|---|---|
| h2 TLS GET | 4 | 16 | 63 394 | 45 894 | 1.38 | 45.2 | 48.0 | 24 | 19 |
| h2 TLS GET | 4 | 64 | 69 942 | 54 511 | 1.28 | 41.2 | 41.5 | 25 | 19 |
| h2 TLS GET | 4 | 256 | 90 402 | 74 609 | 1.21 | 35.3 | 37.8 | 34 | 22 |
| h2 TLS GET | 24 | 16 | 62 220 | 42 031 | 1.48 | 51.3 | 64.2 | 34 | 23 |
| h2 TLS GET | 24 | 64 | 67 784 | 48 892 | 1.39 | 49.4 | 60.4 | 38 | 25 |
| h2 TLS GET | 24 | 256 | 86 500 | 51 601 | 1.68 | 48.3 | 96.4 | 54 | 26 |
| h2 TLS POST 1 KB | 4 | 16 | 36 088 | 30 403 | 1.19 | 93.5 | 77.8 | 34 | 19 |
| h2 TLS POST 1 KB | 4 | 64 | 35 278 | 31 747 | 1.11 | 94.7 | 76.1 | 33 | 19 |
| h2 TLS POST 1 KB | 4 | 256 | 52 157 | 50 001 | 1.04 | 72.2 | 58.3 | 38 | 19 |
| h2 TLS POST 1 KB | 24 | 16 | 35 468 | 27 347 | 1.30 | 103.2 | 104.4 | 46 | 23 |
| h2 TLS POST 1 KB | 24 | 64 | 34 968 | 27 930 | 1.25 | 104.0 | 105.4 | 49 | 24 |
| h2 TLS POST 1 KB | 24 | 256 | 52 420 | 34 334 | 1.53 | 91.8 | 137.2 | 60 | 24 |
| h2c GET | 4 | 16 | 66 784 | 44 794 | 1.49 | 43.3 | 50.5 | 22 | 18 |
| h2c GET | 4 | 64 | 71 420 | 51 840 | 1.38 | 40.1 | 46.1 | 23 | 19 |
| h2c GET | 4 | 256 | 88 764 | 69 874 | 1.27 | 36.2 | 40.8 | 27 | 20 |
| h2c GET | 24 | 16 | 65 627 | 40 886 | 1.61 | 49.4 | 67.4 | 33 | 22 |
| h2c GET | 24 | 64 | 69 994 | 46 857 | 1.49 | 49.1 | 63.5 | 42 | 23 |
| h2c GET | 24 | 256 | 86 938 | 53 103 | 1.64 | 47.2 | 88.2 | 52 | 24 |
| h2c POST 1 KB | 4 | 16 | 41 926 | 29 594 | 1.42 | 79.3 | 81.4 | 29 | 18 |
| h2c POST 1 KB | 4 | 64 | 40 822 | 29 252 | 1.40 | 80.0 | 82.9 | 28 | 18 |
| h2c POST 1 KB | 4 | 256 | 54 480 | 47 945 | 1.14 | 66.4 | 61.8 | 32 | 18 |
| h2c POST 1 KB | 24 | 16 | 41 104 | 27 160 | 1.51 | 91.5 | 103.5 | 47 | 22 |
| h2c POST 1 KB | 24 | 64 | 40 382 | 26 708 | 1.51 | 91.4 | 105.6 | 49 | 22 |
| h2c POST 1 KB | 24 | 256 | 54 660 | 34 762 | 1.57 | 85.3 | 131.0 | 56 | 23 |

Managed objects created per request (the collector's cycle log, 4 workers, c = 64): 19 for GET over h2 and h2c, 28 to 30 for the POST (25 and 34 to 36 before a response went out in one write and the streams' table and fields were taken in place), against about 10 and 16 over HTTP/1.1. No system allocation on the path of a request in either protocol (the allocation probe of DESIGN 277): only the collector's own, per cycle. HPACK's encoder (a linear search of the static table and the dynamic one per field) costs 42 to 50 ns a field, 170 ns for a response's four fields: under half a per cent of a request.

What the numbers say. The module's server serves more requests than Go's in every cell, 1.04 to 1.68 times, with lower latencies (p50 and p99 in the output of the run). Its CPU per request is at or below Go's for GET in every cell (35 to 45 µs against 38 to 50 at four workers; about half of Go's at 24 workers and c = 256) and for h2c POST but at four workers and c = 256 (66 against 62). POST over TLS at four workers is the one gap: 72 to 95 µs against 58 to 78, 20 to 24 per cent above Go; over h2c the same request is level with Go, so the difference lies in reading the request's body through TLS (Go's client sends a request's HEADERS and DATA as two records), not yet profiled. At 24 workers every POST is at or below Go's. Its resident size is 1.2 to 2.5 times Go's and grows with the workers, the pools per worker of the collector (by design: locality before the size of the heap).