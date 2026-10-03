[sgcl](../../README.md) › [core](../README.md) › [ordered_set](../ordered_set.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::contains

```cpp
/*(1)*/ bool contains(const key_type& key) const noexcept;
/*(2)*/ template<class K> bool contains(const K& key) const noexcept(/* see below */);
```

Checks whether an element equal to `key` is there: a lookup by the hash, in place of the walk of every element
that [mixin::enumerable](../mixin/enumerable.md)'s `contains` would be.

1. The key is of the key type.
2. The key is of any type the hash and the equality take. Takes part only when `Hash` and `KeyEqual` both
   declare `is_transparent`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the element to look for |

## Return value

`true` when the element is there, `false` otherwise.

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
    ordered_set<string> s = {"apple"};
    println("{} {}", s.contains("apple"), s.contains("pear"));  // no string made for a literal

    string line = "apple pie";
    println("{}", s.contains(line.as_slice(0, 5)));
}
```

Output:

```text
true false
true
```

## See also

- [find](find.md): an iterator to an element
- [count](count.md): the number of equal elements
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](../ordered_set.md)
