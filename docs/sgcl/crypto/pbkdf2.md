# sgcl::crypto::pbkdf2

```cpp
#include "sgcl/crypto/pbkdf2.h"   // or "sgcl/crypto/crypto.h"

namespace sgcl::crypto {
    template<class H> class pbkdf2;      // PBKDF2 (RFC 8018 §5.2) with HMAC over a digest of the module
}
```

PBKDF2, Go's `crypto/pbkdf2`: a key from a password, made slow on purpose — `iterations` rounds of HMAC per block of output — so that every guess of an attacker who has the salt and the result costs what the derivation cost. It is for deriving a key from a password where a format or a protocol names PBKDF2: PKCS #5 and PKCS #12 files, WPA2, a password manager's vault, an encrypted archive. Written from RFC 8018, tested against RFC 6070 (HMAC-SHA-1) and RFC 7914 §11 (HMAC-SHA-256) and against OpenSSL.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **For deriving keys, not for storing passwords.** A server that keeps password hashes to check logins wants a memory-hard function, which makes an attacker's specialised hardware no faster than the server: Argon2id or scrypt, which the module will have after version 1. PBKDF2 costs time only, and GPUs have plenty of it.
- **The iterations** are the work factor: OWASP names 600 000 for HMAC-SHA-256 and 210 000 for HMAC-SHA-512 (2023). Zero is `std::invalid_argument` (RFC 8018 asks for at least one); so is an output of more than 2^32 − 1 blocks of the digest's size (RFC 8018 §5.2).
- **The salt** is random, at least 16 bytes ([`random::bytes(16)`](random.md)), stored beside the result: the same password with two salts gives two unrelated keys, and no table of precomputed guesses serves two users.
- **The password is used as the HMAC's key**, bytes or text as given; a password longer than the digest's block is hashed first, as HMAC does. The two keyed states are made once, and every round costs two runs of the digest's compression.
- **What `derive` gives is a `vector<byte>`**, a managed buffer that stays in memory until the collector reuses its space; `derive_to` writes into a buffer of the caller's for a key that must not linger, and [`secure_zero`](secure_zero.md) clears it when done. The keyed states and every intermediate block are zeroed before the call returns.

## Members

```cpp
static secret_bytes derive(const slice<const byte>& password, const slice<const byte>& salt, uint32_t iterations, size_t n);   // a secret_bytes (secret.md)
static void derive_to(const slice<byte>& out, const slice<const byte>& password, const slice<const byte>& salt, uint32_t iterations);
```

The password and the salt take bytes or text, which a `slice<const byte>` takes both: a `string`, a literal, a `vector<byte>`.

## Example

```cpp
#include "sgcl/crypto/crypto.h"
#include "sgcl/encoding/encoding.h"
#include "sgcl/io/io.h"

using namespace sgcl;

int main() {
    // a vault's key: the salt is stored with the vault, the password is not
    auto key = crypto::pbkdf2<crypto::sha256>::derive("correct horse battery staple",
                                                      "salt of this vault", 600000, 32);
    println(encoding::hex::encode(key));
}
```

Output:

```text
00a51b9f93bbc93ad36b33ad019b27fffc5c39603f1ddb32ae000e0bf2807a2c
```

## See also

[`hmac`](hmac.md), which it is made of; [`hkdf`](hkdf.md) for input that is already a secret; [`random`](random.md) for the salt.
