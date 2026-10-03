[sgcl](../../README.md) › [core](../README.md) › [sorted_multimap](../sorted_multimap.md)

# sgcl::sorted_multimap\<Key, T, Compare\>::contains

```cpp
/*(1)*/ bool contains(const key_type& key) const noexcept;
/*(2)*/ template<class K> bool contains(const K& key) const noexcept(/* see below */);
```

Checks whether the multimap holds an element under `key`. It hides
[mixin::enumerable](../mixin/enumerable.md)'s [contains](../mixin/enumerable/contains.md), a walk comparing the
elements: a multimap is asked by the key.

- (2) Takes part only when `Compare` declares `is_transparent`: the key is of any type the comparison takes, and
  no `Key` is built for the search.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to look for |

## Return value

`true` when the multimap holds an element under `key`.

## Complexity

Logarithmic in the size of the multimap.

## Exceptions

- (1) None.
- (2) What the calls of `Compare` with a `K` throw; none when they are noexcept or `Compare` is a function object
  of `std`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_multimap<int, string> m = {{1, "a"}, {1, "b"}};
    println("{} {}", m.contains(1), m.contains(2));
}
```

Output:

```text
true false
```

## See also

- [find](find.md): finds the first element under a key
- [count](count.md): the number of elements under a key
- [sgcl::sorted_multimap\<Key, T, Compare\>](../sorted_multimap.md)
