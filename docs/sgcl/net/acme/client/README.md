[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md)

# sgcl::net::acme::client

```cpp
#include "sgcl/net/acme/client.h"   // or "sgcl/net/acme.h"

namespace sgcl::net::acme {
    class client;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::acme::client` is the protocol of ACME (RFC 8555) from the client's side, for one CA and one
[account key](../account_key/README.md): Go's `golang.org/x/crypto/acme.Client`. Every request is a JWS of the key —
signed with the account's URL once the account is known, with the key's JWK before it — sent with a fresh replay nonce,
and every answer is read as the RFC says: the objects as [plain values](../README.md), a problem document as an
[io::error](../../../io/error/README.md) of the category `"acme"`.

What a certificate takes is a few calls: [new_order](new_order.md) of the names; for each of its
[authorizations](authorization.md), a [challenge](../challenge.md) solved (the value served or published, made by
[key_authorization](key_authorization.md), [dns01_value](dns01_value.md) or [tls_alpn01_identity](tls_alpn01_identity.md))
and [accept](accept.md)ed, then [wait_authorization](wait_authorization.md); [wait_order](wait_order.md) until it is
ready; [finalize](finalize.md) with a CSR of the certificate's key; [certificate](certificate.md). A
[manager](../manager/README.md) does all of it by itself for a TLS server.

## Rules

- **A handle of one word.** Its copies are the same client: the directory read once, the account's URL, the nonces the
  responses gave; safe from many threads and tasks at once.
- **Two forms**, as everything that waits in the module: `acme.new_order(names)` blocks a thread of the program,
  `co_await acme.async_new_order(names)` a task. The blocking forms are for a thread of the program, never a worker.
- **The account is found by its key.** The URL comes from [register_account](register_account.md), from
  `options::account_url`, or is looked up by the key on the first request that needs it (a newAccount with
  `onlyReturnExisting`): `errc::account_does_not_exist` when the key has none.
- **The nonces**: each response's Replay-Nonce is kept (16 at most) for the next request; with none kept, a HEAD of
  newNonce. A `badNonce` is sent again with the nonce of its answer, up to `options::bad_nonce_retries` (5) times.
- **Retry-After is honoured**: a rate limit (`rateLimited`) or a 503 that says when to come back within
  `options::max_retry_after` (a minute) is waited for and the request sent again, three times at most; a longer one is
  the error, `(retry after N s)` in its text. Polling ([wait_order](wait_order.md), [wait_authorization](wait_authorization.md),
  [finalize](finalize.md)) goes at the CA's Retry-After, else every `options::poll_interval` (a second), and ends with
  `ETIMEDOUT` past `options::poll_timeout` (three minutes).
- **The URLs** the CA gives (Location, Link, the fields of the objects) are made absolute against the request's.
- **Every request goes through `options::http`**, an [http::client](../../http/client/README.md): its TLS roots, proxy
  and timeouts are the program's; a CA on `http://` (a test server) is spoken to as it is.

## Member types

| Type | Definition |
|---|---|
| [options](../client-options.md) | how the client talks to its CA |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](client.md) | a client of a CA's directory and an account key |

#### Observers

| Function | Description |
|---|---|
| [key](key.md) | the key requests are signed with |
| [account_url](account_url.md) | the account's URL, its kid |
| [directory_url](directory_url.md) | the URL of the CA's directory |

#### The directory and the account

| Function | Description |
|---|---|
| [directory, async_directory](directory.md) | the CA's directory, read once |
| [register_account, async_register_account](register_account.md) | a new account of the key, or the one it has |
| [account, async_account](account.md) | the account as the CA has it now |
| [update_account, async_update_account](update_account.md) | the account's contact replaced |
| [deactivate_account, async_deactivate_account](deactivate_account.md) | the account deactivated |
| [change_key, async_change_key](change_key.md) | the account's key rolled over |

#### Orders

| Function | Description |
|---|---|
| [new_order, async_new_order](new_order.md) | a new order of names |
| [order, async_order](order.md) | an order as it is now |
| [wait_order, async_wait_order](wait_order.md) | an order polled until it is ready or valid |
| [finalize, async_finalize](finalize.md) | an order finalized with a CSR, polled until valid |
| [certificate, async_certificate](certificate.md) | the chain of a certificate URL, with its alternates |

#### Authorizations and challenges

| Function | Description |
|---|---|
| [authorization, async_authorization](authorization.md) | an authorization as it is now |
| [wait_authorization, async_wait_authorization](wait_authorization.md) | an authorization polled until it is no longer pending |
| [deactivate_authorization, async_deactivate_authorization](deactivate_authorization.md) | an authorization deactivated |
| [challenge, async_challenge](challenge.md) | a challenge as it is now |
| [accept, async_accept](accept.md) | a challenge answered: the CA told to validate it |
| [key_authorization](key_authorization.md) | the key authorization of a token: what http-01 serves |
| [http01_path](http01_path.md) | where http-01 serves it (static) |
| [dns01_name](dns01_name.md) | the TXT record of dns-01 for a domain (static) |
| [dns01_value](dns01_value.md) | the value of dns-01's TXT record for a token |
| [tls_alpn01_identity](tls_alpn01_identity.md) | the certificate tls-alpn-01 serves for a name |

#### Revocation and renewal

| Function | Description |
|---|---|
| [revoke, async_revoke](revoke.md) | a certificate revoked, by the account or by its own key |
| [renewal_info, async_renewal_info](renewal_info.md) | when the CA suggests a certificate be renewed (RFC 9773) |
| [renewal_id](renewal_id.md) | the ARI identifier of a certificate (static) |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    // a CA on the loopback, which validates the challenge for real at the program's own http server
    net::http::server http;
    net::listener port80 = net::tcp::listen("127.0.0.1:0");
    net::acme::test_server ca({.http_port = port80.local_endpoint().port()});

    net::acme::client acme(ca.directory_url(), net::acme::account_key());
    acme.register_account({.contact = {"mailto:admin@example.com"}, .terms_agreed = true});
    net::acme::order o = acme.new_order({"example.com"});

    // http-01: the key authorization served at its path
    net::acme::authorization az = acme.authorization(o.authorizations[0]);
    net::acme::challenge ch;
    for (auto& c : az.challenges) {
        if (c.type == "http-01") {
            ch = c;
        }
    }
    string answer = acme.key_authorization(ch.token);
    http.route("GET " + net::acme::client::http01_path(ch.token),
               [answer](net::http::request, net::http::response_writer w) { w.write(answer); });
    auto serving = async::spawn(http.async_serve(port80));
    acme.accept(ch);
    println("{}", net::acme::to_string(acme.wait_authorization(az.url)->status));

    // the certificate's key, its CSR, the chain
    auto key = crypto::p256::private_key::generate();
    crypto::x509::certificate_request_template t;
    t.dns_names = {"example.com"};
    auto csr = crypto::x509::create_certificate_request(t, key);
    net::acme::order done = acme.finalize(acme.wait_order(o.url), csr);
    net::acme::certificate_chain chain = acme.certificate(done.certificate);
    println("{} until {}", chain.certificates[0].dns_names()[0],
            chain.certificates[0].not_after().unix() > time::now().unix());
    http.close();
    serving.wait();
}
```

Output:

```text
valid
example.com until true
```

## See also

- [manager](../manager/README.md): the same, by itself, for a TLS server
- [test_server](../test_server/README.md): the CA of the examples
- [account_key](../account_key/README.md), [options](../client-options.md)
- [net::acme](../README.md)
