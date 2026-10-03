[sgcl](../../README.md) › [compress](../README.md) › [gzip](../gzip.md) › [reader](../gzip-reader.md)

# sgcl::compress::gzip::reader::header, async_header

```cpp
expected<gzip::header, error> header();                                // (1)
async::task<expected<gzip::header, error>> async_header() noexcept;    // (2)
```

Returns the [header](../gzip_header.md) of the member being read, reading it now if it was not read yet (the first
read reads it as well): the name, the comment and the time of the data, its extra field, the system that made it.
The strings are taken as they are when their bytes are UTF-8 and converted from the format's ISO 8859-1 when they
are not ([gzip_header](../gzip_header.md)); `modified` is `nullopt` where the member says 0.
Past the end of a member, the reader goes on into the next and this is the next one's header.

1. Blocks the calling thread for the reads of `in`.
2. Returns a task that does the same and gives the worker back while `in` reads.

## Parameters

None.

## Return value

The header, or the [error](../error.md) of reading it: not gzip (`errc::invalid_header`), a method other than
deflate (`errc::unsupported`), a header CRC-16 that does not match (`errc::checksum`), a header past 1 MiB
(`errc::too_large`), the stream cut short (`errc::unexpected_end`), a failure of `in` (`errc::io`).

## Complexity

Linear in the size of the header.

## Exceptions

- (1) What the `read` of `in` throws.
- (2) None: the task carries what it throws.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    compress::gzip::options o;
    o.header.name = "access.log";
    auto packed = compress::gzip::compress("GET /\nGET /about\n", o);

    compress::gzip::reader r{io::buffer(packed)};
    if (auto h = r.header()) {
        println("{} {}", h->name, h->modified.has_value());
    }
    io::buffered_reader lines(r);
    for (auto line : lines.lines()) {
        println("{}", line);
    }
}
```

Output:

```text
access.log false
GET /
GET /about
```

## See also

- [gzip_header](../gzip_header.md)
- [sgcl::compress::gzip::reader](../gzip-reader.md)
