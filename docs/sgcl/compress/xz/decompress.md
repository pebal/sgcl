[sgcl](../../README.md) › [compress](../README.md) › [xz](../xz.md)

# sgcl::compress::xz::decompress

```cpp
static expected<vector<byte>, error> decompress(const slice<const byte>& data) noexcept;    // (1)
static expected<vector<byte>, error> decompress(const slice<const byte>& data,              // (2)
                                                const limits& l) noexcept;
```

Decompresses the whole of an `.xz` stream at once: every stream there is, with the stream padding between them,
every block of each, through its filters, each block's check compared and each stream's index checked against the
blocks read. A block's header is checked against the limits first: its dictionary against `max_memory`, its size,
when it gives one, against `max_size`, so data past them fails before any work; an output that grows past
`max_size` stops there.

1. With the default [limits](../limits.md): 1 GiB of output, a dictionary of up to 1 GiB.
2. With the limits given.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the compressed bytes |
| `l` | the bounds on a block's dictionary and the output ([limits](../limits.md)) |

## Return value

The decompressed bytes, or the [error](../error.md): not xz, or something else after a stream
(`errc::invalid_header`), a check or a CRC-32 of a header that does not match (`errc::checksum`), data the format
does not allow, an index that does not list the blocks read, padding not a multiple of four (`errc::corrupt`), a check
type the format reserves (`errc::unsupported`), data cut short (`errc::unexpected_end`), a dictionary or an output
past the limits (`errc::too_large`).

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
    auto a = compress::xz::compress("first stream, ");
    auto b = compress::xz::compress("second stream");
    a.insert(a.end(), b.begin(), b.end());  // as `cat a.xz b.xz`
    println("{}", string(slice<const byte>(*compress::xz::decompress(a))));

    a[a.size() - 40] ^= byte(1);  // a bit of the second stream flipped
    println("{}", compress::xz::decompress(a).error().message());
}
```

Output:

```text
first stream, second stream
offset 116: xz: the block's check does not match its data
```

## See also

- [compress](compress.md): the other way
- [xz::reader](../xz-reader.md): a stream
- [limits](../limits.md)
- [sgcl::compress::xz](../xz.md)
