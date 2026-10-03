[sgcl](../../README.md) › [core](../README.md) › [array](../array.md)

# sgcl::array\<T, N\>::operator[]

```cpp
/*(1)*/ constexpr reference operator[](size_type pos) noexcept;
/*(2)*/ constexpr const_reference operator[](size_type pos) const noexcept;
```

Returns a reference to the element at `pos`, without bounds checking: `pos` must be less than `N`. `array<T, 0>`
has no `operator[]`.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | the position of the element |

## Return value

A reference to the element.

## Complexity

Constant.

## Exceptions

None.

## Notes

[at](at.md) is the same access with the check. In a constant expression a `pos` outside the array is an error at
compile time; at run time it is undefined behaviour, as with `std::array`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    array<tracked_ptr<string>, 3> names = {};
    names[0] = make_tracked<string>("Ada");
    names[2] = make_tracked<string>("Grace");

    for (int i : range(3)) {
        println("{}: {}", i, names[i] ? *names[i] : "-");
    }

    constexpr array<int, 3> primes = {2, 3, 5};
    constexpr int last = primes[2];
    println("{}", last);
}
```

Output:

```text
0: Ada
1: -
2: Grace
5
```

## See also

- [at](at.md): access an element with bounds checking
- [front](front.md), [back](back.md): the first and the last element
- [sgcl::array\<T, N\>](../array.md)
