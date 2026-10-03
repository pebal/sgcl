[sgcl](../../README.md) › [core](../README.md) › [vector](../vector.md)

# sgcl::vector\<T\>::front

```cpp
reference front() noexcept;                // (1)
const_reference front() const noexcept;    // (2)
```

Returns a reference to the first element, `(*this)[0]`. The vector must not be empty.

## Parameters

None.

## Return value

A reference to the first element.

## Complexity

Constant.

## Exceptions

None.

## Notes

`front` on an empty vector is undefined behaviour, as with `std::vector`. The reference does not keep the
buffer alive: it is valid until the element is removed, the vector reallocates or the vector is destroyed.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector scores = {70, 85, 90};
    scores.front() += 5;
    println("{} of {}", scores.front(), scores.size());
}
```

Output:

```text
75 of 3
```

## See also

- [back](back.md): access the last element
- [operator[]](operator_at.md): access the element at a position
- [sgcl::vector\<T\>](../vector.md)
