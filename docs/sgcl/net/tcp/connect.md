[sgcl](../../README.md) › [net](../README.md) › [tcp](../tcp.md)

# sgcl::net::tcp::connect, async_connect

```cpp
/*(1)*/ static expected<net::connection, io::error> connect(const string& address);
/*(2)*/ static expected<net::connection, io::error> connect(const string& address,
                                                            async::stop_token stop);
/*(3)*/ static expected<net::connection, io::error> connect(const string& address,
                                                            duration timeout);
/*(4)*/ static expected<net::connection, io::error> connect(const net::endpoint& to);
/*(5)*/ static async::task<expected<net::connection, io::error>> async_connect(const string& address) noexcept;
/*(6)*/ static async::task<expected<net::connection, io::error>> async_connect(const string& address,
                                                                              async::stop_token stop) noexcept;
/*(7)*/ static async::task<expected<net::connection, io::error>> async_connect(const string& address,
                                                                              duration timeout) noexcept;
/*(8)*/ static async::task<expected<net::connection, io::error>> async_connect(const net::endpoint& to) noexcept;
```

Connects to `address` over TCP: Go's `net.Dial("tcp", address)`, `DialContext` with a stop token, `DialTimeout`
with a timeout.

- (1–3, 5–7) `address` is `"host:port"`: a name, an IPv4 address, an IPv6 address in brackets, or no host, which is
  this machine (`127.0.0.1` and `::1`). The name is looked up ([dns::lookup](../dns/lookup.md)) and the addresses
  are raced as RFC 8305 has it, happy eyeballs: the addresses of the two families taken in turns (§4), the first
  family the first address's; an attempt started, the next one 250 ms later (§5) or at once when one fails; the
  first connection the one returned, the other attempts stopped, and a connection that comes after the winner's
  closed. When every attempt fails, the error is the first attempt's, as in Go.
- (2, 6) A stop of `stop` ends the whole of it, the lookup included, with `ECANCELED`; the stop wins over a
  connection that comes at the same moment.
- (3, 7) `timeout` bounds the whole of it, the lookup included, with `ETIMEDOUT`. A timeout of zero or less is
  `ETIMEDOUT` at once, with no attempt made, as a read past its deadline reads nothing. A timeout at the clock's end is
  none: `duration::max()` saturates at `time_point::max()`, where no timer fires.
- (4, 8) The endpoint as it is: no lookup, no race, one attempt.
- (1–3) Run the race on the scheduler and wait for it on the calling thread: they are for a thread, as
  [task::wait](../../async/task/wait.md) is (debug builds assert); a task awaits (5–7).
- (4) Blocks the calling thread wherever it is, a worker included, as a blocking read of `io::file` does: a worker
  so blocked runs no other task meanwhile, so a task awaits (8).
- (5–8) The same for a task, which holds no worker while it waits.

The connection has Nagle's algorithm off and keep-alive probes after 15 s of silence, as in Go.

## Parameters

| Parameter | Description |
|---|---|
| `address` | `"host:port"`; the port a number, never a service name |
| `stop` | ends the connect when stopped |
| `timeout` | the longest the whole connect may take |
| `to` | the address and port to dial as they are |

## Return value

The [connection](../connection.md); or the [io::error](../../io/error.md), its operation `dial tcp` (`lookup` for a
failure of the lookup) and its path the address given:

- `net::errc::invalid_address` for an address that is not `"host:port"`: no port, a port past 65535 or not a number,
  an IPv6 host without brackets;
- `net::errc::host_not_found` for a name the resolver does not know, an `EAI_*` code of
  [lookup_category](../lookup_category.md) for a failure of the resolver;
- `ECANCELED` for the stop (2, 6), `ETIMEDOUT` (`is_timeout()`) for the timeout (3, 7);
- the `errno` of the first attempt otherwise (`ECONNREFUSED`, `ENETUNREACH`).

The message of a race that failed names every attempt, in the order they were made, so that a name of one address
is told from one whose every address failed: `dial tcp localhost:80 (every address failed: [::1]:80: Connection
refused; 127.0.0.1:80: Connection refused): Connection refused`, or `(its only address: ...)`.

## Complexity

The lookup, and one attempt per address until one connects.

## Exceptions

- (1–3) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (4) `std::system_error` when the connect has to wait and the thread of the reactor, or of the timers, which its
  first use starts, cannot be made.
- (5–8) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    net::listener l = net::tcp::listen("127.0.0.1:0");
    string port = to_string(l.local_endpoint().port());

    // localhost is ::1 and 127.0.0.1: ::1 is refused, 127.0.0.1 wins the race
    net::connection c = net::tcp::connect("localhost:" + port, 5s);
    println("{}", c.remote_endpoint().address());

    async::stop_source source;
    source.request_stop();
    auto stopped = net::tcp::connect("localhost:" + port, source.token());
    println("{}", stopped.error().code() == std::errc::operation_canceled);

    auto bad = net::tcp::connect("localhost");
    println("{}", bad.error().message());

    l.close();
    auto refused = net::tcp::connect(l.local_endpoint());
    println("{}", refused.error().code() == std::errc::connection_refused);
}
```

Output:

```text
127.0.0.1
true
dial tcp localhost: invalid address
true
```

A task awaits the connect:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;
using namespace std::chrono_literals;

async::task<> greet(net::listener l) {
    net::connection c = co_await l.async_accept();
    co_await c.async_write("hello");
    co_await c.async_close();
}

async::task<string> fetch(net::endpoint at) {
    net::connection c = co_await net::tcp::async_connect(at.to_string(), 2s);
    string text = (co_await c.async_read_all_text()).value();
    co_await c.async_close();
    co_return text;
}

int main() {
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto server = async::spawn(greet(l));
    println("{}", async::run(fetch(l.local_endpoint())));
    server.wait();
    l.close();
}
```

Output:

```text
hello
```

## See also

- [listen, async_listen](listen.md): the other side
- [dns::lookup](../dns/lookup.md): the lookup of the name, with the same stop and limit
- [stop_source](../../async/stop_source.md): what makes a stop token
- [sgcl::net::tcp](../tcp.md)
