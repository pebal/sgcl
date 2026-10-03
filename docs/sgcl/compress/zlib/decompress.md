[sgcl](../../README.md) › [compress](../README.md) › [zlib](README.md)

# sgcl::compress::zlib::decompress

```cpp
static expected<vector<byte>, error> decompress(const slice<const byte>& data) noexcept;    // (1)
static expected<vector<byte>, error> decompress(const slice<const byte>& data,              // (2)
                                                const limits& l) noexcept;
static expected<vector<byte>, error> decompress(const slice<const byte>& data,              // (3)
                                                const options& o) noexcept;
static expected<vector<byte>, error> decompress(const slice<const byte>& data,              // (4)
                                                const options& o,
                                                const limits& l) noexcept;
```

Decompresses the whole of a zlib stream at once, made by any encoder: the header is checked, the data decoded and the
Adler-32 compared. It stops at the end of the stream and reads nothing after it. A stream made with a preset dictionary
needs that dictionary in the options: without it, or with another (the Adler-32 in the header tells), it is
`errc::dictionary_required`; [dictionary_id](dictionary_id.md) reads which one the stream needs. The output stops at the
limits' `max_size` (1 GiB unless told otherwise) with `errc::too_large`: data from outside may decompress a thousand
times over.

- (1) With no dictionary and the default [limits](../limits.md): 1 GiB of output.
- (2) With the limits given.
- (3) With the dictionary of the options (its level is not read).
- (4) With both.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the compressed bytes |
| `o` | the dictionary the data was compressed with ([options](../zlib-options.md)) |
| `l` | the bound on the output ([limits](../limits.md)) |

## Return value

The decompressed bytes, or the [error](../error/README.md): a header that is not zlib's (`errc::invalid_header`), the data as
for flate (`errc::corrupt`, `errc::unexpected_end`), an Adler-32 that does not match (`errc::checksum`), a dictionary
missing or another (`errc::dictionary_required`), output past `max_size` (`errc::too_large`).

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
    auto packed = compress::zlib::compress("hello, hello, hello");
    auto back = compress::zlib::decompress(packed);
    println("{}", string(slice<const byte>(*back)));

    auto small = compress::zlib::decompress(packed, {.max_size = 5});
    println("{}", small.error().message());

    packed.pop_back();  // cut short
    println("{}", compress::zlib::decompress(packed).error().message());
}
```

Output:

```text
hello, hello, hello
offset 12: decompressed data past the limit
offset 12: unexpected end of data
```

## See also

- [compress](compress.md): the other way
- [zlib::reader](../zlib-reader/README.md): a stream
- [limits](../limits.md)
- [sgcl::compress::zlib](README.md)
