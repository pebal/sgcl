[sgcl](../../README.md) › [compress](../README.md) › [snappy](README.md)

# sgcl::compress::snappy::decompress_block

```cpp
static expected<vector<byte>, error> decompress_block(const slice<const byte>& data) noexcept;    // (1)
static expected<vector<byte>, error> decompress_block(const slice<const byte>& data,              // (2)
                                                      const limits& l) noexcept;
```

Decompresses one block of Snappy's block format. The length in its head is held against the limits' `max_size` before
anything is made, the room for exactly that many bytes is taken, and the elements must make exactly that many.

1. With the default [limits](../limits.md): 1 GiB of output.
2. With the limits given.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the block |
| `l` | the bound on the output ([limits](../limits.md)) |

## Return value

The decompressed bytes, or the [error](../error/README.md): a length past the limit (`errc::too_large`), a length that
cannot be read, a copy before the data or of offset 0, data that differs from the length (`errc::corrupt`), data cut
short (`errc::unexpected_end`).

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
    vector<byte> block = compress::snappy::compress_block(string("x").repeat(2000));
    println("{}", compress::snappy::decompress_block(block)->size());
    println("{}", compress::snappy::decompress_block(block, {.max_size = 1000}).error().message());
}
```

Output:

```text
2000
offset 0: snappy: decompressed data past the limit
```

## See also

- [compress_block](compress_block.md): the other way
- [decompressed_size](decompressed_size.md): the length alone
- [sgcl::compress::snappy](README.md)
