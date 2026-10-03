[sgcl](../../README.md) › [core](../README.md) › [map](README.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::operator=

```cpp
map& operator=(const map& other);                                  // (1)
map& operator=(map&& other)                                        // (2)
    noexcept(std::is_nothrow_move_constructible_v<hasher> &&
             std::is_nothrow_move_constructible_v<key_equal> &&
             std::is_nothrow_move_assignable_v<hasher> &&
             std::is_nothrow_move_assignable_v<key_equal>);
map& operator=(std::initializer_list<value_type> ilist);           // (3)
```

Replaces the elements of the map.

1. Builds a copy of `other`, as the copy constructor does, and swaps it in; the old elements are destroyed
   before the call returns. Assigning a map to itself does nothing.
2. Clears this map, destroying its elements at once, and takes the table of `other` over, with its hasher, its
   equality and its `max_load_factor`; `other` is left empty, without buckets.
3. Builds a new table of the elements of `ilist` with this map's hasher, equality and `max_load_factor`, and
   swaps it in; of several elements with one key the first is kept. The old elements are destroyed before the
   call returns.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the map the elements are copied or taken from |
| `ilist` | the list the elements are copied from |

## Return value

`*this`.

## Complexity

- (1) Linear in `size()` and `other.size()`.
- (2) Linear in `size()`, the elements destroyed.
- (3) Linear in `size()` and `ilist.size()` on average.

## Exceptions

- (1) What the copy of an element, of `Hash` or of `KeyEqual` throws.
- (2) What the move of `Hash` or of `KeyEqual` throws; none when it is noexcept.
- (3) What the copy of an element, of `Hash` or of `KeyEqual` throws.

If an exception is thrown, the map is as it was before the call: (1) and (3) build the new table aside, and (2)
moves the hasher and the equality out of `other` before it changes anything.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    map<int, string> a = {{1, "one"}};
    map<int, string> b;

    b = a;
    println("{} {}", b.size(), b.at(1));

    b = {{5, "five"}, {6, "six"}, {5, "cinq"}};  // "one" is destroyed here
    println("{} {} {}", b.size(), b.at(5), b.contains(1));

    a = std::move(b);
    println("{} {} {} {}", a.size(), a.at(6), b.size(), b.bucket_count());
}
```

Output:

```text
1 one
2 five false
2 six 0 0
```

## See also

- [(constructor)](map.md): constructs a map
- [swap](swap.md): exchanges the contents of two maps
- [sgcl::map\<Key, T, Hash, KeyEqual\>](README.md)
