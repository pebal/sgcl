# sgcl::crypto::secure_zero

```cpp
#include "sgcl/crypto/secure_zero.h"   // or "sgcl/crypto/crypto.h"

namespace sgcl::crypto {
    void secure_zero(const slice<byte>& bytes) noexcept;
}
```

Zeros over a buffer that held a secret — a key, a password, what a key derivation gave — that the compiler cannot remove. A `memset` of memory nobody reads afterwards is a dead store, and optimizers drop it (the buffer is about to be freed, the object to die), which leaves the secret in memory for whoever reads it next: a core dump, a swap file, a bug that reads past a buffer. `secure_zero` writes the zeros and then passes the pointer to an empty asm that clobbers memory, so the compiler must assume they are read (what BoringSSL's `OPENSSL_cleanse` and glibc's `explicit_bzero` do); a compiler with no GNU asm gets a loop of stores through a volatile pointer.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **The module's secret types do it themselves**: [`hmac`](hmac.md), [`hkdf::prk`](hkdf.md), the ciphers ([`aes`](aes.md), [`aes_gcm`](aes_gcm.md), [`aes_ctr`](aes_ctr.md), [`chacha20`](chacha20.md), [`chacha20_poly1305`](chacha20_poly1305.md)) and the private keys ([`x25519`](x25519.md), [`ed25519`](ed25519.md)) zero their memory in their destructors and when moved from, and [`pbkdf2`](pbkdf2.md) and `hkdf` zero their intermediate blocks. What the module derives, decrypts or draws for a key is a [`secret_bytes`](secret.md#secret_bytes), which zeroes itself. `secure_zero` is for the program's own buffers: a stack array, a `std::array`, a buffer given to a `_to` form.
- **What it cannot reach**: copies the compiler made in registers and on the stack, a value returned by copy (a digest, a tag), a buffer a `vector` left behind when it grew, and a managed buffer after the program dropped it (it stays until the collector reuses its space). Keep secrets in few places and in memory of fixed address (the stack, a `unique_ptr`), fill them with `random::fill` or `derive_to`, and clear them when done.

## Example

```cpp
#include "sgcl/crypto/pbkdf2.h"
#include "sgcl/crypto/secure_zero.h"

#include <array>

using namespace sgcl;

void open_vault(const string& password, const slice<const byte>& salt) {
    std::array<byte, 32> key;
    crypto::pbkdf2<crypto::sha256>::derive_to(key, password, salt, 600000);
    // ... decrypt with the key ...
    crypto::secure_zero(key);
}
```

## See also

[`random`](random.md), [`constant_time`](constant_time.md), [`hkdf`](hkdf.md), [`pbkdf2`](pbkdf2.md).
