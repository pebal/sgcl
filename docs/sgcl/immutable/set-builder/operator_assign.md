[sgcl](../../README.md) › [immutable](../README.md) › [set](../set.md) › [builder](../set-builder.md)

# sgcl::immutable::set\<Key, Hash, KeyEqual\>::builder::operator=

```cpp
builder& operator=(builder&& other) noexcept;    // (1)
builder& operator=(const builder&) = delete;     // (2)
```

1. Takes the trie of `other` over; `other` is empty after, with the same hash and equality. What this builder held
   before is dropped: its nodes no set holds are left to the collector. An assignment of a builder to itself does
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
    immutable::set<int> base = {1, 2};
    auto b = base.thaw();
    immutable::set<int>::builder other;
    other.insert(7);
    b = std::move(other);  // what b held is dropped; base is untouched
    println("{} {} {} {}", b.size(), b.contains(7), other.empty(), base.size());
}
```

Output:

```text
1 true true 2
```

## See also

- [(constructor)](set-builder.md): constructs a builder
- [sgcl::immutable::set\<Key, Hash, KeyEqual\>::builder](../set-builder.md)
