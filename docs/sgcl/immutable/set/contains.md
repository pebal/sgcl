[sgcl](../../README.md) › [immutable](../README.md) › [set](../set.md)

# sgcl::immutable::set\<Key, Hash, KeyEqual\>::contains

```cpp
bool contains(const Key& key) const noexcept;                                     // (1)
template<class K> bool contains(const K& key) const noexcept(/* see below */);    // (2)
```

Checks whether the set has an element equal to `key`, by the hash. It hides the `contains` of
[mixin::enumerable](../../core/mixin/enumerable/contains.md), which would compare every element.

1. Looks up `key`.
2. Looks up a key of another type without building a `Key`. Takes part only when `Hash::is_transparent` and
   `KeyEqual::is_transparent` are both types.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to look up |

## Return value

`true` when the set has an element equal to the key, `false` otherwise.

## Complexity

Logarithmic in `size()`, base 32, and one comparison of keys; one per element of a chain when hashes collide to
the last bit.

## Exceptions

- (1) None.
- (2) What `Hash` and `KeyEqual` called with a `K` throw; none when their calls are noexcept, as those of the
  function objects of `std` are taken to be.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::set<string> seen = {"alice", "bob"};
    auto with_carol = seen.insert("carol");
    println("{} {}", seen.contains("carol"), with_carol.contains("carol"));
}
```

Output:

```text
false true
```

## See also

- [find](find.md): the element equal to a key, as an iterator
- [count](count.md): the number of elements equal to a key
- [sgcl::immutable::set\<Key, Hash, KeyEqual\>](../set.md)
