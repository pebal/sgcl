[sgcl](../../README.md) › [immutable](../README.md) › [set](../set.md) › [builder](../set-builder.md)

# sgcl::immutable::set\<Key, Hash, KeyEqual\>::builder::insert

```cpp
bool insert(const Key& key) noexcept(std::is_nothrow_copy_constructible_v<value_type>);    // (1)
bool insert(Key&& key)                                                                     // (2)
    noexcept(std::is_nothrow_copy_constructible_v<value_type> &&
             std::is_nothrow_move_constructible_v<value_type>);
```

Adds `key` when no equal element is there, and does nothing when one is, as the set's [insert](../set/insert.md).
The element goes into a node the builder made, in place when the node has room; a node the builder shares with a
set is copied once, the first time a change goes through it.

1. The new element is a copy of `key`.
2. The new element is `key`, moved.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the element to add |

## Return value

`true` when the element was added, `false` when an equal one was there.

## Complexity

Logarithmic in `size()`, base 32: the key looked up, then a walk down the trie, a node copied the first time a
change goes through it.

## Exceptions

What the copy or the move of `Key`, and the copy of the elements of the nodes the builder copies, throw; none
when they are noexcept.

When an exception is thrown, the builder holds the elements it held before, and the sets it froze are untouched.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::set<string>::builder b;
    bool first = b.insert("alice");
    bool again = b.insert("alice");
    println("{} {} {}", first, again, b.size());
}
```

Output:

```text
true false 1
```

## See also

- [erase](erase.md): takes out an element
- [sgcl::immutable::set\<Key, Hash, KeyEqual\>::builder](../set-builder.md)
