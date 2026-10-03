[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [client](../client.md)

# sgcl::net::http::client::send, async_send

```cpp
expected<response, io::error> send(const request& req) const;                                // (1)
async::task<expected<response, io::error>> async_send(const request& req) const noexcept;    // (2)
```

Sends the [request](../request.md) and follows the redirects, Go's `Client.Do`: a connection is taken from the pool
or dialed, the head and the body written, and the head of the response read; the body stays on the connection, to be
read from the [response](../response.md). Every other method of the client that sends is this one with a request it
builds. What is sent, what is refused before a byte goes out, the redirects, the retries and the timeouts are the
[client's rules](../client.md#rules), read from the settings when the send starts.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `req` | the request: its method, URL, fields and body; a body given as a stream is read as it is sent |

## Return value

The response, its body not read yet; a 4xx or a 5xx is a response. Or the [error](../../../io/error.md), naming the
method and the URL: the connection's (`ECONNREFUSED`, a TLS error), `ETIMEDOUT` past a timeout,
`std::errc::invalid_argument` for a request that cannot be sent (a method, a field or a target that is not valid on
the wire), the error of a body's stream that fails to be read (`io::errc::unexpected_eof` for one that ends before its
length), over HTTP/1.1 and HTTP/2 alike, or one of [net::errc](../../errc.md): `invalid_url`, `unsupported_scheme`,
`malformed_response`, `header_too_large`, `too_many_redirects`.

## Complexity

One exchange for each redirect, on a connection of the pool or a new one.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

A request built by hand: a method of its own, a field and a body:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("PUT /notes/{id}", [](net::http::request req, net::http::response_writer w)
                                     -> async::task<> {
        auto text = co_await req.async_text();
        w.write("note " + req.path_value("id") + " (" + req.header("Content-Type") + "): " +
                text.value() + "\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::request note("PUT", base + "/notes/7");
    note.set_header("Content-Type", "text/plain");
    note.set_body("buy milk");
    net::http::client web;
    net::http::response res = web.send(note);
    print("{} {}", res.status(), res.text().value());
    srv.close();
}
```

Output:

```text
200 note 7 (text/plain): buy milk
```

`timeout` bounds the whole exchange; past it the send is `ETIMEDOUT`:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    net::http::server srv;
    srv.route("GET /slow", [](net::http::request, net::http::response_writer w) -> async::task<> {
        co_await async::sleep(1s);
        w.write("late\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    web.timeout = 200ms;
    auto slow = web.send(net::http::request("GET", base + "/slow"));
    println("{}", slow.error().is_timeout());
    srv.close();
}
```

Output:

```text
true
```

## See also

- [get](get.md), [head](head.md), [post](post.md): the requests the client builds
- [request](../request.md): what is sent; [response](../response.md): what comes back
- [sgcl::net::http::client](../client.md)
