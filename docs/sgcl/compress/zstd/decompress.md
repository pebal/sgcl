[sgcl](../../README.md) › [compress](../README.md) › [zstd](README.md)

# sgcl::compress::zstd::decompress

```cpp
static expected<vector<byte>, error> decompress(const slice<const byte>& data) noexcept;        // (1)
static expected<vector<byte>, error> decompress(const slice<const byte>& data,                  // (2)
                                                const limits& l) noexcept;
static expected<vector<byte>, error> decompress(const slice<const byte>& data,                  // (3)
                                                const options& o) noexcept;
static expected<vector<byte>, error> decompress(const slice<const byte>& data,                  // (4)
                                                const options& o, const limits& l) noexcept;
```

Decompresses the whole of zstd data at once: every frame there is, one after another, skippable frames passed over,
the content's checksum compared where a frame carries it and the content's size, where it gives one, held to the size
decoded. A frame's header is checked against the limits first: its window against `max_memory`, its content size,
when it gives one, against `max_size`, so data past them fails before any work; an output that grows past
`max_size` stops there. Without a dictionary every block is decoded straight into the result.

1. With the default [limits](../limits.md) and no dictionary.
2. With the limits given.
3. With the dictionary of the options (the only part read), checked against the id a frame names.
4. Both.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the compressed bytes |
| `l` | the bounds on a frame's window and the output ([limits](../limits.md)) |
| `o` | the dictionary ([options](../zstd-options.md)) |

## Return value

The decompressed bytes, or the [error](../error/README.md): not zstd, reserved bits set, or something else after a
frame (`errc::invalid_header`), the checksum that does not match (`errc::checksum`), a table that is not valid, a block
larger than the frame allows, a match before the data, a content size that differs (`errc::corrupt`), a frame made
with a dictionary read without it or with another (`errc::dictionary_required`), data cut short
(`errc::unexpected_end`), a window or an output past the limits (`errc::too_large`).

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
    auto a = compress::zstd::compress("first frame, ");
    auto b = compress::zstd::compress("second frame");
    a.insert(a.end(), b.begin(), b.end());  // as `cat a.zst b.zst`
    println("{}", string(slice<const byte>(*compress::zstd::decompress(a))));

    a[a.size() - 2] ^= byte(1);  // a bit of the second frame's checksum flipped
    println("{}", compress::zstd::decompress(a).error().message());
}
```

Output:

```text
first frame, second frame
offset 47: zstd: the content checksum does not match
```

## See also

- [compress](compress.md): the other way
- [zstd::reader](../zstd-reader/README.md): a stream
- [limits](../limits.md)
- [sgcl::compress::zstd](README.md)
