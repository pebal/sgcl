[sgcl](../../README.md) › [immutable](../README.md) › [map](../map.md) › [builder](../map-builder.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::builder::operator=

```cpp
builder& operator=(builder&& other) noexcept;    // (1)
builder& operator=(const builder&) = delete;     // (2)
```

1. Takes the trie of `other` over; `other` is empty after, with the same hash and equality. What this builder held
   before is dropped: its nodes no map holds are left to the collector. An assignment of a builder to itself does
   nothing.
2. A builder is not copied: two builders would change one node.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the builder taken over |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::map<int, int> base = {{1, 10}};
    auto b = base.thaw();
    immutable::map<int, int>::builder other;
    other.insert(7, 70);
    b = std::move(other);  // what b held is dropped; base is untouched
    println("{} {} {}", b.size(), b.contains(7), other.empty());
    println("{}", base.size());
}
```

Output:

```text
1 true true
1
```

## See also

- [(constructor)](map-builder.md): constructs a builder
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::builder](../map-builder.md)
