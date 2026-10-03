[sgcl](../../README.md) › [io](../README.md) › [buffer](README.md)

# sgcl::io::buffer::reserve

```cpp
void reserve(size_t n) const;
```

Makes room for `n` bytes beside the bytes not yet read, so that writes up to that size allocate nothing more. A
buffer whose final size is known up front reserves it once, as `std::vector::reserve` does. Nothing held changes.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of bytes to make room for, counted from the first byte not yet read |

## Return value

None.

## Complexity

Linear in the bytes held when the memory grows; constant otherwise.

## Exceptions

`length_error` when `n` passes the largest size of a vector. The buffer is as it was.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer frame;
    frame.reserve(1024);
    for (string part : {"head,", "body,", "tail"}) {
        frame.write(part);
    }
    println("{} bytes: {}", frame.size(), frame.text());
}
```

Output:

```text
14 bytes: head,body,tail
```

## See also

- [clear](clear.md): drops the bytes, keeps the memory
- [sgcl::io::buffer](README.md)
