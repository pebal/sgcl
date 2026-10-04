[sgcl](../README.md) › [io](README.md)

# Benchmarks: io

The setup, the machine and how the timers are read are described with
[the benchmarks of the engine](../../garbage_collector/benchmarks.md). The cases of the module are in
`benchmarks/io/io.cpp` (built as `bench_io`), Go's in `benchmarks/go`, run by `benchmarks/compare.sh`.

## Child processes

A [command](command/README.md) that starts `true` and waits for it, `command("true").run()`, on an Apple M-series core
(`bench_io`, `benchmarks/go/exec`): the child is made with `posix_spawn` and its exit waited for on the reactor, where
Go forks on macOS. The SGCL column is from a run on 4 October 2026 at `-O3`, the best of three, in a clean environment (`env -i`; the
shell of the earlier runs set `MallocNanoZone=0`, which turns macOS's nano allocator off); the Go column is from
the earlier run, not run again.

| Case | SGCL | Go |
|---|---|---|
| one child, started and waited for | 1.0 ms | 1.9 ms |
| 32 children at once, per child | 0.4 ms | 0.8 ms |

## Errors as values

The errors of the module are values ([README](README.md)) rather than exceptions, because a missing file, a reset
connection or a full disk is an outcome the code handles where it occurs; and in a server a `throw` per dropped
connection, a microsecond and a lock on the unwinder each, would be the most expensive path of the program.
