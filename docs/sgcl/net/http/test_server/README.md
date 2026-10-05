[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::test_server

```cpp
#include "sgcl/net/http/test.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class test_server {
    public:
        struct options;
    };
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`net::http::test_server` is a [server](../server/README.md) on a port of the loopback, started in one line for a
test, Go's `httptest.Server`: a handler (or the routes of a server the program made) served at once on
`127.0.0.1` at a port the system chose, its [url](url.md), a [client](client.md) made for it, and, when asked, the
requests it received. With [options](../test_server-options.md) it speaks https on a certificate made when it starts
— a test CA and a leaf it signed, for `localhost`, `127.0.0.1` and `::1`, P-256, valid for a year — with the client
trusting the CA, and HTTP/2: by ALPN over TLS, by prior knowledge (h2c) on the plain port, HTTP/1.1 served beside
it either way.

Against Go, the certificate is made for each server (Go embeds one, the same in every program), the client has no
proxy of the environment, and the server is closed when the object goes: a guard of its scope, moved but never
copied. [close](close.md) is the close that also waits for the handlers.

## Rules

- The constructor listens and starts serving; it does not wait, so it may run on a worker. A failure to listen
  throws `std::system_error`.
- The destructor closes at once, without waiting: the listener and every connection closed, every request's
  [stop](../request/stop.md) stopped. [close](close.md) does the same and then waits for the handlers to end;
  it blocks the thread, and a task writes `co_await ts.async_close()`.
- A moved-from server has no URL and its [close](close.md) does nothing; its [client](client.md),
  [server](server.md), [certificate](certificate.md) and [requests](requests.md) throw `invalid_argument`.
- [client](client.md) is the same client each time, its pool shared by the copies; any URL may be asked through
  it. Its idle connections are closed with the server.

## Member types

| Type | Definition |
|---|---|
| [options](../test_server-options.md) | TLS, HTTP/2, the requests kept |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](test_server.md) | starts a server of a handler or of a server's routes |
| `(destructor)` | closes the server at once |
| `operator=` | moves another server in, closing this one |

#### Observers

| Function | Description |
|---|---|
| [url](url.md) | the URL to ask: `http://127.0.0.1:port` |
| [endpoint](endpoint.md) | the address it listens on |
| [client](client.md) | a client made for it |
| [server](server.md) | the server serving |
| [certificate](certificate.md) | the CA its certificate leads to |
| [requests](requests.md) | the requests it received |

#### Closing

| Function | Description |
|---|---|
| [close_client_connections](close_client_connections.md) | closes the clients' connections, the server going on |
| [close, async_close](close.md) | closes the server and waits for its handlers |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::test_server ts([](net::http::request r, net::http::response_writer w) {
        w.write("hello " + r.query("name") + " over " + r.proto() + "\n");
    }, {.tls = true, .http2 = true});
    net::http::response r = ts.client().get(ts.url() + "/?name=test");
    print("{}", r.text().value());
    println("{} {}", ts.url().starts_with("https://127.0.0.1:"),
            ts.certificate()->subject().to_string());
}
```

Output:

```text
hello test over HTTP/2.0
true CN=sgcl test CA
```

## See also

- [response_recorder](../response_recorder/README.md): a handler run without the network
- [test_request](../test_request.md): a request as a handler receives one
- [server](../server/README.md): what serves
