[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [manager](README.md)

# sgcl::net::acme::manager::tls_config

```cpp
tls::config tls_config() const;
```

A server's [TLS config](../../tls/config.md) of the manager's certificates: its `identity_for` gives each hello the
identity of its name ([certificate](certificate.md)), and to the CA's hello of tls-alpn-01 (`acme-tls/1` alone) the
challenge's certificate; its `alpn` is `h2`, `http/1.1` and `acme-tls/1`, so that tls-alpn-01 is validated on the
server's own port (an HTTP server completes the list as it does, [server::serve_tls](../../http/server/serve_tls.md)).
The rest is the defaults of a config, for the program to change in the copy it gets.

## Parameters

None.

## Return value

The config.

## Complexity

Constant.

## Exceptions

None but running out of memory.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::test_server ca({.skip_validation = true});
    net::acme::manager certificates({"example.com"},
                                    {.directory_url = ca.directory_url(), .accept_terms = true});
    net::listener incoming = net::tls::listen("127.0.0.1:0", certificates.tls_config());
    auto accepting = async::spawn([](net::listener l) -> async::task<> {
        auto c = co_await l.async_accept();
        co_await c->async_write("hello\n");
        co_await c->async_close();
    }(incoming));

    net::tls::config cfg;
    cfg.server_name = "example.com";
    cfg.roots = ca.roots();   // the test CA's
    auto c = net::tls::connect("127.0.0.1:" + to_string(incoming.local_endpoint().port()), cfg);
    auto served = net::tls::state_of(*c)->peer_certificates[0];
    println("{} {}", **c->read_line(), served.dns_names()[0]);
    accepting.wait();
    incoming.close();
    certificates.close();
}
```

Output:

```text
hello example.com
```

## See also

- [tls::config](../../tls/config.md): `identity_for`
- [http::serve_tls](../../http/serve_tls.md)
- [sgcl::net::acme::manager](README.md)
