[sgcl](../../README.md) › [core](../README.md) › [sorted_multiset](README.md)

# sgcl::sorted_multiset\<Key, Compare\>::operator=

```cpp
sorted_multiset& operator=(const sorted_multiset& other) = default;     // (1)
sorted_multiset& operator=(sorted_multiset&& other) = default;          // (2)
sorted_multiset& operator=(std::initializer_list<value_type> ilist);    // (3)
```

Replaces the contents of the multiset.

1. Copy assignment: the elements of this multiset are destroyed at once, the comparison of `other` is taken, and
   the tree of `other` is copied shape for shape, as the copy constructor copies it. Assigning a multiset to itself
   does nothing.
2. Move assignment: the elements of this multiset are destroyed at once, and the tree and the comparison of `other`
   are taken over; `other` is left empty.
3. The elements of this multiset are destroyed at once, and every element of `ilist` is inserted, as the
   [constructor](sorted_multiset.md) inserts them; the comparison stays.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the multiset to copy or to move from |
| `ilist` | the list the elements are copied from |

## Return value

`*this`.

## Complexity

- (1) Linear in the size of this multiset and of `other`.
- (2) Linear in the size of this multiset.
- (3) Linear in the size of this multiset, plus *N* log *N* in the size *N* of `ilist`; linear when it is sorted.

## Exceptions

- (1) What the copy of `Compare` or of an element throws.
- (2) What the move assignment of `Compare` throws; none when it is noexcept.
- (3) What the copy of an element throws.

If the copy of `Compare` throws in (1), the multiset is as it was. If the copy of an element throws, (1) leaves the
multiset empty and (3) leaves the elements inserted before it.

## Notes

The old elements die in the assignment, not when the collector comes: their destructors run before it returns.
Their nodes are left to the collector.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_multiset<int> a = {1, 1};
    sorted_multiset<int> b;

    b = a;
    println("{} {}", a, b);

    b = {5};  // the two 1s are destroyed here
    println("{}", b);

    a = std::move(b);
    println("{} {}", a, b.empty());
}
```

Output:

```text
{1, 1} {1, 1}
{5}
{5} true
```

## See also

- [(constructor)](sorted_multiset.md): constructs a multiset
- [clear](clear.md): destroys every element
- [sgcl::sorted_multiset\<Key, Compare\>](README.md)
