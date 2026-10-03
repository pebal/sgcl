[sgcl](../../README.md) › [slog](../README.md) › [memory](README.md)

# sgcl::slog::memory::handle

```cpp
void handle(const record& r) const;
```

Keeps `r.clone()`, a copy of the record that owns all it holds ([clone](../record/clone.md)), after the records
kept so far: what makes `memory` a [handler](../handler/README.md). A logger calls it; a test may too.

## Parameters

| Parameter | Description |
|---|---|
| `r` | the record to keep |

## Return value

None.

## Complexity

Linear in the size of the record, which is copied.

## Exceptions

What the clone of the record throws: what a value of the program read through its operations throws.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::memory kept;
    kept.handle(slog::record());
    println("{} {}", kept.size(), kept.records()[0].empty());
}
```

Output:

```text
1 true
```

## See also

- [enabled](enabled.md)
- [handler::handle](../handler/handle.md)
- [sgcl::slog::memory](README.md)
