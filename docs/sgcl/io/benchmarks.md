[sgcl](../README.md) › [io](README.md)

# Benchmarks: io

The setup, the machine and how the timers are read are described with
[the benchmarks of the engine](../../garbage_collector/benchmarks.md). The cases of the module are in
`benchmarks/io/io.cpp` (built as `bench_io`), Go's in `benchmarks/go`, run by `benchmarks/compare.sh`.

## Child processes

A [command](command/README.md) that starts `true` and waits for it, `command("true").run()`, on an Apple M-series core
(`bench_io`, `benchmarks/go/exec`): the child is made with `posix_spawn` and its exit waited for on the reactor, where
Go forks on macOS.

| Case | SGCL | Go |
|---|---|---|
| one child, started and waited for | 1.1 ms | 1.9 ms |
| 32 children at once, per child | 0.4 ms | 0.8 ms |

## Errors as values

The errors of the module are values ([README](README.md)) rather than exceptions, because a missing file, a reset
connection or a full disk is an outcome the code handles where it occurs; and in a server a `throw` per dropped
connection, a microsecond and a lock on the unwinder each, would be the most expensive path of the program.
