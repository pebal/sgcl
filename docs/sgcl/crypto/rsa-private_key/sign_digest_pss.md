[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [private_key](../rsa-private_key.md)

# sgcl::crypto::rsa::private_key::sign_digest_pss

```cpp
vector<byte> sign_digest_pss(hash_id id, const slice<const byte>& digest) const;
```

Returns a PSS signature (RFC 8017 §8.1.1) of the digest, JWS's PS256 with SHA-256 and what TLS 1.3 signs with: MGF1
over the digest's own hash, and a random salt as long as the digest (Go's `PSSSaltLengthEqualsHash`, what FIPS 186-5
allows at most), drawn from [crypto::random](../random.md): a new signature every time.

## Parameters

| Parameter | Description |
|---|---|
| `id` | the hash that made the digest, and the hash of MGF1 |
| `digest` | the digest of the message |

## Return value

The signature, [size](size.md) bytes.

## Complexity

Cubic in the bits of the modulus, as [sign_digest](sign_digest.md).

## Exceptions

- `invalid_argument` when the digest is not of the length of `id`'s hash, `id` is not a [hash_id](../hash_id.md) the
  module has, or the key is too small for the hash and the salt (a 1024-bit key with SHA-512).
- `logic_error` when the key was moved from.
- `runtime_error` when the signature does not verify with the public key: a fault in the computation.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto pem = crypto::read_secret("tests/net/tls_testdata/rsa.key");  // the tree's test key
    crypto::rsa::private_key key = crypto::rsa::private_key::from_pem(pem);

    auto digest = crypto::sha256::of("{\"sub\":\"1234567890\"}");
    auto a = key.sign_digest_pss(crypto::hash_id::sha256, digest);
    auto b = key.sign_digest_pss(crypto::hash_id::sha256, digest);
    println("{} bytes, the same twice: {}", a.size(), a == b);
    crypto::rsa::public_key pub = key.public_key();
    println("{}", pub.verify_digest_pss(crypto::hash_id::sha256, digest, a));
    println("{}", pub.verify_digest_pss(crypto::hash_id::sha256, digest, b));
}
```

Output:

```text
256 bytes, the same twice: false
true
true
```

## See also

- [public_key::verify_digest_pss](../rsa-public_key/verify_digest_pss.md): checks the signature
- [sign_digest](sign_digest.md): the deterministic signature of PKCS #1 v1.5
- [sign_pss](sign_pss.md): a message, hashed inside
- [sgcl::crypto::rsa::private_key](../rsa-private_key.md)
