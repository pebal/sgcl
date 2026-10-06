[sgcl](../../../README.md) › [net](../../README.md) › [pop3](../README.md) › [client](README.md)

# sgcl::net::pop3::client::noop, async_noop

```cpp
expected<void, io::error> noop() const;
async::task<expected<void, io::error>> async_noop() const noexcept;
```

NOOP (RFC 1939 §5): the server asked whether it is there, the session kept from its idle timeout.

`noop` waits on the calling thread; a task awaits `async_noop`.

## Parameters

None.

## Return value

Nothing; a refusal is the error.

## Complexity

One round trip.

## Exceptions

- `noop`: `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- `async_noop`: none.

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
    mail.append("alice", "INBOX", "From: carol@example.com\r\nSubject: Report\r\n\r\nLine 1\r\nLine 2\r\n");
    net::pop3::server srv;
    srv.backend = mail;
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    net::pop3::client::options o;
    o.user = "alice";
    o.password = "secret";
    o.security = net::pop3::security::none;  // the loopback, no TLS
    net::pop3::client c = net::pop3::client::connect(l.local_endpoint().to_string(), o).value();
    println("{}", c.noop().has_value());
    c.quit();
    srv.close();
    serving.wait();
}
```

Output:

```text
true
```

## See also

- [quit](quit.md)
- [client](README.md)
