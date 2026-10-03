[sgcl](../../README.md) › [core](../README.md) › [slice](README.md)

# sgcl::slice\<T\>::first

```cpp
slice first(size_type n) const noexcept;
```

The first `n` elements as a slice of the same owner, as `std::span::first`. Precondition: `n <= size()`; a debug
build asserts it.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of elements |

## Return value

A slice of the first `n` elements, with the same owner.

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
    vector<byte> buffer(16);
    slice<byte> room = buffer;
    size_t n = 5;  // what a read put in the buffer
    slice<byte> data = room.first(n);
    println("{} {}", data.size(), data.owned());
}
```

Output:

```text
5 true
```

## See also

- [last](last.md): the last `n` elements
- [subslice](subslice.md): the elements `[pos, pos + n)`
- [sgcl::slice\<T\>](README.md)
