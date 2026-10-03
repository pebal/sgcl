[sgcl](../../README.md) › [core](../README.md) › [multimap](../multimap.md)

# sgcl::multimap\<Key, T, Hash, KeyEqual\>::count

```cpp
size_type count(const key_type& key) const noexcept;                                // (1)
template<class K> size_type count(const K& key) const noexcept(/* see below */);    // (2)
```

Returns the number of elements under `key`: the length of the key's run, which [find](find.md) starts.

1. The key is of the key type.
2. The key is of any type the hash and the equality take. Takes part only when `Hash` and `KeyEqual` both declare
   `is_transparent`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to count |

## Return value

The number of elements under `key`, 0 when there is none.

## Complexity

Constant on average, plus the number of elements under the key.

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
    multimap<string, int> m = {{"a", 1}, {"a", 2}, {"b", 3}};
    println("{} {} {}", m.count("a"), m.count("b"), m.count("c"));  // no string built
}
```

Output:

```text
2 1 0
```

## See also

- [equal_range](equal_range.md): the run of the elements under a key
- [contains](contains.md): checks whether a key is there
- [sgcl::multimap\<Key, T, Hash, KeyEqual\>](../multimap.md)
