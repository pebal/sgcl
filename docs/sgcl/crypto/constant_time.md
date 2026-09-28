# sgcl::crypto::constant_time

```cpp
#include "sgcl/crypto/constant_time.h"   // or "sgcl/crypto/crypto.h"

namespace sgcl::crypto::constant_time {
    [[nodiscard]] bool equal(const slice<const byte>& a, const slice<const byte>& b) noexcept;
}
```

A comparison whose time does not depend on the bytes compared, Go's `subtle.ConstantTimeCompare`: what a tag, a MAC or a token is checked with. `==` and `memcmp` stop at the first byte that differs, and an attacker who can send forgeries and time the answers learns, byte by byte, how much of a forgery was right; `equal` reads every byte of both whatever they hold and ORs the differences together, and turns the sum into the result with no branch on it (an empty asm keeps the compiler from turning it back into an early exit).

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **The lengths are not secret**: two slices of different sizes are unequal at once, as in Go. A tag is compared with one of the length its protocol fixes.
- **`[[nodiscard]]`**: a comparison whose result is dropped is a check that was never made; the compiler warns. The library's own checks — [`hmac::verify`](hmac.md), and the AEADs' `open` — go through it.
- **Takes whatever converts to a slice of bytes**: a digest (`array<byte, N>`), a `vector<byte>`, a `std::array`, a `std::vector<std::byte>`, a slice.

## Example

```cpp
#include "sgcl/crypto/constant_time.h"
#include "sgcl/crypto/sha256.h"

using namespace sgcl;

bool token_matches(const slice<const byte>& presented, const array<byte, 32>& stored_digest) {
    return crypto::constant_time::equal(crypto::sha256::of(presented), stored_digest);
}
```

## See also

[`hmac`](hmac.md) (`verify`); [`secure_zero`](secure_zero.md).
