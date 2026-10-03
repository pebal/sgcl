[sgcl](../../README.md) › [io](../README.md) › [buffered_reader](../buffered_reader.md)

# sgcl::io::buffered_reader::underlying

```cpp
io::reader underlying() const noexcept;
```

Returns the stream the reader reads through its block, as the [io::reader](../reader.md) it was made of: a copy of
that handle, the same stream. A read from it goes past the block: it does not see what the block holds, and what it
takes the buffered reader does not see.

## Parameters

None.

## Return value

The stream underneath.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer source("data");
    io::buffered_reader in(source);
    io::reader under = in.underlying();
    println("{} {}", under == io::reader(source), under.has_async_read());
}
```

Output:

```text
true true
```

## See also

- [io::reader](../reader.md): what it is
- [close](close.md): closes it
- [sgcl::io::buffered_reader](../buffered_reader.md)
