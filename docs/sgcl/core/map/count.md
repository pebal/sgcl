[sgcl](../../README.md) › [core](../README.md) › [map](../map.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::count

```cpp
/*(1)*/ size_type count(const key_type& key) const noexcept;
/*(2)*/ template<class K> size_type count(const K& key) const noexcept(/* see below */);
```

Returns the number of elements under `key`, 1 or 0, as `std::unordered_map::count` does: the search of
[find](find.md).

1. The key is of the key type.
2. The key is of any type the hash and the equality take. Takes part only when `Hash` and `KeyEqual` both declare
   `is_transparent`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to count |

## Return value

1 when an element is under `key`, 0 otherwise.

## Complexity

Constant on average, linear in the size when every key falls into one bucket.

## Exceptions

- (1) None.
- (2) None when the calls of `Hash` and `KeyEqual` with a `K` are noexcept or they are the function objects of
  `std`; otherwise what they throw.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    map<int, string> names = {{1, "Ada"}, {2, "Grace"}};
    size_t found = 0;
    for (int id : range(5)) {
        found += names.count(id);
    }
    println("{}", found);
}
```

Output:

```text
2
```

## See also

- [contains](contains.md): the same answer as a `bool`
- [find](find.md): an iterator to the element
- [sgcl::map\<Key, T, Hash, KeyEqual\>](../map.md)
