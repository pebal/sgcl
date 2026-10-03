[sgcl](../../README.md) › [concurrent](../README.md) › [map](README.md)

# sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>::count

```cpp
size_type count(const Key& key) const noexcept;                    // (1)
template<class K> size_type count(const K& key) const noexcept;    // (2)
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

Constant on average, as [find](find.md).

## Exceptions

None.

## Notes

Wait-free once the key's bucket has its dummy node, as `find`; [contains](contains.md) is the same question with
a `bool` for the answer.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::map<int, string> names = {{1, "Ada"}, {2, "Grace"}};

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
- [sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>](README.md)
