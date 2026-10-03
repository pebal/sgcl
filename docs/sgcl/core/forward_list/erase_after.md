[sgcl](../../README.md) › [core](../README.md) › [forward_list](README.md)

# sgcl::forward_list\<T\>::erase_after

```cpp
iterator erase_after(const_iterator pos) noexcept;                           // (1)
iterator erase_after(const_iterator first, const_iterator last) noexcept;    // (2)
```

Erases elements: destroys them and unlinks their nodes.

1. Erases the element after `pos`; [before_begin()](before_begin.md) erases the first. When `pos` is the last
   element, nothing is erased.
2. Erases the elements of the range `(first, last)`, between `first` and `last`, both excluded.

The unlinked nodes are the collector's: it reclaims them once nothing refers to them. Iterators and references to
the other elements stay valid.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | the element before the one to erase, or `before_begin()` |
| `first`, `last` | the elements around the range to erase |

## Return value

- (1) The iterator to the element after the erased one; `end()` when there is none after `pos`.
- (2) `last`.

## Complexity

- (1) Constant.
- (2) Linear in the number of erased elements.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    forward_list l = {1, 2, 3, 4, 5};
    auto it = l.erase_after(l.before_begin());
    println("{}, it -> {}", l, *it);

    l.erase_after(it, l.end());
    println("{}", l);

    println("{}", l.erase_after(it) == l.end());  // nothing after it
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
- [pop_front](pop_front.md): removes the first element
- [sgcl::forward_list\<T\>](README.md)
