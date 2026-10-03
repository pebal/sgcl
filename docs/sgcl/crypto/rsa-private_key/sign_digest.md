[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [private_key](../rsa-private_key.md)

# sgcl::crypto::rsa::private_key::sign_digest

```cpp
vector<byte> sign_digest(hash_id id, const slice<const byte>& digest) const;
```

Returns the PKCS #1 v1.5 signature (RFC 8017 §8.2.1) of a digest the program made by the hash `id` names:
`sign_digest(hash_id::sha256, sha256::of(message))` is JWS's RS256. Deterministic, as the scheme is: the same key and
digest give the same signature. The private operation is blinded, CRT over p and q, and checked with the public
exponent before the signature leaves ([The rules](../rsa.md#rules) of rsa).

## Parameters

| Parameter | Description |
|---|---|
| `id` | the hash that made the digest |
| `digest` | the digest of the message |

## Return value

The signature, [size](size.md) bytes.

## Complexity

Cubic in the bits of the modulus: two exponentiations by numbers of half its length.

## Exceptions

- `invalid_argument` when the digest is not of the length of `id`'s hash, `id` is not a [hash_id](../hash_id.md) the
  module has, or the key is too small for the hash's encoding (never at 1024 bits and more).
- `logic_error` when the key was moved from.
- `runtime_error` when the signature does not verify with the public key: a fault in the computation, the signature
  withheld.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto pem = crypto::read_secret("tests/net/tls_testdata/rsa.key");  // the tree's test key
    crypto::rsa::private_key key = crypto::rsa::private_key::from_pem(pem);

    auto digest = crypto::sha256::of("release 1.0");
    auto sig = key.sign_digest(crypto::hash_id::sha256, digest);
    println("{}...", encoding::hex::encode(sig.as_slice().subslice(0, 16)));
    println("{}", sig == key.sign_digest(crypto::hash_id::sha256, digest));
    try {
        key.sign_digest(crypto::hash_id::sha512, digest);
    } catch (const invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
7848b25e0093759a5cc0ff74608cba5c...
true
sgcl::crypto::rsa: the digest is not of the hash's length
```

## See also

- [public_key::verify_digest](../rsa-public_key/verify_digest.md): checks the signature
- [sign_digest_pss](sign_digest_pss.md): the PSS signature
- [sign](sign.md): a message, hashed inside
- [sgcl::crypto::rsa::private_key](../rsa-private_key.md)
