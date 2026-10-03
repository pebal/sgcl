[sgcl](../../README.md) › [core](../README.md) › [multimap](../multimap.md)

# sgcl::multimap\<Key, T, Hash, KeyEqual\>::operator=

```cpp
multimap& operator=(const multimap& other);                        // (1)
multimap& operator=(multimap&& other)                              // (2)
    noexcept(std::is_nothrow_move_constructible_v<hasher> &&
             std::is_nothrow_move_constructible_v<key_equal> &&
             std::is_nothrow_move_assignable_v<hasher> &&
             std::is_nothrow_move_assignable_v<key_equal>);
multimap& operator=(std::initializer_list<value_type> ilist);      // (3)
```

Replaces the elements of the multimap.

1. Builds a copy of `other`, as the copy constructor does, and swaps it in; the old elements are destroyed
   before the call returns. Assigning a multimap to itself does nothing.
2. Clears this multimap, destroying its elements at once, and takes the table of `other` over, with its hasher,
   its equality and its `max_load_factor`; `other` is left empty, without buckets.
3. Builds a new table of the elements of `ilist`, every one kept, with this multimap's hasher, equality and
   `max_load_factor`, and swaps it in; the old elements are destroyed before the call returns.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the multimap the elements are copied or taken from |
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

If an exception is thrown, the multimap is as it was before the call: (1) and (3) build the new table aside, and
(2) moves the hasher and the equality out of `other` before it changes anything.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    multimap<int, string> a = {{1, "one"}, {1, "uno"}};
    multimap<int, string> b;

    b = a;
    println("{} {}", b.size(), b.count(1));

    b = {{5, "five"}, {5, "cinq"}, {6, "six"}};  // "one" and "uno" are destroyed here
    println("{} {} {}", b.size(), b.count(5), b.contains(1));

    a = std::move(b);
    println("{} {} {} {}", a.size(), a.count(6), b.size(), b.bucket_count());
}
```

Output:

```text
2 2
3 2 false
3 1 0 0
```

## See also

- [(constructor)](multimap.md): constructs a multimap
- [swap](swap.md): exchanges the contents of two multimaps
- [sgcl::multimap\<Key, T, Hash, KeyEqual\>](../multimap.md)
