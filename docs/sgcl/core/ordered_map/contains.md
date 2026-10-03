[sgcl](../../README.md) › [core](../README.md) › [ordered_map](README.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::contains

```cpp
bool contains(const key_type& key) const noexcept;                                // (1)
template<class K> bool contains(const K& key) const noexcept(/* see below */);    // (2)
```

Checks whether an element is under `key`: a lookup by the key, in place of the walk of every element that
[mixin::enumerable](../mixin/enumerable/README.md)'s `contains` would be.

1. The key is of the key type.
2. The key is of any type the hash and the equality take. Takes part only when `Hash` and `KeyEqual` both
   declare `is_transparent`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to look for |

## Return value

`true` when the key is there, `false` otherwise.

## Complexity

Constant on average, linear in `size()` in the worst case.

## Exceptions

- (1) None.
- (2) None when the calls of `Hash` and `KeyEqual` with a `K` are noexcept or they are the function objects of
  `std`; otherwise what those calls throw.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_map<string, int> m = {{"apple", 1}};
    println("{} {}", m.contains("apple"), m.contains("pear"));  // no string made for a literal
}
```

Output:

```text
true false
```

## See also

- [find](find.md): an iterator to the element under a key
- [count](count.md): the number of elements under a key
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](README.md)
