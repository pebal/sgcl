[sgcl](../../README.md) › [compress](../README.md) › [snappy](README.md)

# sgcl::compress::snappy::decompress

```cpp
static expected<vector<byte>, error> decompress(const slice<const byte>& data) noexcept;    // (1)
static expected<vector<byte>, error> decompress(const slice<const byte>& data,              // (2)
                                                const limits& l) noexcept;
```

Decompresses the whole of framed Snappy data at once: every chunk of every stream, each chunk's masked CRC-32C
compared, padding and the skippable chunks passed over; an output that grows past the limits' `max_size` stops there.

1. With the default [limits](../limits.md): 1 GiB of output.
2. With the limits given.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the compressed bytes |
| `l` | the bound on the output ([limits](../limits.md)) |

## Return value

The decompressed bytes, or the [error](../error/README.md): no stream identifier first, or a wrong one
(`errc::invalid_header`), a chunk's checksum that does not match (`errc::checksum`), a reserved chunk that may not be
skipped, a chunk of more than 64 KB of data, a copy before the data, a block whose data differs from its length
(`errc::corrupt`), data cut short (`errc::unexpected_end`), an output past the limit (`errc::too_large`).

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
    auto a = compress::snappy::compress("first stream, ");
    auto b = compress::snappy::compress("second stream");
    a.insert(a.end(), b.begin(), b.end());  // two streams, read as one
    println("{}", string(slice<const byte>(*compress::snappy::decompress(a))));

    a[a.size() - 1] ^= byte(1);  // a bit of the second stream's data flipped
    println("{}", compress::snappy::decompress(a).error().message());
}
```

Output:

```text
first stream, second stream
offset 46: snappy: a chunk's checksum does not match
```

## See also

- [compress](compress.md): the other way
- [decompress_block](decompress_block.md): a block without framing
- [snappy::reader](../snappy-reader/README.md): a stream
- [sgcl::compress::snappy](README.md)
