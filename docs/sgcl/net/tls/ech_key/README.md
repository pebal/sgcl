[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md)

# sgcl::net::tls::ech_key

```cpp
#include "sgcl/net/tls.h"

namespace sgcl::net::tls {
    class ech_key {
    public:
        struct options;
    };
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`net::tls::ech_key` is a server's key of Encrypted Client Hello (RFC 9849), Go's `tls.EncryptedClientHelloKey`: an
ECHConfig — its id, the public key of its KEM, the HPKE suites it takes, the public name the outer hello names — and
the HPKE private key that opens the hellos sealed to it. A server's [config](../config.md) holds its keys in
`ech_keys`, and publishes their [ech_config_list](../ech_config_list.md) in its DNS HTTPS record (RFC 9848) for clients
to put in their `ech_config_list`. A client that has the list seals its real hello, the ClientHelloInner with the name
it wants, inside an outer hello that names only the public name; a server with the key opens it and answers the inner
hello, and nobody on the path learns the name.

It is a handle of one word whose state is made by [generate](generate.md) or [from_bytes](from_bytes.md): copies share
one key. The private key sits in an unmanaged block of its own, never copied, zeroed when the last handle's state is
collected.

## Rules

- **Rotation.** A server may hold several keys; a hello sealed to any of them opens. A key whose
  [retry](retry.md) is true is sent in the `retry_configs` of a rejected hello, so that a client with an old list
  connects again with the current one ([connect](../connect.md) does it by itself, once).
- **The public name** is a DNS name the server holds a certificate of: a rejected hello is answered as the outer one,
  and the client checks the server for that name before it takes the `retry_configs`.

## Member types

| Type | Definition |
|---|---|
| [options](../ech_key-options.md) | what a new key is made of: its id, its KEM, its suites, the longest name, retry |

## Member functions

| Function | Description |
|---|---|
| [generate](generate.md) | a new key of a public name (static) |
| [from_bytes](from_bytes.md) | a key of an ECHConfig and its private key (static) |

#### Observers

| Function | Description |
|---|---|
| [config](config.md) | the ECHConfig |
| [private_key](private_key.md) | the HPKE private key, in a secret_bytes |
| [config_id](config_id.md) | the config's id |
| [public_name](public_name.md) | the public name |
| [retry](retry.md) | checks whether the config is sent in retry_configs |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    // the server: an identity of the name it serves and of the public name, and an ECH key
    auto chain = io::read_text("tests/net/tls_testdata/ecdsa.pem");  // a leaf for localhost
    net::tls::config server_cfg;
    server_cfg.identities = {net::tls::identity(chain, crypto::read_secret("tests/net/tls_testdata/ecdsa.key"))};
    auto key = net::tls::ech_key::generate("localhost");
    server_cfg.ech_keys = {key};
    net::listener incoming = net::tls::listen("127.0.0.1:0", server_cfg);
    auto serving = async::spawn([](net::listener l) -> async::task<> {
        auto c = co_await l.async_accept();
        (void)co_await c->async_close();
    }(incoming));

    // the client: the server's ECHConfigList, as its DNS would give it
    net::tls::config cfg;
    cfg.roots = crypto::x509::certificate_pool::from_pem(io::read_text("tests/net/tls_testdata/ca.pem"));
    cfg.ech_config_list = net::tls::ech_config_list({key});
    auto c = net::tls::connect("localhost:" + to_string(incoming.local_endpoint().port()), cfg);
    println("{}", net::tls::state_of(*c)->ech_accepted);
    c->close();
    serving.wait();
    incoming.close();
}
```

Output:

```text
true
```

## See also

- [ech_config_list](../ech_config_list.md): what a server publishes
- [config](../config.md): `ech_config_list`, `ech_keys`
- [crypto::hpke](../../../crypto/hpke.md): what the hellos are sealed with
- [net::tls](../README.md)
