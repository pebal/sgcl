[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response_writer](README.md)

# sgcl::net::http::response_writer::trailers

```cpp
http::headers& trailers() const noexcept;
```

Returns the trailers of the response, the fields sent after its body (RFC 9110 §6.5), Go's `Header` under
`TrailerPrefix`: set any time before the response ends, they go when it ends. Over HTTP/1.1 a trailer section needs a
chunked body, so a response with trailers is chunked to a client of HTTP/1.1 even when it was written whole; over
HTTP/2 they are the stream's last HEADERS. The names known when the head goes are announced in a `Trailer` field,
unless the handler set one itself, and a name set after that still goes.

A trailer is what the body could not know before it ended: a checksum, a status of the work done (gRPC's
`grpc-status`), a count.

## Parameters

None.

## Return value

The trailers, changed in place.

## Complexity

Constant.

## Exceptions

None.

## Notes

- A field that frames or routes a message, or that a recipient must read before the body, is left out of the
  trailer section (RFC 9110 §6.5.1): Content-Length, Transfer-Encoding, Host, Trailer, the fields of a connection,
  Content-Type, Content-Encoding, Content-Range, Cache-Control, Expect, Authorization, Cookie and Set-Cookie; so is
  a field whose name is not a token or whose value holds a control. The parsers of the library refuse a trailer
  section with the first four.
- None go to a client of HTTP/1.0, to a HEAD, with a status that has no body (1xx, 204, 304), or after a flush of a
  response whose Content-Length the handler set: those bodies are not chunked.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::test_server ts([](net::http::request, net::http::response_writer w) {
        w.write("payload");
        w.trailers().set("X-Checksum", "7");
    });
    net::http::response r = ts.client().get(ts.url());
    string body = r.text();
    println("{} {} | {}", r.header("Transfer-Encoding"), r.header("Trailer"), body);
    println("{}", r.trailers().get("X-Checksum"));
}
```

Output:

```text
chunked X-Checksum | payload
7
```

## See also

- [response::trailers](../response/trailers.md): the trailers a client reads
- [request::trailers](../request/trailers.md): a request's
- [flush](flush.md): a body sent in pieces
- [sgcl::net::http::response_writer](README.md)
