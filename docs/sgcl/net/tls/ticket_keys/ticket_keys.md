[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md) › [ticket_keys](README.md)

# sgcl::net::tls::ticket_keys::ticket_keys

```cpp
ticket_keys() noexcept;                            // (1)
ticket_keys(const ticket_keys& other) noexcept;    // (2), implicitly declared
ticket_keys(ticket_keys&& other) noexcept;         // (3), implicitly declared
```

1. Keys of their own: the first, 32 bytes of the system's random generator and a random name, is drawn when the
   first ticket is sealed under them.
2. A handle of the same keys as `other`: a ticket either seals, either opens.
3. The same; `other` still holds the keys, since the move of the word inside is its copy.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle whose keys this one shares |

## Complexity

Constant.

## Exceptions

None.

## Example

A server whose config takes the keys of another's resumes that one's tickets.

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    net::tls::identity id(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                          crypto::read_secret("tests/net/tls_testdata/ecdsa.key"));
    net::tls::config first, second, third;
    first.identities = second.identities = third.identities = {id};
    second.ticket_keys = first.ticket_keys;
    net::listener transport = net::tcp::listen("127.0.0.1:0");
    string address = "localhost:" + to_string(transport.local_endpoint().port());

    net::tls::config cfg;
    cfg.roots = crypto::x509::certificate_pool::from_pem(
        io::read_text("tests/net/tls_testdata/ca.pem"));
    cfg.session_cache = net::tls::session_cache();
    for (const net::tls::config* server : {&first, &second, &third}) {
        auto pending = async::spawn(net::tls::async_connect(address, cfg));
        net::connection served = net::tls::server(transport.accept().value(), *server).value();
        auto c = pending.wait();
        served.write("hello\n");
        c->read_line();
        println("{}", net::tls::state_of(*c)->resumed);
        c->close();
        served.close();
    }
    transport.close();
}
```

Output:

```text
false
true
false
```

## See also

- [rotate](rotate.md): a new key
- [sgcl::net::tls::ticket_keys](README.md)
