[sgcl](../../README.md) › [concurrent](../README.md) › [hyperloglog](README.md)

# sgcl::concurrent::hyperloglog::add

```cpp
void add(std::string_view key) noexcept;    // (1)
template<class Bytes>
void add(const Bytes& key) noexcept;        // (2)
void add(uint64_t key) noexcept;            // (3)
```

Counts the key: its register raised to the rank of its hash, by an atomic maximum, so threads add at once without a
lock. A key added again changes nothing.

1. Text: a literal, a `std::string`, a [string](../../core/string/README.md).
2. Bytes: a slice of bytes or what converts to one; takes part only for a key that converts to `slice<const byte>`
   and not to `std::string_view`.
3. A number, hashed as its eight bytes, little-endian.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key |

## Return value

None.

## Complexity

Constant: one hash, one register.

## Exceptions

None.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::hyperloglog h;
    h.add("ann");
    h.add("ann");
    h.add(42);
    println("{:.0f}", h.estimate());
}
```

Output:

```text
2
```

## See also

- [estimate](estimate.md)
- [sgcl::concurrent::hyperloglog](README.md)
