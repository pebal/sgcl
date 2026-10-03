[sgcl](../../README.md) › [immutable](../README.md) › [map](../map.md) › [builder](../map-builder.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::builder::builder

```cpp
builder();                                                                 // (1)
explicit builder(const Hash& hash, const KeyEqual& equal = KeyEqual());    // (2)
builder(builder&& other) noexcept;                                         // (3)
builder(const builder&) = delete;                                          // (4)
```

Constructs a builder. A builder over the elements of a map is made by the map's [thaw()](../map/thaw.md).

1. An empty builder.
2. An empty builder with the hash and the equality given.
3. Takes the trie of `other` over; `other` is empty after, with the same hash and equality.
4. A builder is not copied: two builders would change one node.

## Parameters

| Parameter | Description |
|---|---|
| `hash` | the hash of the keys |
| `equal` | the equality of the keys |
| `other` | the builder taken over |

## Complexity

Constant.

## Exceptions

- (1) None, unless the default constructor of `Hash` or `KeyEqual` throws.
- (2) What the copy of `hash` and `equal` throws.
- (3) None.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::map<string, int>::builder counts;
    for (const char* word : {"a", "b", "a"}) {
        if (const int* n = counts.try_get(word)) {
            counts.set(word, *n + 1);
        } else {
            counts.insert(word, 1);
        }
    }
    auto taken = std::move(counts);
    println("{} {}", taken.size(), counts.size());
    println("{}", taken.freeze().at("a"));
}
```

Output:

```text
2 0
2
```

## See also

- [map::thaw](../map/thaw.md): a builder over a map
- [operator=](operator_assign.md): takes another builder over
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::builder](../map-builder.md)
