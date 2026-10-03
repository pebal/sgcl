[sgcl](../../README.md) › [immutable](../README.md) › [set](../set/README.md) › [builder](README.md)

# sgcl::immutable::set\<Key, Hash, KeyEqual\>::builder::erase

```cpp
bool erase(const Key& key) noexcept(std::is_nothrow_copy_constructible_v<value_type>);    // (1)
template<class K> bool erase(const K& key) noexcept(/* see below */);                     // (2)
```

Takes out the element equal to `key`. The key is looked up first, so that an absent key copies nothing; then the
element is taken out of its node in place, as the map's builder [erase](../map-builder/erase.md) does. A node the
builder shares with a set is copied once, the first time a change goes through it.

1. Erases the element equal to `key`.
2. Erases the element equal to a key of another type without building a `Key`. Takes part only when
   `Hash::is_transparent` and `KeyEqual::is_transparent` are both types.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element to take out |

## Return value

`true` when the element was taken out, `false` when none was equal to the key.

## Complexity

Logarithmic in `size()`, base 32: the key looked up, then a second walk down the trie, a node copied the first
time a change goes through it.

## Exceptions

- (1) What the copy of an element throws; none when it is noexcept.
- (2) The same, and what `Hash` and `KeyEqual` called with a `K` throw; none when their calls are noexcept, as
  those of the function objects of `std` are taken to be.

When an exception is thrown, the builder holds what it held before the call, and the sets it froze are untouched.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::set<string> names = {"alice", "bob"};
    auto b = names.thaw();
    bool gone = b.erase("bob");
    bool absent = b.erase("carol");
    println("{} {} {} {}", gone, absent, b.size(), names.size());
}
```

Output:

```text
true false 1 2
```

## See also

- [insert](insert.md): adds an element
- [sgcl::immutable::set\<Key, Hash, KeyEqual\>::builder](README.md)
