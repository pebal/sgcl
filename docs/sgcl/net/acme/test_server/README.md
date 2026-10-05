[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md)

# sgcl::net::acme::test_server

```cpp
#include "sgcl/net/acme/test_server.h"   // or "sgcl/net/acme.h"

namespace sgcl::net::acme {
    class test_server;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::acme::test_server` is an ACME server on the loopback for tests, what Pebble is to Let's Encrypt, written
from RFC 8555 as the [client](../client/README.md) is: the directory and the nonces, accounts (External Account
Binding, key rollover, deactivation), orders of DNS names, wildcards and IP addresses, authorizations and the three
challenges validated for real — http-01 and tls-alpn-01 at the ports the options name, every name at the loopback;
dns-01 through a lookup the test gives — finalization, certificates issued by a CA of its own (a root and an
intermediate made at its start, alternate chains under further roots on request), revocation, renewal information
(RFC 9773). Go's `x/crypto/acme` client runs through every flow against it in the tree's tests. A test of a program's
ACME code needs no CA of the internet, and no domain.

What a real CA does that it does not: CAA, rate limits of its own (a test asks for one with [fail_next](fail_next.md)),
validation from several places, a certificate transparency log, an OCSP responder.

## Rules

- **The object is the server.** It listens when it is made and stops when it is destroyed or [close](close.md)d; it is
  not copied, and a move hands it on (a moved-from server is only destroyed or assigned).
- **Plain HTTP by default**, the API at `http://127.0.0.1:port/acme/directory`; `options::tls` serves it over https with
  a certificate of its CA, which [roots](roots.md) gives the client to trust.
- **Validation** at `options::validation_host` (the loopback): http-01 at `http_port`, tls-alpn-01 at `tls_port` with
  the SNI of the name (an address's reverse name), dns-01 by `options::lookup_txt`; `options::skip_validation` takes
  every challenge as valid when it is answered. A wildcard's authorization offers dns-01 alone, an address's http-01
  and tls-alpn-01.
- **Its objects live in memory** for as long as it does; [orders](orders.md), [certificates](certificates.md) and
  [revoked](revoked.md) report on them.

## Member types

| Type | Definition |
|---|---|
| [options](../test_server-options.md) | what the server is and how it validates |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](test_server.md) | a server listening at once |
| `(destructor)` | stops it |
| [directory_url](directory_url.md) | the URL of its directory |
| [roots](roots.md) | the roots of its CA |
| [fail_next](fail_next.md) | the next requests answered with a problem |
| [set_renewal_window](set_renewal_window.md) | the window renewalInfo suggests from now on |
| [revoked](revoked.md) | checks whether a certificate was revoked |
| [orders](orders.md) | the orders made |
| [certificates](certificates.md) | the certificates issued |
| [close](close.md) | stops it |

## See also

- [client](../client/README.md), [manager](../manager/README.md): what runs against it
- [options](../test_server-options.md)
- [net::acme](../README.md)
