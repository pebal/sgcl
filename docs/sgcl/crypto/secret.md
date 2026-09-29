# sgcl::crypto::secret

```cpp
#include "sgcl/crypto/secret.h"   // or "sgcl/crypto/crypto.h"

namespace sgcl::crypto {
    template<size_t N> class secret;   // N secret bytes, move-only, zeroed when they go
    class secret_bytes;                // the same for a length known when the program runs
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

## secret_bytes

```cpp
class secret_bytes {
public:
    static constexpr size_t inline_capacity = 64;

    secret_bytes() noexcept;                      // empty
    explicit secret_bytes(size_t n);              // n bytes, all zero
    secret_bytes(secret_bytes&& other) noexcept;  // other left empty
    secret_bytes& operator=(secret_bytes&& other) noexcept;
    secret_bytes(const secret_bytes&) = delete;
    ~secret_bytes();                              // what it held zeroed
    secret_bytes clone() const;

    slice<const byte> as_slice() const noexcept;
    slice<byte> as_slice() noexcept;
    operator slice<const byte>() const noexcept;

    size_t size() const noexcept;
    bool empty() const noexcept;
    void resize(size_t n);                        // the prefix kept, the rest zero

    friend bool operator==(const secret_bytes& a, const secret_bytes& b) noexcept;   // in constant time
};

expected<secret_bytes, io::error> read_secret(const string& path);   // "sgcl/crypto/read_secret.h"
```

A secret whose length is known only when the program runs: what [`hkdf`](hkdf.md) and [`pbkdf2`](pbkdf2.md) derive, SHAKE's output ([`sha3`](sha3.md)), a key from [`random::secret`](random.md), a private key's export, a key file read by `read_secret`. The module's rule: a secret is never in managed memory. A plaintext is not a secret of this kind: what an [AEAD](aead.md) opens and what [`rsa`](rsa.md) decrypts is the user's data and comes back as a `vector<byte>` (their `_to` forms write into a buffer of the program's, a `secret_bytes` among them, for a key unwrapped).

- **Where the bytes are.** Up to 64 bytes live in the object itself, so keys of 32, 48 or 64 bytes, derived keys and shared secrets allocate nothing. Past that they live in a block of plain memory (`::operator new`), zeroed before it is freed. A growth zeroes the block it leaves.
- **Move-only.** A copy is asked for by name with `clone()`; a move leaves the source empty. The destructor zeroes what the object held.
- **Read and written through `as_slice()`.** It gives a slice without an owner, which every function of the module that takes bytes takes (the conversion to `slice<const byte>` makes that implicit). There is no `push_back` and no `operator[]`: a secret is not a container.
- **Where it lives.** On the stack or in a `unique_ptr`, its bytes are gone when its scope ends. Inside a managed object, its inline bytes would lie in managed memory until that object is collected, so keep it out of one.
- **Reading a file.** `read_secret` reads a file (a private key's PEM, a password) straight into a `secret_bytes`, never through a managed buffer; `io::read_file` gives a managed `vector<byte>`.

## See also

[`p256`](p256.md), [`p384`](p384.md), [`hkdf`](hkdf.md), [`secure_zero`](secure_zero.md), [`constant_time`](constant_time.md).
