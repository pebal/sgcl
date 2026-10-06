[sgcl](../../README.md) › [concurrent](../README.md) › [hyperloglog](README.md)

# sgcl::concurrent::hyperloglog::merge

```cpp
void merge(const hyperloglog& other);
```

Raises each register to the other's where the other's is higher: the union, the sketch of every key added to either,
exactly as if they had all been added here. The sketches must have one precision; a sketch merged into itself is left
as it is. On NEON sixteen registers are compared at once, and only those the other raises are written.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the sketch whose keys to add |

## Return value

None.

## Complexity

Linear in 2^*p*.

## Exceptions

`invalid_argument` for a sketch of another precision, nothing changed.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"
#include <cmath>

using namespace sgcl;

int main() {
    concurrent::hyperloglog monday, tuesday;
    for (uint64_t i = 0; i < 10'000; ++i) {
        monday.add(i);
        tuesday.add(i + 5'000);
    }
    monday.merge(tuesday);
    println("within 3%: {}", std::abs(monday.estimate() / 15'000 - 1) < 0.03);
}
```

Output:

```text
within 3%: true
```

## See also

- [clone](clone.md): a union kept apart
- [sgcl::concurrent::hyperloglog](README.md)
