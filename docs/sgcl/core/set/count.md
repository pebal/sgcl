[sgcl](../../README.md) › [core](../README.md) › [set](../set.md)

# sgcl::set\<Key, Hash, KeyEqual\>::count

```cpp
/*(1)*/ size_type count(const key_type& key) const noexcept;
/*(2)*/ template<class K> size_type count(const K& key) const noexcept(/* see below */);
```

Returns the number of elements with the key `key`: 0 or 1, the keys of a set being unique.

- (2) The key is of any type the hash and the equality take, and no `Key` is built for the search. Takes part
  only when `Hash` and `KeyEqual` both declare `is_transparent`, as `std::hash` and `std::equal_to` of a
  [string](../string.md) do.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the elements to count |

## Return value

The number of elements with the key, 0 or 1.

## Complexity

Constant on average, the walk of one bucket.

## Exceptions

- (1) None.
- (2) None when the calls of `Hash` and `KeyEqual` with a `K` are noexcept or they are std's function objects;
  otherwise what they throw.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    set<string> s = {"apple", "pear"};
    println("{} {}", s.count("apple"), s.count("fig"));
}
```

Output:

```text
1 0
```

## See also

- [contains](contains.md): checks whether the set holds a key
- [find](find.md): an iterator to the element with a key
- [sgcl::set\<Key, Hash, KeyEqual\>](../set.md)
