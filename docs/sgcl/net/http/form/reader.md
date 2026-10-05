[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [form](README.md)

# sgcl::net::http::form::reader

```cpp
expected<io::reader, io::error> reader() const noexcept;
```

The body as a [stream](../../../io/reader/README.md), what a client sends: each part's head, a field's text, a file's
bytes read from it as the stream reaches them (the file opened then and closed at its end), the close delimiter.
The files' sizes are taken now: a file that shrank since is `io::errc::unexpected_eof` when it is read, one that grew
gives the bytes of the size it had. A stream of its own each call, read once.

## Parameters

None.

## Return value

The stream; or the [io::error](../../../io/error/README.md) of a file that cannot be read now (`ENOENT`, `EISDIR`).

## Complexity

Linear in the number of parts, a `stat` of each file; the reads, linear in the body.

## Exceptions

None.

## Example

The body written to a file, the whole of it never in memory:

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    io::write_file("big.bin", string(1000000, 'x'));
    net::http::form f{{"name", "big"}, net::http::form::file("data", "big.bin")};
    io::reader body = f.reader().value();
    io::file out = io::create("body.multipart").value();
    uint64_t copied = io::copy(out, body).value();
    out.close();
    println("{}", copied == f.content_length().value());
}
```

Output:

```text
true
```

## See also

- [content_length](content_length.md): its length
- [multipart_reader](../multipart_reader/README.md): such a body read back
- [form](README.md)
