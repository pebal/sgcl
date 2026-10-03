[sgcl](../../README.md) › [core](../README.md) › [multimap](../multimap.md)

# sgcl::multimap\<Key, T, Hash, KeyEqual\>::contains

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
    multimap<string, int> visits = {{"ann", 1}, {"ann", 2}};
    println("{} {}", visits.contains("ann"), visits.contains("bob"));
}
```

Output:

```text
true false
```

## See also

- [count](count.md): the number of elements under a key
- [find](find.md): an iterator to the first element under a key
- [sgcl::multimap\<Key, T, Hash, KeyEqual\>](../multimap.md)
