[sgcl](../../../README.md) › [net](../../README.md) › [pop3](../README.md) › [server](README.md)

# sgcl::net::pop3::server::shutdown, async_shutdown

```cpp
void shutdown() const;
async::task<> async_shutdown() const noexcept;
```

Gracefully: the listeners closed, each session waiting for a command told `-ERR [SYS/TEMP]` and closed, the others after the command they are in; returns when all have ended. A session ended so removes nothing.

`shutdown` waits on the calling thread; a task awaits `async_shutdown`.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the connections.

## Exceptions

- `shutdown`: `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- `async_shutdown`: none.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/imap.h"
#include "sgcl/net/pop3.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::imap::memory_backend mail;
    mail.add_user("alice", "secret");
    mail.append("alice", "INBOX", "From: bob@example.com\r\nSubject: Lunch\r\n\r\nNoon?\r\n");
    net::pop3::server srv;
    srv.backend = mail;
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    net::pop3::client::options o;
    o.user = "alice";
    o.password = "secret";
    o.security = net::pop3::security::none;  // the loopback, no TLS
    net::pop3::client c = net::pop3::client::connect(l.local_endpoint().to_string(), o).value();
    srv.shutdown();
    println("{} {}", srv.connections(), c.noop().has_value());
    println("{}", serving.wait().error().code() == net::errc::server_closed);
}
```

Output:

```text
0 false
true
```

## See also

- [close](close.md)
- [server](README.md)
