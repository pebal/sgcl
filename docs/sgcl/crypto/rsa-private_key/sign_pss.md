[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [private_key](../rsa-private_key.md)

# sgcl::crypto::rsa::private_key::sign_pss

```cpp
vector<byte> sign_pss(hash_id id, const slice<const byte>& message) const;
```

Returns a PSS signature (RFC 8017 §8.1.1) of `message`, hashed here by the hash `id` names:
[sign_digest_pss](sign_digest_pss.md) of its digest, what Go's `SignPSS` takes hashed. `sign_pss(hash_id::sha256, m)`
is JWS's PS256. MGF1 over the same hash, a random salt as long as the digest: a new signature every time. The message
is bytes or a text; its digest is made on the stack.

## Parameters

| Parameter | Description |
|---|---|
| `id` | the hash of the message, and the hash of MGF1 |
| `message` | the bytes or the text to sign |

## Return value

The signature, [size](size.md) bytes.

## Complexity

Linear in the size of the message for its digest, then as [sign_digest_pss](sign_digest_pss.md): cubic in the bits
of the modulus.

## Exceptions

- `invalid_argument` when `id` is not a [hash_id](../hash_id.md) the module has, or the key is too small for the hash
  and the salt (a 1024-bit key with SHA-512).
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
    crypto::rsa::public_key pub = key.public_key();

    string token = "{\"sub\":\"1234567890\"}";
    auto sig = key.sign_pss(crypto::hash_id::sha256, token);
    println("{} {}", sig.size(), sig == key.sign_pss(crypto::hash_id::sha256, token));
    println("{}", pub.verify_pss(crypto::hash_id::sha256, token, sig));
}
```

Output:

```text
256 false
true
```

## See also

- [public_key::verify_pss](../rsa-public_key/verify_pss.md): checks the signature
- [sign](sign.md): the PKCS #1 v1.5 signature of a message
- [sign_digest_pss](sign_digest_pss.md): the signature of a digest made by the program
- [sgcl::crypto::rsa::private_key](../rsa-private_key.md)
