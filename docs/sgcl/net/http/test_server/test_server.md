[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [test_server](README.md)

# sgcl::net::http::test_server::test_server

```cpp
template<class Handler>                                  // (1)
explicit test_server(Handler handler);
template<class Handler>                                  // (2)
test_server(Handler handler, const options& o);
explicit test_server(const http::server& s);             // (3)
test_server(const http::server& s, const options& o);    // (4)
test_server(test_server&& other) noexcept;               // (5)
test_server(const test_server&) = delete;                // (6)
```

Starts a server on `127.0.0.1` at a port the system chose and returns once it listens, Go's `httptest.NewServer`
and `NewTLSServer` in one; the serving goes on on the scheduler. Without TLS the [url](url.md) is
`http://127.0.0.1:port`, with it `https://127.0.0.1:port` on a certificate made now, a test CA and a leaf it signed.

1. A server of the one handler, on every method and path (the route `/`): a function of `(request,
   response_writer)` that returns `void` or `async::task<>`, as a [route](../server/route.md)'s. Plain HTTP/1.1.
2. The same, served as `o` says ([options](../test_server-options.md)): TLS, HTTP/2, the requests kept.
3. The routes of a server the program made, with its fields (timeouts, limits, `on_error`) as they are, but
   `http2` and `h2c`, which the options decide. Routes added to it later reach the test server too: the copies
   share them.
4. The same, served as `o` says.
5. The server of `other` taken over: `other` is left without one (no URL, its close does nothing).
6. Not copyable: the server is closed when its object goes, once.

## Parameters

| Parameter | Description |
|---|---|
| `handler` | what answers every request |
| `s` | a server whose routes answer the requests |
| `o` | TLS, HTTP/2, the requests kept |
| `other` | the server to move from |

## Complexity

- (1–4) A socket bound and a task started; with TLS two P-256 keys made and two certificates signed, a millisecond
  or so.
- (5) Constant.

## Exceptions

- (1–4) `std::system_error` when the port cannot be had (the system's error of the listen).
- (5) None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::server routes;
    routes.route("GET /users/{id}", [](net::http::request r, net::http::response_writer w) {
        w.write("user " + r.path_value("id") + "\n");
    });
    net::http::test_server ts(routes);
    print("{}", ts.client().get(ts.url() + "/users/7")->text().value());
    println("{}", ts.client().get(ts.url() + "/nobody")->status());

    net::http::test_server moved = std::move(ts);
    println("{} {}", ts.url().empty(), moved.client().get(moved.url() + "/users/8")->status());
}
```

Output:

```text
user 7
404
true 200
```

## See also

- [close, async_close](close.md): the end that waits
- [options](../test_server-options.md)
- [sgcl::net::http::test_server](README.md)
