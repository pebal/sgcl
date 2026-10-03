[sgcl](../../README.md) › [core](../README.md) › [dynamic_array](README.md)

# sgcl::dynamic_array\<T\>::size

```cpp
size_type size() const noexcept;
```

Returns the number of elements: the count the array was made with, which the handle holds beside the pointer to
the buffer.

## Parameters

None.

## Return value

The number of elements.

## Complexity

Constant.

## Exceptions

None.

## Notes

There is no capacity: the size class of the buffer may give it room for more elements, but the array never uses
it and never grows into it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    int n = 3;
    dynamic_array<double> weights(n * 2);  // a size known at run time
    println("{}", weights.size());

    weights = {0.5, 1.5};
    println("{}", weights.size());
}
```

Output:

```text
6
2
```

## See also

- [empty](empty.md): checks whether the array is empty
- [max_size](max_size.md): the largest number of elements an array may hold
- [sgcl::dynamic_array\<T\>](README.md)
