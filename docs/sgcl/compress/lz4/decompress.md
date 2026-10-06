[sgcl](../../README.md) › [compress](../README.md) › [lz4](README.md)

# sgcl::compress::lz4::decompress

```cpp
static expected<vector<byte>, error> decompress(const slice<const byte>& data) noexcept;        // (1)
static expected<vector<byte>, error> decompress(const slice<const byte>& data,                  // (2)
                                                const limits& l) noexcept;
static expected<vector<byte>, error> decompress(const slice<const byte>& data,                  // (3)
                                                const options& o) noexcept;
static expected<vector<byte>, error> decompress(const slice<const byte>& data,                  // (4)
                                                const options& o, const limits& l) noexcept;
```

Decompresses the whole of LZ4 data at once: every frame there is, one after another, skippable frames passed over,
legacy frames (`lz4 -l`) read, every checksum a frame asks for compared and its content's size, when it gives one,
held to the size decoded. A frame's header is checked against the limits first: its block size against `max_memory`,
its content size, when it gives one, against `max_size`, so data past them fails before any work; an output that
grows past `max_size` stops there. Without a dictionary every block is decoded straight into the result.

1. With the default [limits](../limits.md) and no dictionary.
2. With the limits given.
3. With the dictionary of the options (the only part read) and its id, checked against the frame's when both are
   given.
4. Both.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the compressed bytes |
| `l` | the bounds on a frame's block size and the output ([limits](../limits.md)) |
| `o` | the dictionary and its id ([options](../lz4-options.md)) |

## Return value

The decompressed bytes, or the [error](../error/README.md): not LZ4, a version other than 01, reserved bits set, or
something else after a frame (`errc::invalid_header`), a checksum that does not match (`errc::checksum`), a block
larger than the frame allows, a match before the data, a content size that differs (`errc::corrupt`), a frame made
with a dictionary read without it or with another (`errc::dictionary_required`), data cut short
(`errc::unexpected_end`), a block size or an output past the limits (`errc::too_large`).

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
    auto a = compress::lz4::compress("first frame, ");
    auto b = compress::lz4::compress("second frame");
    a.insert(a.end(), b.begin(), b.end());  // as `cat a.lz4 b.lz4`
    println("{}", string(slice<const byte>(*compress::lz4::decompress(a))));

    a[a.size() - 2] ^= byte(1);  // a bit of the second frame's checksum flipped
    println("{}", compress::lz4::decompress(a).error().message());
}
```

Output:

```text
first frame, second frame
offset 75: lz4: the content checksum does not match
```

## See also

- [compress](compress.md): the other way
- [decompress_block](decompress_block.md): a block without a frame
- [lz4::reader](../lz4-reader/README.md): a stream
- [limits](../limits.md)
- [sgcl::compress::lz4](README.md)
