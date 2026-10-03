[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](../request.md)

# sgcl::net::http::request::set_body

```cpp
/*(1)*/ request& set_body(const string& text) noexcept;
/*(2)*/ request& set_body(vector<byte> bytes) noexcept;
/*(3)*/ request& set_body(const io::reader& stream, optional<uint64_t> length = nullopt) noexcept;
```

Sets the body the request sends, in place of any set before, Go's `r.Body` with its `ContentLength`. The client sends
the length it knows as `Content-Length` and chunked framing when it knows none.

1. Text, held in memory and sent with its `Content-Length`. A string is a handle: the request holds the same text.
2. Bytes, held in memory and sent with their `Content-Length`. Taken by value: a vector moved in is not copied.
3. A [stream](../../../io/reader.md), read when the request is sent: with its `length` given, `Content-Length` and
   that many bytes (a stream that ends sooner fails the send, `io::errc::unexpected_eof`); without, chunked. A stream
   is read once, so a request with one is neither sent again on a new connection nor redirected by a 307 or a 308
   (the 307 is then the response).

- (1–2) A body in memory can be sent again: on a new connection after a pooled one failed, and after a 307 or a 308.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the body as text |
| `bytes` | the body as bytes |
| `stream` | the stream the body is read from when the request is sent |
| `length` | the number of bytes the stream gives, or `nullopt` for chunked |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("PUT /", [](net::http::request req, net::http::response_writer w) -> async::task<> {
        string text = co_await req.async_text();
        string framing = req.content_length() ? "Content-Length" : req.header("Transfer-Encoding");
        w.write(text + " (" + framing + ")\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    net::http::request req("PUT", base + "/");
    req.set_body("as text");
    print("{}", web.send(req)->text().value());
    req.set_body(vector<byte>{byte('b'), byte('y'), byte('t'), byte('e'), byte('s')});
    print("{}", web.send(req)->text().value());
    req.set_body(io::buffer("from a stream"));
    print("{}", web.send(req)->text().value());
    req.set_body(io::buffer("from a stream"), 13);
    print("{}", web.send(req)->text().value());
    srv.close();
}
```

Output:

```text
as text (Content-Length)
bytes (Content-Length)
from a stream (chunked)
from a stream (Content-Length)
```

## See also

- [post](../client/post.md): a POST of text in one call
- [text](text.md): the body read on the server
- [sgcl::net::http::request](../request.md)
