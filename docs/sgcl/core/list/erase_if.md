[sgcl](../../README.md) › [core](../README.md) › [list](README.md)

# sgcl::erase, sgcl::erase_if (sgcl::list)

```cpp
#include "sgcl/core/list.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T, class U>
    typename list<T>::size_type erase(list<T>& c, const U& value) noexcept(/* see below */);    // (1)
    template<class T, class Pred>
    typename list<T>::size_type erase_if(list<T>& c, Pred pred) noexcept(/* see below */);      // (2)
}

namespace std {
    using sgcl::erase;
    using sgcl::erase_if;
}
```

[remove_if](remove.md) under the names of `std`.

1. Erases every element equal to `value`. A `value` of the type `T` goes through [remove](remove.md), which
   erases an element of `c` that `value` refers to last, after the comparisons that read it (`erase(c, c.front())`);
   a `value` of another type through `c.remove_if` with `element == value`.
2. Erases every element for which `pred` returns `true`, as `c.remove_if(pred)`.

The elements that stay keep their order and their nodes.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the list to erase from |
| `value` | the value to compare the elements with; anything an element compares with by `==` |
| `pred` | a predicate called with each element, `bool pred(T&)` |

## Return value

The number of erased elements.

## Complexity

Linear in `c.size()`: one comparison or call of `pred` per element.

## Exceptions

- (1) What `T`'s `==` with a `U` throws; none when it is noexcept.
- (2) What the copy or the call of `pred` throws; none when both are noexcept.

If an exception is thrown, the list stays valid: the elements erased before it are gone, the others are in their
places.

## Notes

The functions are declared in `sgcl` and brought into `std`, as the other containers' are: `std::erase(l, x)`
calls them, and so does `erase(l, x)` written without a namespace, found by the argument's type.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    list l = {1, 2, 2, 3, 4};

    size_t twos = std::erase(l, 2);
    println("{} erased: {}", twos, l);

    size_t big = erase_if(l, [](int x) { return x > 2; });
    println("{} erased: {}", big, l);
}
```

Output:

```text
2 erased: [1, 3, 4]
2 erased: [1]
```

## See also

- [remove, remove_if](remove.md): the same as members
- [erase](erase.md): erases the elements at a position or in a range
- [sgcl::list\<T\>](README.md)
