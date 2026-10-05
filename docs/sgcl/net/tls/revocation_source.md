[sgcl](../../README.md) › [net](../README.md) › [tls](README.md)

# sgcl::net::tls::revocation_source

```cpp
#include "sgcl/net/tls.h"

namespace sgcl::net::tls {
    enum class revocation_source : uint8_t {
        none,
        staple,
        ocsp,
        crl,
    };
}
```

Where the revocation status of a connection's peer came from: the [state](state.md)'s `revocation_source`, beside
its `revocation`, the status of the peer's leaf as the check of [revocation_mode](revocation_mode.md) found it.

| Value | Description |
|---|---|
| `none` | not checked (`revocation_mode::off`, a resumed session, a chain not verified), or checked and nothing said: the status `unknown` |
| `staple` | the OCSP response the peer stapled to its leaf |
| `ocsp` | an OCSP responder of the leaf's authority information access, asked online, or the answer of one kept in the [revocation_cache](revocation_cache/README.md) |
| `crl` | a CRL: one of `config::crls`, or one fetched from a distribution point of the leaf (or kept in the cache) |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    string dir = "tests/crypto/data/revocation/";
    net::tls::identity good(io::read_text(dir + "good.pem").value() +
                                io::read_text(dir + "int.pem").value(),
                            crypto::read_secret(dir + "good.key"));
    net::tls::config server_cfg;
    server_cfg.identities = {good};
    net::listener incoming = net::tls::listen("127.0.0.1:0", server_cfg);
    string address = "localhost:" + to_string(incoming.local_endpoint().port());

    net::tls::config cfg;
    cfg.roots = crypto::x509::certificate_pool::from_pem(io::read_text(dir + "root.pem"));
    cfg.revocation = net::tls::revocation_mode::staple_only;
    auto c = net::tls::connect(address, cfg);
    println("{}", net::tls::state_of(*c)->revocation_source == net::tls::revocation_source::none);

    // the CA's list of revoked certificates, given by the program
    cfg.crls = {crypto::x509::revocation_list::parse(
        io::read_file(dir + "int.crl").value()).value()};
    auto d = net::tls::connect(address, cfg);
    println("{}", net::tls::state_of(*d)->revocation_source == net::tls::revocation_source::crl);
    c->close();
    d->close();
    incoming.close();
}
```

Output:

```text
true
true
```

## See also

- [state](state.md): `revocation_source`
- [revocation_mode](revocation_mode.md): what is checked
- [net::tls](README.md)
