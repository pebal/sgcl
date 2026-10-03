[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response](../response.md)

# sgcl::net::http::response::content_length

```cpp
optional<uint64_t> content_length() const noexcept;
```

Returns the length of the body as the response declared it, Go's `resp.ContentLength`: its `Content-Length`, which
for a response to HEAD is the length a GET would have. A chunked body, and one that lasts to the close of the
connection, give `nullopt`. The client holds the body to the length: one that ends sooner is
`io::errc::unexpected_eof`.

## Parameters

None.

## Return value

The declared length, or `nullopt` when the response declared none.

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
    srv.route("GET /whole", [](net::http::request, net::http::response_writer w) {
        w.write("all at once\n");
    });
    srv.route("GET /flushed", [](net::http::request, net::http::response_writer w)
                                  -> async::task<> {
        w.write("first part\n");
        co_await w.async_flush();
        w.write("second part\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    for (string path : {"/whole", "/flushed"}) {
        net::http::response res = web.get(base + path);
        auto length = res.content_length();
        println("{}: {}", path, length ? to_string(*length) : string("none"));
        res.close();
    }
    srv.close();
}
```

Output:

```text
/whole: 12
/flushed: none
```

## See also

- [text](text.md): the body read
- [sgcl::net::http::response](../response.md)
