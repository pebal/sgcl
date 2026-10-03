[sgcl](../../README.md) › [immutable](../README.md) › [set](../set.md)

# sgcl::immutable::set\<Key, Hash, KeyEqual\>::insert

```cpp
set insert(const Key& key) const noexcept(std::is_nothrow_copy_constructible_v<value_type>);    // (1)
set insert(Key&& key) const                                                                     // (2)
    noexcept(std::is_nothrow_copy_constructible_v<value_type> &&
             std::is_nothrow_move_constructible_v<value_type>);
```

Returns the set with `key` added when no equal element is there, and this same set, sharing everything, when one
is: as every `insert` of the library, it keeps what it finds. This set is unchanged. The nodes on the path to the
element are copied, log32(*n*) of them, and the rest is shared.

1. The new element is a copy of `key`.
2. The new element is `key`, moved.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the element to add |

## Return value

The new set, one element more; this set when an equal element was there.

## Complexity

Logarithmic in `size()`, base 32: the key looked up, then the nodes on the path copied, up to 32 elements each.

## Exceptions

What the copy or the move of `Key`, and the copy of the elements on the path, throw; none when they are
noexcept.

This set is never changed, so an exception leaves it as it was; no new set is made.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::set<string> seen = {"alice", "bob"};
    auto three = seen.insert("carol");
    auto same = three.insert("bob");  // bob is there: the same set
    println("{} {} {}", seen.size(), three.size(), same == three);
}
```

Output:

```text
2 3 true
```

## See also

- [erase](erase.md): the set without an element
- [thaw](thaw.md): a builder, for many changes at once
- [sgcl::immutable::set\<Key, Hash, KeyEqual\>](../set.md)
