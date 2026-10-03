[sgcl](../../README.md) › [io](../README.md) › [error](README.md)

# sgcl::io::error::is_eof

```cpp
bool is_eof() const noexcept;
```

Checks whether the operation failed because the stream ended before what was required: `errc::unexpected_eof`,
which [read_full](../read_full.md) reports for a stream that ends with the buffer part filled, Go's
`io.ErrUnexpectedEOF`. The end of a stream itself is not an error: a read returns 0.

## Parameters

None.

## Return value

`true` when the code is `errc::unexpected_eof`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer header("SGCL");
    vector<byte> magic(8);
    auto r = header.read_full(magic);
    println("{}: eof? {}", r.error().message(), r.error().is_eof());
}
```

Output:

```text
read: unexpected end of stream: eof? true
```

## See also

- [read_full](../read_full.md)
- [sgcl::io::error](README.md)
