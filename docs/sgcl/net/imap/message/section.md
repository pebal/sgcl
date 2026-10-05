[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [message](README.md)

# sgcl::net::imap::message::section

```cpp
optional<string> section(const string& part) const noexcept;
```

Returns the bytes of a section the fetch asked for, the section named as [fetch_options](../fetch_options.md)'s
`sections` named it, in any case: `""` the whole message, `"HEADER"`, `"TEXT"`, `"1.2"`, `"HEADER.FIELDS (FROM)"`.

## Parameters

| Parameter | Description |
|---|---|
| `part` | the section |

## Return value

The bytes; `nullopt` for a section not fetched. A section of no part of the message is an empty string, as the server
gives it.

## Complexity

Linear in the number of sections fetched.

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
    session.select("INBOX");
    net::imap::fetch_options what;
    what.sections = {"HEADER"};
    auto messages = session.fetch(1, what);
    print("{}", *(*messages)[0].section("header"));
    println("{}", (*messages)[0].section("TEXT").has_value());
    srv.close();
}
```

Output:

```text
From: Bob <bob@example.com>
Subject: Lunch

false
```

## See also

- [text](text.md)
- [message](README.md)
