[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](../request.md)

# sgcl::net::http::request::trailers

```cpp
http::headers trailers() const noexcept;
```

Returns the trailer fields of a received request's chunked body, Go's `r.Trailer`: the fields that come after the last
chunk, a checksum or a status known only at the end. They are there once the body has been read to its end; before
that, and for a body that is not chunked, the list is empty. The client of the library sends no trailers.

## Parameters

None.

## Return value

A copy of the trailer fields; empty before the end of the body.

## Complexity

Linear in the number of trailer fields.

## Exceptions

None.

## Example

A request written by hand on a connection, its chunked body followed by a trailer:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("POST /upload", [](net::http::request req, net::http::response_writer w)
                                  -> async::task<> {
        size_t before = req.trailers().size();
        string text = co_await req.async_text();
        w.set_header("Connection", "close");
        w.write(text + ", " + to_string(before) + " trailers before, checksum " +
                req.trailers().get("X-Checksum") + "\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));

    net::connection c = net::tcp::connect(listener.local_endpoint());
    c.write("POST /upload HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n"
            "4\r\nbuy \r\n4\r\nmilk\r\n0\r\nX-Checksum: 42\r\n\r\n");
    string answer = c.read_all_text().value();
    print("{}", answer.substr(answer.find("\r\n\r\n") + 4));
    srv.close();
}
```

Output:

```text
buy milk, 0 trailers before, checksum 42
```

## See also

- [response::trailers](../response/trailers.md): the trailers of a response
- [sgcl::net::http::request](../request.md)
