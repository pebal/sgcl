[sgcl](../../README.md) › [concurrent](../README.md) › [cache](../cache.md)

# sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>::put

```cpp
void put(const Key& key, const T& value)                     // (1)
    noexcept(std::is_nothrow_copy_constructible_v<Key> &&
             std::is_nothrow_copy_constructible_v<T>);
void put(const Key& key, T&& value)                          // (2)
    noexcept(std::is_nothrow_copy_constructible_v<Key> &&
             std::is_nothrow_copy_constructible_v<T> &&
             std::is_nothrow_move_constructible_v<T>);
```

Inserts the value under `key`, or replaces the value there; then, if the size is past the capacity, evicts down to
it, one pass of `sample` entries per entry over.

1. Puts a copy of `value`.
2. Puts `value`: copied into the node of a new entry, moved into the box of a replacement.

A new entry is a node of the map with a copy of the value, so that an insertion lost to another thread's of the
same key (the node built, then found taken, and dropped) loses nothing; the `put` that lost replaces the value of
the entry that won. A replacement puts the value in a box of its own that the entry points to from then on: there
is no moment of absence, and a `get` in flight reads the old value whole. Either way the entry is stamped with the
tick of the `put`, newer than every use before it, and its time to live runs from now.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the entry |
| `value` | the value to put under it |

## Return value

None.

## Complexity

Constant on average for the insertion or the replacement. An insertion past the capacity adds the eviction: a walk
of `sample` entries on from the stripe's cursor and an erasure, for each entry over.

## Exceptions

- (1) What the copy constructors of `Key` and `T` throw; none when they are noexcept.
- (2) What the copy constructors of `Key` and `T` and the move constructor of `T` throw; none when they are
  noexcept.

If an exception is thrown, the entry is not linked and no box is stored: the cache is as it was.

## Notes

Lock-free: the map's `try_emplace`, linearizable at the compare-exchange that links the node, or a store of the box
into an entry already there; then the eviction, the map's lock-free erasures. A `put` that replaces the value of an
entry another thread is erasing at that moment puts it again, as a new entry: a `put` is never lost under an
erasure. Two shared words are written by every `put`, the tick and, for a new entry, the count, each on a cache
line of its own.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::cache<string, string> recent(2);  // the LRU cache of two of ordered_map, shared
    recent.put("a", "1");
    recent.put("b", "2");
    recent.get("a");  // a is newer than b now
    recent.put("c", "3");  // full: b, the oldest, goes
    for (const char* key : {"a", "b", "c"}) {
        println("{} {}", key, recent.get(key).has_value());
    }

    recent.put("a", "one");  // replaced: the size stays
    string a = *recent.get("a");
    println("{} {}", a, recent.size());
}
```

Output:

```text
a true
b false
c true
one 2
```

## See also

- [get](get.md): a copy of the value
- [get_or_compute](get_or_compute.md): puts the value computed when a `get` finds none
- [erase](erase.md): erases the entry of a key
- [sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>](../cache.md)
