[sgcl](../../README.md) › [net](../README.md) › [imap](README.md)

# Benchmarks: imap

The setup, the machine and how the timers are read are described with [the benchmarks of the
engine](../../../garbage_collector/benchmarks.md). Go's standard library has no IMAP, and go-imap is not taken (no
module outside the standard library): the Go side, `benchmarks/go/net/imap.go`, is a minimal client written by hand
from RFC 9051 with the standard library alone — commands written whole, responses read line by line with their
literals, FETCH's items taken apart into a struct per message — against the same server as the module's client, the
module's own (`bench_imap server`). The pairs therefore compare the two clients over one server; the server's rate is
`imap_pipeline`'s, and Python's imaplib loads it from several connections in `benchmarks/net/imap_load.py`.

## Single operations

`CASES=imap VARIANTS="sgcl go" benchmarks/compare.sh build-release`: one server for both sides (INBOX of 1000 messages
of about 1 KB, "Big" of 100 messages of 10 KB, a memory_backend), every case one process a side, the best of three.
Nanoseconds per operation, less is better; the last column is SGCL's time over Go's. The cases:

- `imap_noop`: NOOP and its answer, per command;
- `imap_fetch_flags`: UID FETCH 1:* (UID FLAGS) of INBOX, per message;
- `imap_fetch_envelope`: UID FETCH 1:* (UID FLAGS ENVELOPE) of INBOX, per message;
- `imap_fetch_body`: UID FETCH 1:* (UID BODY.PEEK[]) of Big, per message;
- `imap_search`: UID SEARCH FROM "carol" of INBOX, the server reading every header, per message searched;
- `imap_append`: APPEND of a 2 KB message, per message;
- `imap_pipeline`: 1000 NOOPs written at once and their answers read, per command (the server's rate, the client
  reading by hand on both sides).

| Case | SGCL | Go | SGCL/Go |
|---|---|---|---|
| imap_noop | — | — | — |
| imap_fetch_flags | — | — | — |
| imap_fetch_envelope | — | — | — |
| imap_fetch_body | — | — | — |
| imap_search | — | — | — |
| imap_append | — | — | — |
| imap_pipeline | — | — | — |

The table is filled by a run on a quiet machine. `imap_noop` and `imap_append` are a round trip each between two
processes, and so measure the reactor's wake of a task when the answer comes more than the client: `imap_rawnoop`
(`bench_imap imap_rawnoop sgcl`), the same NOOP over a raw connection read by hand, shows the same time.

## See also

- [sgcl::net::imap](README.md)
- [Benchmarks: net](../benchmarks.md)
