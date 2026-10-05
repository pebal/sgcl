[sgcl](../../README.md) › [slog](../README.md)

# sgcl::slog::memory

```cpp
#include "sgcl/slog/memory.h"   // or "sgcl/slog.h"

namespace sgcl::slog {
    class memory;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::slog::memory` is a handler that keeps the records, for tests: `slog::logger(kept)`, then `kept.records()`.
Each record kept is a [clone](../record/clone.md), which owns its texts and its attributes, so it may be read long
after the call that logged it. It takes every level; the logger's level decides.

## Rules

- A handle of one word, made empty by its constructor; the copies share the records. Given to a
  [handler](../handler/README.md), it is copied, so the logger and the handle share them too.
- Any thread may log through it: the records are kept under a lock, in the order they came.
## Member functions

| Function | Description |
|---|---|
| [(constructor)](memory.md) | constructs an empty keeper |
| `(destructor)` | drops the word; the records live while a copy or a logger holds them |
| `operator=` | copies or moves the word of another keeper; the keeper moved from shares the same records still |

#### Records

| Function | Description |
|---|---|
| [records](records.md) | the records kept, in the order they came |
| [size](size.md) | the number of records kept |
| [clear](clear.md) | drops the records kept |

#### Handler

| Function | Description |
|---|---|
| [handle](handle.md) | keeps a clone of a record |
| [enabled](enabled.md) | every level: `true` |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::memory kept;
    auto test = slog::logger(kept, slog::level::debug);
    test.debug("first", "n", 1);
    test.warn("second", "ratio", 0.5);
    for (const auto& r : kept.records()) {
        auto a = *r.begin();
        println("{} {} {}", r.message(), a.key(), a.value().json());
    }
}
```

Output:

```text
first n 1
second ratio 0.5
```

## See also

- [handler](../handler/README.md), [record](../record/README.md)
- [sgcl::slog](../README.md)
