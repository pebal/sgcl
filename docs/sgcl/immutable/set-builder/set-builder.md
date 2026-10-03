[sgcl](../../README.md) › [immutable](../README.md) › [set](../set.md) › [builder](../set-builder.md)

# sgcl::immutable::set\<Key, Hash, KeyEqual\>::builder::builder

```cpp
/*(1)*/ builder();
/*(2)*/ explicit builder(const Hash& hash, const KeyEqual& equal = KeyEqual());
/*(3)*/ builder(builder&& other) noexcept;
/*(4)*/ builder(const builder&) = delete;
```

Constructs a builder. A builder over the elements of a set is made by the set's [thaw()](../set/thaw.md).

1. An empty builder.
2. An empty builder with the hash and the equality given.
3. Takes the trie of `other` over; `other` is empty after, with the same hash and equality.
4. A builder is not copied: two builders would change one node.

## Parameters

| Parameter | Description |
|---|---|
| `hash` | the hash of the elements |
| `equal` | the equality of the elements |
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
    immutable::set<string>::builder words;
    for (const char* word : {"to", "be", "or", "not", "to", "be"}) {
        words.insert(word);
    }
    auto taken = std::move(words);
    println("{} {}", taken.size(), words.size());
}
```

Output:

```text
4 0
```

## See also

- [set::thaw](../set/thaw.md): a builder over a set
- [operator=](operator_assign.md): takes another builder over
- [sgcl::immutable::set\<Key, Hash, KeyEqual\>::builder](../set-builder.md)
