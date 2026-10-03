[sgcl](../../README.md) › [slog](../README.md) › [level_var](../level_var.md)

# sgcl::slog::level_var::level_var

```cpp
explicit level_var(level l = level::info) noexcept;
```

Constructs a variable at the level `l`: a new state of its own, which the copies of this one and the loggers made
with it share.

## Parameters

| Parameter | Description |
|---|---|
| `l` | the level it starts at; `info` by default |

## Complexity

Constant: one managed allocation.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::level_var a;
    slog::level_var b(slog::level::error);
    slog::level_var c = b;  // the same level as b
    c.set(slog::level::debug);
    println("{} {} {}", a.get() == slog::level::info, b.get() == slog::level::debug,
            c.get() == slog::level::debug);
}
```

Output:

```text
true true true
```

## See also

- [set](set.md), [get](get.md)
- [sgcl::slog::level_var](../level_var.md)
