[sgcl](../../README.md) › [core](../README.md) › [vector](../vector.md)

# sgcl::erase, sgcl::erase_if (sgcl::vector)

```cpp
#include "sgcl/core/vector.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T, class U> size_t erase(vector<T>& v, const U& value);     // (1)
    template<class T, class Pred> size_t erase_if(vector<T>& v, Pred pred);    // (2)
}

namespace std {
    using sgcl::erase;
    using sgcl::erase_if;
}
```

1. Erases every element equal to `value`. `value` may be an element of `v` (`erase(v, v[0])`): that element is
   moved out first, or copied when its move may throw, and the others are compared with it.
2. Erases every element for which `pred` returns `true`.

The elements that stay keep their order and move down over the erased ones; the vector destroys the last
elements, as many as were erased. The capacity stays.

## Parameters

| Parameter | Description |
|---|---|
| `v` | the vector to erase from |
| `value` | the value to compare the elements with; anything an element compares with by `==` |
| `pred` | a predicate called with each element, `bool pred(const T&)` |

## Return value

The number of erased elements.

## Complexity

Linear in `v.size()`: one comparison or call of `pred` per element.

## Exceptions

What the comparison, `pred` or the move assignment of `T` throws; (1) with an element of `v` as `value`, what
its copy throws when its move may throw.

## Notes

The functions are declared in `sgcl` and brought into `std`, as the other containers' are: `std::erase(v, x)`
calls them, and so does `erase(v, x)` written without a namespace, found by the argument's type.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector v = {1, 2, 2, 3, 4, 5};

    size_t twos = std::erase(v, 2);
    println("{} erased: {}", twos, v);

    size_t odd = erase_if(v, [](int x) { return x % 2 != 0; });
    println("{} erased: {}", odd, v);
}
```

Output:

```text
2 erased: [1, 3, 4, 5]
3 erased: [4]
```

## See also

- [erase](erase.md): erases the elements at a position or in a range
- [sgcl::vector\<T\>](../vector.md)
