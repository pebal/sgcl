[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [client](README.md)

# sgcl::net::smtp::client::verify, async_verify

```cpp
expected<reply, io::error> verify(const string& address) const;
async::task<expected<reply, io::error>> async_verify(const string& address) const noexcept;
```

VRFY: what the server says of an address (RFC 5321 §3.5.1). Most servers answer 252, "cannot verify, but will try", and do not tell who is there.

`verify` waits on the calling thread; a task awaits `async_verify`.

## Parameters

None.

## Return value

The [reply](../reply/README.md) when it is positive; a refusal is the error `smtp_reply`.

## Complexity

One round trip.

## Exceptions

- `verify`: `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- `async_verify`: none.

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
    srv.hostname = "mx.example";
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    string url = string::concat("smtp://", l.local_endpoint().to_string());
    net::smtp::client c = net::smtp::client::connect(url);
    println("{}", c.verify("bob@example.org")->code);
    c.quit();
    srv.close();
    serving.wait();
}
```

Output:

```text
252
```

## See also

- [client](README.md)
