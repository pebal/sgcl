[sgcl](../README.md) › [net](README.md)

# Benchmarks: net

The setup, the machine and how the timers are read are described with
[the benchmarks of the engine](../../garbage_collector/benchmarks.md). The single-operation cases of the module (a
ping-pong over TCP, a stream, a connect, an HTTP request after another) are in `benchmarks/net/net.cpp` and
`benchmarks/go/net`, run by `benchmarks/compare.sh` (`CASES=net`). The server under load, HTTP/1.1, https and HTTP/2
against Go's, is on [the benchmarks of http](http/benchmarks.md).

## Single operations

Before and after the reactor per descriptor (2026-09-26: a descriptor registered once, edge-triggered, at its first
wait in a direction, where every wait made a one-shot registration before; the [reactor](../async/readable.md), and the
change told on [the benchmarks of http](http/benchmarks.md#the-reactor-per-descriptor-2026-09-26)). The same
binaries, five processes each alternated, the median (ns per operation; the stream in GB/s):

| Case | Before | After |
|---|---|---|
| async ping-pong (two tasks, rendezvous channels, per hop; no descriptor on the way) | 175.7 | 178.3 |
| net ping-pong (64 B there and back over TCP) | 20 187 | 19 731 |
| net connect (connect, accept, both closed) | 53 733 | 52 605 |
| net http_hello (a GET after another over one kept connection) | 38 033 | 37 353 |
| net stream (8 GB one way, 32 KB writes; eight pairs) | 4.10 GB/s, 6.5 s CPU | 4.02 GB/s, 7.4 s CPU |

The stream is the one case the change costs something in: a descriptor registered for good gets an event from the
kernel for every segment that arrives or every acknowledgement that frees room in the send buffer, whether a task
waits or not, where a one-shot registration made only by a wait got one event per wait. Each event is a
compare-exchange on the reactor's thread, but also a wake of that thread in the kernel: the process takes about 14
per cent more CPU for the same bytes, and the throughput moved by about 2 per cent, near the run-to-run spread. A
readiness kept from before a try is dropped before the next try (Go's `pollReset`); without that the stream was 4 per
cent slower, since a wait took the stale readiness and made one more system call for nothing.
