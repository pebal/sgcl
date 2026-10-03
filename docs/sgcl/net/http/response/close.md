[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response](../response.md)

# sgcl::net::http::response::close

```cpp
void close() const;
```

Gives the body up without reading it, and without waiting, Go's `resp.Body.Close()` before the end: when the rest of
the body is already in the connection's buffer it is dropped and the connection goes back to the client's pool;
otherwise the connection is closed. Over HTTP/2 a body not read to its end resets its stream (`CANCEL`), and the
connection goes on carrying the other requests. A body read to its end has given its connection back already, and
the close does nothing more. A response neither read nor closed keeps its connection out of the pool until the
collector finds it.

## Parameters

None.

## Return value

None.

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
    srv.route("GET /", [](net::http::request req, net::http::response_writer w) {
        w.write(to_string(req.remote_endpoint().port()));
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    net::http::response first = web.get(base + "/");
    string port = first.text().value();
    net::http::response unread = web.get(base + "/");  // its body is in the buffer already
    unread.close();
    string again = web.get(base + "/")->text().value();
    println("the connection went back: {}", port == again);
    srv.close();
}
```

Output:

```text
the connection went back: true
```

## See also

- [body](body.md): the body as a stream, with no close of its own
- [sgcl::net::http::response](../response.md)
