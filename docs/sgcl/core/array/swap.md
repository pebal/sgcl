[sgcl](../../README.md) › [core](../README.md) › [array](README.md)

# sgcl::array\<T, N\>::swap

```cpp
constexpr void swap(array& other) noexcept(std::is_nothrow_swappable_v<T>);
```

Exchanges the elements of this array with those of `other`, element by element: the elements lie inline, so there
is no buffer to exchange. For `array<T, 0>` it does nothing, and is `noexcept`.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the array to exchange the elements with |

## Return value

None.

## Complexity

Linear in `N`.

## Exceptions

What the swap of two elements throws; none when it is noexcept. If it throws, the elements before the pair whose
swap threw are exchanged, the others are not.

## Notes

An iterator, a pointer or a reference to an element stays in its own array after the swap, and sees there the
value the other array had: the elements are exchanged, not the storage, as with `std::array`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    array<string, 2> a = {"a", "b"};
    array<string, 2> b = {"x", "y"};
    string* first = &a[0];

    a.swap(b);
    println("{} {} {}", a, b, *first);
}
```

Output:

```text
["x", "y"] ["a", "b"] x
```

## See also

- [swap](swap2.md): the non-member form
- [fill](fill.md): assigns a value to every element
- [sgcl::array\<T, N\>](README.md)
