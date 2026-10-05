[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [multipart_reader](README.md)

# sgcl::net::http::multipart_reader::next, async_next

```cpp
expected<optional<part>, io::error> next() const;                                // (1)
async::task<expected<optional<part>, io::error>> async_next() const noexcept;    // (2)
```

Goes to the next part and gives its head, Go's `NextPart`: what was left of the current part's content is read past
(and dropped), the boundary's line read, the part's fields parsed. The reads give that part's content from then on.
At the close delimiter it gives `nullopt`, and so does every call after it.

1. Reads the body on the calling thread. Over a request's body the read runs on the scheduler and the thread waits
   for it: for a thread of the program, never a handler.
2. Returns a task that does the same: a handler writes `co_await parts.async_next()`.

## Parameters

None.

## Return value

The [part](../multipart_reader-part.md)'s head; `nullopt` after the last part; or the
[io::error](../../../io/error/README.md), operation `multipart`, that ends the reader:

- `net::errc::malformed_multipart` for a body with no boundary, a head that is not fields, a boundary of no
  character or of more than 70;
- `net::errc::header_too_large` for a head past `limits::max_header_bytes`, `net::errc::too_many_parts` for a part
  past `limits::max_parts`;
- `io::errc::unexpected_eof` for a body that ends before its close delimiter;
- the stream's own error (`net::errc::body_too_large` past a server's `max_body_bytes`).

## Complexity

Linear in what is read past and in the head.

## Exceptions

- (1) `std::system_error` when the stream is a request's body and the wait starts the scheduler and a worker's thread
  cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    string body = "--b\r\nContent-Disposition: form-data; name=\"skipped\"\r\n\r\n" + string(100000, 'x') +
                  "\r\n--b\r\nContent-Disposition: form-data; name=\"read\"\r\n\r\nshort\r\n--b--\r\n";
    net::http::multipart_reader parts(io::reader(make_tracked<io::buffer>(body)), "b");
    println("{}", parts.next().value()->name);  // its content never read
    println("{}", parts.next().value()->name);
    println("{}", parts.read_all_text().value());
    println("{}", parts.next().value().has_value());

    net::http::multipart_reader cut(io::reader(make_tracked<io::buffer>("--b\r\n\r\nno end")), "b");
    (void)cut.next();
    println("{}", cut.read_all_text().error().message());
}
```

Output:

```text
skipped
read
short
false
multipart: unexpected end of stream
```

## See also

- [read, async_read](read.md): the part's content
- [multipart_reader](README.md)
