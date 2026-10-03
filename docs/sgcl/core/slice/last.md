[sgcl](../../README.md) › [core](../README.md) › [slice](../slice.md)

# sgcl::slice\<T\>::last

```cpp
slice last(size_type n) const noexcept;
```

The last `n` elements as a slice of the same owner, as `std::span::last`. Precondition: `n <= size()`; a debug build
asserts it.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of elements |

## Return value

A slice of the last `n` elements, with the same owner.

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
    string name = "report.txt";
    string_slice s = name;
    println("{}", s.last(3));
}
```

Output:

```text
txt
```

## See also

- [first](first.md): the first `n` elements
- [sgcl::slice\<T\>](../slice.md)
