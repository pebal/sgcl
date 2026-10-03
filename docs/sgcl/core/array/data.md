[sgcl](../../README.md) › [core](../README.md) › [array](README.md)

# sgcl::array\<T, N\>::data

```cpp
constexpr T* data() noexcept;                // (1)
constexpr const T* data() const noexcept;    // (2)
```

Returns a pointer to the first element: the elements lie inline, one after another, and `[data(), data() + N)` is
a valid range. For `array<T, 0>` it is `nullptr`.

## Parameters

None.

## Return value

A pointer to the first element; `nullptr` for `array<T, 0>`.

## Complexity

Constant.

## Exceptions

None.

## Notes

The pointer is into the array itself, which has no memory of its own: it is valid as long as the array. A
[slice](../slice/README.md) from [as_slice](as_slice.md) is the same view with its length.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <cstring>

using namespace sgcl;

int main() {
    array<char, 6> word = {'h', 'e', 'l', 'l', 'o', '\0'};
    println("{} {}", std::strlen(word.data()), word.data());

    array<int, 3> a = {1, 2, 3};
    int* p = a.data();
    p[1] = 20;
    println("{} {}", a, (void*)p == (void*)&a);

    array<int, 0> none;
    println("{}", none.data() == nullptr);
}
```

Output:

```text
5 hello
[1, 20, 3] true
true
```

## See also

- [as_slice, operator slice](as_slice.md): the elements as a slice
- [begin, cbegin](begin.md): an iterator to the first element
- [sgcl::array\<T, N\>](README.md)
