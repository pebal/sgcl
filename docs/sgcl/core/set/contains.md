[sgcl](../../README.md) › [core](../README.md) › [set](../set.md)

# sgcl::set\<Key, Hash, KeyEqual\>::contains

```cpp
bool contains(const key_type& key) const noexcept;                                // (1)
template<class K> bool contains(const K& key) const noexcept(/* see below */);    // (2)
```

Checks whether the set holds an element with the key `key`, by a lookup in its bucket. It hides the `contains`
of [mixin::enumerable](../mixin/enumerable/contains.md), which would walk every element.

- (2) The key is of any type the hash and the equality take, and no `Key` is built for the search. Takes part
  only when `Hash` and `KeyEqual` both declare `is_transparent`, as `std::hash` and `std::equal_to` of a
  [string](../string.md) do.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to look for |

## Return value

`true` when an element has the key, `false` otherwise.

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
    set<string> s = {"apple"};
    println("{}", s.contains("apple"));  // no string built for the literal

    string line = "apple pie";
    println("{}", s.contains(line.as_slice(0, 5)));  // nor for a slice of another string
    println("{}", s.contains(line));
}
```

Output:

```text
true
true
false
```

## See also

- [find](find.md): an iterator to the element with a key
- [count](count.md): the number of elements with a key
- [sgcl::set\<Key, Hash, KeyEqual\>](../set.md)
