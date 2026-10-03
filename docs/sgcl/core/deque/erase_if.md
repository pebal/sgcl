[sgcl](../../README.md) › [core](../README.md) › [deque](README.md)

# sgcl::erase, sgcl::erase_if (sgcl::deque)

```cpp
#include "sgcl/core/deque.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T, class U>
    typename deque<T>::size_type erase(deque<T>& c, const U& value);    // (1)
    template<class T, class Pred>
    typename deque<T>::size_type erase_if(deque<T>& c, Pred pred);      // (2)
}

namespace std {
    using sgcl::erase;
    using sgcl::erase_if;
}
```

1. Erases every element equal to `value`. `value` may be an element of `c` (`erase(c, c[0])`): that element is
   moved out first, or copied when its move may throw, and the others are compared with it.
2. Erases every element for which `pred` returns `true`.

The elements that stay keep their order and move toward the front over the erased ones; the deque pops the last
elements, as many as were erased.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the deque to erase from |
| `value` | the value to compare the elements with; anything an element compares with by `==` |
| `pred` | a predicate called with each element, `bool pred(const T&)` |

## Return value

The number of erased elements.

## Complexity

Linear in `c.size()`: one comparison or call of `pred` per element.

## Exceptions

What the comparison, `pred` or the move assignment of `T` throws; (1) with an element of `c` as `value`, what
its copy throws when its move may throw.

If an exception is thrown, the deque stays consistent and every element is destroyed exactly once, but the values
may have moved.

## Notes

The functions are declared in `sgcl` and brought into `std`, as the other containers' are: `std::erase(d, x)`
calls them, and so does `erase(d, x)` written without a namespace, found by the argument's type.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    deque d = {1, 2, 2, 3, 4};

    size_t twos = std::erase(d, 2);
    println("{} erased: {}", twos, d);

    size_t big = erase_if(d, [](int x) { return x > 2; });
    println("{} erased: {}", big, d);
}
```

Output:

```text
2 erased: [1, 3, 4]
2 erased: [1]
```

## See also

- [erase](erase.md): erases the elements at a position or in a range
- [sgcl::deque\<T\>](README.md)
