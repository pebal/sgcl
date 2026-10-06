[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [client](README.md)

# sgcl::net::ssh::client::serve_socks5, async_serve_socks5

```cpp
expected<void, io::error> serve_socks5(const string& address) const;                                                          // (1)
async::task<expected<void, io::error>> async_serve_socks5(string address) const noexcept;                                     // (2)
expected<void, io::error> serve_socks5(const net::listener& l, const net::socks5::server& proxy = {}) const;                  // (3)
async::task<expected<void, io::error>> async_serve_socks5(net::listener l, net::socks5::server proxy = {}) const noexcept;    // (4)
```

Dynamic forwarding, `ssh -D` (OpenSSH's `DynamicForward`): a SOCKS5 proxy here whose connections the server makes,
each CONNECT a direct-tcpip channel (RFC 4254 §7.2) to its target, the name resolved by the server. It serves until
this SSH connection ends (then nothing for its close, its error otherwise) or the listener is closed
(`net::errc::server_closed`). BIND and UDP ASSOCIATE are refused: SSH has no way for them.

- (1–2) Listens on the address ("127.0.0.1:1080").
- (3–4) A listener the program made, and a [socks5::server](../../socks5-server/README.md) whose authentication,
  rules and timeouts are taken (its `dial` is this connection's).

`serve_socks5` waits on the calling thread; a task awaits `async_serve_socks5`.

## Parameters

| Parameter | Description |
|---|---|
| `address` | where the proxy listens |
| `l` | a listener |
| `proxy` | the proxy's settings |

## Return value

Nothing at the SSH connection's close; its error; `net::errc::server_closed` for the listener's close.

## Complexity

Linear in the connections forwarded.

## Exceptions

- (1), (3) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2), (4) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    // a server of the program's own that makes direct-tcpip connections
    net::ssh::server srv;
    srv.host_keys = {net::ssh::private_key::generate()};
    srv.check_password = [](const string& user, const string& password) { return user == "me" && password == "pw"; };
    srv.allow_direct_tcpip = [](const string&, const string&, uint16_t) { return true; };
    net::listener sl = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(sl));
    net::listener target = net::tcp::listen("127.0.0.1:0");
    auto echoing = async::spawn([](net::listener l) -> async::task<> {
        auto c = co_await l.async_accept();
        if (c) {
            (void)co_await c->async_copy_to(*c);
        }
    }(target));
    net::ssh::client::options o;
    o.user = "me";
    o.password = "pw";
    o.insecure_ignore_host_key = true;  // the example's own server
    net::ssh::client c = net::ssh::client::connect(sl.local_endpoint().to_string(), o).value();
    net::listener proxy = net::tcp::listen("127.0.0.1:0");
    auto forwarding = async::spawn(c.async_serve_socks5(proxy));
    net::connection through = net::socks5::connect(proxy.local_endpoint().to_string(), target.local_endpoint().to_string()).value();
    through.write("over ssh -D");
    through.close_write();
    println("{}", through.read_all_text().value());
    c.close();
    println("{}", bool(forwarding.wait()));
    srv.close();
    serving.wait();
}
```

Output:

```text
over ssh -D
true
```

## See also

- [dial](dial.md)
- [socks5::server](../../socks5-server/README.md)
- [client](README.md)
