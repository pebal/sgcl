[sgcl](../README.md) › [slog](README.md)

# sgcl::slog::logger

```cpp
#include "sgcl/slog/logger.h"   // or "sgcl/slog.h"

namespace sgcl::slog {
    class logger;
}
```

`sgcl::slog::logger` is slog's `Logger`: what a record is written as — text or JSON on a [writer](../io/writer.md),
or given to a [handler](handler.md) of the program — from which level, with which attributes of its own. It is made
once, by its constructor, from [options](options.md), from a writer and a level for text lines, or from a handler and
a level; nothing of it changes later but a [level_var](level_var.md)'s level. The verbs —
[debug, info, warn, error and log](logger/log.md) — write a record; [with](logger/with.md) and
[group](logger/group.md) give a new logger and leave the one they were called on as it was.

Against Go, the handler is chosen by the options rather than built and passed, and the pairs of a record are checked
by the compiler. The logger the free functions write through is the [default logger](default_logger.md).

## Rules

- **Children.** `with` and `group` give a new logger: `base.with("k", 1)` is a child, `base` still writes without
  `k`.
- **The output is shared.** The copies of a logger and the loggers made from it by `with` and `group` share its
  output: the writer, its batches when buffered, the count of the records lost. Each constructor makes an output of
  its own.
- **Where a logger lives.** A handle, one tracked word: on a stack, in a task, in a managed object; in a global or a
  `std` container a [rooted](../core/rooted.md) of it
  (`rooted<slog::logger> log(slog::logger(slog::options{.out = file, .json = true}));`). The default logger is the
  module's own, kept that way.
- **From every thread.** The verbs may be called from any thread at once; a record is one write to the writer, or
  one call of the handler ([The rules](README.md#the-rules)).

## Member functions

| Function | Description |
|---|---|
| [(constructor)](logger/logger.md) | constructs the logger from options, a writer or a handler |
| `(destructor)` | drops the word; the output lives while a copy or a child holds it |
| `operator=` | copies or moves the word of another logger; a move is a copy, as a `tracked_ptr`'s is: the logger moved from is the same logger still |

#### Children

| Function | Description |
|---|---|
| [with](logger/with.md) | a logger that writes these attributes in every record |
| [group](logger/group.md) | a logger whose later attributes are in a group |

#### Records

| Function | Description |
|---|---|
| [log, debug, info, warn, error](logger/log.md) | writes a record of a level |

#### Observers

| Function | Description |
|---|---|
| [enabled](logger/enabled.md) | checks whether a record of a level is written |
| [dropped](logger/dropped.md) | the records whose write failed |

#### Output

| Function | Description |
|---|---|
| [flush](logger/flush.md) | writes the batches of a buffered logger now |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    io::buffer out;
    slog::level_var least(slog::level::warn);
    slog::logger log(slog::options{.out = out, .level_var = least, .utc = true});

    log.info("not written");
    least.set(slog::level::debug);
    log.info("written", "attempt", 2);

    auto child = log.with("user", "ala").group("req");
    child.warn("slow", "path", "/users", "took", 250 * millisecond);
    log.info("the parent as it was");

    print(out.text());
    println("{} records lost, debug {}", log.dropped(), log.enabled(slog::level::debug));
}
```

Output:

```text
time=2026-09-28T12:05:01.123Z level=INFO msg=written attempt=2
time=2026-09-28T12:05:01.123Z level=WARN msg=slow user=ala req.path=/users req.took=250ms
time=2026-09-28T12:05:01.123Z level=INFO msg="the parent as it was"
0 records lost, debug true
```

## See also

- [options](options.md): what a logger is made with
- [default_logger](default_logger.md), [set_default](set_default.md): the logger of the free functions
- [handler](handler.md): the records given to the program
- `tests/slog/logger.cpp`, `tests/slog/oracle.cpp`: every behaviour of the logger, checked
- [sgcl::slog](README.md)
