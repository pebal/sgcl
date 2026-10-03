[sgcl](../../README.md) › [immutable](../README.md) › [map](../map/README.md) › [builder](README.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::builder::erase

```cpp
bool erase(const Key& key) noexcept(std::is_nothrow_copy_constructible_v<value_type>);    // (1)
template<class K> bool erase(const K& key) noexcept(/* see below */);                     // (2)
```

Takes out the element under `key`. The key is looked up first, so that an absent key copies nothing; then the
element is taken out of its node in place, the elements after it moved down, a node emptied dropped and a subtrie
left with one element folded into its parent, as the map's [erase](../map/erase.md) does. A node the builder
shares with a map is copied once, the first time a change goes through it.

1. Erases the element under `key`.
2. Erases the element under a key of another type without building a `Key`. Takes part only when
   `Hash::is_transparent` and `KeyEqual::is_transparent` are both types.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element to take out |

## Return value

`true` when the element was taken out, `false` when the key was absent.

## Complexity

Logarithmic in `size()`, base 32: the key looked up, then a second walk down the trie, a node copied the first
time a change goes through it.

## Exceptions

- (1) What the copy of an element throws; none when it is noexcept.
- (2) The same, and what `Hash` and `KeyEqual` called with a `K` throw; none when their calls are noexcept, as
  those of the function objects of `std` are taken to be.

When an exception is thrown, the builder holds what it held before the call, and the maps it froze are untouched.

## Notes

Nothing is destroyed at once when the node is shared with a map: the map still holds the element. The lookup
before the change is a second walk, which puts an edit through a builder behind immer's transient: 137 ns per
change against 103 ([Benchmarks: The builder](../benchmarks.md#the-builder)).

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::map<string, int> stock = {{"apples", 3}, {"pears", 0}, {"plums", 0}};
    auto b = stock.thaw();
    for (const auto& [item, count] : stock) {
        if (count == 0) {
            b.erase(item);
        }
    }
    println("{} {} {}", b.size(), b.erase("kiwis"), stock.size());
}
```

Output:

```text
1 false 3
```

## See also

- [insert](insert.md), [set](set.md): add an element
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::builder](README.md)
