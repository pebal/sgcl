[sgcl](../../README.md) › [compress](../README.md) › [flate](README.md)

# sgcl::compress::flate::decompress

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

Decompresses the whole of the DEFLATE data at once, made by any encoder. It stops at the end of the data and reads
nothing after it, as zlib and Go do. The output stops at the limits' `max_size` (1 GiB unless told otherwise) with
`errc::too_large`: data from outside may decompress a thousand times over.

- (1) With no dictionary and the default [limits](../limits.md): 1 GiB of output.
- (2) With the limits given.
- (3) With the dictionary of the options (its level is not read).
- (4) With both.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the compressed bytes |
| `o` | the dictionary the data was compressed with ([options](../flate-options.md)) |
| `l` | the bound on the output ([limits](../limits.md)) |

## Return value

The decompressed bytes, or the [error](../error/README.md): a code that is not one, a distance before the start, a stored block
whose length does not match its complement (`errc::corrupt`), data cut short (`errc::unexpected_end`), output past
`max_size` (`errc::too_large`).

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
    auto packed = compress::flate::compress("hello, hello, hello");
    auto back = compress::flate::decompress(packed);
    println("{}", string(slice<const byte>(*back)));

    auto small = compress::flate::decompress(packed, {.max_size = 5});
    println("{}", small.error().message());

    packed.pop_back();  // cut short
    println("{}", compress::flate::decompress(packed).error().message());
}
```

Output:

```text
hello, hello, hello
offset 10: decompressed data past the limit
offset 9: unexpected end of the compressed data
```

## See also

- [compress](compress.md): the other way
- [flate::reader](../flate-reader/README.md): a stream
- [limits](../limits.md)
- [sgcl::compress::flate](README.md)
