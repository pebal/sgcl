[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [public_key](README.md)

# sgcl::crypto::rsa::public_key::verify_pss

```cpp
[[nodiscard]] bool verify_pss(hash_id id, const slice<const byte>& message,                 // (1)
                          const slice<const byte>& signature) const;
[[nodiscard]] bool verify_pss(hash_id id, const slice<const byte>& message,                 // (2)
                          const slice<const byte>& signature, size_t salt_length) const;
```

Checks that `signature` is a PSS signature (RFC 8017 §8.1, JWS's PS256) of `message` under this key, the message
hashed here by the hash `id` names, MGF1 over the same hash: [verify_digest_pss](verify_digest_pss.md) of its
digest, what Go's `VerifyPSS` takes hashed. The message is bytes or a text; its digest is made on the stack.

1. The salt of any length, read from the signature, as Go's `PSSSaltLengthAuto`: what
   [sign_pss](../rsa-private_key/sign_pss.md) makes, a salt as long as the digest, and every other length verify.
2. The salt of exactly `salt_length` bytes: a signature with a salt of any other length is `false`, as is a length
   the key has no room for.

## Parameters

| Parameter | Description |
|---|---|
| `id` | the hash of the message, and the hash of MGF1 |
| `message` | the bytes or the text that was signed |
| `signature` | the signature, [size](size.md) bytes |
| `salt_length` | the salt's length in bytes |

## Return value

`true` when the signature is the key's over the message, `false` otherwise.

## Complexity

Linear in the size of the message for its digest, then as [verify_digest_pss](verify_digest_pss.md).

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

    string token = "{\"sub\":\"1234567890\"}";
    auto sig = key.sign_pss(crypto::hash_id::sha256, token);
    println("{}", pub.verify_pss(crypto::hash_id::sha256, token, sig));
    println("{}", pub.verify_pss(crypto::hash_id::sha256, token, sig, 32));
    println("{}", pub.verify_pss(crypto::hash_id::sha256, token, sig, 20));
    println("{}", pub.verify(crypto::hash_id::sha256, token, sig));
}
```

Output:

```text
true
true
false
false
```

## See also

- [private_key::sign_pss](../rsa-private_key/sign_pss.md): makes the signature
- [verify](verify.md): the PKCS #1 v1.5 signatures of a message
- [verify_digest_pss](verify_digest_pss.md): the signature of a digest made by the program
- [sgcl::crypto::rsa::public_key](README.md)
