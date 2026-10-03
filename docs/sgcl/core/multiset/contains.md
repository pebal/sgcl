[sgcl](../../README.md) › [core](../README.md) › [multiset](../multiset.md)

# sgcl::multiset\<Key, Hash, KeyEqual\>::contains

```cpp
bool contains(const key_type& key) const noexcept;                                // (1)
template<class K> bool contains(const K& key) const noexcept(/* see below */);    // (2)
```

Checks whether the multiset holds an element with the key `key`, by a lookup in its bucket. It hides the
`contains` of [mixin::enumerable](../mixin/enumerable/contains.md), which would walk every element.

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
    multiset<string> s = {"apple", "apple"};
    println("{} {}", s.contains("apple"), s.contains("pear"));

    s.erase(s.find("apple"));
    println("{}", s.contains("apple"));
}
```

Output:

```text
true false
true
```

## See also

- [find](find.md): an iterator to the first element with a key
- [count](count.md): the number of elements with a key
- [sgcl::multiset\<Key, Hash, KeyEqual\>](../multiset.md)
