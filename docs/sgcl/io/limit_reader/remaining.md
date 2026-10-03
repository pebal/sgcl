[sgcl](../../README.md) › [io](../README.md) › [limit_reader](../limit_reader.md)

# sgcl::io::limit_reader::remaining

```cpp
uint64_t remaining() const noexcept;
```

Returns the number of bytes the limit still lets through: the count given to the constructor less the bytes read.
It says nothing of the source: a source that ends first leaves bytes remaining, which tells a reader that wanted
exactly `n` bytes that it got fewer (Go's `LimitedReader.N`).

## Parameters

None.

## Return value

The bytes the limit lets through before its end.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::limit_reader body(io::buffer("short"), 8);
    auto text = body.read_all_text();
    println("[{}] {} remain", *text, body.remaining());
}
```

Output:

```text
[short] 3 remain
```

## See also

- [read, async_read](read.md)
- [sgcl::io::limit_reader](../limit_reader.md)
