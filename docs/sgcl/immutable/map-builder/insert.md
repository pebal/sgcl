[sgcl](../../README.md) › [immutable](../README.md) › [map](../map/README.md) › [builder](README.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::builder::insert

```cpp
bool insert(const Key& key, const T& value)                                         // (1)
    noexcept(std::is_nothrow_copy_constructible_v<value_type> &&
             std::is_nothrow_constructible_v<value_type, const Key&, const T&>);
bool insert(const Key& key, T&& value)                                              // (2)
    noexcept(std::is_nothrow_copy_constructible_v<value_type> &&
             std::is_nothrow_constructible_v<value_type, const Key&, T&&>);
bool insert(Key&& key, const T& value)                                              // (3)
    noexcept(std::is_nothrow_copy_constructible_v<value_type> &&
             std::is_nothrow_constructible_v<value_type, Key&&, const T&>);
bool insert(Key&& key, T&& value)                                                   // (4)
    noexcept(std::is_nothrow_copy_constructible_v<value_type> &&
             std::is_nothrow_constructible_v<value_type, Key&&, T&&>);
bool insert(const value_type& value)                                                // (5)
    noexcept(std::is_nothrow_copy_constructible_v<value_type> &&
             std::is_nothrow_constructible_v<value_type, const Key&, const T&>);
```

Adds `value` under `key` when the key is absent, and does nothing when it is there, as the map's
[insert](../map/insert.md). The element goes into a node the builder made, in place when the node has room; a
node the builder shares with a map is copied once, with room for one entry more, the first time a change goes
through it.

- (1–4) The element is made of `key` and `value`, each copied or moved as it is passed.
- (5) The element is a copy of the pair `value`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element |
| `value` | the value of the element; (5) the element, a pair of the key and the value |

## Return value

`true` when the element was added, `false` when the key was there.

## Complexity

Logarithmic in `size()`, base 32: the key looked up, then a walk down the trie, a node copied the first time a
change goes through it.

## Exceptions

What the copy or the move of `Key` and `T` into the element, and the copy of the elements of the nodes the
builder copies, throw; none when they are noexcept.

When an exception is thrown, the builder holds the elements it held before, and the maps it froze are untouched.

## Notes

200,000 random `long` keys built one at a time through a builder take 88 ns each, against 455 ns for an insert
a version each ([Benchmarks: The builder](../benchmarks.md#the-builder)): a node the builder made takes the next
element where it lies until it is full, and grows into the next size once.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::map<string, int>::builder b;
    bool first = b.insert("http", 80);
    bool again = b.insert("http", 8080);  // the key is there: kept as it is
    println("{} {} {}", first, again, *b.try_get("http"));
}
```

Output:

```text
true false 80
```

## See also

- [set](set.md): puts a value under a key, added or in place
- [emplace](emplace.md): the value constructed in place
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::builder](README.md)
