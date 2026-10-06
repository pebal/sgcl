[sgcl](../../README.md) › [concurrent](../README.md) › [bloom_filter](README.md)

# sgcl::concurrent::bloom_filter::contains

```cpp
bool contains(std::string_view key) const noexcept;    // (1)
template<class Bytes>
bool contains(const Bytes& key) const noexcept;        // (2)
bool contains(uint64_t key) const noexcept;            // (3)
```

Checks whether every bit of the key is set: `false` is certain, the key was never added; `true` is wrong at the
filter's rate of false positives. The keys are those of [add](add.md): (1) text, (2) bytes, which takes part only
for a key that converts to `slice<const byte>` and not to `std::string_view`, (3) a number.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key |

## Return value

`true` when the key may have been added.

## Complexity

Constant: one hash, at most *k* words read.

## Exceptions

None.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::bloom_filter f(1000, 0.001);
    f.add("apple");
    println("{} {}", f.contains("apple"), f.contains("pear"));
}
```

Output:

```text
true false
```

## See also

- [add](add.md)
- [false_positive_rate](false_positive_rate.md): how often a true is wrong
- [sgcl::concurrent::bloom_filter](README.md)
