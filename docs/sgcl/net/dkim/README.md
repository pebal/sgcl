[sgcl](../../README.md) › [net](../README.md) › dkim

# sgcl::net::dkim

```cpp
#include "sgcl/net/dkim.h"   // namespace sgcl::net::dkim; or "sgcl/net/smtp.h"
```

DomainKeys Identified Mail (RFC 6376): a domain's signature over a message's head and body, made with a private key
whose public half the domain publishes in DNS, at `<selector>._domainkey.<domain>`, and checked by whoever receives
the message. Go has no DKIM in its standard library nor in `golang.org/x`; Python leaves it to `dkimpy`. A
[signer](signer/README.md) holds the domain, the selector and the key; its [sign](signer/sign.md) puts a `DKIM-Signature` field
at the head of a message, [verify](verify.md) checks every signature a message carries and gives a
[result](result.md) for each. An [smtp::client](../smtp/client/README.md) signs everything it sends when its
[options](../smtp/options.md) name a signer, and an [smtp::server](../smtp/server/README.md) verifies what it takes
when its `sender_checks` is set.

Both algorithms of today: `rsa-sha256` and `ed25519-sha256` (RFC 8463), both canonicalizations, `simple` and
`relaxed`, of the head and of the body, a body length (`l=`), an identity (`i=`), an expiration (`x=`). `rsa-sha1`
and RSA keys under 1024 bits are refused as RFC 8301 asks.

## The rules

1. DKIM signs bytes, not a structure: [sign](signer/sign.md) and [verify](verify.md) take the message's text as it is sent,
   its line breaks CRLF (a bare LF is taken as CRLF). A message written again after it was signed is other bytes.
2. A verification never fails: what went wrong is the [status](status.md) and the reason of its result — `fail` for
   a message changed, `permerror` for a signature or a key that cannot be used, `temperror` for a DNS failure.
3. The keys are asked of DNS through the module's own resolver ([dns::options](../dns-options.md); its servers empty:
   `/etc/resolv.conf`'s), with no cache.
4. A [signer](signer/README.md) is a handle of one word; its key lives outside the managed heap and is never copied.

## Functions

| Function | Header | Description |
|---|---|---|
| [to_string](to_string.md) | `dkim.h` | a status as Authentication-Results writes it |
| [verify, async_verify](verify.md) | `dkim.h` | a result per signature of a message |

## Classes

| Class | Header | Description |
|---|---|---|
| [result](result.md) | `dkim.h` | one signature's verdict: the status, the domain, the selector, why |
| [sign_options](sign_options.md) | `dkim.h` | how a message is signed: the canonicalizations, the fields, `i=`, `l=`, `t=`, `x=` |
| [signer](signer/README.md) | `dkim.h` | a signing identity: the domain, the selector, the private key |
| [verify_options](verify_options.md) | `dkim.h` | how signatures are checked: the resolver, how many, the time |

## Enumerations

| Enumeration | Header | Description |
|---|---|---|
| [algorithm](algorithm.md) | `dkim.h` | `rsa_sha256`, `ed25519_sha256` |
| [canonicalization](canonicalization.md) | `dkim.h` | `simple`, `relaxed` |
| [status](status.md) | `dkim.h` | a signature's verdict as RFC 8601 names it |

## See also

- [spf](../spf/README.md), [dmarc](../dmarc/README.md): the other checks of a message's origin
- [smtp::check_sender](../smtp/check_sender.md): the three checks of a message at once
- [Benchmarks](../benchmarks.md)
- RFC 6376, RFC 8463, RFC 8301; `tests/net/mail/dkim.cpp` (RFC 8463's message, a verifier written apart in Python
  with OpenSSL)
