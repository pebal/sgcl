[sgcl](../../README.md) › [crypto](../README.md)

# sgcl::crypto::scrypt

```cpp
#include "sgcl/crypto/scrypt.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    class scrypt;
}
```

`sgcl::crypto::scrypt` is scrypt of RFC 7914, Go's `golang.org/x/crypto/scrypt`: a key from a password, made slow and
hungry for memory on purpose, so that each guess of an attacker costs `128 · r · N` bytes of memory as well as time.
PBKDF2-HMAC-SHA256 spreads the password and the salt over `p` blocks, each block goes through ROMix — `N` blocks
written in turn, then read back at places the data chooses — and a second PBKDF2 over the blocks gives the key. It
is the key derivation of Tarsnap, of Litecoin, of many wallets and of formats that name it; its costs are a
[options](../scrypt-options.md), Go's recommended `N = 32768`, `r = 8`, `p = 1` (32 MiB) by default.

For storing passwords to check later, [argon2](../argon2/README.md) is the better choice and the one with a stored
form: scrypt has no standard string of its own.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **Costs out of their ranges are a broken contract**: `std::invalid_argument`, as for [pbkdf2](../pbkdf2/README.md).
- **The memory is plain memory** from the system, never the collector's, zeroed before it is freed; the `p` blocks
  go through ROMix one after the other in one buffer, as Go and OpenSSL run them.
- **Side channels**: ROMix reads its memory at places the data chooses, by the design of RFC 7914; every operation of
  Salsa20/8 is an addition, a rotation or a XOR.

## Member types

| Type | Definition |
|---|---|
| [options](../scrypt-options.md) | the costs: N, r, p |

## Member functions

| Function | Description |
|---|---|
| [derive](derive.md) | a key of `n` bytes from a password and a salt (static) |
| [derive_to](derive_to.md) | the same into the program's buffer (static) |

## Complexity

Linear in `N · r · p`: two passes of `N` BlockMixes of `2r` Salsa20/8 cores for each of the `p` blocks. One path,
plain C++, on every processor: Salsa20/8 on NEON was measured at 84 ns a core against 45 for the plain one, its
rounds a chain of dependent operations where four scalar quarter-rounds run side by side.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::secret_bytes key = crypto::scrypt::derive("correct horse battery staple", "0123456789abcdef");
    println(encoding::hex::encode(key));
}
```

Output:

```text
f6b71517e0d9f2e53beeacf71ffbf6f7e9f683c73cefb00e0915d242f0bf7ecd
```

## See also

- [options](../scrypt-options.md): N, r and p
- [argon2](../argon2/README.md): the password hash for storage
- [pbkdf2](../pbkdf2/README.md): a key from a password where a format names PBKDF2
- [The module](../README.md)
