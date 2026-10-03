[sgcl](../../README.md) › [core](../README.md) › [list](../list.md)

# sgcl::list\<T\>::remove, remove_if

```cpp
/*(1)*/ size_type remove(const T& value) noexcept(/* see below */);
/*(2)*/ template<class UnaryPredicate>
        size_type remove_if(UnaryPredicate pred)
            noexcept(std::is_nothrow_invocable_v<UnaryPredicate&, T&>);
```

Erases elements, walking the list once from the first to the last; the others keep their order and their nodes.

1. Erases every element equal to `value`, compared by `==`. A `value` that is an element of this list is erased
   last, after the comparisons that read it.
2. Erases every element for which `pred` returns `true`.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value to compare the elements with |
| `pred` | a predicate called with each element, `bool pred(T&)` |

## Return value

The number of erased elements.

## Complexity

Linear in `size()`: one comparison or call of `pred` per element.

## Exceptions

- (1) What `==` of the elements throws; none when it is noexcept, as for `int`.
- (2) What `pred` throws; none when its call is noexcept.

If an exception is thrown, the list stays valid: the elements erased before it are gone, the others are in their
places.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    list l = {1, 2, 2, 3, 4};
    size_t twos = l.remove(2);
    println("{} erased: {}", twos, l);

    size_t big = l.remove_if([](int x) { return x > 2; });
    println("{} erased: {}", big, l);

    list m = {5, 1, 5, 5};
    size_t fives = m.remove(m.front());  // an element of m itself
    println("{} erased: {}", fives, m);
}
```

Output:

```text
2 erased: [1, 3, 4]
2 erased: [1]
3 erased: [1]
```

## See also

- [erase, erase_if](erase_if.md): the same under the names of `std`, as non-member functions
- [unique](unique.md): erases consecutive equal elements
- [erase](erase.md): erases the elements at a position or in a range
- [sgcl::list\<T\>](../list.md)
