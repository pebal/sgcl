[sgcl](../../README.md) › [slog](../README.md) › [logger](../logger.md)

# sgcl::slog::logger::logger

```cpp
/*(1)*/ logger() noexcept;
/*(2)*/ explicit logger(const options& o) noexcept;
/*(3)*/ explicit logger(const io::writer& out, slog::level l = slog::level::info) noexcept;
/*(4)*/ explicit logger(const slog::handler& h, slog::level l = slog::level::info) noexcept;
```

Constructs a logger with an output of its own.

1. Text lines on `io::stderr` from `info` up, in the local time: what the [default logger](../default_logger.md)
   starts as.
2. As [options](../options.md) say: the output (text or JSON on a writer, or a handler of the program), the level
   or a [level_var](../level_var.md), `source`, `utc`, `buffered` and the sampling.
3. Text lines on `out` from the level `l`: `options{.out = out, .level = l}`. An empty writer is no output: every
   record is lost and counted by [dropped](dropped.md), as a failed write is.
4. The records given to the handler `h` from the level `l`: `options{.handler = h, .level = l}`. Any type with
   `handle(const record&)` converts to a [handler](../handler.md), so `slog::logger(kept)` of a
   [memory](../memory.md) takes it. An empty handler is none: the lines go to `io::stderr`, as in (3).

## Parameters

| Parameter | Description |
|---|---|
| `o` | what the logger is made with |
| `out` | the writer the text lines go to |
| `h` | the handler the records are given to |
| `l` | the least level written; `info` by default |

## Complexity

Constant: a few managed allocations, and the sampler's counters with `sample_per`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::logger to_stderr;
    slog::logger text(io::stdout, slog::level::debug);
    slog::logger json(slog::options{.out = io::stdout, .json = true, .utc = true});
    slog::memory kept;
    slog::logger to_memory(kept, slog::level::warn);

    text.debug("text from debug");
    json.info("json");
    to_memory.info("below the level");
    to_memory.error("kept");
    println("{} kept", kept.size());
}
```

Output:

```text
time=2026-09-28T14:05:01.123+02:00 level=DEBUG msg="text from debug"
{"time":"2026-09-28T12:05:01.123456Z","level":"INFO","msg":"json"}
1 kept
```

## See also

- [options](../options.md)
- [with](with.md), [group](group.md): loggers that share this one's output
- [sgcl::slog::logger](../logger.md)
