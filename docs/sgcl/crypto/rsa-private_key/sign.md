[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [private_key](../rsa-private_key.md)

# sgcl::crypto::rsa::private_key::sign

```cpp
vector<byte> sign(hash_id id, const slice<const byte>& message) const;
```

Returns the PKCS #1 v1.5 signature (RFC 8017 §8.2.1) of `message`, hashed here by the hash `id` names:
[sign_digest](sign_digest.md) of its digest, what Go's `SignPKCS1v15` takes hashed. `sign(hash_id::sha256, m)` is
JWS's RS256. The message is bytes or a text; its digest is made on the stack. Deterministic, as the scheme is: the
same key and message give the same signature.

## Parameters

| Parameter | Description |
|---|---|
| `id` | the hash of the message |
| `message` | the bytes or the text to sign |

## Return value

The signature, [size](size.md) bytes.

## Complexity

Linear in the size of the message for its digest, then as [sign_digest](sign_digest.md): cubic in the bits of the
modulus.

## Exceptions

- `invalid_argument` when `id` is not a [hash_id](../hash_id.md) the module has, or the key is too small for the
  hash's encoding (never at 1024 bits and more).
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

    auto sig = key.sign(crypto::hash_id::sha256, "release 1.0");
    println("{}...", encoding::hex::encode(sig.as_slice().subslice(0, 16)));
    auto digest = crypto::sha256::of("release 1.0");
    println("{}", sig == key.sign_digest(crypto::hash_id::sha256, digest));
    println("{}", key.public_key().verify(crypto::hash_id::sha256, "release 1.0", sig));
}
```

Output:

```text
7848b25e0093759a5cc0ff74608cba5c...
true
true
```

## See also

- [public_key::verify](../rsa-public_key/verify.md): checks the signature
- [sign_pss](sign_pss.md): the PSS signature of a message
- [sign_digest](sign_digest.md): the signature of a digest made by the program
- [sgcl::crypto::rsa::private_key](../rsa-private_key.md)
