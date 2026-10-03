[sgcl](../../README.md) › [slog](../README.md) › [logger](README.md)

# sgcl::slog::logger::enabled

```cpp
bool enabled(slog::level l) const;
```

Checks whether a record of the level `l` is written, slog's `Enabled`: what a record asks first. It compares `l` with
the logger's level, or with its [level_var](../level_var/README.md)'s now, and, for a handler of the program, asks the
handler's own `enabled` too. A record below the level makes nothing, but its arguments are computed by the caller,
as for any call: an argument that costs to compute goes under `if (log.enabled(slog::level::debug))`.

## Parameters

| Parameter | Description |
|---|---|
| `l` | the level asked about |

## Return value

`true` when a record of `l` is written.

## Complexity

Constant: one comparison, and the handler's `enabled` for a handler of the program.

## Exceptions

What the `enabled` of a handler of the program throws; none for a logger that writes lines.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

string expensive_dump() {
    return "a long text";
}

int main() {
    slog::logger log(io::stdout);
    if (log.enabled(slog::level::debug)) {
        log.debug("state", "dump", expensive_dump());
    }
    println("{} {}", log.enabled(slog::level::debug), log.enabled(slog::level::warn));
}
```

Output:

```text
false true
```

## See also

- [level](../level.md), [level_var](../level_var/README.md)
- [handler::enabled](../handler/enabled.md): the handler's answer
- [sgcl::slog::logger](README.md)
