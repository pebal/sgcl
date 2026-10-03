[sgcl](../README.md) › [slog](README.md)

# sgcl::slog::collector_log

```cpp
#include "sgcl/slog/collector_log.h"   // or "sgcl/slog.h"

namespace sgcl::slog {
    void collector_log(const logger& log) noexcept;    // (1)
    void collector_log() noexcept;                     // (2)
}
```

Makes the collector's own lines (`SGCL_LOG_PRINT_LEVEL` above 0, [config](../core/config.md#sgcl_log_print_level))
records of a logger from now on, instead of lines on `std::cout`.

1. Records of `log`. Called again, the lines go to that logger instead.
2. Records of the [default logger](default_logger.md) as it is at the call.

- (1–2) Every record is at `info`, `msg=collector`, with the macro's level as `verbosity` (1 to 3). The line of a
  cycle (level 2) comes as its numbers: `mem_allocs`, `mem_removed`, `total_mem`, `objects_created`,
  `objects_removed`, `live_objects`, `cycle` (`full` or `young`), `helpers`, `helpers_used`, `time_ms`,
  `total_time_ms`; any other line as `line`, its text without `[sgcl] `. Without the macro the collector says
  nothing, and neither does this.

## Parameters

| Parameter | Description |
|---|---|
| `log` | the logger the lines become records of |

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Notes

- **Only when asked.** Until the call the lines go to `std::cout` byte for byte as before, in a program that
  includes slog too (a benchmark that parses them sees them there); a line said before the call, the collector's
  start, is there.
- **When they are written.** A line comes from the collector's thread or from a thread's registration, where nothing
  managed may be touched: it is copied into a queue of plain memory there, and written by a thread that may log —
  a worker on its way to sleep, the next record of any logger, [flush](logger/flush.md), the exit (`atexit`,
  `at_quick_exit`), which writes the batches of a buffered logger after the last lines. At most 4096 lines wait; past that they are counted, and a `warn` record,
  `msg="collector lines dropped"`, says how many with `count`.
- A handler of the program that throws while it is given the collector's lines loses the rest of that batch; the
  program goes on.

## Example

```cpp
// the collector's lines of level 1: its start and stop, every force_collect
#define SGCL_LOG_PRINT_LEVEL 1
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::collector_log(slog::logger(io::stdout));
    collector::force_collect(true);  // optional, for the demonstration only
    slog::info("done");  // a record takes the collector's waiting lines first
}
```

Sample output:

```text
[sgcl] start collector id: 0x16b9b7000
time=2026-09-29T11:02:15.207+02:00 level=INFO msg=collector verbosity=1 line="force collect and wait from id: 0x1f0121d80"
time=2026-09-29T11:02:15.209+02:00 level=INFO msg=done
time=2026-09-29T11:02:15.209+02:00 level=INFO msg=collector verbosity=1 line="terminate collector from id: 0x1f0121d80"
time=2026-09-29T11:02:15.209+02:00 level=INFO msg=collector verbosity=1 line="stop collector id: 0x16b9b7000"
```

## See also

- [config](../core/config.md#sgcl_log_print_level): `SGCL_LOG_PRINT_LEVEL`
- [collector](../core/collector.md): what the lines report
- [sgcl::slog](README.md)
