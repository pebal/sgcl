[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [server](README.md)

# sgcl::net::smtp::server::serve, async_serve

```cpp
expected<void, io::error> serve(const string& address) const;                                 // (1)
expected<void, io::error> serve(const net::listener& l) const;                                // (2)
async::task<expected<void, io::error>> async_serve(const string& address) const noexcept;     // (3)
async::task<expected<void, io::error>> async_serve(const net::listener& l) const noexcept;    // (4)
```

Serves until [shutdown](shutdown.md) or [close](close.md): each connection a session of its own, as a task.

- (1, 3) Listens on the address (`":2525"`, `"127.0.0.1:25"`).
- (2, 4) The connections of a listener the program made; a TLS listener's sessions are TLS from the first byte.
- (1–2) Block the calling thread; (3–4) for a task.

## Parameters

| Parameter | Description |
|---|---|
| `address` | where to listen, `host:port` |
| `l` | the listener |

## Return value

`net::errc::server_closed` after a shutdown or a close; the error of the listen or of an accept.

## Complexity

As long as the server runs.

## Exceptions

- (1–2) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (3–4) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::smtp::server srv;
    srv.hostname = "mx.example";
    srv.handle([](net::smtp::message m) {
        println("{} -> {}: {}", m.envelope().from, m.envelope().to[0], m.email()->subject());
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    string url = string::concat("smtp://", l.local_endpoint().to_string());
    encoding::email m("alice@example.com", "bob@example.org", "Hello", "Hi, Bob.");
    net::smtp::send(url, m);
    srv.close();
    println("{}", serving.wait().error().code() == net::errc::server_closed);
}
```

Output:

```text
alice@example.com -> bob@example.org: Hello
true
```

## See also

- [serve_tls](serve_tls.md)
- [server](README.md)
