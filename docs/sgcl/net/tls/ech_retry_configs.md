[sgcl](../../README.md) › [net](../README.md) › [tls](README.md)

# sgcl::net::tls::ech_retry_configs

```cpp
vector<byte> ech_retry_configs(const io::error& e) noexcept;
```

The `retry_configs` of a server that rejected a client's Encrypted Client Hello (RFC 9849 §6.1.6), Go's
`ECHRejectionError.RetryConfigList`: the ECHConfigList of the server's current keys, for the client's
[config](config.md)'s `ech_config_list` on its next try. [connect](connect.md) dials once more with them by itself;
over a transport of the program's, [client](client.md) gives the `alert::ech_required` error, and this function
reads the list from it. The error's text ends with the list as `ech=` and its base64, as a DNS HTTPS record shows
it (RFC 9460), so a logged error carries it too.

## Parameters

| Parameter | Description |
|---|---|
| `e` | an error of [client](client.md) or [connect](connect.md) |

## Return value

The list; empty for an error of another code than [alert](alert.md)`::ech_required` and for a server that sent
none (a server without ECH keys).

## Complexity

Linear in the length of the error's text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    // a server whose key is newer than the client's list; its identity is of the public name
    net::tls::config server_cfg;
    server_cfg.identities = {net::tls::identity(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                                                crypto::read_secret("tests/net/tls_testdata/ecdsa.key"))};
    auto current = net::tls::ech_key::generate("localhost", {.config_id = 2});
    server_cfg.ech_keys = {current};
    net::listener incoming = net::tls::listen("127.0.0.1:0", server_cfg);
    auto serving = async::spawn([](net::listener l) -> async::task<> {
        while (auto c = co_await l.async_accept()) {   // the handshakes that succeed, until closed
            (void)co_await c->async_close();
        }
    }(incoming));

    net::tls::config cfg;
    cfg.server_name = "localhost";   // client() has no address to take it from
    cfg.roots = crypto::x509::certificate_pool::from_pem(io::read_text("tests/net/tls_testdata/ca.pem"));
    cfg.ech_config_list = net::tls::ech_config_list({net::tls::ech_key::generate("localhost", {.config_id = 1})});
    const string address = "localhost:" + to_string(incoming.local_endpoint().port());
    auto first = net::tls::client(net::tcp::connect(address).value(), cfg);
    println("{}", first.error().code() == net::tls::alert::ech_required);
    cfg.ech_config_list = net::tls::ech_retry_configs(first.error());
    auto second = net::tls::client(net::tcp::connect(address).value(), cfg);
    println("{}", net::tls::state_of(*second)->ech_accepted);
    second->close();
    incoming.close();
    serving.wait();
}
```

Output:

```text
true
true
```

## See also

- [ech_key](ech_key/README.md), [ech_config_list](ech_config_list.md)
- [connect](connect.md): the second try made by itself
- [net::tls](README.md)
