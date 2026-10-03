[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::operator=

```cpp
ordered_map& operator=(const ordered_map& other);                   // (1)
ordered_map& operator=(ordered_map&& other)                         // (2)
    noexcept(std::is_nothrow_move_constructible_v<hasher> &&
             std::is_nothrow_move_constructible_v<key_equal> &&
             std::is_nothrow_move_assignable_v<hasher> &&
             std::is_nothrow_move_assignable_v<key_equal>);
ordered_map& operator=(std::initializer_list<value_type> ilist);    // (3)
```

Replaces the contents of the map.

1. A copy of `other`, its order with it: the copy is built aside and swapped in, and the old elements are
   destroyed with it. Assigning a map to itself does nothing.
2. Destroys the elements of this map at once and takes the table of `other` over, the order with it; `other`
   is empty after, with no bucket array. Assigning a map to itself does nothing.
3. The elements of `ilist`, in its order, of two with one key the first: a new table with this map's hash,
   equality and `max_load_factor` is built aside and swapped in.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the map the elements are copied or taken from |
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

If an exception is thrown, the map is as it was before the call: (1) and (3) build the new table before they
touch this one, and (2) moves the hash and the equality out of `other` before it clears this map.

## Notes

Every iterator into this map is invalidated, its `end()` included: the map gets the sentinel of the new table.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_map<string, int> a = {{"x", 1}, {"y", 2}};
    ordered_map<string, int> b;

    b = a;
    println("{}", b);

    b = {{"q", 7}, {"p", 8}, {"q", 9}};  // the old elements are destroyed here
    println("{}", b);

    a = std::move(b);
    println("{} {}", a, b.size());
}
```

Output:

```text
{"x": 1, "y": 2}
{"q": 7, "p": 8}
{"q": 7, "p": 8} 0
```

## See also

- [(constructor)](ordered_map.md): constructs a map
- [swap](swap.md): exchanges the contents of two maps
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)
