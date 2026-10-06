[sgcl](../../README.md) › [async](../README.md) › [singleflight](README.md)

# sgcl::async::singleflight::singleflight

```cpp
singleflight();                                          // (1)
singleflight(const singleflight&) noexcept = default;    // (2)
singleflight(singleflight&&) noexcept = default;         // (3)
```

1. A new table, with no call in flight.
2. A handle of the same table: a call run through either is shared with the callers of both.
3. The same, taken from the other handle, which stands for the same table still.

## Parameters

| Parameter | Description |
|---|---|
| `const singleflight&`, `singleflight&&` | the handle of the table to share |

## Complexity

- (1) Constant: the table's allocation.
- (2–3) Constant: one tracked word copied.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::singleflight flights;
    async::singleflight same = flights;
    async::singleflight other;
    println("{} {}", same == flights, other == flights);
}
```

Output:

```text
true false
```

## See also

- [run](run.md): a call through it
- [sgcl::async::singleflight](README.md)
