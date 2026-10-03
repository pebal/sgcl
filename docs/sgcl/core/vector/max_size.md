[sgcl](../../README.md) › [core](../README.md) › [vector](README.md)

# sgcl::vector\<T\>::max_size

```cpp
size_type max_size() const noexcept;
```

Returns the largest number of elements a vector of `T` may hold: the largest `ptrdiff_t` divided by
`sizeof(T)`, so that the distance between any two iterators is a `difference_type`. It is the bound above which
[reserve](reserve.md), [resize](resize.md), the insertions and the constructors throw `length_error`; the
memory of the machine ends long before it.

## Parameters

None.

## Return value

The largest number of elements.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<int> v;
    println("{}", v.max_size());

    try {
        v.reserve(v.max_size() + 1);
    } catch (const length_error& e) {
        println("length error: {}", e.what());
    }
}
```

Output:

```text
2305843009213693951
length error: sgcl::vector
```

## See also

- [capacity](capacity.md): the number of elements the buffer holds
- [reserve](reserve.md): reserves storage
- [sgcl::vector\<T\>](README.md)
