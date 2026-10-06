[sgcl](../../README.md) › [net](../README.md) › [socks5](../socks5/README.md)

# sgcl::net::socks5::server

```cpp
#include "sgcl/net/socks5.h"   // or "sgcl/net.h"

namespace sgcl::net {
    struct socks5 {
        class server;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::socks5::server` is a SOCKS5 proxy (RFC 1928): its clients' connections made from here and relayed both
ways — CONNECT to a target, BIND for a connection that comes to it, UDP ASSOCIATE for datagrams — with no
authentication or a username and password (RFC 1929) the program checks. A function of the program's may make the
connections out instead (`dial`): through an SSH connection, as `ssh -D` does
([ssh::client::serve_socks5](../ssh/client/serve_socks5.md)). Go's standard library has no SOCKS5 server.

## Rules

- A handle of one word: copies share the connections; the fields are read when `serve` is called.
- [shutdown](shutdown.md) and [close](close.md) are final: the server stays closed, a later `serve` ends at once.
- A name in a request is resolved here (or by `dial`'s other end); an IPv4 or IPv6 address is taken as it is.
- A target that cannot be reached is the reply's REP: refused (5), host unreachable (4), network unreachable (3),
  timed out (6); `allow`'s refusal is "not allowed by the ruleset" (2); a command it does not take is 7.
- BIND listens on the address the client reached the proxy by, its first reply the listener's address, its second
  the peer that came; UDP ASSOCIATE relays datagrams of RFC 1928 §7 (a fragment dropped) for as long as the TCP
  connection lasts. With `dial` set both are refused.
- A relay ends with either side's end, the other side told by a half close; a relay silent `idle_timeout` one way
  ends.

## Member objects

| Member | Description |
|---|---|
| `authenticate` | a `function<bool(const string& username, const string& password)>`: set, a username and password required (RFC 1929); empty by default: no authentication |
| `allow` | a `function<bool(const endpoint& client, const string& target)>`: each request's target ("host:port") checked; empty by default: every one |
| `dial` | a `function<async::task<expected<net::connection, io::error>>(string target, async::stop_token stop)>`: the connections out; empty by default: TCP from here |
| `bind` | BIND taken (without `dial`); true by default |
| `udp` | UDP ASSOCIATE taken (without `dial`); true by default |
| `timeout` | the handshake, the connection out, a BIND's wait; 30 s by default |
| `idle_timeout` | a relay silent one way that long ended; 10 minutes by default; zero: none |
| `max_connections` | past it a client is closed at once; zero by default: none |
| `on_error` | an accept's failure; none by default |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](server.md) | a server, or a copy that shares one |
| `(destructor)` | drops the handle |
| `operator=` | the handle of another server |
| [serve, async_serve](serve.md) | listens and serves |
| [shutdown, async_shutdown](shutdown.md) | the listeners closed, the relays left to end |
| [close](close.md) | everything closed at once |
| [connections](connections.md) | the clients being served |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    // a target: an echo of one line
    net::listener target = net::tcp::listen("127.0.0.1:0");
    auto echoing = async::spawn([](net::listener l) -> async::task<> {
        auto c = co_await l.async_accept();
        if (c) {
            (void)co_await c->async_copy_to(*c);
        }
    }(target));
    net::socks5::server proxy;
    proxy.authenticate = [](const string& user, const string& password) { return user == "alice" && password == "secret"; };
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(proxy.async_serve(l));
    net::socks5::options o;
    o.username = "alice";
    o.password = "secret";
    net::connection c = net::socks5::connect(l.local_endpoint().to_string(), target.local_endpoint().to_string(), o).value();
    c.write("through the proxy");
    c.close_write();
    println("{}", c.read_all_text().value());
    proxy.close();
    serving.wait();
}
```

Output:

```text
through the proxy
```

## See also

- [socks5](../socks5/README.md)
- [ssh::client::serve_socks5](../ssh/client/serve_socks5.md)
