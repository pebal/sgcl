[sgcl](../../README.md) › [net](../README.md) › [smtp](README.md)

# sgcl::net::smtp::reply_of

```cpp
optional<reply> reply_of(const io::error& e) noexcept;
```

The reply an error carries: an error of the codes that come from a reply (`net::errc::smtp_reply`,
`smtp_auth_failed`) has it in its path, `550 5.1.1 No such user`, the lines after the first joined by `"; "`.

## Parameters

| Parameter | Description |
|---|---|
| `e` | an error of the module |

## Return value

The [reply](reply/README.md); nothing for an error of another code.

## Complexity

Linear in the size of the error's path.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::smtp::server srv;
    srv.on_recipient = [](const net::smtp::envelope&, const string&) {
        return net::smtp::reply{550, "5.1.1", "No such user here"};
    };
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    string url = string::concat("smtp://", l.local_endpoint().to_string());
    encoding::email m("alice@example.com", "bob@example.org", "Hello", "Hi, Bob.");

    auto r = net::smtp::send(url, m);
    println("{}", r.error().message());
    auto reply = net::smtp::reply_of(r.error());
    println("{} | {} | {}", reply->code, reply->enhanced, reply->text);
    println("{}", net::smtp::reply_of(io::error(io::errc::closed, "x")).has_value());
    srv.close();
    serving.wait();
}
```

Output:

```text
RCPT TO:<bob@example.org> 550 5.1.1 No such user here: the SMTP server refused
550 | 5.1.1 | No such user here
false
```

## See also

- [reply](reply/README.md)
- [smtp](README.md)
