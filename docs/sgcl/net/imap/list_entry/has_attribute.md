[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [list_entry](README.md)

# sgcl::net::imap::list_entry::has_attribute

```cpp
bool has_attribute(const string& a) const noexcept;
```

Returns whether the entry has an attribute, in any case.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the attribute: `"\\HasChildren"`, `net::imap::special_use::trash` |

## Return value

`true` when the attributes hold it.

## Complexity

Linear in the number of attributes.

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
    mail.append("alice", "INBOX", "From: Bob <bob@example.com>\r\nSubject: Lunch\r\n\r\nNoon?\r\n");
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
    session.create("Trash", net::imap::special_use::trash);
    auto boxes = session.list("Trash");
    println("{}", (*boxes)[0].has_attribute(net::imap::special_use::trash));
    println("{}", (*boxes)[0].has_attribute("\\hasnochildren"));
    srv.close();
}
```

Output:

```text
true
true
```

## See also

- [list_entry](README.md)
