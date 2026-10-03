[sgcl](../../README.md) › [slog](../README.md) › [handler](../handler.md)

# sgcl::slog::handler::enabled

```cpp
bool enabled(slog::level l) const;
```

Checks whether the handler held wants records of the level `l`: its own `enabled(l)` when its type has one, `true`
otherwise, slog's `Handler.Enabled`. A logger asks it after its own level, before a record is made, so a record the
handler refuses costs nothing more. An empty handler throws `logic_error`.

## Parameters

| Parameter | Description |
|---|---|
| `l` | the level asked about |

## Return value

What the handler's `enabled` returns; `true` for a handler without one.

## Complexity

One indirect call, then what the handler's own `enabled` costs.

## Exceptions

- What the handler's own `enabled` throws.
- `logic_error` when no handler is held: `sgcl::slog::handler: no handler is held`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

struct everything {
    void handle(const slog::record&) const {
    }
};

struct warnings {
    void handle(const slog::record&) const {
    }

    bool enabled(slog::level l) const {
        return l >= slog::level::warn;
    }
};

int main() {
    slog::handler a = everything();
    slog::handler b = warnings();
    println("{} {}", a.enabled(slog::level::debug), b.enabled(slog::level::debug));
    println("{}", slog::logger(b, slog::level::debug).enabled(slog::level::info));
}
```

Output:

```text
true false
false
```

## See also

- [handle](handle.md)
- [logger::enabled](../logger/enabled.md): the logger's level, then this
- [sgcl::slog::handler](../handler.md)
