[sgcl](../README.md) › [crypto](README.md)

# sgcl::crypto::secure_zero

```cpp
#include "sgcl/crypto/secure_zero.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    void secure_zero(const slice<byte>& bytes) noexcept;
}
```

Writes zeros over a buffer that held a secret — a key, a password, what a key derivation gave — that the compiler
cannot remove. A `memset` of memory nobody reads afterwards is a dead store, and optimizers drop it (the buffer is
about to be freed, the object to die), which leaves the secret in memory for whoever reads it next: a core dump, a
swap file, a bug that reads past a buffer. `secure_zero` writes the zeros and then passes the pointer to an empty asm
that clobbers memory, so the compiler must assume they are read, which is what BoringSSL's `OPENSSL_cleanse` and
glibc's `explicit_bzero` do; a compiler with no GNU asm gets a loop of stores through a volatile pointer.

The module's types that hold a secret do it themselves: [hmac](hmac/README.md), [hkdf-prk](hkdf-prk/README.md), the ciphers
([aes](aes/README.md), [aes_gcm](aes_gcm/README.md), [aes_ctr](aes_ctr/README.md), [chacha20](chacha20/README.md),
[chacha20_poly1305](chacha20_poly1305/README.md)) and the private keys ([x25519](x25519-private_key/README.md),
[ed25519](ed25519-private_key/README.md)) zero their memory in their destructors and when moved from, and
[pbkdf2](pbkdf2/README.md) and [hkdf](hkdf/README.md) zero their intermediate blocks. What the module derives, decrypts or draws
for a key is a [secret_bytes](secret_bytes/README.md) or a [secret\<N\>](secret/README.md), which zero themselves. `secure_zero`
is for the program's own buffers: a stack array, a `std::array`, a buffer given to a `_to` form.

**The implementation has not been through an independent cryptographic audit.**

## Parameters

| Parameter | Description |
|---|---|
| `bytes` | the buffer zeroed |

## Return value

None.

## Complexity

Linear in the size of `bytes`.

## Exceptions

None.

## Notes

What it cannot reach: copies the compiler made in registers and on the stack, a value returned by copy (a digest, a
tag), a buffer a `vector` left behind when it grew, and a managed buffer after the program dropped it, which keeps
its bytes until the collector gives its space out again. Keep secrets in few places and in memory of fixed address
(the stack, a `unique_ptr`), fill them with [random::fill](random/fill.md) or a `derive_to`, and clear them when done.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

#include <array>

using namespace sgcl;

int main() {
    std::array<byte, 32> key;
    crypto::pbkdf2<crypto::sha256>::derive_to(key, "correct horse", "a salt", 1000);
    // ... the key used ...
    crypto::secure_zero(key);

    std::array<byte, 32> zeros{};
    println("{}", key == zeros);
}
```

Output:

```text
true
```

## See also

- [secret_bytes](secret_bytes/README.md): a secret that zeroes itself
- [random](random/README.md): random bytes into a buffer
- [constant_time](constant_time/README.md): the comparison of secrets
