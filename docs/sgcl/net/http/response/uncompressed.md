[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response](README.md)

# sgcl::net::http::response::uncompressed

```cpp
bool uncompressed() const noexcept;
```

Whether the client decoded the body by itself, Go's `Response.Uncompressed`: the client asked for a coding on its own
(`Accept-Encoding: gzip, deflate, br, zstd`, its member `decompress`, on by default) and the response came in one of them. The
body read through [text](text.md), [bytes](bytes.md) or [body](body.md) is then the decoded one, and the response has
no `Content-Encoding` and no `Content-Length` (that of the coded bytes): [content_length](content_length.md) is
`nullopt`. A request that sets its own `Accept-Encoding` gets the bytes as they came, and `false` here.

## Parameters

None.

## Return value

`true` when the body is decoded.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.use(net::http::compression());
    srv.route("GET /", [](net::http::request, net::http::response_writer w) {
        w.set_header("Content-Type", "text/plain");
        w.write(string("compress me ").repeat(200));
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "http://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/";

    net::http::client web;
    auto res = web.get(url);
    println("{} {} {}", res->uncompressed(), res->header("Content-Encoding").empty(), res->text()->size());
    srv.close();
}
```

Output:

```text
true true 2400
```

## See also

- [client](../client/README.md): `decompress`
- [compression](../compression/README.md): the server's side
- [response](README.md)
