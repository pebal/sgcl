[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md)

# sgcl::net::acme::manager

```cpp
#include "sgcl/net/acme/manager.h"   // or "sgcl/net/acme.h"

namespace sgcl::net::acme {
    class manager;
    inline constexpr char lets_encrypt_url[] = "https://acme-v02.api.letsencrypt.org/directory";
    inline constexpr char lets_encrypt_staging_url[] =
        "https://acme-staging-v02.api.letsencrypt.org/directory";
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::acme::manager` is certificates obtained and renewed by themselves for a TLS server: Go's
`golang.org/x/crypto/acme/autocert.Manager`. It is given the names the server answers to (exact names, and wildcards
such as `*.example.com`), or a policy that decides, and its [tls_config](tls_config.md) is given to the server; a
server is `net::http::serve_tls(":443", manager.tls_config(), handler)`. The first handshake for a name obtains the
name's certificate from the CA — however many handshakes for it wait meanwhile, one order — and serves it; the next
handshakes are served from memory; the next start of the program reads it from the cache directory.

A certificate is obtained by tls-alpn-01 on the server's own port by default (the CA's hello of `acme-tls/1` is
answered with the challenge's certificate by the same config), by http-01 through [http_handler](http_handler.md) on
port 80, or by dns-01 through a publisher of TXT records of the program's own (a wildcard's only way), in the order
of `options::challenges`. Each is renewed in the background before it expires — when the CA's renewal information
(RFC 9773) suggests, at a random point of its window, else at two thirds of its lifetime — and the old one is served
until the new one is there.

## Rules

- **A handle of one word.** Its copies, and the configs [tls_config](tls_config.md) makes, are the same manager: its
  account, its certificates, its renewals.
- **The account** is registered on first use with `options::contact`, the terms agreed to when
  `options::accept_terms` says so (Let's Encrypt refuses an account otherwise), and an external account binding when
  given; its key is `options::key`, else the cache's (`acme_account+key`), else a new P-256 key kept there.
- **The cache** (`options::cache`, empty for memory alone) is a directory made at 0700, a file per certificate (its
  name, a wildcard's `*` as `_`), the key's PEM and the chain in it, written whole through a `.tmp` and renamed, at
  0600. A cached certificate past two thirds of its lifetime is renewed rather than served. A cache that cannot be
  written leaves the certificates in memory.
- **What is refused**: a hello for a name neither the names nor `options::host_policy` allow ends its handshake with
  `internal_error`, as Go's server answers a `GetCertificate` that fails, and [certificate](certificate.md) is
  `errc::host_not_allowed`; a hello without SNI takes `options::default_name`, else is refused.
- **Failures are kept for their backoff**: a name whose certificate could not be had is answered with the same error,
  without a request, for a minute, then two, doubling to a day; a renewal that fails is tried again on the same
  backoff, the old certificate served meanwhile.
- **[close](close.md)** stops the renewals and the obtaining; what is held is still served.

## Member types

| Type | Definition |
|---|---|
| [options](../manager-options.md) | where certificates come from and how |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](manager.md) | a manager of names, or of a policy |
| [tls_config](tls_config.md) | a server's TLS config of the manager's certificates |
| [http_handler](http_handler.md) | a handler for port 80: http-01, else a redirect to https |
| [certificate, async_certificate](certificate.md) | the identity of a name: from memory, the cache or the CA |
| [client, async_client](client.md) | the ACME client of the manager's account |
| [close](close.md) | the renewals stopped |

## See also

- [options](../manager-options.md)
- [http::serve_tls](../../http/serve_tls.md): a server in one call
- [client](../client/README.md): the protocol by hand
- [net::acme](../README.md)
