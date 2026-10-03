[sgcl](../README.md) › [slog](README.md)

# sgcl::slog::options

```cpp
#include "sgcl/slog/logger.h"   // or "sgcl/slog.h"

namespace sgcl::slog {
    struct options {
        io::writer out = io::writer(io::stderr);
        slog::handler handler;
        slog::level level = slog::level::info;
        optional<slog::level_var> level_var;
        bool json = false;
        bool source = false;
        bool utc = false;
        bool buffered = false;
        uint32_t sample_first = 0;
        uint32_t sample_then = 0;
        sgcl::duration sample_per = {};
    };
}
```

`sgcl::slog::options` is what a [logger](logger.md) is made with: Go's `HandlerOptions` with the choice of handler.
By default text lines on `io::stderr` from `info` up, in the local time. A plain struct, filled by designated
initializers in the order of its fields, naming what differs from the defaults:
`slog::logger log(slog::options{.out = file, .level = slog::level::debug, .json = true});`.

## Member objects

| Member | Description |
|---|---|
| `out` | the [writer](../io/writer.md) the text or JSON lines go to; `io::stderr` by default. Without `buffered`, every record is one `write` to it from the thread that logs, so it takes writes from many threads at once (a file opened for appending, `io::stderr`, a connection). An empty writer is no output: every record is lost, counted by [dropped](logger/dropped.md) and said once on `io::stderr` |
| `handler` | when set, the records go to this [handler](handler.md) of the program instead of `out`, and `json` is not read; empty by default |
| `level` | the least level written; `info` by default |
| `level_var` | when set, the least level is read from this [level_var](level_var.md) at every record, in place of `level`; empty by default |
| `json` | JSON lines, slog's `JSONHandler`; text lines, slog's `TextHandler`, otherwise; `false` by default |
| `source` | where the call is, written as `source` (slog's `AddSource`): `source=src/main.cpp:42` in text, `"source":{"function":"int main()","file":"src/main.cpp","line":42}` in JSON; `false` by default |
| `utc` | the time in UTC (`Z`), not the local zone (`+02:00`); `false` by default |
| `buffered` | the lines gathered per worker of the scheduler, a batch of 32 KB each, written by one write when it is full, at a record of `warn` and up (itself included), when the worker goes to sleep, at [flush](logger/flush.md) and at exit (`atexit`, `at_quick_exit`). The order across workers holds only within each worker; every line has its time. A thread that is no worker shares one batch with the others. For a writer that must not take writes from many threads (a `buffered_writer`), and where many lines a second go to a file. Each batch has one lock that only a `flush()` or the exit meets from another thread; a line longer than a batch is written alone. A logger with a `handler` is not batched: the handler is given each record as it comes. `false` by default |
| `sample_first` | sampling, as zap's sampler: of the records of one level and one message in each span of `sample_per`, the first `sample_first` are written; `0` by default |
| `sample_then` | and after them every `sample_then`-th; `0`, the default, is none more |
| `sample_per` | the span the records are counted in; zero, the default, is no sampling. The spans are counted from the epoch, per worker, without a lock between workers (a hash of the level and the message into 256 counters each). The records left out are said once, in a line of their own before the first record after their span: `level=WARN msg="records sampled out" count=6` |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::logger log(slog::options{
        .out = io::stdout, .level = slog::level::debug, .json = true, .utc = true});
    log.debug("json from debug");

    slog::logger sampled(slog::options{
        .out = io::stdout, .sample_first = 2, .sample_then = 3, .sample_per = 1 * hour});
    for (int i : range(10)) {
        sampled.info("tick", "i", i);
    }
}
```

Output:

```text
{"time":"2026-09-28T12:05:01.123456Z","level":"DEBUG","msg":"json from debug"}
time=2026-09-28T14:05:01.123+02:00 level=INFO msg=tick i=0
time=2026-09-28T14:05:01.123+02:00 level=INFO msg=tick i=1
time=2026-09-28T14:05:01.123+02:00 level=INFO msg=tick i=4
time=2026-09-28T14:05:01.123+02:00 level=INFO msg=tick i=7
```

## See also

- [logger](logger.md): what is made of the options
- [level_var](level_var.md), [handler](handler.md): what the options may name
- [sgcl::slog](README.md)
