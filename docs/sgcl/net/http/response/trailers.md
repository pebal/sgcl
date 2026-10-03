[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response](../response.md)

# sgcl::net::http::response::trailers

```cpp
http::headers trailers() const noexcept;
```

Returns the trailer fields of a chunked body, Go's `resp.Trailer`: the fields the server sent after the last chunk, a
checksum or a status known only at the end. Over HTTP/2 they are the fields the server sent after the body. They are
there once the body has been read to its end; before that, and for a body that is not chunked, the list is empty.

## Parameters

None.

## Return value

A copy of the trailer fields; empty before the end of the body.

## Complexity

Linear in the number of trailer fields.

## Exceptions

None.

## Example

A server written by hand on a connection, its chunked answer followed by a trailer:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

async::task<> answer(net::listener listener) {
    net::connection c = co_await listener.async_accept();
    vector<byte> request(4096);
    (void)co_await c.async_read(request);
    co_await c.async_write("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n"
                           "Connection: close\r\n\r\n"
                           "4\r\nbuy \r\n4\r\nmilk\r\n0\r\nX-Checksum: 42\r\n\r\n");
    (void)c.close();
}

int main() {
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto answering = async::spawn(answer(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    net::http::response res = web.get(base + "/");
    size_t before = res.trailers().size();
    string text = res.text();
    println("{}, {} trailers before, checksum {}", text, before, res.trailers().get("X-Checksum"));
}
```

Output:

```text
buy milk, 0 trailers before, checksum 42
```

## See also

- [request::trailers](../request/trailers.md): the trailers of a request
- [sgcl::net::http::response](../response.md)
