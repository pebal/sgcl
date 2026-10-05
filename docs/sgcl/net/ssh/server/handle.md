[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [server](README.md)

# sgcl::net::ssh::server::handle

```cpp
template<class Handler>
server& handle(Handler h);
```

The handler of the sessions: a function of a [server_session](../server_session/README.md), returning `void` or
`async::task<>`, which the server tells apart by the type. It runs once the client started a command, a shell or a
subsystem: one returning `void` on a thread of the blocking pool, where the blocking forms of the session's streams
may wait; one returning `async::task<>` as a task, which awaits the `async_` forms. When it returns, the session ends:
the exit status (0 unless [exit](../server_session/exit.md) or [exit_signal](../server_session/exit_signal.md) sent
one), the end of the output, the close. A second call replaces the first, for the connections served from then on;
without one, a session ends at once with status 0.

## Parameters

| Parameter | Description |
|---|---|
| `h` | the handler |

## Return value

The server itself.

## Complexity

Constant.

## Exceptions

What the copy of `h` throws.

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
    srv.handle([](net::ssh::server_session s) -> async::task<> {
        string text = (co_await s.input().async_read_all_text()).value();
        co_await s.output().async_write(text + text);
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    net::ssh::session s = c.open_session();
    s.exec("twice");
    s.input().write("ab");
    s.close_input();
    println("{}", s.output().read_all_text().value());
    srv.close();
}
```

Output:

```text
abab
```

## See also

- [server_session](../server_session/README.md)
- [sgcl::net::ssh::server](README.md)
