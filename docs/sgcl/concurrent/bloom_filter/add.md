[sgcl](../../README.md) › [concurrent](../README.md) › [bloom_filter](README.md)

# sgcl::concurrent::bloom_filter::add

```cpp
bool add(std::string_view key) noexcept;    // (1)
template<class Bytes>
bool add(const Bytes& key) noexcept;        // (2)
bool add(uint64_t key) noexcept;            // (3)
```

Sets the *k* bits of the key, and returns whether one of them was unset: `true` says the key was certainly not
added before (nor any key with the same bits); `false` says it may have been. A bit is set by an atomic OR of its
word after a look that finds it unset, so threads add at once without a lock.

1. Text: a literal, a `std::string`, a [string](../../core/string/README.md).
2. Bytes: a slice of bytes or what converts to one, a `vector<byte>`; text goes to (1). Takes part only for a key
   that converts to `slice<const byte>` and not to `std::string_view`.
3. A number, hashed as its eight bytes, little-endian.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key |

## Return value

`true` when the key is certainly new.

## Complexity

Constant: one hash, *k* words looked at.

## Exceptions

None.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::bloom_filter ids(100);
    println("{} {}", ids.add(7), ids.add(7));
    vector<byte> raw = {byte('h'), byte('i')};
    ids.add(raw);
    println("{}", ids.contains("hi"));  // text and its bytes are one key
}
```

Output:

```text
true false
true
```

## See also

- [contains](contains.md): the question alone
- [sgcl::concurrent::bloom_filter](README.md)
