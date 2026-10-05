[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md)

# sgcl::net::smtp::message

```cpp
#include "sgcl/net/smtp/server.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::smtp {
    class message;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::smtp::message` is a message a [server](../server/README.md) received, as its handler gets it: the
[envelope](envelope.md) — the reverse path, the recipients, the parameters, what the session learned — and the
bytes of the message, its dots taken off and its line breaks CRLF, read whole before the handler runs. A reader of
those bytes ([read](read.md)), all of them at once ([bytes](bytes.md)), or the message parsed in one call
([email](email.md)).

## Rules

- A handle: a copy is the same message and reads on from where it is.
- It is an `io::req::reader` and its async form: `io::copy(file, m)` saves the message as it came.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](message.md) | a message that holds none, or a copy |
| [envelope](envelope.md) | the envelope |
| [read, async_read](read.md) | the next bytes of the message |
| [size](size.md) | the size in bytes |
| [bytes](bytes.md) | the whole message |
| [email](email.md) | the message parsed |
| [operator bool](operator_bool.md) | whether the handle holds a message |

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
    srv.handle([](net::smtp::message m) {
        io::write_file("received.eml", m.bytes());
        println("{} bytes from {}", m.size() > 0, m.envelope().client.address().to_string());
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    string url = string::concat("smtp://", l.local_endpoint().to_string());
    net::smtp::send(url, encoding::email("a@example.com", "b@example.org", "Saved", "Hello."));
    println("{}", encoding::email::load("received.eml")->subject());
    srv.close();
    serving.wait();
}
```

Output:

```text
true bytes from 127.0.0.1
Saved
```

## See also

- [server::handle](../server/handle.md)
- [smtp](../README.md)
