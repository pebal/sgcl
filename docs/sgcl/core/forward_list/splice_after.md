[sgcl](../../README.md) › [core](../README.md) › [forward_list](../forward_list.md)

# sgcl::forward_list\<T\>::splice_after

```cpp
void splice_after(const_iterator pos, forward_list& other) noexcept;                        // (1)
void splice_after(const_iterator pos, forward_list&& other) noexcept;                       // (2)
void splice_after(const_iterator pos, forward_list& other, const_iterator it) noexcept;     // (3)
void splice_after(const_iterator pos, forward_list&& other, const_iterator it) noexcept;    // (4)
void splice_after(const_iterator pos, forward_list& other,                                  // (5)
                  const_iterator first, const_iterator last) noexcept;
void splice_after(const_iterator pos, forward_list&& other,                                 // (6)
                  const_iterator first, const_iterator last) noexcept;
```

Moves nodes of `other` after `pos`, without copying or destroying an element; [before_begin()](before_begin.md)
puts them at the front.

- (1–2) Every node of `other`, which is empty after. `other` is walked to its last node. Splicing a list into
  itself does nothing.
- (3–4) The node after `it`. `other` may be this list; when there is no node after `it`, or `pos` is `it` or that
  node, nothing moves.
- (5–6) The nodes of the range `(first, last)`, between `first` and `last`, both excluded. `other` may be this
  list, with `pos` outside the range.

The nodes are relinked as they are: iterators and references to the moved elements stay valid and name elements of
this list now.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | the element after which the nodes go, or `before_begin()` |
| `other` | the list the nodes come from |
| `it` | the element before the node to move, in `other`, or its `before_begin()` |
| `first`, `last` | the elements around the range of nodes to move, in `other` |

## Return value

None.

## Complexity

- (1–2) Linear in the number of elements of `other`.
- (3–4) Constant.
- (5–6) Linear in the number of moved nodes.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    forward_list a = {1, 2};
    forward_list b = {3, 4, 5};
    int& three = b.front();

    a.splice_after(a.begin(), b, b.before_begin());
    println("{} {}", a, b);

    a.splice_after(a.before_begin(), b);
    println("{} {}", a, b);

    // the two nodes after the first to the front: within one list
    a.splice_after(a.before_begin(), a, a.begin(), std::next(a.begin(), 3));
    three *= 10;
    println("{}", a);
}
```

Output:

```text
[1, 3, 2] [4, 5]
[4, 5, 1, 3, 2] []
[5, 1, 4, 30, 2]
```

## See also

- [merge](merge.md): moves the nodes of a sorted list into their order
- [insert_after](insert_after.md): inserts copies of elements
- [sgcl::forward_list\<T\>](../forward_list.md)
