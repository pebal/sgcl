[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [message](README.md)

# sgcl::net::imap::message::has_flag

```cpp
bool has_flag(const string& f) const noexcept;
```

Returns whether the message has a flag, in any case: a system flag ([imap::flag](../README.md#objects-and-types)) or a
keyword.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the flag: `net::imap::flag::seen`, `"$Work"` |

## Return value

`true` when the flags fetched hold it.

## Complexity

Linear in the number of flags.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/imap.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::imap::memory_backend mail;
    mail.add_user("alice", "secret");
    mail.append("alice", "INBOX", "Subject: x\r\n\r\ny\r\n", {net::imap::flag::seen, "$Work"});
    net::imap::server srv;
    srv.backend = mail;
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));

    net::imap::client::options o;
    o.user = "alice";
    o.password = "secret";
    o.security = net::imap::security::none;   // the loopback, no TLS
    string address = "127.0.0.1:" + to_string(listener.local_endpoint().port());
    net::imap::client session = net::imap::client::connect(address, o);
    session.select("INBOX");
    auto messages = session.fetch(1);
    println("{}", (*messages)[0].has_flag(net::imap::flag::seen));
    println("{}", (*messages)[0].has_flag("$work"));
    srv.close();
}
```

Output:

```text
true
true
```

## See also

- [add_flags](../client/add_flags.md)
- [message](README.md)
