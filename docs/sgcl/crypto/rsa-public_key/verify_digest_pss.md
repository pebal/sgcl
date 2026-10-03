[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [public_key](README.md)

# sgcl::crypto::rsa::public_key::verify_digest_pss

```cpp
[[nodiscard]] bool verify_digest_pss(hash_id id, const slice<const byte>& digest,    // (1)
                                 const slice<const byte>& signature) const;
[[nodiscard]] bool verify_digest_pss(hash_id id, const slice<const byte>& digest,    // (2)
                                 const slice<const byte>& signature,
                                 size_t salt_length) const;
```

Checks that `signature` is a PSS signature (RFC 8017 §8.1, JWS's PS256) of `digest` under this key, MGF1 over the same
hash as the digest. A digest of another length than `id`'s is `false`, as for
[verify_digest](verify_digest.md).

1. The salt of any length, read from the signature, as Go's `PSSSaltLengthAuto`: what
   [sign_digest_pss](../rsa-private_key/sign_digest_pss.md) makes, a salt as long as the digest, and every other
   length verify.
2. The salt of exactly `salt_length` bytes (RFC 8017 §9.1.2 as written; Go's `PSSSaltLengthEqualsHash` when it is
   the digest's length): a signature with a salt of any other length is `false`, as is a length the key has no
   room for. What a certificate's RSASSA-PSS parameters ask, and what [x509](../x509.md) verifies with.

## Parameters

| Parameter | Description |
|---|---|
| `id` | the hash that made the digest, and the hash of MGF1 |
| `digest` | the digest of the message |
| `signature` | the signature, [size](size.md) bytes |
| `salt_length` | the salt's length in bytes |

## Return value

`true` when the signature is the key's over the digest, `false` otherwise.

## Complexity

Linear in the bits of the modulus times the bits of e, as [verify_digest](verify_digest.md).

## Exceptions

- `invalid_argument` when `id` is not a [hash_id](../hash_id.md) the module has.
- `logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto pem = crypto::read_secret("tests/net/tls_testdata/rsa.key");  // the tree's test key
    crypto::rsa::private_key key = crypto::rsa::private_key::from_pem(pem);

    crypto::rsa::public_key pub = key.public_key();
    auto digest = crypto::sha256::of("{\"sub\":\"1234567890\"}");
    auto sig = key.sign_digest_pss(crypto::hash_id::sha256, digest);
    println("{}", pub.verify_digest_pss(crypto::hash_id::sha256, digest, sig));
    println("{}", pub.verify_digest_pss(crypto::hash_id::sha256, digest, sig, 32));
    println("{}", pub.verify_digest_pss(crypto::hash_id::sha256, digest, sig, 20));
}
```

Output:

```text
true
true
false
```

## See also

- [sign_digest_pss](../rsa-private_key/sign_digest_pss.md): makes the signature
- [verify_digest](verify_digest.md): the PKCS #1 v1.5 signatures
- [verify_pss](verify_pss.md): a message, hashed inside
- [sgcl::crypto::rsa::public_key](README.md)
