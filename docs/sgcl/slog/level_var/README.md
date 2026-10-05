[sgcl](../../README.md) › [slog](../README.md)

# sgcl::slog::level_var

```cpp
#include "sgcl/slog/level.h"   // or "sgcl/slog.h"

namespace sgcl::slog {
    class level_var;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::slog::level_var` is one level for many loggers, changed while the program runs: slog's `LevelVar`. A logger
made with one, [options](../options.md)`{.level_var = v}`, reads its least level from it at every record, in place of
`options::level`; a [set](set.md) is seen by the next record of every such logger, on any thread.

## Rules

- A handle of one word, made at its level by the constructor; the copies share the level.
- A relaxed atomic: a record in flight on another thread may still be judged by the level before.
## Member functions

| Function | Description |
|---|---|
| [(constructor)](level_var.md) | constructs the variable at a level |
| `(destructor)` | drops the word; the level lives while a copy or a logger holds it |
| `operator=` | copies or moves the word of another variable; the variable moved from is the same variable still |

#### Operations

| Function | Description |
|---|---|
| [get](get.md) | the level now |
| [set](set.md) | changes the level for every logger made with it |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::level_var least(slog::level::warn);
    slog::logger log(slog::options{.out = io::stdout, .level_var = least});
    log.info("not written");
    least.set(slog::level::debug);
    log.debug("written");
}
```

Output:

```text
time=2026-09-28T14:05:01.123+02:00 level=DEBUG msg=written
```

## See also

- [level](../level.md): the levels
- [options](../options.md): `level` and `level_var`
- [logger::enabled](../logger/enabled.md): whether a record of a level is written
- [sgcl::slog](../README.md)
