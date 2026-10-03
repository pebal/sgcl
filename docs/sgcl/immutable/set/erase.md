[sgcl](../../README.md) › [immutable](../README.md) › [set](README.md)

# sgcl::immutable::set\<Key, Hash, KeyEqual\>::erase

```cpp
set erase(const Key& key) const noexcept(std::is_nothrow_copy_constructible_v<value_type>);    // (1)
template<class K> set erase(const K& key) const noexcept(/* see below */);                     // (2)
```

Returns the set without the element equal to `key`. This set is unchanged. The path to the element is copied, a
node emptied is dropped, and a subtrie left with one element is folded into its parent. When no element is equal
to the key, the result is this same set, sharing everything.

1. Erases the element equal to `key`.
2. Erases the element equal to a key of another type without building a `Key`. Takes part only when
   `Hash::is_transparent` and `KeyEqual::is_transparent` are both types.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element to erase |

## Return value

The new set, one element fewer; this set when the key was absent.

## Complexity

Logarithmic in `size()`, base 32: the nodes on the path copied, up to 32 elements each; nothing copied when the
key is absent.

## Exceptions

- (1) What the copy of the elements throws; none when it is noexcept.
- (2) The same, and what `Hash` and `KeyEqual` called with a `K` throw; none when their calls are noexcept, as
  those of the function objects of `std` are taken to be.

This set is never changed, so an exception leaves it as it was; no new set is made.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::set<string> names = {"alice", "bob", "carol"};
    auto without_bob = names.erase("bob");  // a literal: no string made
    println("{} {} {}", names.size(), without_bob.size(), without_bob.contains("bob"));
}
```

Output:

```text
3 2 false
```

## See also

- [insert](insert.md): the set with one more element
- [sgcl::immutable::set\<Key, Hash, KeyEqual\>](README.md)
