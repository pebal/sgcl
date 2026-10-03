[sgcl](../../README.md) › [core](../README.md) › [set](../set.md)

# sgcl::set\<Key, Hash, KeyEqual\>::operator=

```cpp
/*(1)*/ set& operator=(const set& other);
/*(2)*/ set& operator=(set&& other)
            noexcept(std::is_nothrow_move_constructible_v<hasher> &&
                     std::is_nothrow_move_constructible_v<key_equal> &&
                     std::is_nothrow_move_assignable_v<hasher> &&
                     std::is_nothrow_move_assignable_v<key_equal>);
/*(3)*/ set& operator=(std::initializer_list<value_type> ilist);
```

Replaces the elements of the set.

1. Builds a copy of `other`, as the copy constructor does, and swaps it in; the old elements are destroyed
   before the call returns.
2. Clears this set, destroying its elements at once, and takes the table of `other` over, with its hasher, its
   equality and its `max_load_factor`; `other` is left empty, without buckets.
3. Builds a new table of the elements of `ilist` with this set's hasher, equality and `max_load_factor`, and
   swaps it in; the old elements are destroyed before the call returns.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the set the elements are copied or taken from |
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

If an exception is thrown, the set is as it was before the call: (1) and (3) build the new table aside, and (2)
moves the hasher and the equality out of `other` before it changes anything.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    set<int> a = {1, 2};
    set<int> b;
    b = a;
    println("{} {}", b.size(), b.contains(2));

    b = {5, 6, 7};  // the old elements die here
    println("{} {}", b.size(), b.contains(1));

    a = std::move(b);
    println("{} {} {}", a.size(), a.contains(5), b.size());
}
```

Output:

```text
2 true
3 false
3 true 0
```

## See also

- [(constructor)](set.md): constructs a set
- [swap](swap.md): exchanges the contents of two sets
- [sgcl::set\<Key, Hash, KeyEqual\>](../set.md)
