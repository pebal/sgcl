[sgcl](../../README.md) › [io](../README.md) › [reader](../reader.md)

# sgcl::io::operator== (sgcl::io::reader)

```cpp
friend bool operator==(const reader& a, const reader& b) noexcept;
```

Checks whether `a` and `b` hold the same stream: the same object. Two readers made of copies of one handle hold its
one object, and are equal; two readers of two buffers with the same bytes are not. Two empty readers are equal.
`!=` is the negation.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the readers to compare |

## Return value

`true` when both hold the same stream, or both are empty.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer data("bytes");
    io::buffer copy = data;  // the same buffer
    io::reader a = data;
    io::reader b = copy;
    io::reader c = io::buffer("bytes");
    println("{} {} {}", a == b, a == c, io::reader() == io::reader());
}
```

Output:

```text
true false true
```

## See also

- [sgcl::io::reader](../reader.md)
