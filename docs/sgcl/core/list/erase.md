[sgcl](../../README.md) › [core](../README.md) › [list](README.md)

# sgcl::list\<T\>::erase

```cpp
iterator erase(const_iterator pos) noexcept;                           // (1)
iterator erase(const_iterator first, const_iterator last) noexcept;    // (2)
```

Erases elements: destroys them and unlinks their nodes.

1. Erases the element at `pos`. `erase(end())` erases nothing.
2. Erases the elements of the range `[first, last)`. A range that starts at the null `end()` of a list that had no
   sentinel yet is empty ([end](end.md)).

The unlinked nodes are the collector's: it reclaims them once nothing refers to them. Iterators and references to
the other elements stay valid.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | the element to erase |
| `first`, `last` | the range of elements to erase |

## Return value

The iterator to the element after the erased ones; `pos` (1) or `last` (2) when nothing was erased.

## Complexity

- (1) Constant.
- (2) Linear in the distance between `first` and `last`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    list l = {1, 2, 3, 4, 5};
    auto it = l.erase(l.begin());
    println("{}, it -> {}", l, *it);

    l.erase(std::next(it), l.end());
    println("{}", l);

    println("{}", l.erase(l.end()) == l.end());  // nothing to erase
}
```

Output:

```text
[2, 3, 4, 5], it -> 2
[2]
true
```

## See also

- [clear](clear.md): destroys every element
- [remove, remove_if](remove.md): erase the elements equal to a value, or satisfying a predicate
- [erase, erase_if](erase_if.md): the same as non-member functions
- [sgcl::list\<T\>](README.md)
