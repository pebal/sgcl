[sgcl](../README.md) › [slog](README.md)

# sgcl::slog::attr

```cpp
#include "sgcl/slog/record.h"   // or "sgcl/slog.h"

namespace sgcl::slog {
    class attr;
}
```

`sgcl::slog::attr` is an attribute of a [record](record.md) as a handler reads it, slog's `Attr`: its key and its
[value](value.md). The range of a record and of a group's value ([value::as_group](value/as_group.md)) gives them
by value. It is made only by the module.

## Rules

- A view of the record's, valid while the record is: in the record a handler is given, while `handle` runs; in a
  [clone](record/clone.md), while the clone is.

## Member functions

#### Observers

| Function | Description |
|---|---|
| [key](attr/key.md) | the key |
| [value](attr/value.md) | the value |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::memory kept;
    slog::logger(kept).info("m", "port", 8080, slog::group("tls", "on", true));
    for (auto a : kept.records()[0]) {
        println("{} -> {}", a.key(), a.value().json());
    }
}
```

Output:

```text
port -> 8080
tls -> {"on":true}
```

## See also

- [record](record.md), [value](value.md)
- [sgcl::slog](README.md)
