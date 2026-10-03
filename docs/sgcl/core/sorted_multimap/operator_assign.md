[sgcl](../../README.md) › [core](../README.md) › [sorted_multimap](README.md)

# sgcl::sorted_multimap\<Key, T, Compare\>::operator=

```cpp
sorted_multimap& operator=(const sorted_multimap& other);               // (1)
sorted_multimap& operator=(sorted_multimap&& other)                     // (2)
    noexcept(std::is_nothrow_move_assignable_v<key_compare>);
sorted_multimap& operator=(std::initializer_list<value_type> ilist);    // (3)
```

Replaces the contents of the multimap. The elements the multimap held are destroyed at once, before the new ones
come.

1. Clears the multimap, takes `other`'s comparator and copies its tree shape for shape, as the copy constructor
   does. Self-assignment does nothing.
2. Clears the multimap and takes the tree of `other` over, with its comparator; `other` is empty after.
   Self-assignment does nothing.
3. Clears the multimap and inserts the elements of `ilist`, as [insert](insert.md) does.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the multimap the elements are copied or taken from |
| `ilist` | the list the elements are copied from |

## Return value

`*this`.

## Complexity

- (1) Linear in `size()` and `other.size()`, with no comparison.
- (2) Linear in `size()`, the elements destroyed.
- (3) Linear in `size()`, plus *n* log *n* comparisons for the *n* elements of `ilist`, linear when they come
  sorted.

## Exceptions

- (1) What the copy constructor of `value_type` and the copy of `Compare` throw.
- (2) What the move assignment of `Compare` throws; none when it is noexcept.
- (3) What the copy constructor of `value_type` throws.

If an element's copy throws in (1), the multimap is left empty; in (3), it holds the elements inserted before.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_multimap<int, int> a = {{1, 1}, {1, 2}}, b;
    b = a;
    println("{} {}", b, a == b);

    b = {{1, 1}, {1, 3}};
    println("{} {}", b, a < b);

    a = std::move(b);
    println("{} {}", a, b.empty());
}
```

Output:

```text
{1: 1, 1: 2} true
{1: 1, 1: 3} true
{1: 1, 1: 3} true
```

## See also

- [(constructor)](sorted_multimap.md): constructs a multimap
- [clear](clear.md): destroys every element
- [sgcl::sorted_multimap\<Key, T, Compare\>](README.md)
