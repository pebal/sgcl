[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](README.md)

# sgcl::net::http::request::content_length

```cpp
optional<uint64_t> content_length() const noexcept;
```

Returns the length of the body as a received request declared it, Go's `r.ContentLength`: the value of its
`Content-Length`. A chunked body, and a request without a length, give `nullopt`. The length is what the request
says, not what was read; the server holds the body to it and to its `max_body_bytes`. A request the program built has
none: the client works its length out from the body when it sends it.

## Parameters

None.

## Return value

The declared length, or `nullopt` when the request declared none.

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
    srv.route("POST /", [](net::http::request req, net::http::response_writer w) {
        auto length = req.content_length();
        w.write(length ? to_string(*length) + " bytes declared\n" : string("no length\n"));
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    print("{}", web.post(base + "/", "text/plain", "buy milk")->text().value());
    net::http::request streamed("POST", base + "/");
    streamed.set_body(io::buffer("buy milk"));
    print("{}", web.send(streamed)->text().value());
    srv.close();
}
```

Output:

```text
8 bytes declared
no length
```

## See also

- [text](text.md): the body read
- [response::content_length](../response/content_length.md): the length a response declared
- [sgcl::net::http::request](README.md)
