[sgcl](../../README.md) › [immutable](../README.md) › [set](../set.md)

# sgcl::immutable::set\<Key, Hash, KeyEqual\>::find

```cpp
/*(1)*/ const_iterator find(const Key& key) const noexcept;
/*(2)*/ template<class K> const_iterator find(const K& key) const noexcept(/* see below */);
```

Returns an iterator to the element equal to `key`, or [end()](end.md) when there is none, as every `find` of the
library does; `++` on the iterator goes on from there in the order [begin()](begin.md) walks.

1. Looks up `key`.
2. Looks up a key of another type, a `string_view` or a literal for a `string` element, without building a
   `Key`. Takes part only when `Hash::is_transparent` and `KeyEqual::is_transparent` are both types.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to look up |

## Return value

An iterator to the element, `end()` when there is none.

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
    immutable::set<string> names = {"alice", "bob"};
    auto it = names.find("bob");  // a literal: no string made
    println("{} {}", *it, names.find("carol") == names.end());
}
```

Output:

```text
bob true
```

## See also

- [contains](contains.md): checks whether the set has an element equal to a key
- [sgcl::immutable::set\<Key, Hash, KeyEqual\>](../set.md)
