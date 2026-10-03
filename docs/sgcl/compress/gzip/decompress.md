[sgcl](../../README.md) › [compress](../README.md) › [gzip](../gzip.md)

# sgcl::compress::gzip::decompress

```cpp
/*(1)*/ static expected<vector<byte>, error> decompress(const slice<const byte>& data);
/*(2)*/ static expected<vector<byte>, error> decompress(const slice<const byte>& data,
                                                        const limits& l);
```

Decompresses the whole of a gzip stream at once: every member, one after another, as gunzip and Go read them. Bytes
after the last member that are not another one are `errc::invalid_header` (fewer than a header's ten bytes,
`errc::unexpected_end`), as in Go. The length the last member's trailer states is taken as a hint for the first buffer,
no larger than the data could make, which grows when the hint is wrong (a member's length is kept modulo 2^32). The
output stops at the limits' `max_size` (1 GiB unless told otherwise) with `errc::too_large`: data from outside may
decompress a thousand times over.

- (1) With the default [limits](../limits.md): 1 GiB of output.
- (2) With the limits given.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the compressed bytes |
| `l` | the bound on the output ([limits](../limits.md)) |

## Return value

The decompressed bytes, or the [error](../error.md): a header that is not gzip's (`errc::invalid_header`), a method
other than deflate (`errc::unsupported`), the data as for flate (`errc::corrupt`, `errc::unexpected_end`), a CRC-32 that
does not match (`errc::checksum`) or a length that does not (`errc::corrupt`), output past `max_size`
(`errc::too_large`).

## Complexity

Linear in the size of the output.

## Exceptions

`length_error` when a name or a comment of a header is past 2 GiB of ISO 8859-1, which as UTF-8 would pass a string's 4
GiB; the errors of the data are returned.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto packed = compress::gzip::compress("hello, hello, hello");
    auto back = compress::gzip::decompress(packed);
    println("{}", string(slice<const byte>(*back)));

    auto small = compress::gzip::decompress(packed, {.max_size = 5});
    println("{}", small.error().message());

    packed.pop_back();  // cut short
    println("{}", compress::gzip::decompress(packed).error().message());
}
```

Output:

```text
hello, hello, hello
offset 20: decompressed data past the limit
offset 20: unexpected end of data
```

## See also

- [compress](compress.md): the other way
- [gzip::reader](../gzip-reader.md): a stream
- [limits](../limits.md)
- [sgcl::compress::gzip](../gzip.md)
