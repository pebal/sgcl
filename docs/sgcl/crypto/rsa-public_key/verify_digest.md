[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [public_key](README.md)

# sgcl::crypto::rsa::public_key::verify_digest

```cpp
[[nodiscard]] bool verify_digest(hash_id id, const slice<const byte>& digest,
                                 const slice<const byte>& signature) const;
```

Checks that `signature` is the PKCS #1 v1.5 signature (RFC 8017 §8.2, JWS's RS256) of `digest`, made by the hash `id`
names, under this key: the signature is exactly [size](size.md) bytes and below n, and the message it opens to is,
byte for byte, the one encoding the digest has. The whole encoded block is compared, never parsed: no DigestInfo
without its NULL, no bytes after the digest, no short padding gets through (the lax parsers behind Bleichenbacher's
forgeries of 2006 and BERserk).

A digest of another length than `id`'s is `false` too: a verification never throws on data, since in X.509 the hash
comes with the data (the signature's AlgorithmIdentifier), and an exception there would be a certificate's way to
stop a program.

## Parameters

| Parameter | Description |
|---|---|
| `id` | the hash that made the digest |
| `digest` | the digest of the message |
| `signature` | the signature, [size](size.md) bytes |

## Return value

`true` when the signature is the key's over the digest, `false` otherwise.

## Complexity

Linear in the bits of the modulus times the bits of e: one exponentiation by e, 17 products for 65537.

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
    auto digest = crypto::sha256::of("release 1.0");
    auto sig = key.sign_digest(crypto::hash_id::sha256, digest);
    println("{}", pub.verify_digest(crypto::hash_id::sha256, digest, sig));
    println("{}", pub.verify_digest(crypto::hash_id::sha512, digest, sig));
    sig[0] ^= byte(1);
    println("{}", pub.verify_digest(crypto::hash_id::sha256, digest, sig));
}
```

Output:

```text
true
false
false
```

## See also

- [sign_digest](../rsa-private_key/sign_digest.md): makes the signature
- [verify_digest_pss](verify_digest_pss.md): the PSS signatures
- [verify](verify.md): a message, hashed inside
- [sgcl::crypto::rsa::public_key](README.md)
