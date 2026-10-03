[sgcl](../../README.md) › [core](../README.md) › [map](../map.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::contains

```cpp
/*(1)*/ bool contains(const key_type& key) const noexcept;
/*(2)*/ template<class K> bool contains(const K& key) const noexcept(/* see below */);
```

Checks whether an element is under `key`: the search of [find](find.md). It hides the `contains` of
[mixin::enumerable](../mixin/enumerable/contains.md), which would compare every element with a pair.

1. The key is of the key type.
2. The key is of any type the hash and the equality take. Takes part only when `Hash` and `KeyEqual` both declare
   `is_transparent`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to look for |

## Return value

`true` when an element is under `key`, `false` otherwise.

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
    map<string, int> stock = {{"apple", 3}, {"pear", 0}};
    println("{} {}", stock.contains("apple"), stock.contains("plum"));
}
```

Output:

```text
true false
```

## See also

- [count](count.md): the same answer as a number
- [find](find.md): an iterator to the element
- [sgcl::map\<Key, T, Hash, KeyEqual\>](../map.md)
