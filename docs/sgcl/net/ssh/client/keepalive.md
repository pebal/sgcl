[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [client](README.md)

# sgcl::net::ssh::client::keepalive, async_keepalive

```cpp
expected<void, io::error> keepalive() const;                                // (1)
async::task<expected<void, io::error>> async_keepalive() const noexcept;    // (2)
```

Whether the server still answers: OpenSSH's `keepalive@openssh.com` global request sent with an answer asked for, and
the answer awaited (a refusal is an answer: OpenSSH refuses it). With `options::keepalive_interval` the client sends
one by itself when the server has been silent that long, and ends the connection when three go unanswered.

1. On a thread.
2. In a task.

## Parameters

None.

## Return value

Nothing; or the connection's error when it ended before the answer.

## Complexity

A round trip.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::ssh::server srv;
    srv.host_keys = {net::ssh::private_key::generate()};
    srv.check_password = [](const string& user, const string& password) { return password == "secret"; };
    srv.handle([](net::ssh::server_session s) { (void)s.output().write("ran " + s.command()); });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    println("{}", c.keepalive().has_value());
    srv.close();
    println("{}", c.keepalive().has_value());
    srv.close();
}
```

Output:

```text
true
false
```

## See also

- [options](../client-options.md): `keepalive_interval`
- [wait](wait.md)
- [sgcl::net::ssh::client](README.md)
