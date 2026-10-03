[sgcl](../README.md) › [crypto](README.md) › [rsa](rsa.md)

# sgcl::crypto::rsa::private_key

```cpp
#include "sgcl/crypto/rsa.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::rsa {
    using private_key = /* unspecified */;
}
```

`sgcl::crypto::rsa::private_key` is an RSA private key: its [public_key](rsa-public_key.md) and the secret numbers d,
p, q, dP, dQ and qInv. It signs digests in PKCS #1 v1.5 and in PSS and decrypts what OAEP encrypted for it. A key is
made by [generate](rsa-private_key/generate.md), or read from PKCS #1, PKCS #8 or PEM, checked whole when it is read.

Go's `rsa.PrivateKey` is a struct whose numbers are `big.Int`s any code may read, copy and change, kept in memory
that nothing zeroes. Here the numbers are in one block of words of the key's own, out of the managed heap, never
given out but as an encoding in a [secret_bytes](secret_bytes.md), and zeroed when the key goes.

## Rules

- **Move-only.** A copy of a secret is made by name, [clone](rsa-private_key/clone.md); a move leaves the source
  empty, and every call on an empty key but the assignment is `logic_error`, `bits` and `size` included. Keep a key
  on the stack or in a `unique_ptr`: it holds no `tracked_ptr` and may live anywhere, its numbers in memory of its
  own.
- **Zeroed when it goes.** The block of the numbers, and every word of scratch an operation uses, is zeroed with
  stores the compiler cannot drop. [to_pkcs1_der](rsa-private_key/to_pkcs1_der.md),
  [to_pkcs8_der](rsa-private_key/to_pkcs8_der.md) and [to_pem](rsa-private_key/to_pem.md) give the secret in a
  [secret_bytes](secret_bytes.md); what [decrypt_oaep](rsa-private_key/decrypt_oaep.md) returns is the user's data,
  a `vector<byte>`, and a key unwrapped goes to [decrypt_oaep_to](rsa-private_key/decrypt_oaep_to.md) instead.
- **Constant time** for every secret, **blinded**, and **checked** with the public exponent before a result leaves
  ([The rules](rsa.md#rules) of rsa).
- **Reading keys never throws**: a key that cannot be read is an [expected](../core/expected.md) with a
  [crypto::error](error.md). Signing throws for the program's own mistakes: a digest of another length than its
  hash's, an unknown [hash_id](hash_id.md).
- **Reading from many threads at once is safe**: no member changes the key but the assignment; each operation draws
  its own blinding.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](rsa-private_key/rsa-private_key.md) | moves a key; a copy is deleted |
| `(destructor)` | zeroes the numbers and frees them |
| [operator=](rsa-private_key/operator_assign.md) | moves another key in |

#### Making and reading

| Function | Description |
|---|---|
| [generate](rsa-private_key/generate.md) | a new key of a number of bits (static) |
| [from_pkcs1_der](rsa-private_key/from_pkcs1_der.md) | a key from PKCS #1's RSAPrivateKey (static) |
| [from_pkcs8_der](rsa-private_key/from_pkcs8_der.md) | a key from a PKCS #8 PrivateKeyInfo (static) |
| [from_pem](rsa-private_key/from_pem.md) | a key from PEM text (static) |
| [clone](rsa-private_key/clone.md) | a second key with the same numbers |

#### Observers

| Function | Description |
|---|---|
| [public_key](rsa-private_key/public_key.md) | the public key |
| [bits](rsa-private_key/bits.md) | the modulus's bits |
| [size](rsa-private_key/size.md) | the modulus's bytes: the length of a signature |

#### Signing

| Function | Description |
|---|---|
| [sign](rsa-private_key/sign.md) | the PKCS #1 v1.5 signature of a message, hashed inside |
| [sign_pss](rsa-private_key/sign_pss.md) | a PSS signature of a message, hashed inside |
| [sign_digest](rsa-private_key/sign_digest.md) | the PKCS #1 v1.5 signature of a digest |
| [sign_digest_pss](rsa-private_key/sign_digest_pss.md) | a PSS signature of a digest |

#### Decryption

| Function | Description |
|---|---|
| [decrypt_oaep](rsa-private_key/decrypt_oaep.md) | the message of an OAEP ciphertext |
| [decrypt_oaep_to](rsa-private_key/decrypt_oaep_to.md) | the message into the program's buffer |

#### Encoding

| Function | Description |
|---|---|
| [to_pkcs1_der](rsa-private_key/to_pkcs1_der.md) | PKCS #1's RSAPrivateKey |
| [to_pkcs8_der](rsa-private_key/to_pkcs8_der.md) | the PKCS #8 PrivateKeyInfo |
| [to_pem](rsa-private_key/to_pem.md) | the PEM of the PKCS #8 |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // a key file: its bytes straight into a secret_bytes
    auto pem = crypto::read_secret("tests/net/tls_testdata/rsa.key");
    auto key = crypto::rsa::private_key::from_pem(pem);
    if (!key) {
        eprintln(key.error().message());
        return 1;
    }
    println("{} bits", key->bits());

    auto digest = crypto::sha256::of("release 1.0");
    auto sig = key->sign_digest(crypto::hash_id::sha256, digest);
    println("{}", key->public_key().verify_digest(crypto::hash_id::sha256, digest, sig));
}
```

Output:

```text
2048 bits
true
```

## See also

- [public_key](rsa-public_key.md): what verifies and encrypts
- [read_secret](read_secret.md): a key file read without passing through managed memory
- [secret_bytes](secret_bytes.md): what the encodings of a key are
- [sgcl::crypto::rsa](rsa.md)
