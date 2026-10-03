[sgcl](../../README.md) › [crypto](../README.md)

# sgcl::crypto::constant_time

```cpp
#include "sgcl/crypto/constant_time.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::constant_time {
    [[nodiscard]] bool equal(const slice<const byte>& a, const slice<const byte>& b) noexcept;
}
```

`sgcl::crypto::constant_time` is comparisons whose time does not depend on the bytes compared, Go's `crypto/subtle`:
what a tag, a MAC, a token or a password hash is checked with. `==` and `memcmp` stop at the first byte that differs,
and an attacker who can send forgeries and time the answers learns, byte by byte, how much of a forgery was right.
[equal](equal.md) reads every byte of both whatever they hold, ORs the differences together, and turns
the sum into the result with no branch on it; an empty asm keeps the compiler from turning it back into an early
exit.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **The lengths are not secret**: two slices of different sizes are unequal at once, as in Go. A tag is compared with
  one of the length its protocol fixes.
- **`[[nodiscard]]`**: a comparison whose result is dropped is a check that was never made; the compiler warns. The
  library's own checks — [hmac](../hmac/README.md)'s `verify`, the AEADs' `open`, the `==` of [secret\<N\>](../secret/README.md) and
  [secret_bytes](../secret_bytes/README.md) — go through it.
- **Takes whatever converts to a slice of bytes**: a digest (`array<byte, N>`), a `vector<byte>`, a `std::array`, a
  `std::vector<std::byte>`, a secret, a slice.

## Member functions

| Function | Description |
|---|---|
| [equal](equal.md) | checks whether two slices hold the same bytes, in a time that depends on their length only |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

// A token checked against the digest the server stored of it
bool token_matches(const string& presented, const array<byte, 32>& stored_digest) {
    return crypto::constant_time::equal(crypto::sha256::of(presented), stored_digest);
}

int main() {
    array<byte, 32> stored = crypto::sha256::of("8f2c51d0e4a7");
    println("{}", token_matches("8f2c51d0e4a7", stored));
    println("{}", token_matches("8f2c51d0e4a8", stored));
}
```

Output:

```text
true
false
```

## See also

- [hmac](../hmac/README.md): a tag under a key, and its `verify`
- [secure_zero](../secure_zero.md): zeros the compiler cannot drop
