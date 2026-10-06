[sgcl](../../README.md) › [net](../README.md) › [dkim](README.md)

# sgcl::net::dkim::verify, async_verify

```cpp
vector<result> verify(const string& message, const verify_options& o = {});
async::task<vector<result>> async_verify(string message, verify_options o = {}) noexcept;
```

A [result](result.md) for each `DKIM-Signature` field of the message, in the head's order (the first
`max_signatures` of them): the signature's tags read and checked (RFC 6376 §6.1.1), its key asked of DNS at
`<s>._domainkey.<d>` and read (§6.1.2), the body hashed again and compared with `bh=` (§6.1.3), the fields `h=`
names hashed with the signature's own field and the signature verified under the key. A message without a
signature gives an empty vector.

A verification never fails: a message changed after it was signed is `fail` ("body hash did not verify",
"signature did not verify"); a signature or a key that cannot be used is `permerror` (a missing tag, `rsa-sha1`, an
`i=` outside `d=`, an expired `x=`, no key, a revoked key, a key of another algorithm, an RSA key under 1024 bits);
a DNS failure is `temperror`.

`verify` waits on the calling thread; a task awaits `async_verify`.

## Parameters

| Parameter | Description |
|---|---|
| `message` | the message's text as it came, its line breaks CRLF (a bare LF taken as CRLF) |
| `o` | the resolver of the keys, how many signatures, the time `x=` is compared against |

## Return value

The results, one per signature checked.

## Complexity

Linear in the message for each signature, a DNS lookup and a signature's verification each.

## Exceptions

- `verify`: `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- `async_verify`: none.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    auto key = crypto::ed25519::private_key::from_seed(crypto::sha256::of("the example's key"));
    net::dkim::signer s("example.com", "s2026", key->to_pem());
    string m = "From: alice@example.com\r\nSubject: Hi\r\n\r\nHello.\r\n";
    println("{}", net::dkim::verify(m).size());
    for (auto& r : net::dkim::verify(s.sign(m).value())) {
        println("{} {} {}: {}", r.domain, r.selector, net::dkim::to_string(r.status), r.reason);
    }
}
```

Sample output:

```text
0
example.com s2026 permerror: no key for signature
```

## See also

- [sign](signer/sign.md)
- [result](result.md), [verify_options](verify_options.md)
- [smtp::check_sender](../smtp/check_sender.md): SPF, DKIM and DMARC of a message at once
- [dkim](README.md)
