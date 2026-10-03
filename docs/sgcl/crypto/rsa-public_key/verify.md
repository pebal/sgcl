[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [public_key](../rsa-public_key.md)

# sgcl::crypto::rsa::public_key::verify

```cpp
[[nodiscard]] bool verify(hash_id id, const slice<const byte>& message,
                          const slice<const byte>& signature) const;
```

Checks that `signature` is the PKCS #1 v1.5 signature (RFC 8017 §8.2, JWS's RS256) of `message` under this key,
the message hashed here by the hash `id` names: [verify_digest](verify_digest.md) of its digest, what Go's
`VerifyPKCS1v15` takes hashed. The message is bytes or a text; its digest is made on the stack. A signature of another
length than the modulus's, one not below n, one of another message or hash is `false`: a verification never throws
on data.

## Parameters

| Parameter | Description |
|---|---|
| `id` | the hash of the message |
| `message` | the bytes or the text that was signed |
| `signature` | the signature, [size](size.md) bytes |

## Return value

`true` when the signature is the key's over the message, `false` otherwise.

## Complexity

Linear in the size of the message for its digest, then as [verify_digest](verify_digest.md).

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

    auto sig = key.sign(crypto::hash_id::sha256, "release 1.0");
    println("{}", pub.verify(crypto::hash_id::sha256, "release 1.0", sig));
    println("{}", pub.verify(crypto::hash_id::sha256, "release 1.1", sig));
    println("{}", pub.verify(crypto::hash_id::sha384, "release 1.0", sig));
}
```

Output:

```text
true
false
false
```

## See also

- [private_key::sign](../rsa-private_key/sign.md): makes the signature
- [verify_pss](verify_pss.md): the PSS signatures of a message
- [verify_digest](verify_digest.md): the signature of a digest made by the program
- [sgcl::crypto::rsa::public_key](../rsa-public_key.md)
