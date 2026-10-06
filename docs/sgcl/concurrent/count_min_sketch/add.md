[sgcl](../../README.md) › [concurrent](../README.md) › [count_min_sketch](README.md)

# sgcl::concurrent::count_min_sketch::add

```cpp
void add(std::string_view key, uint64_t count = 1) noexcept;    // (1)
template<class Bytes>
void add(const Bytes& key, uint64_t count = 1) noexcept;        // (2)
void add(uint64_t key, uint64_t count = 1) noexcept;            // (3)
```

Adds `count` to the key: to one counter of each row, atomically, so threads add at once without a lock, and to the
[total](total.md).

1. Text: a literal, a `std::string`, a [string](../../core/string/README.md).
2. Bytes: a slice of bytes or what converts to one; takes part only for a key that converts to `slice<const byte>`
   and not to `std::string_view`.
3. A number, hashed as its eight bytes, little-endian.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key |
| `count` | how much to add; one by default |

## Return value

None.

## Complexity

Constant: one hash, *d* counters.

## Exceptions

None.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::count_min_sketch bytes_sent;
    bytes_sent.add("client-7", 1500);
    bytes_sent.add("client-7", 500);
    println("{}", bytes_sent.estimate("client-7"));
}
```

Output:

```text
2000
```

## See also

- [estimate](estimate.md)
- [sgcl::concurrent::count_min_sketch](README.md)
