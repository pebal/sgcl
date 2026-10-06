[sgcl](../../README.md) › [net](../README.md) › [socks5](../socks5/README.md) › [server](README.md)

# sgcl::net::socks5::server::serve, async_serve

```cpp
expected<void, io::error> serve(const string& address) const;                                 // (1)
async::task<expected<void, io::error>> async_serve(const string& address) const noexcept;     // (2)
expected<void, io::error> serve(const net::listener& l) const;                                // (3)
async::task<expected<void, io::error>> async_serve(const net::listener& l) const noexcept;    // (4)
```

Listens and serves until [shutdown](shutdown.md) or [close](close.md), or the listener's close by the program: then
`net::errc::server_closed`.

- (1–2) Listens on the address ("127.0.0.1:1080").
- (3–4) The connections of a listener the program made.

`serve` waits on the calling thread; a task awaits `async_serve`.

## Parameters

| Parameter | Description |
|---|---|
| `address` | where to listen |
| `l` | a listener |


## Return value

`net::errc::server_closed` at its end; a listen's error.

## Complexity

Linear in the connections served.

## Exceptions

- (1), (3) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2), (4) None.

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
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(proxy.async_serve(l));
    net::connection c = net::socks5::connect(l.local_endpoint().to_string(), target.local_endpoint().to_string()).value();
    c.write("ping");
    c.close_write();
    println("{}", c.read_all_text().value());
    proxy.close();
    serving.wait();
}
```

Output:

```text
ping
```

## See also

- [shutdown](shutdown.md)
- [server](README.md)
