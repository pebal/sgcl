[sgcl](../../README.md) › [core](../README.md) › [list](../list.md)

# sgcl::list\<T\>::splice

```cpp
/*(1)*/ void splice(const_iterator pos, list& other) noexcept;
/*(2)*/ void splice(const_iterator pos, list&& other) noexcept;
/*(3)*/ void splice(const_iterator pos, list& other, const_iterator it) noexcept;
/*(4)*/ void splice(const_iterator pos, list&& other, const_iterator it) noexcept;
/*(5)*/ void splice(const_iterator pos, list& other,
                    const_iterator first, const_iterator last) noexcept;
/*(6)*/ void splice(const_iterator pos, list&& other,
                    const_iterator first, const_iterator last) noexcept;
```

Moves nodes of `other` before `pos`, without copying or destroying an element.

- (1–2) Every node of `other`, which is empty after. Splicing a list into itself does nothing.
- (3–4) The node `it` names. `other` may be this list; when `it` is `pos` or the node just before it, nothing moves.
  `it` equal to `other.end()` moves nothing.
- (5–6) The nodes of the range `[first, last)`. `other` may be this list, with `pos` outside the range. A range that
  starts at the null `end()` of a list that had no sentinel yet is empty ([end](end.md)).

The nodes are relinked as they are: iterators and references to the moved elements stay valid and name elements of
this list now. The counts of both lists follow the nodes.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | the element before which the nodes go; `end()` appends |
| `other` | the list the nodes come from |
| `it` | the node to move, an element of `other` |
| `first`, `last` | the range of nodes to move, in `other` |

## Return value

None.

## Complexity

- (1–4) Constant.
- (5–6) Linear in the distance between `first` and `last` when `other` is another list, whose nodes are counted;
  constant within one list.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    list a = {1, 2};
    list b = {3, 4, 5};
    int& three = b.front();

    a.splice(a.end(), b, b.begin());
    println("{} {}", a, b);

    a.splice(a.begin(), b);
    println("{} {}", a, b);

    a.splice(a.end(), a, a.begin(), std::next(a.begin(), 2));  // within one list
    three *= 10;
    println("{}, size {}", a, a.size());
}
```

Output:

```text
[1, 2, 3] [4, 5]
[4, 5, 1, 2, 3] []
[1, 2, 30, 4, 5], size 5
```

## See also

- [merge](merge.md): moves the nodes of a sorted list into their order
- [insert](insert.md): inserts copies of elements
- [sgcl::list\<T\>](../list.md)
