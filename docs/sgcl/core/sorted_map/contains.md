[sgcl](../../README.md) › [core](../README.md) › [sorted_map](README.md)

# sgcl::sorted_map\<Key, T, Compare\>::contains

```cpp
bool contains(const key_type& key) const noexcept;                                // (1)
template<class K> bool contains(const K& key) const noexcept(/* see below */);    // (2)
```

Checks whether the map holds an element under `key`. It hides
[mixin::enumerable](../mixin/enumerable/README.md)'s [contains](../mixin/enumerable/contains.md), a walk comparing the
elements: a map is asked by the key.

- (2) Takes part only when `Compare` declares `is_transparent`: the key is of any type the comparison takes, and
  no `Key` is built for the search.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to look for |

## Return value

`true` when the map holds an element under `key`.

## Complexity

Logarithmic in the size of the map.

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
    sorted_map<string, int> m = {{"apple", 1}};
    println("{} {}", m.contains("apple"), m.contains("pear"));  // no string is built
}
```

Output:

```text
true false
```

## See also

- [find](find.md): finds the element under a key
- [count](count.md): the number of elements under a key
- [sgcl::sorted_map\<Key, T, Compare\>](README.md)
