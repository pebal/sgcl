[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md)

# sgcl::crypto::rsa::public_key

```cpp
#include "sgcl/crypto/rsa.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::rsa {
    using public_key = /* unspecified */;
}
```

`sgcl::crypto::rsa::public_key` is an RSA public key: the modulus n and the public exponent e. It verifies the
signatures a [private_key](../rsa-private_key/README.md) makes, in PKCS #1 v1.5 and in PSS, and encrypts with OAEP for the
holder of that key. It is made from an encoding (PKCS #1, SubjectPublicKeyInfo), from the modulus and the exponent
(a JWK's `n` and `e`), by [private_key::public_key](../rsa-private_key/public_key.md), or read from a certificate
([x509::public_key](../x509-public_key/README.md)), and checked when it is made: n odd, of 1024 to 16384 bits, e odd, 3 to
2³¹ − 1.

Go's `rsa.PublicKey` is a struct of `N` and `E` that any code may change, checked again at each use; here the numbers
are read through [modulus](modulus.md) and [exponent](exponent.md), and a key that
exists is a valid one.

## Rules

- **A plain value.** The modulus and its Montgomery constants are in memory of the key's own, outside the managed
  heap, and nothing in it is a `tracked_ptr`: a key lives anywhere, on a stack, in a `std` container, in a global,
  and is copied and compared freely. Nothing in it is secret.
- **Made by a factory.** There is no default constructor: `from_modulus`, `from_pkcs1_der` and `from_pkix_der`
  return an [expected](../../core/expected/README.md) whose error is a [crypto::error](../error/README.md). A key moved from is empty,
  and every call on it but the assignment is `logic_error`, `bits`, `size`, `exponent` and `==` included.
- **Never an exception from data.** A verification answers `false` for anything a signature or a digest can be; an
  encryption throws `invalid_argument` only for the program's own mistakes (a message too long for the key, an
  unknown [hash_id](../hash_id.md)).
- **Reading from many threads at once is safe**: no member changes the key but the assignment.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](rsa-public_key.md) | copies or moves a key |
| [operator=](operator_assign.md) | assigns another key |

#### Reading

| Function | Description |
|---|---|
| [from_modulus](from_modulus.md) | a key from its modulus and exponent (static) |
| [from_pkcs1_der](from_pkcs1_der.md) | a key from PKCS #1's RSAPublicKey (static) |
| [from_pkix_der](from_pkix_der.md) | a key from a SubjectPublicKeyInfo (static) |

#### Observers

| Function | Description |
|---|---|
| [bits](bits.md) | the modulus's bits |
| [size](size.md) | the modulus's bytes: the length of a signature and of a ciphertext |
| [modulus](modulus.md) | n, big-endian |
| [exponent](exponent.md) | e |

#### Encoding

| Function | Description |
|---|---|
| [to_pkcs1_der](to_pkcs1_der.md) | PKCS #1's RSAPublicKey |
| [to_pkix_der](to_pkix_der.md) | the SubjectPublicKeyInfo |

#### Verification

| Function | Description |
|---|---|
| [verify](verify.md) | checks a PKCS #1 v1.5 signature of a message, hashed inside |
| [verify_pss](verify_pss.md) | checks a PSS signature of a message, hashed inside |
| [verify_digest](verify_digest.md) | checks a PKCS #1 v1.5 signature of a digest |
| [verify_digest_pss](verify_digest_pss.md) | checks a PSS signature of a digest |

#### Encryption

| Function | Description |
|---|---|
| [encrypt_oaep](encrypt_oaep.md) | encrypts a message with OAEP |
| [max_oaep_message_size](max_oaep_message_size.md) | the longest message OAEP takes under a hash |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | compare the modulus and the exponent |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto pem = crypto::read_secret("tests/net/tls_testdata/rsa.key");  // the tree's test key
    crypto::rsa::private_key key = crypto::rsa::private_key::from_pem(pem);

    // what the signer publishes: its public key as a SubjectPublicKeyInfo
    auto published = key.public_key().to_pkix_der();
    auto digest = crypto::sha256::of("release 1.0");
    auto sig = key.sign_digest(crypto::hash_id::sha256, digest);

    crypto::rsa::public_key pub = crypto::rsa::public_key::from_pkix_der(published);
    println("{} bits, e = {}", pub.bits(), pub.exponent());
    println("{}", pub.verify_digest(crypto::hash_id::sha256, digest, sig));
    auto other = crypto::sha256::of("release 1.1");
    println("{}", pub.verify_digest(crypto::hash_id::sha256, other, sig));
}
```

Output:

```text
2048 bits, e = 65537
true
false
```

## See also

- [private_key](../rsa-private_key/README.md): the key that signs and decrypts
- [x509::public_key](../x509-public_key/README.md): the key of a certificate
- [encoding::pem](../../encoding/pem/README.md): a public key's PEM
- [sgcl::crypto::rsa](../rsa.md)
