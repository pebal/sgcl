[sgcl](../../README.md) › [compress](../README.md) › [brotli](README.md)

# sgcl::compress::brotli::decompress

```cpp
static expected<vector<byte>, error> decompress(const slice<const byte>& data) noexcept;    // (1)
static expected<vector<byte>, error> decompress(const slice<const byte>& data,              // (2)
                                                const limits& l) noexcept;
```

Decompresses a whole brotli stream at once, to its last meta-block: metadata meta-blocks passed over, stored ones
copied, the commands of the compressed ones carried out, copies from the static dictionary transformed. The window the
stream's header asks for is checked against `max_memory` first; an output that grows past `max_size` stops there. The
stream must end where the data does: bytes after its last meta-block are not another stream (the format has no frame
for one).

1. With the default [limits](../limits.md).
2. With the limits given.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the compressed bytes |
| `l` | the bounds on the stream's window and the output ([limits](../limits.md)) |

## Return value

The decompressed bytes, or the [error](../error/README.md): a prefix code that is not complete, a context map past its
trees, a distance before the data or past the dictionary, a meta-block that does not hold what its length says,
padding bits that are not zero, bytes after the end (`errc::corrupt`), the large-window extension
(`errc::invalid_header`), data cut short (`errc::unexpected_end`), a window or an output past the limits
(`errc::too_large`).

## Complexity

Linear in the size of the output.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto packed = compress::brotli::compress("a message", {.level = 5});
    println("{}", string(slice<const byte>(*compress::brotli::decompress(packed))));

    packed.push_back(byte(0));  // a byte after the stream's end
    println("{}", compress::brotli::decompress(packed).error().message());
}
```

Output:

```text
a message
offset 14: brotli: bytes after the end of the stream
```

## See also

- [compress](compress.md): the other way
- [brotli::reader](../brotli-reader/README.md): a stream
- [limits](../limits.md)
- [sgcl::compress::brotli](README.md)
