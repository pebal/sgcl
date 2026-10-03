[sgcl](../../README.md) › [compress](../README.md) › [error](README.md)

# sgcl::compress::error::offset

```cpp
uint64_t offset() const noexcept;
```

Returns the byte of the compressed input (of the archive) where the error was found, counted from its start. For a
checksum it is where the checksum lies; for data cut short, the end of what there was. An error that did not come
from the data (a file that does not open, cannot be made, written or closed, a failure of what a writer writes
into, a mistake of the calls to a writer, a name the archive does not hold, a limit the caller set on what is
extracted) has an offset of 0, which its [message](message.md) does not show.

## Parameters

None.

## Return value

The offset in bytes.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto packed = compress::gzip::compress("hello, hello, hello");
    auto cut = compress::gzip::decompress(slice<const byte>(packed).first(16));
    println("{} of {}: {}", cut.error().offset(), packed.size(), cut.error().message());
}
```

Output:

```text
16 of 28: offset 16: unexpected end of the compressed data
```

## See also

- [message](message.md): the offset and the rest in one sentence
- [sgcl::compress::error](README.md)
