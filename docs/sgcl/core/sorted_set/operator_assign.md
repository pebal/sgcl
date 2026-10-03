[sgcl](../../README.md) › [core](../README.md) › [sorted_set](README.md)

# sgcl::sorted_set\<Key, Compare\>::operator=

```cpp
sorted_set& operator=(const sorted_set& other) = default;          // (1)
sorted_set& operator=(sorted_set&& other) = default;               // (2)
sorted_set& operator=(std::initializer_list<value_type> ilist);    // (3)
```

Replaces the contents of the set.

1. Copy assignment: the elements of this set are destroyed at once, the comparison of `other` is taken, and the
   tree of `other` is copied shape for shape, as the copy constructor copies it. Assigning a set to itself does
   nothing.
2. Move assignment: the elements of this set are destroyed at once, and the tree and the comparison of `other` are
   taken over; `other` is left empty.
3. The elements of this set are destroyed at once, and the elements of `ilist` are inserted, as the
   [constructor](sorted_set.md) inserts them; the comparison stays.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the set to copy or to move from |
| `ilist` | the list the elements are copied from |

## Return value

`*this`.

## Complexity

- (1) Linear in the size of this set and of `other`.
- (2) Linear in the size of this set.
- (3) Linear in the size of this set, plus *N* log *N* in the size *N* of `ilist`; linear when it is sorted.

## Exceptions

- (1) What the copy of `Compare` or of an element throws.
- (2) What the move assignment of `Compare` throws; none when it is noexcept.
- (3) What the copy of an element throws.

If the copy of `Compare` throws in (1), the set is as it was. If the copy of an element throws, (1) leaves the set
empty and (3) leaves the elements inserted before it.

## Notes

The old elements die in the assignment, not when the collector comes: their destructors run before it returns.
Their nodes are left to the collector.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_set<int> a = {1, 2};
    sorted_set<int> b;

    b = a;
    println("{} {}", a, b);

    b = {6, 5};  // 1 and 2 are destroyed here
    println("{}", b);

    a = std::move(b);
    println("{} {}", a, b.empty());
}
```

Output:

```text
{1, 2} {1, 2}
{5, 6}
{5, 6} true
```

## See also

- [(constructor)](sorted_set.md): constructs a set
- [clear](clear.md): destroys every element
- [sgcl::sorted_set\<Key, Compare\>](README.md)
