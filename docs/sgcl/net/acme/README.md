
[sgcl](../../README.md) › [net](../README.md) › acme

# sgcl::net::acme

```cpp
#include "sgcl/net/acme.h"   // namespace sgcl::net::acme
```

ACME (RFC 8555), the protocol of Let's Encrypt and of every CA that issues certificates without a human: Go's
`golang.org/x/crypto/acme` and its `autocert` in one namespace. A [client](client/README.md) speaks the whole protocol to
one CA with one [account key](account_key/README.md): the directory, the replay nonces, accounts (with External
Account Binding, key rollover, deactivation), orders of DNS names, wildcards and IP addresses (RFC 8738), the three
challenges (http-01, dns-01, tls-alpn-01 of RFC 8737) with the values and the certificate each needs, finalization
with a CSR of [crypto::x509](../../crypto/x509.md), the chain and its alternates, revocation and the renewal
information of RFC 9773. A [manager](manager/README.md) does all of it by itself for a TLS server: given the names, its
[tls_config](manager/tls_config.md) obtains each name's certificate at the first handshake, keeps it in a cache
directory and renews it before it expires; a server is `net::http::serve_tls(":443", manager.tls_config(), handler)`.

The idea it rests on is that the protocol's objects are values and its two machines are handles. An
[order](order.md), an [authorization](authorization.md), a [challenge](challenge.md) are what the CA said when it was
asked, plain structs, never updated behind the program's back; the client and the manager are handles of one word,
their copies the same client, safe from many tasks at once. Every call that talks to the CA has the two forms of the
module ([The rules](../README.md#the-rules), 2), and every failure is an [io::error](../../io/error/README.md): a problem
document of the CA (RFC 7807) is one of the category `"acme"` ([errc](errc.md)), its type the code, its detail the
text.

A CA of the program's own, on the loopback, is [test_server](test_server/README.md): what Pebble is to Let's Encrypt,
written from the same RFC, which validates challenges for real at ports of the program's choosing, issues from a CA
it makes at its start and fails on demand. The examples of these pages run against it.

## The rules

1. **Two forms.** Every call of the [client](client/README.md) and the [manager](manager/README.md) that talks to the CA
   blocks the calling thread in its plain form, for a thread of the program, never a worker, and is awaited in its
   `async_` form in a task, which throws nothing.
2. **Errors are values.** A problem document of the CA is an `io::error` of the category `"acme"`: its code the
   problem's type ([errc](errc.md): `rate_limited`, `bad_csr`, `unauthorized`, …), its operation the request
   (`acme new-order`), its text the detail and every subproblem's with its identifier. The transport's failures stay
   what they are (`ECONNREFUSED`, the TLS errors); a response that breaks the RFC is `errc::malformed_response`; a wait
   past its time is `ETIMEDOUT`.
3. **The CA's pacing is honoured.** A badNonce is sent again with the nonce of its answer (5 times by default); a
   rate limit or a 503 whose Retry-After is within `client::options::max_retry_after` (a minute) is waited for and the
   request sent again; a longer one is the error, the wait in its text; polling goes at the CA's Retry-After, else at
   `poll_interval`. The manager keeps a failure for its backoff (a minute, doubling to a day) and asks for nothing
   meanwhile.
4. **The account's key never lies in managed memory**, nor does a certificate's: an [account_key](account_key/README.md)
   keeps its key in an unmanaged block, never copied; the manager writes keys to its cache through `secret_bytes`, at
   0600, in a directory of 0700.
5. **A handle holds a `tracked_ptr`**, so it lives where one may: on a stack, in a task, in a managed object; in a
   global or a `std` container, a [rooted](../../core/rooted/README.md) of it. A copy of a [client](client/README.md),
   a [manager](manager/README.md) or an [account_key](account_key/README.md) is the same one; a
   [test_server](test_server/README.md) is not copied, and stops when it is destroyed.
6. **Names as the CA wants them.** The client writes a name's identifier itself: an IP address as RFC 5952 writes it
   (type `ip`), else a DNS name in lower case, without its trailing dot, in A-labels (IDNA), a wildcard's `*.` kept;
   a name that cannot be one is `errc::rejected_identifier` before a request.
7. **Not in this version**: the pre-authorization of `newAuthz` (no CA in use offers it; the directory's URL is read),
   the challenges of other drafts (tls-sni, dns-account-01, onion-csr-01), a certificate of several names from the
   manager (it obtains one per name, as Go's autocert does).

## Functions

| Function | Header | Description |
|---|---|---|
| [category](category.md) | `error.h` | the error category of ACME, `"acme"` |
| [make_error_code](make_error_code.md) | `error.h` | an `errc` as an `error_code` |
| [to_string](to_string.md) | `types.h` | the name of a status, as the RFC writes it |

## Classes

| Class | Header | Description |
|---|---|---|
| [account](account.md) | `types.h` | an account as the CA has it: its URL, status, contact, terms (RFC 8555 §7.1.2) |
| [account_key](account_key/README.md) | `key.h` | the key an account signs its requests with, ES256, ES384, EdDSA or RS256; its JWK and thumbprint |
| [account_options](account_options.md) | `types.h` | what a new account is made with: the contact, the terms, an external account binding |
| [authorization](authorization.md) | `types.h` | an authorization of an identifier and its challenges (RFC 8555 §7.1.4) |
| [certificate_chain](certificate_chain.md) | `types.h` | a certificate the CA issued, its chain and the URLs of its alternate chains |
| [challenge](challenge.md) | `types.h` | a challenge of an authorization: its type, URL, status, token, error (RFC 8555 §7.1.5) |
| [client](client/README.md) | `client.h` | the protocol of RFC 8555 from the client's side, for one CA and one account key: Go's `acme.Client` |
| [client::options](client-options.md) | `client.h` | how a client talks to its CA: the HTTP client, the retries, the polling |
| [directory](directory.md) | `types.h` | the resources of a CA and what it says of itself (RFC 8555 §7.1.1) |
| [external_account](external_account.md) | `types.h` | the key id and the MAC key of an External Account Binding (RFC 8555 §7.3.4) |
| [identifier](identifier.md) | `types.h` | what a certificate is asked for: a DNS name or an IP address |
| [manager](manager/README.md) | `manager.h` | certificates obtained at the first handshake and renewed by themselves: Go's `autocert.Manager` |
| [manager::options](manager-options.md) | `manager.h` | where a manager's certificates come from and how: the CA, the cache, the challenges, the policy |
| [order](order.md) | `types.h` | an order of a certificate (RFC 8555 §7.1.3) |
| [order_options](order_options.md) | `types.h` | what a new order asks for besides its names: the validity, the certificate it replaces, a profile |
| [problem](problem/README.md) | `types.h` | a problem document (RFC 7807) of the CA |
| [renewal_info](renewal_info.md) | `types.h` | when the CA suggests a certificate be renewed (RFC 9773) |
| [subproblem](subproblem.md) | `types.h` | one error of a compound problem, of one identifier |
| [test_server](test_server/README.md) | `test_server.h` | an ACME server on the loopback for tests, with a CA of its own |
| [test_server::options](test_server-options.md) | `test_server.h` | what a test server is and how it validates |

## Enumerations

| Enumeration | Header | Description |
|---|---|---|
| [errc](errc.md) | `error.h` | the error types of RFC 8555 §6.7 and the client's own |
| [key_algorithm](key_algorithm.md) | `key.h` | the JWS algorithms of an account key |
| [revocation_reason](revocation_reason.md) | `types.h` | the reasons of a revocation (RFC 5280 §5.3.1) |
| [status](status.md) | `types.h` | the status of an account, an order, an authorization or a challenge |

## Objects and types

| Name | Header | Description |
|---|---|---|
| `lets_encrypt_url` | `manager.h` | `inline constexpr char lets_encrypt_url[]`: Let's Encrypt's production directory, `https://acme-v02.api.letsencrypt.org/directory` |
| `lets_encrypt_staging_url` | `manager.h` | `inline constexpr char lets_encrypt_staging_url[]`: Let's Encrypt's staging directory, `https://acme-staging-v02.api.letsencrypt.org/directory`, whose certificates no browser trusts, under rate limits far higher |

## See also

- [net::tls](../tls/README.md): the server whose [config](../tls/config.md) a manager feeds (`identity_for`)
- [net::http](../http/README.md): the client the requests go through, the server that serves http-01
- [crypto::x509](../../crypto/x509.md): the certificate requests and the certificates
- RFC 8555 (ACME), RFC 8737 (tls-alpn-01), RFC 8738 (IP identifiers), RFC 9773 (renewal information), RFC 7515,
  7518, 7638 and 8037 (JWS, its algorithms, JWK thumbprints, Ed25519)
- `tests/net/acme/`: the client and the manager against the test server, Go's `x/crypto/acme` against it, the
  vectors of RFC 7515, 7638 and 8037 and Wycheproof's JWS tests
- [net](../README.md)
