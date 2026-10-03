[sgcl](../README.md) › [slog](README.md)

# sgcl::slog::level

```cpp
#include "sgcl/slog/level.h"   // or "sgcl/slog.h"

namespace sgcl::slog {
    enum class level : int8_t { debug = -4, info = 0, warn = 4, error = 8 };
}
```

The importance of a record, slog's `Level`, with Go's numbers, so that the levels between them compare and read as
Go's do. A logger writes the records at its level and above ([options](options.md)`::level`,
[level_var](level_var.md)). Any value of `int8_t` is a level: one between the named ones is written from the
nearest named level at or below it and the distance from it, `slog::level(2)` as `INFO+2`, `slog::level(10)` as
`ERROR+2`, and one below `debug` from `debug`, `slog::level(-5)` as `DEBUG-1`.

| Value | Description |
|---|---|
| `debug` | -4: what is wanted while a program is being looked into, `DEBUG` |
| `info` | 0: the default level of a logger, `INFO` |
| `warn` | 4: something to look at; a record of `warn` and up writes a buffered logger's batch at once, `WARN` |
| `error` | 8: a failure, `ERROR` |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::logger log(io::stdout, slog::level(-6));
    log.log(slog::level::warn, "named");
    log.log(slog::level(2), "between info and warn");
    log.log(slog::level(10), "above error");
    log.log(slog::level(-5), "below debug");
    println("{}", slog::level::warn > slog::level::info);
}
```

Output:

```text
time=2026-09-28T14:05:01.123+02:00 level=WARN msg=named
time=2026-09-28T14:05:01.123+02:00 level=INFO+2 msg="between info and warn"
time=2026-09-28T14:05:01.123+02:00 level=ERROR+2 msg="above error"
time=2026-09-28T14:05:01.123+02:00 level=DEBUG-1 msg="below debug"
true
```

## See also

- [logger::log](logger/log.md): a record of any level
- [level_var](level_var.md): a level changed while the program runs
- [options](options.md): the level a logger is made with
- [sgcl::slog](README.md)
