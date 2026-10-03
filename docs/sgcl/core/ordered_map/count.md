[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::count

```cpp
size_type count(const key_type& key) const noexcept;                                // (1)
template<class K> size_type count(const K& key) const noexcept(/* see below */);    // (2)
```

Returns the number of elements under `key`: 1 or 0, the keys being unique.

1. The key is of the key type.
2. The key is of any type the hash and the equality take. Takes part only when `Hash` and `KeyEqual` both
   declare `is_transparent`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to count |

## Return value

1 when the key is there, 0 otherwise.

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
    ordered_map<string, int> m = {{"a", 1}};
    println("{} {}", m.count("a"), m.count("b"));
}
```

Output:

```text
1 0
```

## See also

- [contains](contains.md): checks whether a key is there
- [find](find.md): an iterator to the element under a key
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)
