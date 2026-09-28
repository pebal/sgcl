# sgcl::crypto::secret

```cpp
#include "sgcl/crypto/secret.h"   // or "sgcl/crypto/crypto.h"

namespace sgcl::crypto {
    template<size_t N> class secret;   // N secret bytes, move-only, zeroed when they go
}
```

N bytes that are a secret — an ECDH shared secret, a private key's scalar — as the module gives them out: every accessor of private material returns one — the shared secrets of [`x25519`](x25519.md), [`p256`](p256.md) and [`p384`](p384.md) (`secret<32>`, `secret<32>`, `secret<48>`), a private key's `bytes()` (its scalar, or X25519's 32 bytes), [`ed25519`](ed25519.md)'s `seed()` (`secret<32>`) and `bytes()` (`secret<64>`). Public keys stay `array<byte, N>`. The bytes live in the object itself, so they have one place, and that place is cleared when the object goes.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **Move-only**: a copy is asked for by name with `clone()`; a move leaves the source zeroed; the destructor zeroes the bytes with stores the compiler cannot drop ([`secure_zero`](secure_zero.md)).
- **Read in place**: `bytes()` (and the conversion to `slice<const byte>`) gives a slice without an owner over the object's own bytes, valid while the object lives. Every function of the module that takes bytes takes it: `hkdf_sha256::derive(salt, shared, info, 32)`, `hmac_sha256(key)`. Compare two with `==`, which is [`constant_time::equal`](constant_time.md), never with a loop that stops at the first difference.
- **Where it lives**: on the stack or in a `unique_ptr`, a secret is gone when its scope ends; in a managed object it stays in memory until the cycle that finds the object dead. Copies made of `bytes()` into buffers of the program's own are the program's to clear.

## Members

```cpp
static constexpr size_t size = N;

secret(secret&& other) noexcept;              // other zeroed
secret& operator=(secret&& other) noexcept;
secret(const secret&) = delete;
~secret();                                    // the bytes zeroed
secret clone() const noexcept;

slice<const byte> bytes() const noexcept;
operator slice<const byte>() const noexcept;

friend bool operator==(const secret& a, const secret& b) noexcept;   // in constant time
```

## See also

[`p256`](p256.md), [`p384`](p384.md), [`hkdf`](hkdf.md), [`secure_zero`](secure_zero.md), [`constant_time`](constant_time.md).
