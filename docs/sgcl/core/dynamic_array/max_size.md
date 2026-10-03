[sgcl](../../README.md) › [core](../README.md) › [dynamic_array](../dynamic_array.md)

# sgcl::dynamic_array\<T\>::max_size

```cpp
size_type max_size() const noexcept;
```

Returns the largest number of elements an array of `T` may be made with: `PTRDIFF_MAX / sizeof(T)`, so that the
distance between two iterators fits a `difference_type`. A constructor asked for more throws `length_error`.

## Parameters

None.

## Return value

`PTRDIFF_MAX / sizeof(T)`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <cstdint>

using namespace sgcl;

int main() {
    dynamic_array<int> a;
    println("{}", a.max_size() == PTRDIFF_MAX / sizeof(int));

    try {
        dynamic_array<int> huge(a.max_size() + 1);
    } catch (const length_error& e) {
        println("length error: {}", e.what());
    }
}
```

Output:

```text
true
length error: sgcl::dynamic_array
```

## See also

- [size](size.md): the number of elements
- [sgcl::dynamic_array\<T\>](../dynamic_array.md)
