[sgcl](../../README.md) › [core](../README.md) › [ordered_set](../ordered_set.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::operator=

```cpp
ordered_set& operator=(const ordered_set& other);                   // (1)
ordered_set& operator=(ordered_set&& other)                         // (2)
    noexcept(std::is_nothrow_move_constructible_v<hasher> &&
             std::is_nothrow_move_constructible_v<key_equal> &&
             std::is_nothrow_move_assignable_v<hasher> &&
             std::is_nothrow_move_assignable_v<key_equal>);
ordered_set& operator=(std::initializer_list<value_type> ilist);    // (3)
```

Replaces the contents of the set.

1. A copy of `other`, its order with it: the copy is built aside and swapped in, and the old elements are
   destroyed with it. Assigning a set to itself does nothing.
2. Destroys the elements of this set at once and takes the table of `other` over, the order with it; `other`
   is empty after, with no bucket array. Assigning a set to itself does nothing.
3. The elements of `ilist`, in its order, of equal ones the first: a new table with this set's hash, equality
   and `max_load_factor` is built aside and swapped in.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the set the elements are copied or taken from |
| `ilist` | the list the elements are copied from |

## Return value

`*this`.

## Complexity

- (1) Linear in `size()` and `other.size()`.
- (2) Linear in `size()`: the old elements are destroyed.
- (3) Linear in `size()` and the size of `ilist` on average.

## Exceptions

- (1) What the copy of an element, of `Hash` or of `KeyEqual` throws.
- (2) What the move of `Hash` or `KeyEqual` throws; none when it is noexcept.
- (3) What the construction of an element, or the copy of `Hash` or `KeyEqual`, throws.

If an exception is thrown, the set is as it was before the call: (1) and (3) build the new table before they
touch this one, and (2) moves the hash and the equality out of `other` before it clears this set.

## Notes

Every iterator into this set is invalidated, its `end()` included: the set gets the sentinel of the new table.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_set<int> a = {2, 1};
    ordered_set<int> b;

    b = a;
    println("{}", b);

    b = {6, 5, 6};  // the old elements are destroyed here
    println("{}", b);

    a = std::move(b);
    println("{} {}", a, b.size());
}
```

Output:

```text
{2, 1}
{6, 5}
{6, 5} 0
```

## See also

- [(constructor)](ordered_set.md): constructs a set
- [swap](swap.md): exchanges the contents of two sets
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](../ordered_set.md)
